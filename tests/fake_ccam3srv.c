#include "common.h"
#include "ccam3_crypto.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// fake server CCcam3 (implementa o fluxo do cccam3 original):
// login -> ACK RSA_AES -> ECM -> CW
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

int main(int argc, char **argv)
{
	int port = atoi(argv[1]);
	int ls = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in a; memset(&a,0,sizeof(a));
	a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = INADDR_ANY;
	bind(ls, (struct sockaddr*)&a, sizeof(a));
	listen(ls, 5);
	printf("fake-cccam3 listening on %d\n", port); fflush(stdout);

	for (;;) {
		int fd = accept(ls, NULL, NULL);
		if (fd < 0) continue;
		// 1. login
		uint8_t hdr[12];
		if (recv_exact(fd, hdr, 12) != 0) { close(fd); continue; }
		uint32_t total = rbe32(hdr+4);
		uint8_t buf[1024];
		if (recv_exact(fd, buf, (int)total-12) != 0) { close(fd); continue; }
		uint8_t cseed[16]; memcpy(cseed, buf, 16);
		int off = 16;
		char user[64], pass[64];
		int ul = (int)strlen((char*)buf+off); memcpy(user, buf+off, ul); user[ul]=0; off += ul+1;
		int pl = (int)strlen((char*)buf+off); memcpy(pass, buf+off, pl); pass[pl]=0; off += pl+1;
		uint32_t ver = rbe32(buf+off);
		printf("login: user=%s pass=%s ver=%u\n", user, pass, ver); fflush(stdout);
		// 2. ACK RSA_AES (60 bytes): seed[16]+iv[12]+ct[16]+tag[16]
		uint8_t sseed[16], ack[60];
		ccam3_randbytes(sseed, 16);
		uint8_t salt[32]; memcpy(salt, cseed, 16); memcpy(salt+16, sseed, 16);
		uint8_t key[32];
		ccam3_pbkdf2_sha256(pass, (size_t)pl, salt, 32, 10000, key, 32);
		memcpy(ack, sseed, 16);
		ccam3_randbytes(ack+16, 12);
		ccam3_aes256_gcm_encrypt(key, ack+16, sseed, 16, ack+28, ack+44);
		uint8_t out[1024];
		be32(out, 0x02); be32(out+4, 12+60); out[8]=0; out[9]=0x11; out[10]=0; out[11]=0;
		memcpy(out+12, ack, 60);
		send(fd, out, 12+60, 0);
		printf("ack enviado (RSA_AES)\n"); fflush(stdout);
		// 3. loop ECM->CW
		uint8_t cw0[16] = {0x01,0x02,0x03,0x06,0x10,0x11,0x12,0x33,0x20,0x21,0x22,0x63,0x30,0x31,0x32,0x93};
		for (;;) {
			if (recv_exact(fd, hdr, 12) != 0) break;
			total = rbe32(hdr+4);
			if (total > 12 && recv_exact(fd, buf, (int)total-12) != 0) break;
			if (rbe32(hdr) == 0x03) {
				uint16_t caid = (uint16_t)((buf[0]<<8)|buf[1]);
				uint16_t sid  = (uint16_t)((buf[4]<<8)|buf[5]);
				printf("ecm: caid=%04x sid=%04x\n", caid, sid); fflush(stdout);
				uint8_t cwmsg[1024];
				be32(cwmsg, 0x04); be32(cwmsg+4, 12+4+16+1+6); cwmsg[8]=0; cwmsg[9]=0x11; cwmsg[10]=0; cwmsg[11]=0;
				int o = 12;
				be32(cwmsg+o, 0); o+=4;      // ecm_time
				memcpy(cwmsg+o, cw0, 16); o+=16;
				cwmsg[o++] = 1;               // hop
				cwmsg[o++] = (uint8_t)(caid>>8); cwmsg[o++] = (uint8_t)caid;
				cwmsg[o++] = (uint8_t)(buf[2]); cwmsg[o++] = (uint8_t)(buf[3]);
				cwmsg[o++] = (uint8_t)(sid>>8); cwmsg[o++] = (uint8_t)sid;
				send(fd, cwmsg, o, 0);
				printf("cw enviado\n"); fflush(stdout);
			}
		}
		close(fd);
	}
	return 0;
}
