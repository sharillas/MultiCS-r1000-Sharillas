///////////////////////////////////////////////////////////////////////////////
// CW CYCLE CHECK (estilo OSCam module-cw-cycle-check)
// Valida ciclos de CW por canal para proteger contra cws fakes/replay:
//  - aprendizagem: mede o intervalo entre mudancas de CW (cycletime)
//  - verificacao: same CW fora da janela, ciclo invalido (metade errada
//    mudou), similaridade da metade fixa (sensitive), replay de ECM antigo
// MultiCS r1000 - by Sharillas@2026
///////////////////////////////////////////////////////////////////////////////

struct cwc_data *cwc_list = NULL;
struct cwc_stats_data cwc_stats;

static pthread_mutex_t cwc_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t cwc_lastclean = 0;

static inline int cwc_cwnonnull(uint8_t cw[16])
{
	int i;
	for (i=0; i<16; i++) if (cw[i]) return 1;
	return 0;
}

// quantos bytes da metade "fixa" sao iguais a anterior (countCWpart do OSCam)
static int cwc_countCWpart(uint8_t oldcw[16], uint8_t cw[16], int nextcyclecw)
{
	int eo = nextcyclecw ? 0 : 8; // metade que NAO deve mudar
	int i, ret = 0;
	for (i=0; i<8; i++) if (oldcw[i+eo]==cw[i+eo]) ret++;
	return ret;
}

static struct cwc_data *cwc_find(uint16_t caid, uint32_t provid, uint16_t sid, uint8_t tag)
{
	struct cwc_data *c = cwc_list;
	while (c) {
		if (c->caid==caid && c->provid==provid && c->sid==sid && c->tag==tag) return c;
		c = c->next;
	}
	return NULL;
}

static struct cwc_data *cwc_new(uint16_t caid, uint32_t provid, uint16_t sid, uint8_t tag,
	uint32_t now, uint8_t cw[16], uint32_t hash, int keepminutes)
{
	struct cwc_data *c = malloc(sizeof(struct cwc_data));
	memset(c, 0, sizeof(struct cwc_data));
	c->caid = caid;
	c->provid = provid;
	c->sid = sid;
	c->tag = tag;
	c->stage = 0;
	c->cycletime = 99000; // 99s ate aprender (como OSCam)
	c->nextcyclecw = 2;
	c->time = now;
	c->locktime = now + CWC_LOCKTIME;
	memcpy(c->cw, cw, 16);
	c->hist[0].hash = hash;
	c->hist[0].tag = tag;
	memcpy(c->hist[0].cw, cw, 16);
	c->histidx = 0;
	c->keepminutes = keepminutes;
	c->next = cwc_list;
	cwc_list = c;
	cwc_stats.entries++;
	return c;
}

static void cwc_histpush(struct cwc_data *c, uint32_t hash, uint8_t tag, uint8_t cw[16])
{
	c->histidx++;
	if (c->histidx > CWC_MAXHIST-1) c->histidx = 0;
	c->hist[c->histidx].hash = hash;
	c->hist[c->histidx].tag = tag;
	memcpy(c->hist[c->histidx].cw, cw, 16);
}

void cwc_cleanup()
{
	uint32_t now = GetTickCount();
	if ( (uint32_t)(now - cwc_lastclean) < 120000 ) return;
	cwc_lastclean = now;
	pthread_mutex_lock(&cwc_mutex);
	struct cwc_data *c = cwc_list;
	struct cwc_data *prev = NULL;
	while (c) {
		uint32_t kct = c->keepminutes ? (uint32_t)c->keepminutes*60000 + 30000 : 900000;
		struct cwc_data *next = c->next;
		if ( (uint32_t)(now - c->time) > kct ) {
			if (prev) prev->next = next;
			else cwc_list = next;
			free(c);
		} else prev = c;
		c = next;
	}
	pthread_mutex_unlock(&cwc_mutex);
}

