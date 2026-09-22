## v1.28 (2026-09-14)
- **Novo tema "Stats Tiles"** (dark + light mode) com **menu lateral** (sidebar fixa): as 14 pÃ¡ginas ficam em coluna Ã  esquerda com badges; dashboard com **tiles de nÃºmeros grandes** (Uptime, Servers, Clients, ECM Totais, RAM, Softcam) e barras de carga.
- **Tabela de Servers**: coluna EcmTime removida; Cards continua na Ãºltima coluna com **chips coloridos por CAID:PROVID** (ident + nome do provider MEO/NOS/etc).
- **ON/OFF**: interruptores visuais (pills verdes/vermelhas) em todas as pÃ¡ginas de estado.
- **Drop-downs e uploads** restilizados (setas prÃ³prias, file-selector button, focus rings).
- **DEBUG**: log com fundo de consola e realce das linhas; botÃµes DBG/INF uniformizados.
- CorreÃ§Ã£o do xmlupdateRow (linhas com nÂº variÃ¡vel de colunas); cache-bust do CSS v=1128; tÃ­tulo/footer v1.28.

# Changelog - MultiCS r1000 by Sharillas
Todas as alteraÃ§Ãµes notÃ¡veis desde a v1.20, por versÃ£o.

## v1.26 (2026-08-31)
- **Estrutura de ficheiros PT**: `profiles.cfg` (perfis + users newcamd), `servidores.cfg` (N:/C:/L: + cache + cacheex), `clientes_cccam/mgcamd/cs378x/camd35/cache.cfg` (users por protocolo). Compatibilidade total: manter tudo no `multics.cfg` continua a funcionar; o parser Ã© o mesmo (INCLUDEs).
- **CWPK LEARNING**: o filtro DCW FILTER aprende novas regras em runtime â€” 5 CWs mÃ¡s iguais do mesmo IP â†’ regra EXACT adicionada (cap 16, FIFO, log + evento no dashboard). `DCW FILTER LEARN: YES` por perfil.
- **Eventos recentes no Dashboard**: secÃ§Ã£o "ProteÃ§Ãµes & Eventos" com uptime do processo, ECMs totais (OK/NOK), regras CWPK aprendidas e o anel dos Ãºltimos eventos (AUTO ativado, FAILBAN, ANTICASCADE, LEARNING).
- **Packages**: coluna "Filtros" com badges por pacote (CAK7, CWPK AUTO/ATIVO/DROP/LOGONLY, LEARN, ECM FILTER).
- **Editor Configs**: CCcam.providers editÃ¡vel; dropdown uniforme com caminho real; multics.css e ip2country.csv fora do editor (geridos Ã  parte).
- **CabeÃ§alhos para leigos** em todos os ficheiros de config (produÃ§Ã£o + exemplos): como fazer N/C/L-line, F-line, MG user, CACHE, CACHEEX, USER newcamd, BISS, providers, channelinfo. Gerador do channelinfo preserva o cabeÃ§alho.
- **install.sh sem git clone**: copia sÃ³ bin + configs e no fim mostra guia completo (onde estÃ£o os ficheiros, como funciona, primeiros passos).
- Fix: ordem dos INCLUDEs no multics.cfg (F-lines/HTTP/TELNET dependem de ordem) â€” documentado no ficheiro.

## v1.25 (2026-08-31)
- **DCW FILTER (CWPK)**: blacklist de CWs de cartÃµes marcados (35 valores extraÃ­dos por reverse engineering do MultiCS r120); modos `AUTO` (ativa-se sozinho no 1Âº hit), `LOGONLY`, `DROP`; regras `EXACT` (atÃ© 8 por regra), `MASK`, `ALLEQUAL`.
- **ECM FILTER (rule engine)**: `PREFIX`, `LEN` multi, `BYTE <pos> INMASK <mask64>` por perfil, modo DROP/LOGONLY â€” a validaÃ§Ã£o iCAM Ã© config (`BYTE 21 INMASK 1300010012`).
- **FAILBAN** por protocolo (CCCAM/NEWCAMD/MGCAMD/CAMD35/CS378X/CACHE) com BANTIME â†’ ipblock.
- **ANTICASCADE**: anti-reshare por zapping excessivo (MAXZAP/WINDOW/BANTIME).
- **ECMRATELIMIT**: proteÃ§Ã£o do cartÃ£o fÃ­sico (SIDTIME/MAXECM por perfil).
- **DCW CAK7 (CAK7 Merlin)**: transformaÃ§Ã£o iCAM da CW (permutaÃ§Ã£o de bits + checksum dos quads, algoritmo extraÃ­do do r120) para Sky DE 098D/MEO/NOS + perfil [SkyDE-098D] de exemplo.

## v1.24 (2026-08-31)
- **SILENT NOK adiado** (2.5s) em todos os protocolos: falhas respondem antes do timeout da box do cliente â€” elimina reconexÃµes e storms de retries.
- GUI: fix do footer do dashboard com versÃ£o hardcoded; DBG/OFF/ON dos perfis corrigidos; status line HTTP nos divs AJAX (autorefresh e botÃµes a funcionar em todos os browsers).
- 098D: suporte a cartÃµes hop1/hop2 no fluxo; limpeza de avisos (MSG_ECHO 0x02 do reader â†’ debug).

