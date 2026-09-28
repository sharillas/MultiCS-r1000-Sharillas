# Benchmark A/B — a nossa build vs a linhagem clássica

Objectivo: decidir com números (não opiniões) qual a melhor build para a fonte do circuito.

## Métricas (as mesmas nos dois períodos)

| Métrica | Fonte |
|---|---|
| Freezes na box de teste (nº + duração) | Watchdog da VU (sinal x/y) + observação |
| Anomalias `cyc:` por canal | CW Monitoring / debug |
| CWs más marcadas (cwbad) | CW Monitoring |
| Tempos de resposta (`found`) | Log da VU |
| Hits do cacheex + CWs recebidas por peer | Página CacheEX + DCW LOG |

## Desenho do teste (proposta ao colega)

**Fase 0 (preparação):** meter a nossa v1.45 a correr no **multics1 dele** (o relay secundário)
- Mesmas linhas/readers que o multics principal usa
- O binário antigo fica ao lado — rollback em 1 minuto
- A box de teste (a VU) liga-se ao multics1 por uma porta dedicada

**Fase 1 (baseline):** 3-5 dias com a build actual dele (PreRelease2026) — recolher as métricas

**Fase 2 (a nossa):** 3-5 dias com a nossa v1.45 — mesmas métricas

**Fase 3 (decisão):** comparar lado a lado — se a nossa ganhar, passa para o multics principal

## Notas
- O multics1 = o campo ideal: mesma posição, risco zero para a produção principal
- A nossa build lê a config dele (a sintaxe é a mesma família); as opções que não conhece ficam com avisos no log
- O watchdog da VU já está activo (corre desde 24/09)
