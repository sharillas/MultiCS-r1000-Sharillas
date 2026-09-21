# Plano v1.40 — MultiCS r1000 by Sharillas (decidido com o dono)

## Decisões
1. Protocolos: só CCcam + Newcamd + MGcamd. Remover radegast/camd35/cs378x/freecccam/ccam3 + páginas GUI deles.
2. Softcam: remover do source + GUI (emu.c, página, [BISS Emu], ENABLE EMULATOR BISS, Softcam.cfg).
3. Perfis: MANTER TODOS (3 satélites no saco de relé). Minimizar opções/funções por perfil; redundante e morto sai.
4. GUI: sem script gerador — editar httpstyle.c directamente, desenvolvimento local no PC; remover o gerador da repo.

## Work packages
### WP1 — Remover protocolos
- src: cli-radegast.c, srv-radegast.c, msg-radegast.c, cli-camd35.c, srv-camd35.c, msg-camd35.c, cli-cs378x.c, srv-cs378x.c, srv-freecccam.c, cli-ccam3.c, srv-ccam3.c, ccam3_crypto.c (+ h files)
- main.c includes, build.ps1 (se preciso), config.c parses (RADEGAST/CAMD35/CS378X/FREECCCAM/CCAM3/C3/L/R), httpserver páginas (Newcamd/Mgcamd/CCcam ficam; páginas radegast/camd35/cs378x/freecccam/ccam3 saem), clientes_*.cfg exemplos
### WP2 — Remover Softcam
- emu.c/emu.h, página Softcam da GUI, opções ENABLE EMULATOR/EMULATOR BISS/CONSTCW, perfil [BISS Emu], Softcam.cfg exemplo, ferramentas de softcam
### WP3 — Código morto
- ifdef PUBLIC (102), WIN32 (20), MONOTHREAD_ACCEPT (12), FREECCCAM_SRV (18), RADEGAST_SRV (12), TWIN/VERBOSE/DEBUG_NETWORK*/NOPACK/FULL_UNROLL/PROXY/BUSY_SERVER/INOTIFY/MULTITHREADED/THREAD_CACHE_PIPE
- 2ª versão ecm_setdcw (ifndef THREAD_DCW) + DCW LOG duplicado
### WP4 — Cycle engine (consolidar 5→1)
- Remover: DCW MINTIME, DCW CYCLE_CHECK, cwc.c inteiro, NAGRA CYCLE, STALE_CHECK antigo
- Novo: motor com o modelo estudado (ciclo 15s + alternância 0/1 por canal; aprender em runtime; marcar desvios)
### WP5 — Anti-fake (consolidar 4→2)
- (1) validação estrutural sempre-on: checksum NAGRA/nano-e0 + null + XOR
- (2) reputação por fonte: cwbad + bad-cw cache por reader+CANAL (TTL configurável) — substitui o blacklist CWPK estático
- Remover: DCW FILTER MODE AUTO/LEARN/RULES + regras EXACT duplicadas nos perfis
### WP6 — Load-balance simplificado
- Novo ranking: SERVERS: (perfil) + health + bad-cw cache + hop1/hopN + priority (básico)
- Remover critérios: val/uphops/prorank/ecmperhr do sort (ficam internos se necessários)
### WP7 — Configs
- Perfis: ficam TODOS; remover opções redundantes/mortas de todos (CACHE STATIC, TIMING, NAGRA extras, DCWCHECK2/3, SKIPCWC_EXCLUDE_*, regras CWPK)
- opções novas: SERVERS:, STALE novo, BADCW TTL, hop=
### WP8 — GUI
- httpstyle.c editado directamente (local); remover tools_generate_httpstyle.py da repo; páginas dos protocolos removidos saem
### WP9 — Testes em produção + release v1.40

## Ordem
WP3 (morto) → WP1+WP2 (remoções grandes) → WP4+WP5 (motores novos) → WP6 → WP7 (configs) → WP8 (GUI) → build/teste contínuo na VPS → WP9.
