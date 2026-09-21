// cwfeed.c - feed live ECM/CW para estudo (GUI painel por server/cliente)
// ring buffer global de eventos: WAIT (pedido), OK (cw), NOK (falha)
// (incluido pelo main.c - contexto unico, como os outros modulos)

#define CWFEED_MAX 128

struct cwfeed_event {
	uint32_t t;          // GetTickCount
	uint16_t caid;
	uint32_t prov;
	uint16_t sid;
	uint8_t  ecm[40];    // bytes do ECM (ate 40)
	int      ecmlen;
	uint8_t  cw[16];
	int      hascw;      // 0=nenhuma 1=parcial 2=completa
	uint16_t ms;
	uint8_t  status;     // 0 WAIT, 1 OK, 2 NOK
	uint8_t  proto;      // 1 cccam, 2 newcamd, 3 mgcamd, 6 cache
	int      srv;        // server id (0 = nenhum)
	int      cli;        // client id (0 = nenhum)
};

static struct cwfeed_event cwfeed_ring[CWFEED_MAX];
static int cwfeed_idx = 0;
static int cwfeed_count = 0;
static pthread_mutex_t cwfeed_mutex = PTHREAD_MUTEX_INITIALIZER;

void cwfeed_add(uint16_t caid, uint32_t prov, uint16_t sid, uint8_t *ecm, int ecmlen,
	uint8_t *cw, int hascw, uint16_t ms, uint8_t status, uint8_t proto, int srv, int cli)
{
	if (!caid && !sid) return;
	pthread_mutex_lock(&cwfeed_mutex);
	struct cwfeed_event *e = &cwfeed_ring[cwfeed_idx];
	cwfeed_idx = (cwfeed_idx + 1) % CWFEED_MAX;
	if (cwfeed_count < CWFEED_MAX) cwfeed_count++;
	memset(e, 0, sizeof(struct cwfeed_event));
	e->t = GetTickCount();
	e->caid = caid;
	e->prov = prov;
	e->sid = sid;
	if (ecm && ecmlen > 0) {
		if (ecmlen > 40) ecmlen = 40;
		memcpy(e->ecm, ecm, ecmlen);
		e->ecmlen = ecmlen;
	}
	if (cw && hascw) {
		memcpy(e->cw, cw, hascw);
		e->hascw = hascw;
	}
	e->ms = ms;
	e->status = status;
	e->proto = proto;
	e->srv = srv;
	e->cli = cli;
	pthread_mutex_unlock(&cwfeed_mutex);
}

static void cwfeed_hex(char *out, uint8_t *d, int n)
{
	int i;
	for (i = 0; i < n; i++) sprintf(out + i * 3, "%02X ", d[i]);
	out[n * 3 - 1] = 0;
}

// render: srv>0 filtra por server; cli>0 por cliente; caid>0 por caid
int cwfeed_render(char *out, int outsz, int srv, int cli, uint16_t caid)
{
	int len = 0;
	char *p = out;
	uint32_t now = GetTickCount();
	int i, n;
	char ecmh[128], cwh[64], tstr[32];

	pthread_mutex_lock(&cwfeed_mutex);
	n = cwfeed_count;
	for (i = 0; i < n; i++) {
		struct cwfeed_event *e = &cwfeed_ring[(cwfeed_idx - n + i + CWFEED_MAX * 2) % CWFEED_MAX];
		if (srv > 0 && e->srv != srv) continue;
		if (cli > 0 && e->cli != cli) continue;
		if (caid && e->caid != caid) continue;

		uint32_t age = (uint32_t)((now - e->t) / 1000);
		sprintf(tstr, "%02ds", (int)(age % 60));

		// nome do canal (CCcam.channelinfo)
		char *chname = getchname(e->caid, e->prov, e->sid);

		ecmh[0] = 0;
		if (e->ecmlen > 0) cwfeed_hex(ecmh, e->ecm, e->ecmlen);
		cwh[0] = 0;
		if (e->hascw == 16) {
			char h0[32], h1[32];
			sprintf(h0, "%02X%02X%02X%02X%02X%02X%02X%02X", e->cw[0], e->cw[1], e->cw[2], e->cw[3], e->cw[4], e->cw[5], e->cw[6], e->cw[7]);
			sprintf(h1, "%02X%02X%02X%02X%02X%02X%02X%02X", e->cw[8], e->cw[9], e->cw[10], e->cw[11], e->cw[12], e->cw[13], e->cw[14], e->cw[15]);
			sprintf(cwh, "%s %s", h0, h1);
		}

		const char *st = (e->status == 1) ? "OK" : (e->status == 2) ? "NOK" : "WAIT";
		const char *cls = (e->status == 1) ? "cwfeed-ok" : (e->status == 2) ? "cwfeed-nok" : "cwfeed-wait";
		char src[48];
		src[0] = 0;
		if (e->srv > 0) sprintf(src, "srv%d", e->srv);
		else if (e->cli > 0) sprintf(src, "cli%d", e->cli);

		len += snprintf(p + len, outsz - len,
			"<div class='cwfeed-row %s'><span class='cwfeed-t'>%s</span>"
			"<span class='cwfeed-n'>%04x:%06x:%04x \"%s\"</span><span class='cwfeed-e'>%s</span><span class='cwfeed-c'>%s</span>"
			"<span class='cwfeed-ms'>%dms</span><span class='cwfeed-s %s'>%s</span><span class='cwfeed-src'>%s</span></div>\n",
			cls, tstr, e->caid, e->prov, e->sid, chname, ecmh, cwh, e->ms, cls, st, src);
		if (len >= outsz - 64) break;
	}
	pthread_mutex_unlock(&cwfeed_mutex);
	if (!len) len += snprintf(p + len, outsz - len, "<div class='cwfeed-empty'>sem eventos ainda (espera por ECMs)</div>");
	return len;
}
