///////////////////////////////////////////////////////////////////////////////
// CCcam 3 SERVER (protocolo CCcam 3 - boxes CCcam3 ligam-se a nos)
// Login com handshake RSA_AES (version>=300) ou legacy; ECM->CW
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "ccam3_crypto.h"

#define C3_MSG_LOGIN      0x01
#define C3_MSG_LOGIN_ACK  0x02
#define C3_MSG_ECM        0x03
#define C3_MSG_CW         0x04
#define C3_MSG_KEEPALIVE  0x06

#define C3_CRYPT_NONE     0x00
#define C3_CRYPT_RC4      0x01
#define C3_CRYPT_AES      0x02
#define C3_CRYPT_AES_GCM  0x11

#define C3_HDR_SIZE       12
#define C3_MAXMSGSIZE     1024
#define CCAM3_MAX_PFD     256

static void c3s_wbe32(uint8_t *p, uint32_t v)
{
	p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}

static uint32_t c3s_rbe32(const uint8_t *p)
{
	return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

static int c3s_send(int fd, struct cc_client_data *cli, uint32_t msgid, const uint8_t *payload, size_t plen)
{
	uint8_t buf[C3_MAXMSGSIZE];
	c3s_wbe32(buf, msgid);
	c3s_wbe32(buf+4, (uint32_t)(C3_HDR_SIZE+plen));
	buf[8]=0; buf[9]=cli->ccam3crypt; buf[10]=0; buf[11]=0;
	if (plen) memcpy(buf+C3_HDR_SIZE, payload, plen);
	size_t total = C3_HDR_SIZE+plen;
	if (cli->ccam3crypt==C3_CRYPT_RC4) ccam3_rc4(cli->ccam3key, 20, buf+C3_HDR_SIZE, plen);
	else if (cli->ccam3crypt==C3_CRYPT_AES) ccam3_aes256_ecb(cli->ccam3key, buf+C3_HDR_SIZE, plen, 1);
	size_t sent = 0;
	while (sent < total) {
		int n = send(fd, buf+sent, total-sent, MSG_NOSIGNAL);
		if (n <= 0) return 0;
		sent += (size_t)n;
	}
	return 1;
}

// le uma mensagem completa do cliente; devolve msgid e payload
static int c3s_recv(int fd, struct cc_client_data *cli, uint8_t *buf, int bufsize, uint32_t *msgid, uint8_t **payload, int *plen)
{
	uint8_t hdr[C3_HDR_SIZE];
	if (recv_nonb(fd, hdr, C3_HDR_SIZE, 10000) != C3_HDR_SIZE) return -1;
	uint32_t total = c3s_rbe32(hdr+4);
	if (total < C3_HDR_SIZE || total > (uint32_t)bufsize) return -1;
	int n = (int)total - C3_HDR_SIZE;
	if (n > 0 && recv_nonb(fd, buf, n, 10000) != n) return -1;
	if (n > 0) {
		if (hdr[9]==C3_CRYPT_RC4) ccam3_rc4(cli->ccam3key, 20, buf, n);
		else if (hdr[9]==C3_CRYPT_AES) ccam3_aes256_ecb(cli->ccam3key, buf, n, 0);
		else if (hdr[9]==C3_CRYPT_AES_GCM) {
			if (n > 28) {
				uint8_t iv[12];
				memcpy(iv, buf, 12);
				uint8_t tag[16];
				memcpy(tag, buf+n-16, 16);
				if (ccam3_aes256_gcm_decrypt(cli->ccam3key, iv, buf+12, n-28, tag, buf+12)==0) n -= 28;
			}
		}
	}
	if (msgid) *msgid = c3s_rbe32(hdr);
	if (payload) *payload = buf;
	if (plen) *plen = n;
	return (int)total;
}

// procura a F-line livre para este user/pass
static struct cc_client_data *ccam3_find_fline(struct cccam_server_data *cccam, const char *user, const char *pass)
{
	struct cc_client_data *cli = cccam->client;
	while (cli) {
		if (cli->handle<=0 && !strcmp(cli->user, user) && !strcmp(cli->pass, pass)) return cli;
		cli = cli->next;
	}
	return NULL;
}

static void ccam3_cli_parsemsg(struct cccam_server_data *cccam, struct cc_client_data *cli, int fd)
{
	uint8_t buf[C3_MAXMSGSIZE];
	uint32_t msgid = 0;
	uint8_t *pay = NULL;
	int paylen = 0;
	if (c3s_recv(fd, cli, buf, C3_MAXMSGSIZE, &msgid, &pay, &paylen) <= 0) {
		cc_disconnect_cli(cli);
		return;
	}
	cli->lastactivity = GetTickCount();
	if (msgid==C3_MSG_KEEPALIVE) return;
	if (msgid!=C3_MSG_ECM) return;
	if (paylen < 6+1) return;

	cli->ecmnb++;
	cli->lastecmtime = GetTickCount();
	uint16_t caid = (uint16_t)((pay[0]<<8)|pay[1]);
	uint32_t provid16 = (uint32_t)((pay[2]<<8)|pay[3]);
	uint16_t sid = (uint16_t)((pay[4]<<8)|pay[5]);
	uint8_t *data = pay+6;
	int datalen = paylen-6;
	uint32_t provid = ecm_getprovid(data, caid);
	if (!provid) provid = provid16<<8;

	// Perfil: por caid/prov ou qualquer perfil com o caid
	struct cardserver_data *cs = getcsbycaidprov(caid, provid);
	if (!cs) cs = getcsbycaprovid(caid, provid);
	if (!cs) {
		cli->ecmdenied++;
		mlogf(LOGINFO,getdbgflag(DBG_CCCAM,cli->parent->id,cli->id)," <|> decode failed to CCcam3 client '%s', ch %04x:%06x:%04x - no profile\n",cli->user,caid,provid,sid);
		return; // o cccam3 nao tem NOK - nao responde
	}

	pthread_mutex_lock(&prg.lockecm);
	if (cli->ecm.busy) {
		cli->ecmdenied++;
		pthread_mutex_unlock(&prg.lockecm);
		return;
	}
	ECM_DATA *ecm = store_ecmdata(cs, data, datalen, sid, caid, provid);
	cc_store_ecmclient(ecm, cs->id, cli);
	mlogf(LOGINFO,getdbgflagpro(DBG_CCCAM,cli->parent->id,cli->id,cs->id)," <- ecm from CCcam3 client '%s' ch %04x:%06x:%04x:%08x\n",cli->user,caid,provid,sid,ecm->hash);
#ifdef ECMLIST
	cli->nextEcm = ecm->client.cccam;
	ecm->client.cccam = cli;
#endif
	ecm->dcwstatus = STAT_DCW_WAIT;
	pthread_mutex_unlock(&prg.lockecm);
	pipe_wakeup( prg.pipe.ecm[1] );
}

// envio da CW a um cliente CCcam3 (chamado pelo cc_senddcw_cli)
int ccam3_send_cw_cli(struct cc_client_data *cli, ECM_DATA *ecm)
{
	uint8_t m[C3_MAXMSGSIZE];
	c3s_wbe32(m, C3_MSG_CW);
	c3s_wbe32(m+4, (uint32_t)(C3_HDR_SIZE+4+16+1+6));
	m[8]=0; m[9]=cli->ccam3crypt; m[10]=0; m[11]=0;
	int o = C3_HDR_SIZE;
	c3s_wbe32(m+o, 0); o+=4;
	memcpy(m+o, ecm->cw, 16); o+=16;
	m[o++] = 1; // hop
	m[o++] = (uint8_t)(ecm->caid>>8);
	m[o++] = (uint8_t)(ecm->caid&0xff);
	m[o++] = (uint8_t)(ecm->provid>>16);
	m[o++] = (uint8_t)(ecm->provid&0xff);
	m[o++] = (uint8_t)(ecm->sid>>8);
	m[o++] = (uint8_t)(ecm->sid&0xff);
	size_t total = (size_t)o;
	if (cli->ccam3crypt==C3_CRYPT_RC4) ccam3_rc4(cli->ccam3key, 20, m+C3_HDR_SIZE, total-C3_HDR_SIZE);
	else if (cli->ccam3crypt==C3_CRYPT_AES) ccam3_aes256_ecb(cli->ccam3key, m+C3_HDR_SIZE, total-C3_HDR_SIZE, 1);
	size_t sent = 0;
	while (sent < total) {
		int n = send(cli->handle, m+sent, total-sent, MSG_NOSIGNAL);
		if (n <= 0) return 0;
		sent += (size_t)n;
	}
	return 1;
}

void *ccam3_srv_thread(void *param)
{
	struct cccam_server_data *cccam = (struct cccam_server_data *)param;
	struct pollfd pfd[CCAM3_MAX_PFD];

	prctl(PR_SET_NAME,"CCcam3 Srv",0,0,0);

	while (!prg.restart) {
		int pfdcount = 0;
		int cliidx[CCAM3_MAX_PFD];
		memset(cliidx, 0, sizeof(cliidx));

		pfd[pfdcount].fd = cccam->ccam3_handle;
		pfd[pfdcount].events = POLLIN|POLLPRI;
		cliidx[pfdcount] = -1;
		pfdcount++;

		struct cc_client_data *cli = cccam->client;
		while (cli && pfdcount<CCAM3_MAX_PFD) {
			if (cli->isccam3 && (cli->handle>0)) {
				pfd[pfdcount].fd = cli->handle;
				pfd[pfdcount].events = POLLIN|POLLPRI;
				cliidx[pfdcount] = cli->id;
				pfdcount++;
			}
			cli = cli->next;
		}

		int retval = poll(pfd, pfdcount, 1000);
		if (retval <= 0) continue;

		int i;
		for (i=0; i<pfdcount; i++) {
			if (!(pfd[i].revents & (POLLIN|POLLPRI))) continue;
			if (cliidx[i] < 0) {
				// nova ligacao
				int newfd = accept(cccam->ccam3_handle, NULL, NULL);
				if (newfd <= 0) continue;
				SetSocketNoDelay(newfd);
				SetSoketNonBlocking(newfd);
				// ---- LOGIN (framing CCcam3) ----
				struct cc_client_data tmpcli;
				memset(&tmpcli, 0, sizeof(tmpcli));
				tmpcli.ccam3crypt = C3_CRYPT_NONE;
				uint8_t buf[C3_MAXMSGSIZE];
				uint32_t msgid = 0;
				uint8_t *pay = NULL;
				int paylen = 0;
				int r = c3s_recv(newfd, &tmpcli, buf, C3_MAXMSGSIZE, &msgid, &pay, &paylen);
				if (r<=0 || msgid!=C3_MSG_LOGIN || paylen < 16+1+1+4) {
					close(newfd);
					continue;
				}
				uint8_t cseed[16];
				memcpy(cseed, pay, 16);
				int off = 16;
				char user[64], pass[64];
				size_t ul = strnlen((char*)pay+off, (size_t)(paylen-off));
				if (ul>=sizeof(user)-1) { close(newfd); continue; }
				memcpy(user, pay+off, ul); user[ul]=0; off += (int)ul+1;
				size_t pl = strnlen((char*)pay+off, (size_t)(paylen-off));
				if (pl>=sizeof(pass)-1) { close(newfd); continue; }
				memcpy(pass, pay+off, pl); pass[pl]=0; off += (int)pl+1;
				uint32_t ver = c3s_rbe32(pay+off);

				cli = ccam3_find_fline(cccam, user, pass);
				if (!cli) {
					mlogf(LOGWARNING,getdbgflag(DBG_CCCAM,cccam->id,0)," CCcam3: login failed from client '%s' (%s)\n", user, ip2string(0));
					close(newfd);
					continue;
				}
				// ---- handshake ----
				uint8_t ack[60];
				int acklen;
				if (ver >= 300) {
					// RSA_AES: seed[16]+iv[12]+ct[16]+tag[16]
					uint8_t sseed[16];
					ccam3_randbytes(sseed, 16);
					uint8_t salt[32];
					memcpy(salt, cseed, 16);
					memcpy(salt+16, sseed, 16);
					ccam3_pbkdf2_sha256(pass, pl, salt, 32, 10000, cli->ccam3key, 32);
					memcpy(ack, sseed, 16);
					ccam3_randbytes(ack+16, 12);
					ccam3_aes256_gcm_encrypt(cli->ccam3key, ack+16, sseed, 16, ack+28, ack+44);
					acklen = 60;
					cli->ccam3crypt = C3_CRYPT_AES_GCM;
				}
				else {
					// legacy
					uint8_t concat[16+16+64];
					memcpy(concat, cseed, 16);
					memcpy(concat+16, ack, 16);
					strcpy((char*)concat+32, pass);
					ccam3_sha1(concat, 32+pl, cli->ccam3key);
					acklen = 16;
					cli->ccam3crypt = C3_CRYPT_NONE;
				}
				if (!c3s_send(newfd, cli, C3_MSG_LOGIN_ACK, ack, (size_t)acklen)) {
					close(newfd);
					continue;
				}
				// ---- registar cliente ----
				cli->handle = newfd;
				cli->isccam3 = 1;
				cli->connection.status = 1;
				cli->connection.time = GetTickCount();
				cli->lastactivity = GetTickCount();
				cli->ecm.busy = 0;
				cli->nblogin++;
				sprintf(cli->version, "3.0.1");
				mlogf(LOGINFO,getdbgflag(DBG_CCCAM,cccam->id,cli->id)," CCcam3: client '%s' connected (ver %u)\n", cli->user, ver);
			}
			else {
				// mensagem de cliente ccam3 existente
				struct cc_client_data *c = cccam->client;
				while (c) {
					if (c->isccam3 && c->id==(uint32_t)cliidx[i]) break;
					c = c->next;
				}
				if (c) ccam3_cli_parsemsg(cccam, c, c->handle);
			}
		}
	}
	return NULL;
}
