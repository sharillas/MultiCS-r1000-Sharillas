///////////////////////////////////////////////////////////////////////////////
// protection.c - ECM FILTER (rule engine), DCW FILTER (CWPK), FAILBAN,
//                ANTICASCADE, ECMRATELIMIT e CAK7 (transformacao da CW)
//                MultiCS r1000 - by Sharillas@2026
// Analise baseada no reverse engineering do MultiCS r120 (Duback).
///////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------------
// CAK7 Merlin: permutacao de bits [0..7] -> [1,7,5,2,6,4,0,3] nos bytes [0],[4],[8],[12]
// da CW + correcao do checksum de cada quad (byte 3 = soma dos outros 3).
// Tabela extraida do r120 (.rodata@0xe8a60) e verificada bijectiva.
// ---------------------------------------------------------------------------
static uint8_t icam_perm[256];
static int icam_ready = 0;

static void icam_init()
{
	if (icam_ready) return;
	int i;
	for (i=0; i<256; i++) {
		uint8_t x = (uint8_t)i, y = 0;
		y |= (uint8_t)((x & 0x01) << 1);
		y |= (uint8_t)((x & 0x02) << 5);
		y |= (uint8_t)((x & 0x04) << 3);
		y |= (uint8_t)((x & 0x08) >> 1);
		y |= (uint8_t)((x & 0x10) << 2);
		y |= (uint8_t)((x & 0x20) >> 1);
		y |= (uint8_t)((x & 0x40) >> 6);
		y |= (uint8_t)((x & 0x80) >> 4);
		icam_perm[i] = y;
	}
	icam_ready = 1;
}

void dcw_cak7_apply(uint8_t cw[16])
{
	icam_init();
	int q;
	for (q=0; q<4; q++) {
		int off = q*4;
		cw[off] = icam_perm[cw[off]];
		cw[off+3] = (uint8_t)(cw[off+1] + cw[off+2] + cw[off]);
	}
}

// ---------------------------------------------------------------------------
// ECM FILTER: regras genericas (PREFIX / LEN / BYTE INMASK)
// retorna NULL se aceite; string de erro se rejeitada (DROP) ou so logada
// ---------------------------------------------------------------------------
char *ecm_filter_check(struct cardserver_data *cs, uint8_t *ecmdata, uint16_t ecmlen)
{
	if (!cs || !cs->option.ecmfilter.enable || !cs->option.ecmfilter.nrules) return NULL;
	int i, j;
	for (i=0; i<cs->option.ecmfilter.nrules; i++) {
		uint8_t type = cs->option.ecmfilter.rules[i].type;
		int ok = 0;
		if (type==1) { // PREFIX: ecmdata comeca por um dos prefixes
			for (j=0; j<cs->option.ecmfilter.rules[i].nvals; j++) {
				uint32_t v = cs->option.ecmfilter.rules[i].vals[j];
				int n = (v>>24)&0xff;
				uint8_t p0 = (v>>16)&0xff, p1 = (v>>8)&0xff, p2 = v&0xff;
				if ( (n>=1) && (ecmlen>=1) && (ecmdata[0]==p0) &&
				     ( (n<2) || (ecmdata[1]==p1) ) &&
				     ( (n<3) || (ecmdata[2]==p2) ) ) { ok = 1; break; }
			}
			if (!ok) {
				char dbg[128];
				char pre[64] = "";
				for (j=0; j<cs->option.ecmfilter.rules[i].nvals; j++) {
					uint32_t v = cs->option.ecmfilter.rules[i].vals[j];
					char t[8]; sprintf(t, " %02X%02X%02X", (v>>16)&0xff, (v>>8)&0xff, v&0xff);
					strncat(pre, t, 63-strlen(pre));
				}
				mlogf(LOGWARNING,getdbgflagpro(DBG_SERVER,0,0,cs->id)," ecmfilter: perfil '%s' ECM header fora da whitelist (%s) ch %04x:%06x:%04x\n", cs->name, pre, cs->card.caid, cs->card.prov[0].id, 0);
				sprintf(dbg, "ECM header not in whitelist");
				if (cs->option.ecmfilter.mode) return strdup(dbg);
				return NULL; // LOGONLY
			}
		}
		else if (type==2) { // LEN: ecmlen tem de estar na lista
			for (j=0; j<cs->option.ecmfilter.rules[i].nvals; j++) {
				if (ecmlen==cs->option.ecmfilter.rules[i].vals[j]) { ok = 1; break; }
			}
			if (!ok) {
				mlogf(LOGWARNING,getdbgflagpro(DBG_SERVER,0,0,cs->id)," ecmfilter: perfil '%s' ECM length %d fora da lista\n", cs->name, ecmlen);
				if (cs->option.ecmfilter.mode) return strdup("ECM length not allowed");
				return NULL;
			}
		}
		else if (type==3) { // BYTE INMASK: bit do valor do byte tem de estar na mask
			uint8_t off = cs->option.ecmfilter.rules[i].off;
			uint64_t mask = cs->option.ecmfilter.rules[i].mask;
			if (off >= ecmlen) { ok = 0; }
			else {
				uint8_t b = ecmdata[off];
				if (b<64 && (mask & (1ULL<<b))) ok = 1;
				else ok = 0;
			}
			if (!ok) {
				mlogf(LOGWARNING,getdbgflagpro(DBG_SERVER,0,0,cs->id)," ecmfilter: perfil '%s' byte ECM[%d]=%02X fora da mask\n", cs->name, off, off<ecmlen?ecmdata[off]:0);
				if (cs->option.ecmfilter.mode) return strdup("ECM byte not allowed");
				return NULL;
			}
		}
	}
	return NULL;
}

