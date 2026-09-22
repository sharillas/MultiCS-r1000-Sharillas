///////////////////////////////////////////////////////////////////////////////
// NAGRA PROTECTION (caid 18xx/19xx/1a0x)
// v1.40: validacao estrutural - checksum das 4 quads + provider do perfil.
// (a validacao de ciclo moveu-se para o CYCLE ENGINE em setdcw.c)
///////////////////////////////////////////////////////////////////////////////

static int nagra_accept_prov(struct cardserver_data *cs, uint32_t provid)
{
	int i;
	for (i=0; i<cs->card.nbprov; i++) {
		if (!cs->card.prov[i].id) return 1; // provider 0 aceita tudo
		if (cs->card.prov[i].id==provid) return 1;
	}
	return 0;
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

	return NAGRA_OK;
}
