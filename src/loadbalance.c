
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

// 0: different ; 1:~equivalent
int cs_cmp_card( struct cs_card_data *card, struct cardserver_data *cs)
{
	int i,j,found;
	int nbsame = 0;
	int nbdiff = 0;

	if (card->caid!=cs->card.caid) return 0;

/*
	if ( ((card->caid & 0xff00)==0x1800)
		|| ((card->caid & 0xff00)==0x0900)
		|| ((card->caid & 0xff00)==0x0b00) ) return 1;
*/
	if ( ((card->caid & 0xff00)!=0x0100) && ((card->caid & 0xff00)!=0x0500) ) return 1;

	for(i=0; i<card->nbprov;i++) {
		found = 0;
		for(j=0; j<cs->card.nbprov;j++)
			if (card->prov[i]==cs->card.prov[j].id) {
				found = 1;
				break;
			}
		if (found) nbsame++; else nbdiff++;
	}

	if ( (nbsame==card->nbprov)||(nbsame==cs->card.nbprov) ) return 1;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int match_card( uint16_t caid, uint32_t prov, struct cs_card_data* card)
{
	if (caid!=card->caid) return 0;
	// Dont care about provider for caid non via/seca
	if ( ((card->caid & 0xff00)!=0x0100) && ((card->caid & 0xff00)!=0x0500) ) return 1;
	int i;
	for(i=0; i<card->nbprov;i++) if (prov==card->prov[i]) return 1;
	return 0;
}

// Search for a card with best sid val.
// TODO: option validecmtime-ecmtime
int sidata_getval(struct server_data *srv, struct cardserver_data *cs, uint16_t caid, uint32_t prov, uint16_t sid, struct cs_card_data **selcard )
{
	struct cs_card_data *card = NULL;

	*selcard = NULL;
	if ( (srv->type==TYPE_NEWCAMD) || (srv->type==TYPE_RADEGAST) || (srv->type==TYPE_CAMD35) || (srv->type==TYPE_CS378X) ) {
		card = srv->card;
		while (card) {
			if ( match_card(caid,prov,card) ) break;
			card = card->next;
		}
		*selcard = card;
		if (card) {
			struct sid_data *sidata = card->sids[sid>>8];
			while (sidata) {
				if (sidata->sid==sid)
				if (sidata->prov==prov) return sidata->val;
				sidata = sidata->next;			
			}
		}
		return 0; // Channel not found in sid cache
	}
#ifdef CCCAM_CLI
	else if (srv->type==TYPE_CCCAM) {
		// Search for availabe card cannot be found in sids
		*selcard = NULL;
		int selsidvalue = 0;
		card = srv->card;
		while (card) {
			if ( match_card(caid,prov,card)
				|| ( !prov && cs && cs_cmp_card(card, cs) ) // use for prov=0
			) {
				// Search fo sid
				int sidvalue = 0; // by default

				struct sid_data *sidata = card->sids[sid>>8];
				while (sidata) {
					if ( (sidata->sid==sid)&&(sidata->prov==prov) ) {
						sidvalue = sidata->val;
						break;
					}
					sidata = sidata->next;			
				}

				// Check with Selected card (sidvalue) TODO: classify by ecmtime, decode, stability
				if (*selcard) {
					if ( selsidvalue<0 ) {
						if (sidvalue>=0) { *selcard = card; selsidvalue = sidvalue; }
						else if (card->uphops<(*selcard)->uphops) { *selcard = card; selsidvalue = sidvalue; }
					}
					else if ( selsidvalue==0 ) {
						if (sidvalue>0) { *selcard = card; selsidvalue = sidvalue; }
						else if (sidvalue==0) if (card->uphops<(*selcard)->uphops) { *selcard = card; selsidvalue = sidvalue; }
					}
					else if (sidvalue>0) if (card->uphops<(*selcard)->uphops) { *selcard = card; selsidvalue = sidvalue; }
				}
				else { *selcard = card; selsidvalue = sidvalue; }
			}
			card = card->next;
		}
		return selsidvalue;
	}
#endif
	return 0;
}



#define MAXSRVTAB 255

struct srvtab_data
{
	struct server_data *srv;
	struct cs_card_data *card; // selected card
	uint32_t shareid; // Selected Card ShareID
	int uphops;
	int val; // sid value
	unsigned int ecmtime; // Card Ecmtime
	uint32_t ecmperhr;
	int health; // health score 0-1000 (0 = nao calculado)
	int prorank; // posicao do protocolo no FALLBACK ORDER (255 = nao listado)
};


struct srvtab_data srvlist[MAXSRVTAB];
struct srvtab_data *psrvlist[MAXSRVTAB];
struct srvtab_data *srvtemp;


// posicao do protocolo do server no FALLBACK ORDER do perfil (255 = nao listado)
int srv_fallback_rank(struct cardserver_data *cs, struct server_data *srv)
{
	int i;
	for (i=0; i<8; i++) {
		if (!cs->option.fallback.order[i]) break;
		if (cs->option.fallback.order[i]==srv->type) return i;
	}
	return 255;
}


// Health score do server: 0-1000 (maior = melhor)
// 0 = nao calculado (sem amostras suficientes ou health desligado)
int srv_healthscore(struct cardserver_data *cs, struct server_data *srv)
{
	if (!cs->option.health.enable) return 0;
	int ecmnb = srv->ecmnb;
	if (ecmnb < cs->option.health.minecms) return 0;

	int wsuc = cs->option.health.wsuc;
	int wlat = cs->option.health.wlat;
	int wsta = cs->option.health.wsta;
	int werr = cs->option.health.werr;
	if (!(wsuc+wlat+wsta+werr)) { wsuc = 40; wlat = 30; wsta = 10; werr = 20; }

	// Sucesso: racio de ECMs com DCW
	int suc = (srv->ecmok*1000)/ecmnb;

	// Latencia: tempo medio de resposta (0ms -> 1000, >=4000ms -> 0)
	int avg = srv->ecmok>0 ? srv->ecmoktime/srv->ecmok : 4000;
	int lat = 1000 - (avg*1000)/4000;
	if (lat<0) lat = 0;

	// Estabilidade: uptime (10min = max)
	uint32_t ticks = GetTickCount();
	uint32_t uptime = ((ticks - srv->connection.time) + srv->connection.uptime) / 1000;
	int sta = (int)((uptime*1000)/600);
	if (sta>1000) sta = 1000;

	// Erros: timeouts + bad dcw (20 erros = penalizacao maxima)
	int err = (srv->ecmtimeout + srv->ecmerrdcw) * 50;
	if (err>1000) err = 1000;

	int score = (suc*wsuc + lat*wlat + sta*wsta - err*werr)/100;
	if (score<0) score = 0;
	if (score>1000) score = 1000;
	return score;
}


// a deve ficar antes de b por health? (1 = sim)
inline int health_better(struct srvtab_data *a, struct srvtab_data *b)
{
	if (a->health && b->health && a->health!=b->health) return a->health < b->health;
	return 0;
}


// score para a GUI: usa os pesos do primeiro perfil com HEALTH que usa este server
int srv_healthscore_gui(struct server_data *srv, int *enabled)
{
	struct cardserver_data *cs = cfg.cardserver;
	while (cs) {
		if (cs->option.health.enable) {
			int i;
			for (i=0; i<MAX_CSPORTS; i++) {
				if (!srv->csport[i]) break;
				if (srv->csport[i]==cs->newcamd.port) {
					*enabled = 1;
					return srv_healthscore(cs, srv);
				}
			}
		}
		cs = cs->next;
	}
	*enabled = 0;
	return 0;
}


int srvtab_arrange(struct cardserver_data *cs, ECM_DATA *ecm, int bestone )
{
	int i,j;
	int nbsrv = 0;
	struct server_data *srv;

	memset( srvlist, 0 , sizeof(srvlist) );
	nbsrv = 0;

	// MULTICARD Servers Selection (Newcamd,CCcam,Mgcamd...) ;)
	unsigned int ticks = GetTickCount();
	srv = cfg.server;
	while ( srv && (nbsrv<MAXSRVTAB) ) {
		if ( !IS_DISABLED(srv->flags)&&(srv->connection.status>0) )
		if (
			( cs->option.fallownewcamd && (srv->type==TYPE_NEWCAMD) )
			|| ( cs->option.fallowcccam && (srv->type==TYPE_CCCAM) )
			|| ( cs->option.fallowradegast && (srv->type==TYPE_RADEGAST) )
			|| ( cs->option.fallowcamd35 && (srv->type==TYPE_CAMD35) )
			|| ( cs->option.fallowcs378x && (srv->type==TYPE_CS378X) )
		)
		// Remove Circular request: check for client ip & srv ip
		if ( (srv->host->ip==0x0100007F) || ( !ecm_checkip(ecm, srv->host->ip) && !ecm_checksrvip(ecm, srv->host->ip) ) )
		{
			// Check for CS PORTS
			for(i=0; i<MAX_CSPORTS; i++ ) {
				if (!srv->csport[i]) break;
				if (srv->csport[i]==cs->newcamd.port) {
					i=0;
					break;
				}
			}
			if (i==0) { // ADD TO PROFILE
				//Check for used servers, dont reuse
				for (i=0; i<20;i++) {
					if (!ecm->server[i].srvid) break;
					if (ecm->server[i].srvid==srv->id) break;
				}
				if ( (i>=20)||(ecm->server[i].srvid!=srv->id) ) {
					// Check for ECM TIMEOUT
					if ( (srv->busy)&&((srv->lastecmtime+9000)<ticks) ) { // timeout
						mlogf(LOGWARNING,getdbgflag(DBG_SERVER,0,srv->id)," ??? server (%s:%d) doesnt send ecm answer\n", srv->host->name,srv->port);
						srv->ecmtimeout++;
						srv->busy = 0;
						disconnect_srv(srv);
					}
					else {
						// Check for newcamd server sids
						i = 0;
						if ( srv->sids && (srv->type==TYPE_NEWCAMD) ) {
							struct sid_chid_data *sid = srv->sids;
							for(i=0; i<MAX_SIDS; i++,sid++) {
								if ( sid->sid==0 ) break;
								if (sid->sid==ecm->sid)
								if (!sid->chid || (sid->chid==ecm->chid) ) { i=0; break; }
							}
						}
						if (i==0) {
							// check for any card to decode
							pthread_mutex_lock(&srv->lock);

							// best card to decode is selected, it may there is only worst one but is returned
							struct cs_card_data *pcard = NULL;
							int val = sidata_getval( srv, cs, ecm->caid, ecm->provid, ecm->sid, &pcard);
							if ( !cs->option.maxfailedecm || (val > -cs->option.maxfailedecm) ) {  // available card+sid : block card that have decode failed on sid
								if (pcard) {
									int ecmtime = 0;
									if (srv->type==TYPE_CCCAM)
										if (pcard->ecmok>10) ecmtime = pcard->ecmoktime/pcard->ecmok; else ecmtime = 0;
									else
										if (srv->ecmok>10) ecmtime = srv->ecmoktime/srv->ecmok; else ecmtime = 0;
									if ( !cs->option.server.validecmtime || (ecmtime<cs->option.server.validecmtime) ) {
										srvlist[nbsrv].srv = srv;
										srvlist[nbsrv].card = pcard; // default card
										srvlist[nbsrv].shareid = pcard->shareid; // default card
										srvlist[nbsrv].uphops = pcard->uphops;
										srvlist[nbsrv].val = val;
										srvlist[nbsrv].ecmtime = ecmtime;
										psrvlist[nbsrv] = &srvlist[nbsrv];
										nbsrv++;
									}
								}
							}

							pthread_mutex_unlock(&srv->lock);
						}
					}
				}
			}
		}
		srv = srv->next;
	}
	//mlogf(LOGDEBUG,0, " A*srvtab_arrange(%04x:%06x:%04x) Servers = %d\n", ecm->caid, ecm->provid, ecm->sid, nbsrv);

	//Remove Cardservers with delay time
	if (cs->option.server.timeperecm) {
		i=0;
		for(j=0; j<nbsrv; j++) {
			//if ( (psrvlist[j]->srv->host->ip!=0x0100007F)&&(psrvlist[j]->srv->type==TYPE_NEWCAMD) )
			if ( (psrvlist[j]->srv->type==TYPE_NEWCAMD) ) {
				unsigned int msperecm = ( (ticks-psrvlist[j]->srv->connection.time) + psrvlist[j]->srv->connection.uptime ) / (psrvlist[j]->srv->ecmnb+1);
				unsigned int tim;
				if ( msperecm > (2*cs->option.server.timeperecm) ) tim = 0;
				else if ( msperecm > cs->option.server.timeperecm ) tim = (2*cs->option.server.timeperecm)-msperecm;
				else tim = cs->option.server.timeperecm;
				if ( (psrvlist[j]->srv->lastecmtime+tim)<=ticks ) {
					if (i<j) psrvlist[i] = psrvlist[j];
					i++;
				}
			}
			else {
				if (i<j) psrvlist[i] = psrvlist[j];
				i++;
			}
		}
		psrvlist[i] = NULL;
		nbsrv = i;
	}
	//mlogf(LOGDEBUG,0, " B*srvtab_arrange(%04x:%06x:%04x) Servers = %d\n", ecm->caid, ecm->provid, ecm->sid, nbsrv);

	// FALLBACK CROSS-PROTOCOL: filtrar por protocolo preferido
	if (cs->option.fallback.enable && cs->option.fallback.order[0]) {
		ticks = GetTickCount();
		int haveprimary = 0;
		for(j=0; j<nbsrv; j++) {
			psrvlist[j]->prorank = srv_fallback_rank(cs, psrvlist[j]->srv);
			if (psrvlist[j]->prorank==0) haveprimary = 1;
		}
		// excluir protocolos fora da lista
		i=0;
		for(j=0; j<nbsrv; j++) {
			if (psrvlist[j]->prorank!=255) {
				if (i<j) psrvlist[i] = psrvlist[j];
				i++;
			}
		}
		psrvlist[i] = NULL;
		nbsrv = i;
		// adiar fallbacks enquanto existir protocolo primario e o ECM for recente
		if (haveprimary && ((uint32_t)(ticks - ecm->recvtime) < (uint32_t)cs->option.fallback.timeout)) {
			i=0;
			for(j=0; j<nbsrv; j++) {
				if (psrvlist[j]->prorank==0) {
					if (i<j) psrvlist[i] = psrvlist[j];
					i++;
				}
			}
			psrvlist[i] = NULL;
			nbsrv = i;
		}
	}

#ifndef PUBLIC
	// Store number of available servers, Runtime ADD SIDS
	if (ecm->sid) {
		for(i=0; i<1024; i++) {
			if (cs->deniedsids[i].sid==ecm->sid) {
				cs->deniedsids[i].nbsrv = nbsrv;
				break;
			}
			if (!cs->deniedsids[i].sid) {
				cs->deniedsids[i].sid = ecm->sid;
				cs->deniedsids[i].nbsrv = nbsrv;
				break;
			}
		}
	}
#endif


#ifndef PUBLIC
	// Check if there is no/few servers to decode, send decode failed to client
	// dont get from few servers (for many cccam servers)
	if (nbsrv<=cs->option.server.threshold) {
		return -1;
	}
#else
	if (!nbsrv) return -1;
#endif


	// Remove Busy Servers
	i=0;
	for(j=0; j<nbsrv; j++) {
		if (!psrvlist[j]->srv->busy) {
			if (i<j) psrvlist[i] = psrvlist[j];
			i++;
		}
	}
	psrvlist[i] = NULL;
	nbsrv = i;


//// ARRANGE

	// Arrange by ECM LAST SENT TIME
	for(i=0; i<nbsrv-1; i++)
		for(j=i+1; j<nbsrv; j++)
			if ( psrvlist[i]->srv->lastecmtime > psrvlist[j]->srv->lastecmtime ) {
				srvtemp = psrvlist[i];
				psrvlist[i] = psrvlist[j];
				psrvlist[j] = srvtemp;
			}

	ticks=GetTickCount();
	for(i=0; i<nbsrv; i++)
		psrvlist[i]->ecmperhr = (psrvlist[i]->srv->ecmnb*3600*1000) / ( 1+ (ticks-psrvlist[i]->srv->connection.time)+psrvlist[i]->srv->connection.uptime );

	// HEALTH SCORING: pontua cada server e remove os doentes (dropoff)
	if (cs->option.health.enable) {
		for(i=0; i<nbsrv; i++) {
			psrvlist[i]->health = srv_healthscore(cs, psrvlist[i]->srv);
			mlogf(LOGDEBUG,getdbgflagpro(DBG_SERVER,0,psrvlist[i]->srv->id,cs->id)," health: server (%s:%d) score=%d (ok=%d/%d tmo=%d baddcw=%d)\n",
				psrvlist[i]->srv->host->name, psrvlist[i]->srv->port, psrvlist[i]->health,
				psrvlist[i]->srv->ecmok, psrvlist[i]->srv->ecmnb, psrvlist[i]->srv->ecmtimeout, psrvlist[i]->srv->ecmerrdcw);
		}

		if (cs->option.health.dropoff>0) {
			int havegood = 0;
			for(j=0; j<nbsrv; j++)
				if (psrvlist[j]->health >= cs->option.health.dropoff) { havegood = 1; break; }
			if (havegood) {
				i=0;
				for(j=0; j<nbsrv; j++) {
					if (psrvlist[j]->health >= cs->option.health.dropoff) {
						if (i<j) psrvlist[i] = psrvlist[j];
						i++;
					}
					else mlogf(LOGDEBUG,getdbgflagpro(DBG_SERVER,0,psrvlist[j]->srv->id,cs->id)," [!] server (%s:%d) health=%d abaixo de dropoff=%d, fora deste pedido\n",
						psrvlist[j]->srv->host->name, psrvlist[j]->srv->port, psrvlist[j]->health, cs->option.health.dropoff);
				}
				psrvlist[i] = NULL;
				nbsrv = i;
			}
		}
	}

	if (!bestone)

		// Arrange by ECM LAST SENT TIME && unbusy state & sid ok
		for(i=0; i<nbsrv-1; i++)
		for(j=i+1; j<nbsrv; j++) {

				if ( (psrvlist[i]->val>=0)&&(psrvlist[i]->srv->priority > psrvlist[j]->srv->priority) ) continue;
				if ( (psrvlist[j]->val>=0)&&(psrvlist[j]->srv->priority > psrvlist[i]->srv->priority) ) { // check if using local card
					srvtemp = psrvlist[i];
					psrvlist[i] = psrvlist[j];
					psrvlist[j] = srvtemp;
				}
				else if (psrvlist[i]->val>=0) {
					if (psrvlist[j]->val>=0) {
						// Check for card uphops
						if (psrvlist[i]->uphops > psrvlist[j]->uphops) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if ( psrvlist[i]->prorank > psrvlist[j]->prorank ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if ( health_better(psrvlist[i], psrvlist[j]) ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if  ( psrvlist[i]->ecmperhr > psrvlist[j]->ecmperhr ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
					}
				}
				else if (psrvlist[i]->val==-1) {
					if (psrvlist[j]->val>=0) {
						srvtemp = psrvlist[i];
						psrvlist[i] = psrvlist[j];
						psrvlist[j] = srvtemp;
					}
					else if (psrvlist[j]->val==-1) {
						if ( psrvlist[i]->prorank > psrvlist[j]->prorank ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if ( health_better(psrvlist[i], psrvlist[j]) ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if  ( psrvlist[i]->ecmperhr > psrvlist[j]->ecmperhr ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
					}
				}
				else {
					if (psrvlist[j]->val>=-1) {
						srvtemp = psrvlist[i];
						psrvlist[i] = psrvlist[j];
						psrvlist[j] = srvtemp;
					}
					else {
						if ( psrvlist[i]->prorank > psrvlist[j]->prorank ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if ( health_better(psrvlist[i], psrvlist[j]) ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if  ( psrvlist[i]->ecmperhr > psrvlist[j]->ecmperhr ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
					}
				}
		}

	else

		for(i=0; i<nbsrv-1; i++)
		for(j=i+1; j<nbsrv; j++) {

				if ( (psrvlist[i]->val>0)&&(psrvlist[i]->srv->priority > psrvlist[j]->srv->priority) ) continue;

				if (psrvlist[i]->val>0) {
					if ( psrvlist[j]->srv->priority > psrvlist[i]->srv->priority ) {
						srvtemp = psrvlist[i];
						psrvlist[i] = psrvlist[j];
						psrvlist[j] = srvtemp;
					}
					else if (psrvlist[j]->val>0) {
						if ( psrvlist[i]->prorank > psrvlist[j]->prorank ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if ( health_better(psrvlist[i], psrvlist[j]) ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if  ( ( psrvlist[i]->ecmperhr > psrvlist[j]->ecmperhr ) ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
					}
				}
				else if (psrvlist[i]->val==0) {
					if (psrvlist[j]->val>0) {
						srvtemp = psrvlist[i];
						psrvlist[i] = psrvlist[j];
						psrvlist[j] = srvtemp;
					}
					else if (psrvlist[j]->val==0) {
						if ( psrvlist[i]->prorank > psrvlist[j]->prorank ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if ( health_better(psrvlist[i], psrvlist[j]) ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
						else if  ( ( psrvlist[i]->ecmperhr > psrvlist[j]->ecmperhr ) ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
					}
				}
				else {
					if (psrvlist[j]->val>=0) {
						srvtemp = psrvlist[i];
						psrvlist[i] = psrvlist[j];
						psrvlist[j] = srvtemp;
					}
					else {
						if  ( psrvlist[i]->val < psrvlist[j]->val ) {
							srvtemp = psrvlist[i];
							psrvlist[i] = psrvlist[j];
							psrvlist[j] = srvtemp;
						}
					}
				}
		}

	return nbsrv;

}