// ---------------------------------------------------------------------------
// CWPK LEARNING: regras aprendidas em runtime (CW de cartao marcado)
// ---------------------------------------------------------------------------
#define LEARN_MAX 16
#define LEARN_SAMPLES 5
static struct {
	uint8_t cw[16];
	int hits;
	uint32_t firstseen;
} learned[LEARN_MAX];
static int nlearned = 0;

static void learn_cw(uint8_t cw[16], uint32_t ip)
{
	int i;
	for (i=0; i<nlearned; i++) {
		if (!memcmp(learned[i].cw, cw, 16)) { learned[i].hits++; return; }
	}
	if (nlearned>=LEARN_MAX) {
		// FIFO: descarta a mais antiga
		memmove(&learned[0], &learned[1], sizeof(learned[0])*(LEARN_MAX-1));
		nlearned--;
	}
	memcpy(learned[nlearned].cw, cw, 16);
	learned[nlearned].hits = 1;
	learned[nlearned].firstseen = GetTickCount();
	nlearned++;
	char dump[64];
	array2hex(cw, dump, 16);
	mlogf(LOGWARNING,0," CWPK LEARNING: nova regra aprendida (CW de cartao marcado, IP %s) => %s\n", (char*)ip2string(ip), dump);
	prot_event_add("CWPK LEARNING: nova regra aprendida => %s", dump);
}

// regras aprendidas ativas? (consulta rapida para o filtro)
int dcw_filter_learned_count()
{
	return nlearned;
}

