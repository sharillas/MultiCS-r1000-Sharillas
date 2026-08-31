///////////////////////////////////////////////////////////////////////////////
// protection.c - ECM FILTER (rule engine), DCW FILTER (CWPK), FAILBAN,
//                ANTICASCADE, ECMRATELIMIT e ICAM (transformacao da CW)
//                MultiCS r1000 - by Sharillas@2026
// Analise baseada no reverse engineering do MultiCS r120 (Duback).
///////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------------
// ICAM: permutacao de bits [0..7] -> [1,7,5,2,6,4,0,3] nos bytes [0],[4],[8],[12]
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

void dcw_icam_apply(uint8_t cw[16])
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
	fb_list[oldest].ip = ip;
	fb_list[oldest].time = GetTickCount();
	fb_list[oldest].count = 0;
	fb_list[oldest].banned = 0;
	return &fb_list[oldest];
}

void failban_bad(uint32_t ip, int proto, char *reason)
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
	e->count++;
	if (e->count>=max) {
		e->banned = 1;
		if (!ipblock_check(ip)) ipblock_add(ip);
		mlogf(LOGWARNING,0," FAILBAN: IP %s banido (%ds) apos %d eventos (%s)\n", (char*)ip2string(ip), cfg.failban.bantime, e->count, reason);
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
