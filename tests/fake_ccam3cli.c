#include "common.h"
#include "ccam3_crypto.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void be32(uint8_t *p, uint32_t v){ p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }
static uint32_t rbe32(const uint8_t *p){ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }

static int recv_exact(int fd, uint8_t *b, int n)
{
	int got = 0;
	while (got < n) {
		int r = recv(fd, b+got, n-got, 0);
		if (r <= 0) return -1;
		got += r;
	}
	return 0;
}

static int send_all(int fd, const uint8_t *b, size_t n)
{
	size_t sent = 0;
	while (sent < n) {
		int r = send(fd, b+sent, n-sent, 0);
		if (r <= 0) return -1;
		sent += (size_t)r;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *host = argv[1];
	int port = atoi(argv[2]);
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in a; memset(&a,0,sizeof(a));
	a.sin_family = AF_INET; a.sin_port = htons(port);
	inet_pton(AF_INET, host, &a.sin_addr);
	if (connect(fd, (struct sockaddr*)&a, sizeof(a)) != 0) { printf("connect fail\n"); return 1; }

	// LOGIN (v301 -> RSA_AES)
	uint8_t seed[16];
	ccam3_randbytes(seed, 16);
	uint8_t buf[1024];
	int off = 0;
	memcpy(buf+off, seed, 16); off+=16;
	strcpy((char*)buf+off, "cli"); off += 4;
	strcpy((char*)buf+off, "pass"); off += 5;
	be32(buf+off, 301); off += 4;
	uint8_t msg[1024];
	be32(msg, 0x01); be32(msg+4, 12+off); msg[8]=0; msg[9]=0; msg[10]=0; msg[11]=0;
	memcpy(msg+12, buf, off);
	send_all(fd, msg, 12+off);

	// ACK
	uint8_t hdr[12];
	if (recv_exact(fd, hdr, 12) != 0) { printf("no ack hdr\n"); return 1; }
	uint32_t total = rbe32(hdr+4);
	if (recv_exact(fd, buf, (int)total-12) != 0) { printf("no ack\n"); return 1; }
	printf("ack len=%u crypt=%02x\n", total-12, hdr[9]);
	if (total-12 >= 60) {
		uint8_t salt[32];
		memcpy(salt, seed, 16);
		memcpy(salt+16, buf, 16);
		uint8_t key[32];
		ccam3_pbkdf2_sha256("pass", 4, salt, 32, 10000, key, 32);
		uint8_t pt[16];
		if (ccam3_aes256_gcm_decrypt(key, buf+16, buf+28, 16, buf+44, pt)==0 && !memcmp(pt, buf, 16))
			printf("handshake RSA_AES OK\n");
		else printf("handshake FAIL\n");
	}

	// ECM (2600:1fff)
	uint8_t ecmdata[8] = {0x80,0x00,0x01,0x02,0x03,0x04,0x05,0x06};
	int o = 0;
	buf[o++] = 0x26; buf[o++] = 0x00;        // caid
	buf[o++] = 0x00; buf[o++] = 0x00;        // provid
	buf[o++] = 0x1f; buf[o++] = 0xff;        // sid
	memcpy(buf+o, ecmdata, 8); o += 8;
	be32(msg, 0x03); be32(msg+4, 12+o); msg[8]=0; msg[9]=0x11; msg[10]=0; msg[11]=0;
	memcpy(msg+12, buf, o);
	send_all(fd, msg, 12+o);
	printf("ecm enviado\n");

	// CW
	if (recv_exact(fd, hdr, 12) != 0) { printf("no cw hdr\n"); return 1; }
	total = rbe32(hdr+4);
	if (recv_exact(fd, buf, (int)total-12) != 0) { printf("no cw\n"); return 1; }
	printf("cw msg len=%u: ", total-12);
	int i;
	for (i=0; i<(int)(total-12); i++) printf("%02x", buf[i]);
	printf("\n");
	return 0;
}
