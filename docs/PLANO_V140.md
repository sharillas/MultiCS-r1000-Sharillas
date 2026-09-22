# Plano v1.40 — CONCLUIDO (todos os WP)

# Plano v1.40 â€” MultiCS r1000 by Sharillas (decidido com o dono)

## DecisÃµes
1. Protocolos: sÃ³ CCcam + Newcamd + MGcamd. Remover radegast/camd35/cs378x/freecccam/ccam3 + pÃ¡ginas GUI deles.
2. Softcam: remover do source + GUI (emu.c, pÃ¡gina, [BISS Emu], ENABLE EMULATOR BISS, Softcam.cfg).
3. Perfis: MANTER TODOS (3 satÃ©lites no saco de relÃ©). Minimizar opÃ§Ãµes/funÃ§Ãµes por perfil; redundante e morto sai.
4. GUI: sem script gerador â€” editar httpstyle.c directamente, desenvolvimento local no PC; remover o gerador da repo.

## Work packages
### WP1 â€” Remover protocolos
- src: cli-radegast.c, srv-radegast.c, msg-radegast.c, cli-camd35.c, srv-camd35.c, msg-camd35.c, cli-cs378x.c, srv-cs378x.c, srv-freecccam.c, cli-ccam3.c, srv-ccam3.c, ccam3_crypto.c (+ h files)
- main.c includes, build.ps1 (se preciso), config.c parses (RADEGAST/CAMD35/CS378X/FREECCCAM/CCAM3/C3/L/R), httpserver pÃ¡ginas (Newcamd/Mgcamd/CCcam ficam; pÃ¡ginas radegast/camd35/cs378x/freecccam/ccam3 saem), clientes_*.cfg exemplos
### WP2 â€” Remover Softcam
- emu.c/emu.h, pÃ¡gina Softcam da GUI, opÃ§Ãµes ENABLE EMULATOR/EMULATOR BISS/CONSTCW, perfil [BISS Emu], Softcam.cfg exemplo, ferramentas de softcam
### WP3 â€” CÃ³digo morto
- ifdef PUBLIC (102), WIN32 (20), MONOTHREAD_ACCEPT (12), FREECCCAM_SRV (18), RADEGAST_SRV (12), TWIN/VERBOSE/DEBUG_NETWORK*/NOPACK/FULL_UNROLL/PROXY/BUSY_SERVER/INOTIFY/MULTITHREADED/THREAD_CACHE_PIPE
- 2Âª versÃ£o ecm_setdcw (ifndef THREAD_DCW) + DCW LOG duplicado
### WP4 â€” Cycle engine (consolidar 5â†’1)
- Remover: DCW MINTIME, DCW CYCLE_CHECK, cwc.c inteiro, NAGRA CYCLE, STALE_CHECK antigo
- Novo: motor com o modelo estudado (ciclo 15s + alternÃ¢ncia 0/1 por canal; aprender em runtime; marcar desvios)
### WP5 â€” Anti-fake (consolidar 4â†’2)
- (1) validaÃ§Ã£o estrutural sempre-on: checksum NAGRA/nano-e0 + null + XOR
- (2) reputaÃ§Ã£o por fonte: cwbad + bad-cw cache por reader+CANAL (TTL configurÃ¡vel) â€” substitui o blacklist CWPK estÃ¡tico
- Remover: DCW FILTER MODE AUTO/LEARN/RULES + regras EXACT duplicadas nos perfis
### WP6 â€” Load-balance simplificado
- Novo ranking: SERVERS: (perfil) + health + bad-cw cache + hop1/hopN + priority (bÃ¡sico)
- Remover critÃ©rios: val/uphops/prorank/ecmperhr do sort (ficam internos se necessÃ¡rios)
### WP7 â€” Configs
- Perfis: ficam TODOS; remover opÃ§Ãµes redundantes/mortas de todos (CACHE STATIC, TIMING, NAGRA extras, DCWCHECK2/3, SKIPCWC_EXCLUDE_*, regras CWPK)
- opÃ§Ãµes novas: SERVERS:, STALE novo, BADCW TTL, hop=
### WP8 â€” GUI
- httpstyle.c editado directamente (local); remover tools_generate_httpstyle.py da repo; pÃ¡ginas dos protocolos removidos saem
### WP9 â€” Testes em produÃ§Ã£o + release v1.40

## Ordem
WP3 (morto) â†’ WP1+WP2 (remoÃ§Ãµes grandes) â†’ WP4+WP5 (motores novos) â†’ WP6 â†’ WP7 (configs) â†’ WP8 (GUI) â†’ build/teste contÃ­nuo na VPS â†’ WP9.