// 0 = passa, -1 = drop
int cwc_check(struct cache_data *req, uint8_t cw[16], int peerid)
{
	// perfil com CWC ativo para este CAID?
	struct cardserver_data *cs = cfg.cardserver;
	struct cardserver_data *cwcprof = NULL;
	while (cs) {
		if ( cs->option.cwc.enable && (cs->card.caid==req->caid) ) {
			cwcprof = cs;
			break;
		}
		cs = cs->next;
	}
	if (!cwcprof) return 0;

	// half-null DCW (canais half-cycle tipo NDS): nao verificar
	if (ishalfnulledcw(cw)) return 0;
	if (!cwc_cwnonnull(cw)) return 0;

	uint32_t now = GetTickCount();
	int sensitive = cwcprof->option.cwc.sensitive;
	int dropold = cwcprof->option.cwc.dropold;
	int dropbad = cwcprof->option.cwc.dropbad;
	int keep = cwcprof->option.cwc.keepcycletime;
	char dump[64];
	array2hex(cw, dump, 16);

	cwc_cleanup();

	pthread_mutex_lock(&cwc_mutex);
	struct cwc_data *c = cwc_find(req->caid, req->provid, req->sid, req->tag);
	if (!c) {
		cwc_new(req->caid, req->provid, req->sid, req->tag, now, cw, req->hash, keep);
		pthread_mutex_unlock(&cwc_mutex);
		mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: new entry ch %04x:%06x:%04x/%02x cw=%s (learn)\n",
			req->caid, req->provid, req->sid, req->tag, dump);
		return 0;
	}
	c->checked++;
	cwc_stats.checked++;

	int ret = CWC_RET_IGN;
	int drop = 0;

	if (c->stage==3 && c->nextcyclecw<2 &&
	    (uint32_t)(now - c->time) < (uint32_t)(c->cycletime*2 - c->dyncycletime - 1000)) {
		// STAGE 3: verificacao
		if (!memcmp(c->cw, cw, 16)) {
			// mesma CW: dentro da janela?
			if ( (uint32_t)(now - c->time) >= (uint32_t)(c->cycletime - c->dyncycletime) ) {
				if (dropold) {
					mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," cwc: same CW too late ch %04x:%06x:%04x/%02x cw=%s peer %d -> DROP\n",
						req->caid, req->provid, req->sid, req->tag, dump, peerid);
					c->bad++; cwc_stats.bad++; cwc_stats.dropped++;
					drop = 1;
				}
				else {
					mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: same CW too late ch %04x:%06x:%04x/%02x (log only)\n",
						req->caid, req->provid, req->sid, req->tag);
				}
			}
			ret = CWC_RET_SAME;
		}
		else {
			int cycleok = -1;
			int i;
			if (c->nextcyclecw==0) { // CW0 nao deve ter mudado
				cycleok = 0;
				for (i=0; i<8; i++) if (c->cw[i]!=cw[i]) { cycleok = -1; break; }
			}
			else if (c->nextcyclecw==1) { // CW1 nao deve ter mudado
				cycleok = 1;
				for (i=0; i<8; i++) if (c->cw[i+8]!=cw[i+8]) { cycleok = -1; break; }
			}
			if (cycleok>=0 && sensitive &&
			    cwc_countCWpart(c->cw, cw, c->nextcyclecw) >= sensitive) cycleok = -2;

			if (cycleok>=0) {
				ret = CWC_RET_OK;
				c->ok++; cwc_stats.ok++;
				c->nextcyclecw = cycleok ? 0 : 1;
				c->dyncycletime = ( (uint32_t)(now - c->time) > (uint32_t)c->cycletime ) ?
					(int32_t)(now - c->time) - c->cycletime : 0;
				c->time = now;
				memcpy(c->cw, cw, 16);
				cwc_histpush(c, req->hash, req->tag, cw);
				mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: valid CW %d cycle ch %04x:%06x:%04x/%02x cycletime=%dms next=CW%d cw=%s peer %d\n",
					cycleok, req->caid, req->provid, req->sid, req->tag, c->cycletime, c->nextcyclecw, dump, peerid);
			}
			else {
				// replay de ECM antigo?
				int k;
				for (k=0; k<CWC_MAXHIST; k++) {
					if ( c->hist[k].hash && c->hist[k].hash==req->hash ) {
						if (!dropold && !memcmp(c->hist[k].cw, cw, 16)) {
							ret = CWC_RET_SAME;
						}
						else {
							ret = CWC_RET_OLD;
							mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," cwc: old ECM (replay) ch %04x:%06x:%04x/%02x cw=%s peer %d -> DROP\n",
								req->caid, req->provid, req->sid, req->tag, dump, peerid);
							c->bad++; cwc_stats.bad++; cwc_stats.dropped++;
							drop = 1;
						}
						break;
					}
				}
				if (ret!=CWC_RET_OLD && ret!=CWC_RET_SAME) {
					// bad cycle
					ret = CWC_RET_NOK;
					if (cycleok==-2)
						mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," cwc: NON valid CW (sensitive %d) ch %04x:%06x:%04x/%02x cw=%s peer %d\n",
							sensitive, req->caid, req->provid, req->sid, req->tag, dump, peerid);
					else
						mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," cwc: bad CW cycle ch %04x:%06x:%04x/%02x cw=%s peer %d\n",
							req->caid, req->provid, req->sid, req->tag, dump, peerid);
					if (dropbad) {
						mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," cwc: bad CW cycle ch %04x:%06x:%04x/%02x -> DROP\n",
							req->caid, req->provid, req->sid, req->tag);
						c->bad++; cwc_stats.bad++; cwc_stats.dropped++;
						drop = 1;
					}
					else {
						c->ign++; cwc_stats.ign++;
						mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," cwc: bad CW cycle ch %04x:%06x:%04x/%02x (log only)\n",
							req->caid, req->provid, req->sid, req->tag);
					}
				}
			}
		}
	}
	else if (c->stage==3) {
		// entrada demasiado velha: keep window ou voltar a aprender
		if (keep>0 && (uint32_t)(now - c->time) < (uint32_t)(keep*60000)) {
			c->stage = 4;
			mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: set stage 4 ch %04x:%06x:%04x/%02x cycletime=%dms\n",
				req->caid, req->provid, req->sid, req->tag, c->cycletime);
		}
		else {
			c->stage = 2;
			mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: back to stage 2 ch %04x:%06x:%04x/%02x\n",
				req->caid, req->provid, req->sid, req->tag);
		}
		memset(c->cw, 0, 16);
		c->nextcyclecw = 2;
		ret = CWC_RET_IGN;
		c->ign++; cwc_stats.ign++;
		c->time = now;
		memcpy(c->cw, cw, 16);
		cwc_histpush(c, req->hash, req->tag, cw);
	}
	else if (c->stage==4) {
		// so verifica qual metade cicla (sem timing)
		int n = memcmp(c->cw, cw, 8);
		int m = memcmp(c->cw+8, cw+8, 8);
		if (!n) c->nextcyclecw = 1;
		if (!m) c->nextcyclecw = 0;
		if (n==m || !cwc_cwnonnull(cw)) c->nextcyclecw = 2;
		if (c->nextcyclecw<2) {
			mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: back to stage 3 ch %04x:%06x:%04x/%02x cycletime=%dms next=CW%d\n",
				req->caid, req->provid, req->sid, req->tag, c->cycletime, c->nextcyclecw);
			c->stage = 3;
			c->stage4_repeat = 0;
		}
		else {
			c->stage4_repeat++;
			if (c->stage4_repeat>12) {
				c->stage = 1;
				mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: back to stage 1 (stage4 repeat) ch %04x:%06x:%04x/%02x\n",
					req->caid, req->provid, req->sid, req->tag);
			}
		}
		ret = CWC_RET_LEARN4;
		c->time = now;
		memcpy(c->cw, cw, 16);
		cwc_histpush(c, req->hash, req->tag, cw);
	}
	else {
		// STAGE 0-2: aprendizagem do cycletime
		ret = CWC_RET_LEARN;
		if ( (int32_t)(now - c->locktime) > 0 ) {
			int32_t diff = (int32_t)(now - c->time) - c->cycletime;
			if (c->stage<=0) {
				if (diff>-CWC_TOLERANCE && diff<CWC_TOLERANCE) {
					c->cycletime = (int32_t)(now - c->time);
					c->stage = 1;
					mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: set stage 1 ch %04x:%06x:%04x/%02x cycletime=%dms\n",
						req->caid, req->provid, req->sid, req->tag, c->cycletime);
				}
			}
			else if (c->stage==1) {
				if (diff>-CWC_TOLERANCE && diff<CWC_TOLERANCE) {
					c->cycletime = (int32_t)(now - c->time);
					c->stage = 2;
					mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: set stage 2 ch %04x:%06x:%04x/%02x cycletime=%dms\n",
						req->caid, req->provid, req->sid, req->tag, c->cycletime);
				}
				else c->stage = 0;
			}
			else if (c->stage==2) {
				if (diff>-CWC_TOLERANCE && diff<CWC_TOLERANCE && c->cycletime>0) {
					int n = memcmp(c->cw, cw, 8);
					int m = memcmp(c->cw+8, cw+8, 8);
					if (!n) c->nextcyclecw = 1;
					if (!m) c->nextcyclecw = 0;
					if (n==m || !cwc_cwnonnull(cw)) c->nextcyclecw = 2;
					if (c->nextcyclecw<2) {
						c->cycletime = (int32_t)(now - c->time);
						c->stage = 3;
						c->stage4_repeat = 0;
						c->locktime = 0;
						mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," cwc: set stage 3 ch %04x:%06x:%04x/%02x cycletime=%dms next=CW%d\n",
							req->caid, req->provid, req->sid, req->tag, c->cycletime, c->nextcyclecw);
					}
					else {
						c->stage = 1;
						mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," cwc: back to stage 1 (no cycle in learning) ch %04x:%06x:%04x/%02x\n",
							req->caid, req->provid, req->sid, req->tag);
					}
				}
				else c->stage = 1;
			}
			if (c->stage<3) c->cycletime = (int32_t)(now - c->time);
			if (c->stage!=3) c->locktime = now + CWC_LOCKTIME;
			c->time = now;
			memcpy(c->cw, cw, 16);
			cwc_histpush(c, req->hash, req->tag, cw);
		}
	}

	pthread_mutex_unlock(&cwc_mutex);
	(void)ret;
	return drop ? -1 : 0;
}
