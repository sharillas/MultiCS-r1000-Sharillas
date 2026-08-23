///////////////////////////////////////////////////////////////////////////////
// NAGRA PROTECTION (caid 18xx/19xx/1a0x)
// Logica reconstruida da build antiga (b3bc2878): mesma maquina de estados,
// mesmos codigos de rejeicao e mesmas mensagens de log.
///////////////////////////////////////////////////////////////////////////////

struct nagra_chn_data *nagra_list = NULL;
uint32_t nagra_checked_total = 0;
uint32_t nagra_dup_total = 0;
uint32_t nagra_conf_total = 0;
static int nagra_list_size = 0;

static pthread_mutex_t nagra_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct nagra_chn_data *nagra_find(uint16_t caid, uint32_t provid, uint16_t sid)
{
	struct nagra_chn_data *c = nagra_list;
	while (c) {
		if (c->caid==caid && c->provid==provid && c->sid==sid) return c;
		c = c->next;
	}
	return NULL;
}

static void nagra_cleanup(uint32_t now)
{
	// remove entradas com mais de 300s
	struct nagra_chn_data *c = nagra_list;
	struct nagra_chn_data *prev = NULL;
	while (c) {
		struct nagra_chn_data *next = c->next;
		if ( (uint32_t)(now - c->time) > 300000 ) {
			if (prev) prev->next = next;
			else nagra_list = next;
			free(c);
			nagra_list_size--;
		} else prev = c;
		c = next;
	}
}

static int nagra_accept_prov(struct cardserver_data *cs, uint32_t provid)
{
	int i;
	for (i=0; i<cs->card.nbprov; i++) {
		if (!cs->card.prov[i].id) return 1; // provider 0 aceita tudo
		if (cs->card.prov[i].id==provid) return 1;
	}
	return 0;
}

static void nagra_update(struct nagra_chn_data *c, uint8_t cw[16], uint32_t hash, uint32_t now)
{
	memcpy(c->cw, cw, 16);
	c->hash = hash;
	c->time = now;
}

