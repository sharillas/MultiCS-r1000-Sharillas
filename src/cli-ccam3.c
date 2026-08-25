///////////////////////////////////////////////////////////////////////////////
// CCcam 3 (protocolo proprio do projeto CCcam-3.0.1 by Sharillas)
// Reader: o MultiCS liga-se a um server CCcam3 (linha C3:) e pede CWs
// Handshake: version >= 300 -> RSA_AES (PBKDF2-SHA256 + AES-256-GCM)
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "ccam3_crypto.h"

#define C3_MSG_LOGIN      0x01
#define C3_MSG_LOGIN_ACK  0x02
#define C3_MSG_ECM        0x03
#define C3_MSG_CW         0x04
#define C3_MSG_KEEPALIVE  0x06
#define C3_MSG_CARD_DATA  0x07

#define C3_CRYPT_NONE     0x00
#define C3_CRYPT_RC4      0x01
#define C3_CRYPT_AES      0x02
#define C3_CRYPT_AES_GCM  0x11

#define C3_HDR_SIZE       12
#define C3_MAXMSGSIZE     1024

// ---------- envio/rececao de mensagens (framing CCcam3) ----------

static void c3_wbe32(uint8_t *p, uint32_t v)
{
	p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}

static uint32_t c3_rbe32(const uint8_t *p)
{
	return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

// envia uma mensagem completa (header 12B + payload). crypt: encripta o
// payload conforme o modo de sessao (RC4/AES-ECB). AES_GCM/NONE = plaintext
// (comportamento identico ao cccam3 original)
static int c3_msg_send(int fd, struct server_data *srv, uint32_t msgid, const uint8_t *payload, size_t plen)
{
	uint8_t buf[C3_MAXMSGSIZE];
	if (plen > C3_MAXMSGSIZE - C3_HDR_SIZE - 28) return 0;
	c3_wbe32(buf, msgid);
	c3_wbe32(buf+4, (uint32_t)(C3_HDR_SIZE + plen));
	buf[8] = 0;
	buf[9] = srv->ccam3crypt;
	buf[10] = 0; buf[11] = 0;
	if (plen) memcpy(buf+C3_HDR_SIZE, payload, plen);
	size_t total = C3_HDR_SIZE + plen;
	if (srv->ccam3crypt==C3_CRYPT_RC4) ccam3_rc4(srv->ccam3key, 20, buf+C3_HDR_SIZE, plen);
	else if (srv->ccam3crypt==C3_CRYPT_AES) ccam3_aes256_ecb(srv->ccam3key, buf+C3_HDR_SIZE, plen, 1);
	size_t sent = 0;
	while (sent < total) {
		int n = send(fd, buf+sent, total-sent, MSG_NOSIGNAL);
		if (n <= 0) return 0;
		sent += (size_t)n;
	}
	return 1;
}

// le uma mensagem completa; devolve o tamanho total ou <=0 em erro
static int c3_msg_recv(int fd, struct server_data *srv, uint8_t *buf, int bufsize, uint32_t *msgid, uint8_t **payload, int *plen)
{
	uint8_t hdr[C3_HDR_SIZE];
	if (recv_nonb(fd, hdr, C3_HDR_SIZE, 5000) != C3_HDR_SIZE) return -1;
	uint32_t total = c3_rbe32(hdr+4);
	if (total < C3_HDR_SIZE || total > (uint32_t)bufsize) return -1;
	int n = (int)total - C3_HDR_SIZE;
	if (n > 0 && recv_nonb(fd, buf, n, 5000) != n) return -1;
	// decifrar payload
	if (n > 0) {
		if (hdr[9]==C3_CRYPT_RC4) ccam3_rc4(srv->ccam3key, 20, buf, n);
		else if (hdr[9]==C3_CRYPT_AES) ccam3_aes256_ecb(srv->ccam3key, buf, n, 0);
		else if (hdr[9]==C3_CRYPT_AES_GCM) {
			// payload = IV(12) + ct + tag(16) (aes-gcm) - so usado no handshake;
			// mensagens normais em AES_GCM vao em plaintext (cccam3 original)
			if (n > 28) {
				uint8_t iv[12];
				memcpy(iv, buf, 12);
				uint8_t tag[16];
				memcpy(tag, buf+n-16, 16);
				if (ccam3_aes256_gcm_decrypt(srv->ccam3key, iv, buf+12, n-28, tag, buf+12)==0) n -= 28;
			}
		}
	}
	if (msgid) *msgid = c3_rbe32(hdr);
	if (payload) *payload = buf;
	if (plen) *plen = n;
	return (int)total;
}

// ---------- connect ----------

int ccam3_connect_srv(struct server_data *srv, int fd)
{
	uint8_t buf[C3_MAXMSGSIZE];
	uint8_t seed[16];
	int n;

	if (fd < 0) return -1;
	srv->progname = NULL;
	memset(srv->version, 0, sizeof(srv->version));
	srv->ccam3crypt = C3_CRYPT_NONE;
	memset(srv->ccam3key, 0, sizeof(srv->ccam3key));

	// 1. LOGIN: seed[16] + user\0 + pass\0 + version(BE32=301)
	ccam3_randbytes(seed, 16);
	int off = 0;
	memcpy(buf+off, seed, 16); off += 16;
	int ul = (int)strlen(srv->user);
	if (ul > 63) ul = 63;
	memcpy(buf+off, srv->user, (size_t)ul); off += ul;
	buf[off++] = 0;
	int pl = (int)strlen(srv->pass);
	if (pl > 63) pl = 63;
	memcpy(buf+off, srv->pass, (size_t)pl); off += pl;
	buf[off++] = 0;
	c3_wbe32(buf+off, 301); off += 4;

	if (!c3_msg_send(fd, srv, C3_MSG_LOGIN, buf, (size_t)off)) {
		static char msg[]= "Login send failed";
		srv->statmsg = msg;
		return -2;
	}

	// 2. LOGIN_ACK: payload de 16 (legacy) ou 60 (RSA_AES) bytes
	uint32_t msgid = 0;
	uint8_t *pay = NULL;
	int paylen = 0;
	n = c3_msg_recv(fd, srv, buf, C3_MAXMSGSIZE, &msgid, &pay, &paylen);
	if (n <= 0 || msgid != C3_MSG_LOGIN_ACK) {
		static char msg[]= "Handshake ACK nao recebido";
		srv->statmsg = msg;
		return -2;
	}

	if (paylen >= 60) {
		// RSA_AES: seed[16] + iv[12] + ct[16] + tag[16]
		uint8_t salt[32];
		memcpy(salt, seed, 16);
		memcpy(salt+16, pay, 16);
		ccam3_pbkdf2_sha256(srv->pass, (size_t)strlen(srv->pass), salt, 32, 10000, srv->ccam3key, 32);
		uint8_t pt[16];
		if (ccam3_aes256_gcm_decrypt(srv->ccam3key, pay+16, pay+28, 16, pay+44, pt) != 0) {
			static char msg[]= "Handshake RSA_AES falhou (tag)";
			srv->statmsg = msg;
			return -2;
		}
		if (memcmp(pt, pay, 16)) {
			static char msg[]= "Handshake RSA_AES falhou (seed)";
			srv->statmsg = msg;
			return -2;
		}
		srv->ccam3crypt = C3_CRYPT_AES_GCM;
	}
	else if (paylen >= 16) {
		// legacy: chave SHA1(seed_c + seed_s + pass)
		uint8_t concat[16+16+64];
		memcpy(concat, seed, 16);
		memcpy(concat+16, pay, 16);
		strcpy((char*)concat+32, srv->pass);
		ccam3_sha1(concat, 32+strlen(srv->pass), srv->ccam3key); // 20 bytes
		srv->ccam3crypt = C3_CRYPT_NONE; // o cccam3 original mapeia legacy->NONE
	}
	else {
		static char msg[]= "Handshake ACK invalido";
		srv->statmsg = msg;
		return -2;
	}

	// 3. carta sintetica (o protocolo CCcam3 nao envia lista de cards)
	{
		struct cs_card_data *card = malloc(sizeof(struct cs_card_data));
		if (card) {
			memset(card, 0, sizeof(struct cs_card_data));
			card->caid = 0xFFFF; // qualquer caid
			card->uphops = 1;
			card->shareid = 0;
			card->next = srv->card;
			srv->card = card;
		}
	}

	static char msg[]= "Connected (CCcam3)";
	srv->statmsg = msg;
	srv->handle = fd;
	srv->connection.status = 1;
	srv->connection.time = GetTickCount();
	srv->busy = 0;
	srv->lastecmtime = 0;
	srv->lastdcwtime = 0;
	srv->msg.len = 0;
	srv->error = 0;
	sprintf(srv->version, "3.0.1");

	mlogf(LOGINFO,getdbgflag(DBG_SERVER,0,srv->id)," CCcam3: connected to server (%s:%d) user '%s'\n", srv->host->name, srv->port, srv->user);

#ifdef EPOLL_ECM
	pipe_pointer( prg.pipe.ecm[1], PIPE_SRV_CONNECTED, srv );
#else
	pipe_cmd( prg.pipe.ecm[1], PIPE_SRV_CONNECTED );
#endif
	return 0;
}

// ---------- send ECM ----------

int ccam3_sendecm_srv(struct server_data *srv, ECM_DATA *ecm)
{
	uint8_t buf[C3_MAXMSGSIZE];
	int off = 0;
	buf[off++] = (uint8_t)(ecm->caid>>8);
	buf[off++] = (uint8_t)(ecm->caid&0xff);
	buf[off++] = (uint8_t)(ecm->provid>>16);
	buf[off++] = (uint8_t)(ecm->provid&0xff);
	buf[off++] = (uint8_t)(ecm->sid>>8);
	buf[off++] = (uint8_t)(ecm->sid&0xff);
	memcpy(buf+off, ecm->ecm, ecm->ecmlen);
	off += ecm->ecmlen;

	srv->lastecmtime = GetTickCount();
	srv->busy = 1;
	if (!c3_msg_send(srv->handle, srv, C3_MSG_ECM, buf, (size_t)off)) return 0;
	srv->ecm.request = ecm;
	srv->ecm.hash = ecm->hash;
	return 1;
}

// ---------- recv ----------

void ccam3_srv_recvmsg(struct server_data *srv)
{
	if (srv->handle<=0) return;
	if (srv->type!=TYPE_CCAM3) return;
	uint8_t buf[C3_MAXMSGSIZE];
	uint32_t msgid = 0;
	uint8_t *pay = NULL;
	int paylen = 0;
	int len = c3_msg_recv(srv->handle, srv, buf, C3_MAXMSGSIZE, &msgid, &pay, &paylen);
	if (len<=0) {
		mlogf(LOGINFO,getdbgflag(DBG_SERVER, 0, srv->id), " CCcam3 server (%s:%d) read failed\n", srv->host->name, srv->port);
		disconnect_srv(srv);
		return;
	}
	uint32_t ticks = GetTickCount();
	if (msgid==C3_MSG_CW) {
		if (!srv->busy) {
			mlogf(LOGINFO,getdbgflag(DBG_SERVER, 0, srv->id), " [!] dcw error from ccam3 server (%s:%d), unknown ecm request\n",srv->host->name,srv->port);
			return;
		}
		if (paylen < 4+16+1+6) {
			mlogf(LOGINFO,getdbgflag(DBG_SERVER, 0, srv->id), " [!] dcw error from ccam3 server (%s:%d), wrong length\n",srv->host->name,srv->port);
			return;
		}
		uint8_t dcw[16];
		memcpy(dcw, pay+4, 16);
		uint16_t caid = (uint16_t)((pay[4+16+1]<<8)|pay[4+16+2]);
		uint16_t sid  = (uint16_t)((pay[4+16+1+4]<<8)|pay[4+16+1+5]);

		srv->busy = 0;
		pipe_cmd( prg.pipe.ecm[1], PIPE_SRV_AVAILABLE );
		srv->lastdcwtime = ticks;

		pthread_mutex_lock(&prg.lockecm);
		ECM_DATA *ecm = srv->ecm.request;
		if (!ecm) {
			mlogf(LOGINFO,getdbgflag(DBG_SERVER, 0, srv->id), " [!] error cw from ccam3 server (%s:%d), ecm not found\n",srv->host->name,srv->port);
			pthread_mutex_unlock(&prg.lockecm);
			return;
		}
		if (ecm->hash!=srv->ecm.hash) {
			mlogf(LOGINFO,getdbgflag(DBG_SERVER, 0, srv->id), " [!] error cw from ccam3 server (%s:%d), ecm deleted\n",srv->host->name,srv->port);
			pthread_mutex_unlock(&prg.lockecm);
			return;
		}
		if ((caid!=ecm->caid)||(sid!=ecm->sid)) {
			mlogf(LOGINFO,getdbgflag(DBG_SERVER, 0, srv->id), " [!] error cw from ccam3 server (%s:%d), channel mismatch\n",srv->host->name,srv->port);
			pthread_mutex_unlock(&prg.lockecm);
			return;
		}
		struct cardserver_data *cs = ecm->cs;
		if (!cs) cs = getcsbycaprovid(ecm->caid, ecm->provid);
		int isnanoe0 = ecm_isnanoe0(ecm->ecm, ecm->caid);
		if (!acceptDCW( dcw, isnanoe0 )) {
			mlogf(LOGDEBUG,getdbgflag(DBG_SERVER,0,srv->id)," [!] dcw error from ccam3 server (%s:%d), bad dcw!!! ch %04x:%06x:%04x nanoe0=%d\n",srv->host->name,srv->port,ecm->caid, ecm->provid, ecm->sid, isnanoe0);
			srv->ecmerrdcw++;
			pthread_mutex_unlock(&prg.lockecm);
			return;
		}
		ecm_setdcw( ecm, dcw, DCW_SOURCE_SERVER, srv->id );
		pthread_mutex_unlock(&prg.lockecm);
	}
	else if (msgid==C3_MSG_CARD_DATA) {
		// nao usado no protocolo atual - ignorar
	}
}
