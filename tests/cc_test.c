#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(void)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	struct timeval tv;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(16400);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) { perror("connect"); return 1; }
	tv.tv_sec = 5; tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	unsigned char buf[64];
	int total = 0;
	int i;
	for (i = 0; i < 8; i++) {
		int n = recv(sock, buf+total, 16-total, 0);
		if (n <= 0) { printf("recv ret=%d (total %d)\n", n, total); break; }
		total += n;
		printf("chunk %d bytes (total %d)\n", n, total);
		if (total >= 16) break;
	}
	printf("total %d bytes:", total);
	for (i = 0; i < total; i++) printf(" %02x", buf[i]);
	printf("\n");
	return 0;
}
