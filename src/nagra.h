///////////////////////////////////////////////////////////////////////////////
// NAGRA PROTECTION (caid 18xx/19xx/1a0x)
// Re-implementacao fiel do modulo que existia na build antiga (reconstruido
// a partir do binario): checksum, provider, ciclo de CW por canal
// (aprendizagem de 6 amostras), similaridade (sensitive), duplicate/
// conflicting/fake-half. MultiCS r1000 - by Sharillas@2026
///////////////////////////////////////////////////////////////////////////////
#ifndef _NAGRA_H_
#define _NAGRA_H_

#define NAGRA_MAXLIST 499

struct nagra_chn_data
{
	struct nagra_chn_data *next;
	uint16_t caid;
	uint32_t provid;
	uint16_t sid;
	uint8_t  cw[16];
	uint32_t hash;
	uint32_t time;
	uint8_t  state;   // 0 = aprendizagem, 1 = canal de CW completa, 2 = half-cycle
	uint8_t  cnta;    // mudancas em ambas as metades
	uint8_t  cntb;    // mudancas numa so metade
	uint32_t checked;
	uint32_t dup;
	uint32_t conf;
};

// codigos de retorno (iguais aos da build antiga)
#define NAGRA_OK      0
#define NAGRA_BADCHK  2
#define NAGRA_BADPROV 3
#define NAGRA_FAKEHALF 4
#define NAGRA_DUP     5
#define NAGRA_SIMILAR 6

extern struct nagra_chn_data *nagra_list;
extern uint32_t nagra_checked_total;
extern uint32_t nagra_dup_total;
extern uint32_t nagra_conf_total;

int nagra_check(ECM_DATA *ecm, uint8_t cw[16]);

#endif