## v1.23 (2026-08-30)
- Login guard anti brute-force (5 falhas/30s por IP) + sessÃµes persistentes + "Terminar todas as sessÃµes" + invalidaÃ§Ã£o ao mudar password.
- ECM Dedup (1 pedido Ãºnico por ECM em voo) com stats na pÃ¡gina Servers.
- NOK cache por reader+canal (~8s) â€” menos trÃ¡fego NOK em zappings.
- ip2country diÃ¡rio (iplocate) com bandeiras; ferramenta update_ip2country.py.
- Pacote dos 3 satÃ©lites (30W/13E/19.2E) em profiles/providers/channelinfo/lite.
- Filtro rigoroso de idents (ACCEPT NULL PROVIDER NO por defeito), nomes de providers corrigidos, parser SRVID com idents a 6 dÃ­gitos.
- GUI: coluna Cards agrupada por perfil, responsividade mobile, fuzz_http.sh, crash-report com core dumps, CI GitHub Actions (build-musl), deploy.ps1.

## v1.20-v1.22
- Base r1000: GUI moderna, Softcam BISS/CW, SKIPCWC, CWC, NAGRA protection, anti-fake XOR 0xF0/nano e0, Health scoring, Fallback cross-protocol, Timing budget, BUILD LITE, upload validado com rollback, editor AJAX, CCcam 3.0.1 (RSA+AES-GCM em C puro), DEDUP, assinaturas de protocolo.

## v1.29 (2026-09-18)
- Rele transparente: CYCLE_CHECK default OFF (circuito multi-hop), half-null Nagra 18xx passa, ACCEPT NULL SID/PROVIDER YES
- CWLR: CW Nagra com checksum invalido = lixo -> nao entregue, espera outra fonte (checksum gate)
- DCW LASTCWONNOK (janela de 2 CWs) + DCW SILENT_NOK - anti-freeze do circuito
- CACHE STATIC + CACHE TIMEOUT 6000 (keep CW por canal)
- NAGRA ONBAD default LOG ONLY (opt-in)
- Feed ECM/CW live nos paineis DBG (servers + clientes), /cwfeed, botoes DBG/ON/OFF corrigidos
- name="..." nos readers, channelinfo 30W com nomes reais, flag ip2country corrigida
- CWPK (DCW FILTER) em standby (estudo CAK7 em curso)
- TUTORIAL.md (plug and play), limpeza da repo (binarios/ficheiros obsoletos fora)

## v1.30 (2026-09-19)
- FIX LASTCWONNOK: a janela de CWs por canal e sempre actualizada (antes so com DCW MINTIME/CYCLE_CHECK activos o LASTCWONNOK nunca tinha CWs para reenviar - o NOK ia sempre ao cliente)
- FIX CACHE STATIC: revertido para NO nos perfis (respondia com a CW velha e descartava a CW fresca do cartao - o canal congelava ate restart)
- DCW STALE_CHECK: hash novo + CW igual as ultimas 2 entregues = stale -> segura 1x por fonte e pede outra (2a vez entrega)
- DCW RETRY: 2 nos perfis de circuito ([MEO]/[NOS]) - cadeia de retries mais curta
- FIX STALE_CHECK no [MEO]: o cartao MEO repete metades legitimamente (15% dos pedidos) - o hold congelava 1 ciclo por repeticao; STALE_CHECK fica OFF no MEO e ON no NOS
- Perfil [SkyDE-098D] ICAM activado (cartao Sky DE no circuito, porta 15052)
- Resultado no circuito: RTP 1 HD NOS (1802:0097) de freezes permanentes para entrega continua
## v1.40 (2026-09-22)
- LIMPEZA: protocolos radegast/camd35/cs378x/freecccam/ccam3 removidos (codigo+GUI+flags+exemplos); Softcam/emu removido; codigo morto removido (PUBLIC 99 blocos, WIN32, MONOTHREAD_ACCEPT, ~80 blocos de macros, setdcw duplicado)
- CYCLE ENGINE (unico): substitui DCW MINTIME/CYCLE_CHECK/CWC/NAGRA CYCLE/STALE_CHECK - aprende cadencia + alternancia CW0/CW1 por canal; STALE hold; anomalias marcadas na fonte (cwbad)
- Anti-fake 2 camadas: validacao estrutural sempre-on + BAD-CW CACHE por reader+CANAL (DCW BADCW TTL - o reader e saltado so no canal mau; quarentena global so se falhar em muitos)
- TIMING absorvido pelo cycle engine (chnbudget removido); CACHE STATIC removido; CWPK (DCW FILTER/LEARN/RULES) removido
- Load-balance: SERVERS: id1,id2 explicito no perfil; hop=1/hopN nos readers (directa preferida); health + badcw integrados
- GUI: paginas dos protocolos removidos e Softcam removidas; httpstyle.c editado directamente (gerador fora da repo)
- Binario -15% (1.34MB -> 1.13MB)

