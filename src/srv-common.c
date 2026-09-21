
#ifdef CACHEEX
#ifdef CS378X_SRV
void forward_cs378x(ECM_DATA *ecm)
{
	struct server_data *srv = cfg.cacheexserver;
	while (srv) {
		if ( (srv->type==TYPE_CS378X) && (srv->cacheex_mode==2) && (srv->handle>0) && (srv->cacheex_forward) ) {
			if ( acceptshare( srv->sharelimits, ecm->caid, ecm->provid) ) {
				if (srv->cacheex_forward==2)
					cs378x_sendecm_extrasrv(srv, ecm);
				else
					cs378x_sendecm_srv(srv, ecm);
				srv->lastecmtime = GetTickCount();
				srv->ecmnb++;
				srv->busy=1;
				srv->ecm.msgid++;
				if (srv->ecm.msgid>0xfff) srv->ecm.msgid = 1;
				srv->ecm.request = ecm;
				srv->cacheex.push[0]++;
			}
		}
		srv = srv->next;
	}
}
#endif
#endif

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