// maquina de estados do ciclo por canal (igual a build antiga)
static int nagra_cycle(ECM_DATA *ecm, uint8_t cw[16], int sens)
{
	uint32_t now = GetTickCount();
	int r9, r10, i;

	pthread_mutex_lock(&nagra_mutex);
	nagra_cleanup(now);
	struct nagra_chn_data *c = nagra_find(ecm->caid, ecm->provid, ecm->sid);
	if (!c) {
		if (nagra_list_size >= NAGRA_MAXLIST) {
			pthread_mutex_unlock(&nagra_mutex);
			return NAGRA_OK;
		}
		c = malloc(sizeof(struct nagra_chn_data));
		memset(c, 0, sizeof(struct nagra_chn_data));
		c->caid = ecm->caid;
		c->provid = ecm->provid;
		c->sid = ecm->sid;
		memcpy(c->cw, cw, 16);
		c->hash = ecm->hash;
		c->time = now;
		c->next = nagra_list;
		nagra_list = c;
		nagra_list_size++;
		pthread_mutex_unlock(&nagra_mutex);
		return NAGRA_OK;
	}

	if (c->hash == ecm->hash) {
		if (!memcmp(c->cw, cw, 16)) {
			c->dup++;
			nagra_dup_total++;
			pthread_mutex_unlock(&nagra_mutex);
			return NAGRA_OK;
		}
		c->conf++;
		nagra_conf_total++;
		mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," nagra: conflicting dcw same ecm ch %04x:%06x:%04x hash %08x\n",
			ecm->caid, ecm->provid, ecm->sid, ecm->hash);
		pthread_mutex_unlock(&nagra_mutex);
		return NAGRA_DUP;
	}

	r9 = memcmp(c->cw, cw, 8) != 0;       // metade0 mudou
	r10 = memcmp(c->cw+8, cw+8, 8) != 0;  // metade1 mudou

	if (!(r9||r10)) {
		c->checked++;
		c->dup++;
		nagra_checked_total++;
		nagra_dup_total++;
		mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," nagra: duplicate dcw (old) ch %04x:%06x:%04x hash %08x\n",
			ecm->caid, ecm->provid, ecm->sid, ecm->hash);
		pthread_mutex_unlock(&nagra_mutex);
		return NAGRA_DUP;
	}

	if (c->state == 0) {
		// aprendizagem: 6 amostras
		if ( (!r9)||(!r10) ) c->cntb++; else c->cnta++;
		if (c->cnta + c->cntb >= 6) {
			if (c->cntb == 0) c->state = 1;
			else if (c->cnta == 0) c->state = 2;
			else { c->cnta = 0; c->cntb = 0; }
			mlogf(LOGDEBUG,getdbgflag(DBG_CACHE,0,0)," nagra: ch %04x:%06x:%04x mode=%s\n",
				ecm->caid, ecm->provid, ecm->sid,
				c->state==2 ? "half-cycle" : (c->state==1 ? "full-cw" : "mixed"));
		}
		nagra_update(c, cw, ecm->hash, now);
		pthread_mutex_unlock(&nagra_mutex);
		return NAGRA_OK;
	}

	if (c->state == 2) {
		// canal half-cycle: aceita sempre (a CWC trata esses)
		c->checked++;
		c->dup++;
		nagra_checked_total++;
		nagra_dup_total++;
		nagra_update(c, cw, ecm->hash, now);
		pthread_mutex_unlock(&nagra_mutex);
		return NAGRA_OK;
	}

	// state == 1: canal de CW completa (ambas as metades mudam)
	c->checked++;
	nagra_checked_total++;
	if ( (r9)&&(r10) ) {
		// ambas mudaram: normal -> verificar similaridade
		if (sens) {
			int same0 = 0, same1 = 0;
			for (i=0; i<8; i++) {
				if (c->cw[i]==cw[i]) same0++;
				if (c->cw[i+8]==cw[i+8]) same1++;
			}
			if (same1 >= sens || same0 >= sens) {
				c->conf++;
				nagra_conf_total++;
				mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," nagra: too similar cw ch %04x:%06x:%04x (same bytes %d/%d)\n",
					ecm->caid, ecm->provid, ecm->sid, same1, same0);
				pthread_mutex_unlock(&nagra_mutex);
				return NAGRA_SIMILAR;
			}
		}
		nagra_update(c, cw, ecm->hash, now);
		pthread_mutex_unlock(&nagra_mutex);
		return NAGRA_OK;
	}

	// so uma metade mudou -> fake half
	c->conf++;
	nagra_conf_total++;
	mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," nagra: fake half (repeated) ch %04x:%06x:%04x hash %08x\n",
		ecm->caid, ecm->provid, ecm->sid, ecm->hash);
	pthread_mutex_unlock(&nagra_mutex);
	return NAGRA_FAKEHALF;
}

int nagra_check(ECM_DATA *ecm, uint8_t cw[16])
{
	struct cardserver_data *cs = ecm->cs;
	if (!cs || !cs->option.nagra.enable) return NAGRA_OK;

	// caid 0x1813..0x1a12 (NAGRA: MEO/NOS 1813/1814 e 18xx/19xx)
	if ( (ecm->caid < 0x1813) || (ecm->caid > 0x1a12) ) return NAGRA_OK;

	// half-null: se uma metade e nula e o byte marcador e 0 -> sem checks
	if (!cw[0] && !cw[1] && !cw[2] && !cw[4] && !cw[5]) {
		if (!cw[6]) return NAGRA_OK;
	}
	else if (!cw[8] && !cw[9] && !cw[10] && !cw[12] && !cw[13]) {
		if (!cw[14]) return NAGRA_OK;
	}

	if (cs->option.nagra.chk) {
		if (!checksumDCW(cw)) {
			mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," nagra: bad checksum ch %04x:%06x:%04x\n",
				ecm->caid, ecm->provid, ecm->sid);
			return NAGRA_BADCHK;
		}
	}

	if (cs->option.nagra.prov) {
		if (!nagra_accept_prov(cs, ecm->provid)) {
			mlogf(LOGINFO,getdbgflag(DBG_CACHE,0,0)," nagra: bad provider %06x ch %04x:%06x:%04x\n",
				ecm->provid, ecm->caid, ecm->provid, ecm->sid);
			return NAGRA_BADPROV;
		}
	}

	if (cs->option.nagra.cycle) {
		return nagra_cycle(ecm, cw, cs->option.nagra.sensitive);
	}

	return NAGRA_OK;
}
