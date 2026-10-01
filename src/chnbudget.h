///////////////////////////////////////////////////////////////////////////////
// CHANNEL TIMING BUDGET (cryptoperiod adaptativo)
// Estima o cryptoperiod por canal (tempo entre mudancas de CW) via EWMA
// e alimenta o timing budget de decode (th-ecm) e o TTL adaptativo da
// cache (clustredcache). MultiCS r1000 - by Sharillas@2026
///////////////////////////////////////////////////////////////////////////////
#ifndef _CHNBUDGET_H_
#define _CHNBUDGET_H_

#define CHNB_MINPERIOD    1000   // amostras abaixo disto sao ignoradas
#define CHNB_MAXPERIOD  120000   // amostras acima disto sao ignoradas

struct chnb_data
{
	struct chnb_data *next;
	uint16_t caid;
	uint32_t provid;
	uint16_t sid;
	uint8_t  cw[16];
	uint32_t lastchange;   // ticks da ultima mudanca de CW
	int32_t  period;       // cryptoperiod estimado (ms) - 0 = desconhecido
	int      observations;
	int      changes;
};

void chnbudget_observe(uint16_t caid, uint32_t provid, uint16_t sid, uint8_t cw[16]);
int  chnbudget_getperiod(uint16_t caid, uint32_t provid, uint16_t sid);
void chnbudget_cleanup();

extern struct chnb_data *chnb_list;

#endif
