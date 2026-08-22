///////////////////////////////////////////////////////////////////////////////
// CHANNEL TIMING BUDGET (cryptoperiod adaptativo)
// Observa as mudancas de CW por canal e estima o cryptoperiod (EWMA).
///////////////////////////////////////////////////////////////////////////////

struct chnb_data *chnb_list = NULL;

static pthread_mutex_t chnb_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t chnb_lastclean = 0;

static struct chnb_data *chnb_find(uint16_t caid, uint32_t provid, uint16_t sid)
{
	struct chnb_data *c = chnb_list;
	while (c) {
		if (c->caid==caid && c->provid==provid && c->sid==sid) return c;
		c = c->next;
	}
	return NULL;
}

void chnbudget_observe(uint16_t caid, uint32_t provid, uint16_t sid, uint8_t cw[16])
{
	if (!caid || !sid) return;
	uint32_t now = GetTickCount();

	pthread_mutex_lock(&chnb_mutex);
	struct chnb_data *c = chnb_find(caid, provid, sid);
	if (!c) {
		c = malloc(sizeof(struct chnb_data));
		memset(c, 0, sizeof(struct chnb_data));
		c->caid = caid;
		c->provid = provid;
		c->sid = sid;
		memcpy(c->cw, cw, 16);
		c->lastchange = now;
		c->next = chnb_list;
		chnb_list = c;
		c->observations = 1;
		pthread_mutex_unlock(&chnb_mutex);
		return;
	}
	c->observations++;
	if (memcmp(c->cw, cw, 16)) {
		// mudanca de CW: amostra do cryptoperiod
		uint32_t delta = now - c->lastchange;
		if (delta >= CHNB_MINPERIOD && delta <= CHNB_MAXPERIOD) {
			if (c->period) c->period = (c->period*3 + (int32_t)delta)/4; // EWMA 75/25
			else c->period = (int32_t)delta;
			c->changes++;
			if (c->changes<=10)
				mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," timing: ch %04x:%06x:%04x delta=%ums period=%dms (samples=%d)\n",
					caid, provid, sid, delta, c->period, c->changes);
		}
		c->lastchange = now;
		memcpy(c->cw, cw, 16);
	}
	pthread_mutex_unlock(&chnb_mutex);
}

int chnbudget_getperiod(uint16_t caid, uint32_t provid, uint16_t sid)
{
	int p = 0;
	pthread_mutex_lock(&chnb_mutex);
	struct chnb_data *c = chnb_find(caid, provid, sid);
	if (c) p = c->period;
	pthread_mutex_unlock(&chnb_mutex);
	return p;
}

void chnbudget_cleanup()
{
	uint32_t now = GetTickCount();
	if ( (uint32_t)(now - chnb_lastclean) < 300000 ) return; // 5min
	chnb_lastclean = now;
	pthread_mutex_lock(&chnb_mutex);
	struct chnb_data *c = chnb_list;
	struct chnb_data *prev = NULL;
	while (c) {
		struct chnb_data *next = c->next;
		if ( (uint32_t)(now - c->lastchange) > 600000 ) { // 10min sem atividade
			if (prev) prev->next = next;
			else chnb_list = next;
			free(c);
		} else prev = c;
		c = next;
	}
	pthread_mutex_unlock(&chnb_mutex);
}