int dcw_filter_learned_check(uint8_t dcw[16])
{
	int i;
	for (i=0; i<nlearned; i++) {
		if (!memcmp(learned[i].cw, dcw, 16)) return 1;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// EVENTOS RECENTES (dashboard): anel de mensagens das protecoes
// ---------------------------------------------------------------------------
#define PROT_EVENTS 16
static struct {
	uint32_t time;
	char msg[160];
} prot_events[PROT_EVENTS];
static int prot_ev_idx = 0;
static uint32_t prot_start_ticks = 0;

void prot_event_add(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(prot_events[prot_ev_idx].msg, sizeof(prot_events[prot_ev_idx].msg), fmt, ap);
	va_end(ap);
	if (n<0) prot_events[prot_ev_idx].msg[0] = 0;
	prot_events[prot_ev_idx].time = GetTickCount();
	prot_ev_idx = (prot_ev_idx+1)%PROT_EVENTS;
}

// devolve o texto de um evento (0=mais recente); NULL se nao existir
char *prot_event_get(int n, uint32_t *age_ms)
{
	int total = 0;
	int i;
	for (i=0; i<PROT_EVENTS; i++) if (prot_events[i].time) total++;
	if (n>=total) return NULL;
	int idx = (prot_ev_idx - 1 - n + PROT_EVENTS*2) % PROT_EVENTS;
	if (age_ms) *age_ms = GetTickCount() - prot_events[idx].time;
	return prot_events[idx].msg;
}

uint32_t prot_uptime_ticks()
{
	if (!prot_start_ticks) prot_start_ticks = GetTickCount();
	return GetTickCount() - prot_start_ticks;
}

// ---------------------------------------------------------------------------
// DCW FILTER: blacklist CWPK (EXACT/MASK/ALLEQUAL)
// retorna 1 se bloqueada (DROP), 0 se aceite (LOGONLY apenas loga)
// ---------------------------------------------------------------------------
int dcw_filter_check(struct cardserver_data *cs, uint8_t dcw[16])
{
	if (!cs || !cs->option.dcwfilter.enable || !cs->option.dcwfilter.nrules) return 0;
	int i;
	for (i=0; i<cs->option.dcwfilter.nrules; i++) {
		uint8_t type = cs->option.dcwfilter.rules[i].type;
		int hit = 0;
		if (type==1) { // EXACT: lista de CWs
			int k;
			for (k=0; k<cs->option.dcwfilter.rules[i].n; k++) {
				if (!memcmp(dcw, cs->option.dcwfilter.rules[i].cw[k], 16)) { hit = 1; break; }
			}
		}
		else if (type==2) { // MASK
			int j, m = 1;
			for (j=0; j<16; j++) {
				if ( (dcw[j] & cs->option.dcwfilter.rules[i].mask[j]) != (cs->option.dcwfilter.rules[i].cw[0][j] & cs->option.dcwfilter.rules[i].mask[j]) ) { m = 0; break; }
			}
			if (m) hit = 1;
		}
		else if (type==3) { // ALLEQUAL
			int j, e = 1;
			for (j=1; j<16; j++) if (dcw[j]!=dcw[0]) { e = 0; break; }
			if (e) hit = 1;
		}
		if (hit) {
			char dump[64];
			array2hex(dcw, dump, 16);
			int mode = cs->option.dcwfilter.mode;
			if (mode==2) {
				// AUTO: desligado por defeito; ativa no 1o hit de cartao marcado
				if (!cs->option.dcwfilter.auto_active) {
					cs->option.dcwfilter.auto_active = 1;
					mlogf(LOGWARNING,getdbgflagpro(DBG_SERVER,0,0,cs->id)," dcwfilter: AUTO ATIVADO no perfil '%s' - detetada CW de cartao marcado (rule %d, tipo %d) => %s\n", cs->name, i+1, type, dump);
					prot_event_add("dcwfilter: AUTO ATIVADO no perfil '%s' => %s", cs->name, dump);
				}
				else {
					mlogf(LOGWARNING,getdbgflagpro(DBG_SERVER,0,0,cs->id)," dcwfilter: perfil '%s' CW bloqueada (rule %d, tipo %d) => %s\n", cs->name, i+1, type, dump);
				}
				return 1;
			}
			mlogf(LOGWARNING,getdbgflagpro(DBG_SERVER,0,0,cs->id)," dcwfilter: perfil '%s' CW bloqueada (rule %d, tipo %d) => %s\n", cs->name, i+1, type, dump);
			return mode ? 1 : 0;
		}
	}
	// CWPK LEARNING: regras aprendidas em runtime (bloqueiam sempre, se ativo)
	if (cs->option.dcwfilter.learn && nlearned>0) {
		if (dcw_filter_learned_check(dcw)) {
			char dump[64];
			array2hex(dcw, dump, 16);
			mlogf(LOGWARNING,getdbgflagpro(DBG_SERVER,0,0,cs->id)," dcwfilter: perfil '%s' CW bloqueada (regra APRENDIDA) => %s\n", cs->name, dump);
			return 1;
		}
	}
	return 0;
}

// ---------------------------------------------------------------------------
// FAILBAN: contadores por IP (bad CW de clientes/peers) -> ipblock
// ---------------------------------------------------------------------------
#define FB_MAX 128
struct fb_entry {
	uint32_t ip;
	uint32_t time;   // inicio da janela
	int count;
	int banned;
	// LEARNING CWPK: amostras das CWs mas recebidas deste IP
	uint8_t cws[8][16];
	int cw_idx;
};

static struct fb_entry fb_list[FB_MAX];
static pthread_mutex_t fb_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct fb_entry *fb_find(uint32_t ip)
{
	int i, oldest = 0;
	uint32_t ot = 0xffffffff;
	for (i=0; i<FB_MAX; i++) {
		if (fb_list[i].ip==ip) return &fb_list[i];
		if (!fb_list[i].ip) { fb_list[i].ip = ip; fb_list[i].time = GetTickCount(); return &fb_list[i]; }
		if (fb_list[i].time<ot) { ot = fb_list[i].time; oldest = i; }
	}
	// reutilizar a entrada mais antiga
	memset(&fb_list[oldest], 0, sizeof(fb_list[oldest]));
	fb_list[oldest].ip = ip;
	fb_list[oldest].time = GetTickCount();
	return &fb_list[oldest];
}

void failban_bad(uint32_t ip, int proto, char *reason, uint8_t *badcw)
{
	if (!cfg.failban.enable || !ip) return;
	int max;
	switch (proto) {
		case TYPE_CCCAM:    max = cfg.failban.max_cccam; break;
		case TYPE_NEWCAMD:  max = cfg.failban.max_newcamd; break;
		case TYPE_MGCAMD:   max = cfg.failban.max_mgcamd; break;
		case TYPE_CAMD35:   max = cfg.failban.max_camd35; break;
		case TYPE_CS378X:   max = cfg.failban.max_cs378x; break;
		case TYPE_CACHE:    max = cfg.failban.max_cache; break;
		default: max = 0;
	}
	if (max<=0) return;
	pthread_mutex_lock(&fb_mutex);
	struct fb_entry *e = fb_find(ip);
	if (e->banned) { pthread_mutex_unlock(&fb_mutex); return; }
	// CWPK LEARNING: guardar amostra da CW ma e contar repeticoes
	if (badcw) {
		memcpy(e->cws[e->cw_idx], badcw, 16);
		e->cw_idx = (e->cw_idx+1)%8;
		int same = 0, i;
		for (i=0; i<8; i++) {
			if (!memcmp(e->cws[i], badcw, 16)) same++;
		}
		if (same>=LEARN_SAMPLES) {
			learn_cw(badcw, ip);
			// reset das amostras para nao reaprender o mesmo padrao em loop
			memset(e->cws, 0, sizeof(e->cws));
			e->cw_idx = 0;
		}
	}
	e->count++;
	if (e->count>=max) {
		e->banned = 1;
		if (!ipblock_check(ip)) ipblock_add(ip);
		mlogf(LOGWARNING,0," FAILBAN: IP %s banido (%ds) apos %d eventos (%s)\n", (char*)ip2string(ip), cfg.failban.bantime, e->count, reason);
		prot_event_add("FAILBAN: IP %s banido (%s)", (char*)ip2string(ip), reason);
	}
	pthread_mutex_unlock(&fb_mutex);
}

// ---------------------------------------------------------------------------
// ANTICASCADE: zapping excessivo por IP (reshare) -> ipblock
// ---------------------------------------------------------------------------
#define AC_MAX 128
struct ac_entry {
	uint32_t ip;
	uint32_t time;   // inicio da janela
	int count;
};
static struct ac_entry ac_list[AC_MAX];

int anticascade_zap(uint32_t ip)
{
	if (!cfg.anticascade.maxzap || !ip) return 0;
	int i, slot = -1;
	uint32_t now = GetTickCount();
	for (i=0; i<AC_MAX; i++) {
		if (ac_list[i].ip==ip) { slot = i; break; }
		if ((slot<0) && !ac_list[i].ip) slot = i;
	}
	if (slot<0) slot = AC_MAX-1;
	if (ac_list[slot].ip!=ip) { ac_list[slot].ip = ip; ac_list[slot].time = now; ac_list[slot].count = 0; }
	uint32_t window = (uint32_t)cfg.anticascade.window*1000;
	if ( (now - ac_list[slot].time) > window ) { ac_list[slot].time = now; ac_list[slot].count = 0; }
	ac_list[slot].count++;
	if (ac_list[slot].count > cfg.anticascade.maxzap) {
		if (!ipblock_check(ip)) {
			ipblock_add(ip);
			mlogf(LOGWARNING,0," ANTICASCADE: IP %s banido (%ds) - %d zaps em %ds\n", (char*)ip2string(ip), cfg.anticascade.bantime, ac_list[slot].count, cfg.anticascade.window);
			prot_event_add("ANTICASCADE: IP %s banido (%d zaps)", (char*)ip2string(ip), ac_list[slot].count);
		}
		return 1;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// ECMRATELIMIT: SIDTIME (ms entre ECMs do mesmo SID) + MAXECM (ECMs/s)
// ---------------------------------------------------------------------------
char *ratelimit_check(struct cardserver_data *cs, uint16_t sid)
{
	if (!cs) return NULL;
	uint32_t now = GetTickCount();
	if (cs->option.ratelimit.sidtime>0) {
		int i;
		for (i=0; i<8; i++) {
			if (cs->rl_sid[i].sid==sid) {
				if ( (now - cs->rl_sid[i].time) < (uint32_t)cs->option.ratelimit.sidtime )
					return "ECM rate limited (SIDTIME)";
				cs->rl_sid[i].time = now;
				goto rl_ok;
			}
		}
		cs->rl_sid[cs->rl_sid_idx].sid = sid;
		cs->rl_sid[cs->rl_sid_idx].time = now;
		cs->rl_sid_idx = (cs->rl_sid_idx+1)%8;
	}
rl_ok:
	if (cs->option.ratelimit.maxecm>0) {
		if ( (now - cs->rl_win_time) > 1000 ) { cs->rl_win_time = now; cs->rl_win_count = 0; }
		if (++cs->rl_win_count > cs->option.ratelimit.maxecm)
			return "ECM rate limited (MAXECM)";
	}
	return NULL;
}
