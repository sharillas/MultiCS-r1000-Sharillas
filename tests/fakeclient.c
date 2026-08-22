#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "des.h"
#include "md5.h"

#define CWS_NETMSGSIZE 1024

static int m_send(int sock, unsigned char *buffer, int len, unsigned char *deskey,
                  unsigned short msgid, unsigned short sid, unsigned short caid, unsigned int provid)
{
	unsigned char netbuf[CWS_NETMSGSIZE];
	if (!len) len = 0x0fff & ((buffer[1]<<8)|buffer[2]);
	else {
		buffer[1] = (buffer[1] & 0xf0) | (((len-3)>>8) & 0x0f);
		buffer[2] = (len-3) & 0xff;
	}
	if (len < 3 || len + 12 > CWS_NETMSGSIZE) return -1;
	memset(netbuf, 0, 12);
	netbuf[2] = (msgid>>8) & 0xff;
	netbuf[3] = msgid & 0xff;
	netbuf[4] = (sid>>8) & 0xff;
	netbuf[5] = sid & 0xff;
	netbuf[6] = (caid>>8) & 0xff;
	netbuf[7] = caid & 0xff;
	netbuf[8] = (provid>>16) & 0xff;
	netbuf[9] = (provid>>8) & 0xff;
	netbuf[10] = provid & 0xff;
	memcpy(netbuf+12, buffer, len);
	len += 12;
	len = des_encrypt(netbuf, len, deskey);
	if (len < 0) return -1;
	netbuf[0] = (len-2) >> 8;
	netbuf[1] = (len-2) & 0xff;
	return send(sock, netbuf, len, MSG_NOSIGNAL);
}

static int m_recv(int sock, unsigned char *buffer, unsigned char *deskey, int timeout)
{
	unsigned char netbuf[CWS_NETMSGSIZE];
	int len = recv(sock, netbuf, 2, MSG_NOSIGNAL);
	if (len != 2) return -1;
	int netlen = (netbuf[0]<<8) | netbuf[1];
	if (netlen > CWS_NETMSGSIZE-2) return -1;
	len = recv(sock, netbuf+2, netlen, MSG_NOSIGNAL);
	if (len != netlen) return -1;
	len += 2;
	len = des_decrypt(netbuf, len, deskey);
	if (len < 15) return -2;
	int returnLen = (((netbuf[13] & 0x0f) << 8) | netbuf[14]) + 3;
	if (returnLen > len-12) return -1;
	memcpy(buffer, netbuf+12, returnLen);
	return returnLen;
}

int main(int argc, char **argv)
{
	const char *host = argv[1];
	int port = atoi(argv[2]);
	const char *user = "testuser";
	const char *pass = "testpass";
	unsigned char profkey[14] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x10,0x11,0x12,0x13,0x14};
	unsigned char keymod[14];
	unsigned char sessionkey[16];
	unsigned char buf[CWS_NETMSGSIZE];
	char passcrypt[120];
	int n, p;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, host, &addr.sin_addr);
	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
		perror("connect"); return 1;
	}

	n = recv(sock, keymod, 14, MSG_NOSIGNAL);
	printf("recv keymod %d bytes\n", n);
	if (n != 14) { printf("NO KEY\n"); return 1; }
	{
		int i;
		printf("keymod: ");
		for (i=0; i<14; i++) printf("%02x", keymod[i]);
		printf("\n");
	}

	des_login_key_get(keymod, profkey, 14, sessionkey);
	{
		int i;
		printf("sessionkey: ");
		for (i=0; i<16; i++) printf("%02x", sessionkey[i]);
		printf("\n");
	}

	__md5_crypt(pass, "$1$abcdefgh$", passcrypt);
	p = 0;
	buf[p++] = 0xE0;
	buf[p++] = 0;
	buf[p++] = 0;
	memcpy(buf+p, user, strlen(user)+1); p += strlen(user)+1;
	memcpy(buf+p, passcrypt, strlen(passcrypt)+1); p += strlen(passcrypt)+1;
	if (m_send(sock, buf, p, sessionkey, 0, 0, 0, 0) < 0) { printf("SEND LOGIN FAIL\n"); return 1; }
	n = m_recv(sock, buf, sessionkey, 5000);
	printf("login resp: n=%d buf0=%02x\n", n, n>0?buf[0]:0);
	if (n < 0 || buf[0] != 0xE1) { printf("LOGIN FAILED\n"); return 1; }
	printf("LOGIN OK\n");

	des_login_key_get(profkey, (unsigned char*)passcrypt, strlen(passcrypt), sessionkey);

	int seqmode = (argc>3 && !strcmp(argv[3], "seq"));
	int seqreplay = (argc>3 && !strcmp(argv[3], "seqreplay"));
	int nreq = seqmode ? 14 : (seqreplay ? 12 : 1);
	int r;
	for (r = 0; r < nreq; r++) {
		unsigned char ecm_payload[28];
		int k;
		int ecmid = r;
		if (seqmode && r == 11) ecmid = 7; // replay do ECM #7
		if (seqreplay && r == 9) ecmid = 2; // replay do ECM #2 (hash antigo)
		for (k = 0; k < 28; k++) ecm_payload[k] = 0xAA;
		ecm_payload[0] = 0x81;
		ecm_payload[1] = 0x00;
		ecm_payload[2] = 0x19; /* len-3 = 25 */
		ecm_payload[3] = 0x00; /* provid 000000 */
		ecm_payload[4] = 0x00;
		ecm_payload[5] = 0x00;
		ecm_payload[6] = (unsigned char)ecmid; /* hash varia por pedido */
		p = 0;
		memcpy(buf, ecm_payload, 28); p = 28;
		if (m_send(sock, buf, p, sessionkey, 1, 0x1FFF, 0x2600, 0) < 0) { printf("SEND ECM FAIL\n"); return 1; }
		n = m_recv(sock, buf, sessionkey, 8000);
		printf("req[%d] ecmid=%d resp: n=%d buf=%02x %02x %02x\n", r, ecmid, n, buf[0], buf[1], buf[2]);
		if (n >= 19 && buf[2] == 0x10) {
			int i;
			printf("  DCW: ");
			for (i=3; i<=18; i++) printf("%02x", buf[i]);
			printf("  SUCCESS\n");
		} else {
			printf("  SEM DCW\n");
		}
		fflush(stdout);
		if ((seqmode||seqreplay) && r < nreq-1) sleep(4);
	}
	return 0;
}
