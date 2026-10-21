///////////////////////////////////////////////////////////////////////////////
// CW CYCLE CHECK (estilo OSCam module-cw-cycle-check)
// Maquina de estados 0-4 por canal (caid:provid:sid:tag):
//   0-2: aprendizagem do cycletime (intervalo entre mudancas de CW)
//   3:   verificacao (same CW fora de janela, ciclo invalido, replay ECM)
//   4:   janela keepcycletime (so verifica qual metade cicla)
// MultiCS r1000 - by Sharillas@2026
///////////////////////////////////////////////////////////////////////////////
#ifndef _CWC_H_
#define _CWC_H_

#define CWC_MAXHIST 15
#define CWC_TOLERANCE 2000   // +/- 2s (equivalente ao OSCam)
#define CWC_LOCKTIME   3000  // lock aprendizagem (equiv fallbacktimeout OSCam)

// resultados internos (espelham ret codes do OSCam checkcwcycle_int)
#define CWC_RET_OK     0 // ciclo valido
#define CWC_RET_NOK    1 // bad CW cycle
#define CWC_RET_OLD    2 // ECM antigo (replay)
#define CWC_RET_IGN    3 // ignorado
#define CWC_RET_SAME   4 // mesma CW (dentro da janela)
#define CWC_RET_FB     5 // fallback reader (nao usado no multics)
#define CWC_RET_LEARN  6 // aprendizagem (stage<3)
#define CWC_RET_LEARN4 7 // aprendizagem (stage 4)
#define CWC_RET_CE     8 // cycletime vindo de cacheex (nao usado ainda)

struct cwc_data
{
	struct cwc_data *next;
	uint16_t caid;
	uint32_t provid;
	uint16_t sid;
	uint8_t  tag;
	int8_t   stage;          // 0-4
	int32_t  cycletime;      // ms (aprendido)
	int32_t  dyncycletime;   // ms (jitter observado)
	uint32_t time;           // ticks (ms) da ultima mudanca de CW
	uint32_t locktime;       // lock da fase de aprendizagem
	int8_t   nextcyclecw;    // 0=CW0 cicla a seguir, 1=CW1, 2=desconhecido
	uint8_t  cw[16];         // ultima CW valida
	uint8_t  stage4_repeat;
	int      keepminutes;    // keepcycletime do perfil no momento da criacao
	struct {
		uint32_t hash;
		uint8_t  tag;
		uint8_t  cw[16];
	} hist[CWC_MAXHIST];     // ring buffer dos ultimos 15 ECMs (replay)
	int8_t   histidx;
	// stats
	int checked;
	int ok;
	int bad;
	int ign;
};

struct cwc_stats_data
{
	int entries;
	int checked;
	int ok;
	int bad;
	int ign;
	int dropped;
};

extern struct cwc_data *cwc_list;
extern struct cwc_stats_data cwc_stats;

// 0 = passa, -1 = drop (motivo no log)
int cwc_check(struct cache_data *req, uint8_t cw[16], int peerid);

// limpeza periodica (chamada internamente a cada 2min)
void cwc_cleanup();

#endif
