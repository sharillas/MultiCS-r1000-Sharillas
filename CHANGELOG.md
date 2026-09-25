## v1.28 (2026-09-14)
- **Novo tema "Stats Tiles"** (dark + light mode) com **menu lateral** (sidebar fixa): as 14 páginas ficam em coluna à esquerda com badges; dashboard com **tiles de números grandes** (Uptime, Servers, Clients, ECM Totais, RAM, Softcam) e barras de carga.
- **Tabela de Servers**: coluna EcmTime removida; Cards continua na última coluna com **chips coloridos por CAID:PROVID** (ident + nome do provider MEO/NOS/etc).
- **ON/OFF**: interruptores visuais (pills verdes/vermelhas) em todas as páginas de estado.
- **Drop-downs e uploads** restilizados (setas próprias, file-selector button, focus rings).
- **DEBUG**: log com fundo de consola e realce das linhas; botões DBG/INF uniformizados.
- Correção do xmlupdateRow (linhas com nº variável de colunas); cache-bust do CSS v=1128; título/footer v1.28.

# Changelog - MultiCS r1000 by Sharillas
Todas as alterações notáveis desde a v1.20, por versão.

## v1.26 (2026-08-31)
- **Estrutura de ficheiros PT**: `profiles.cfg` (perfis + users newcamd), `servidores.cfg` (N:/C:/L: + cache + cacheex), `clientes_cccam/mgcamd/cs378x/camd35/cache.cfg` (users por protocolo). Compatibilidade total: manter tudo no `multics.cfg` continua a funcionar; o parser é o mesmo (INCLUDEs).
- **CWPK LEARNING**: o filtro DCW FILTER aprende novas regras em runtime — 5 CWs más iguais do mesmo IP → regra EXACT adicionada (cap 16, FIFO, log + evento no dashboard). `DCW FILTER LEARN: YES` por perfil.
- **Eventos recentes no Dashboard**: secção "Proteções & Eventos" com uptime do processo, ECMs totais (OK/NOK), regras CWPK aprendidas e o anel dos últimos eventos (AUTO ativado, FAILBAN, ANTICASCADE, LEARNING).
- **Packages**: coluna "Filtros" com badges por pacote (CAK7, CWPK AUTO/ATIVO/DROP/LOGONLY, LEARN, ECM FILTER).
- **Editor Configs**: CCcam.providers editável; dropdown uniforme com caminho real; multics.css e ip2country.csv fora do editor (geridos à parte).
- **Cabeçalhos para leigos** em todos os ficheiros de config (produção + exemplos): como fazer N/C/L-line, F-line, MG user, CACHE, CACHEEX, USER newcamd, BISS, providers, channelinfo. Gerador do channelinfo preserva o cabeçalho.
- **install.sh sem git clone**: copia só bin + configs e no fim mostra guia completo (onde estão os ficheiros, como funciona, primeiros passos).
- Fix: ordem dos INCLUDEs no multics.cfg (F-lines/HTTP/TELNET dependem de ordem) — documentado no ficheiro.

## v1.25 (2026-08-31)
- **DCW FILTER (CWPK)**: blacklist de CWs de cartões marcados (35 valores extraídos por reverse engineering do MultiCS r120); modos `AUTO` (ativa-se sozinho no 1º hit), `LOGONLY`, `DROP`; regras `EXACT` (até 8 por regra), `MASK`, `ALLEQUAL`.
- **ECM FILTER (rule engine)**: `PREFIX`, `LEN` multi, `BYTE <pos> INMASK <mask64>` por perfil, modo DROP/LOGONLY — a validação iCAM é config (`BYTE 21 INMASK 1300010012`).
- **FAILBAN** por protocolo (CCCAM/NEWCAMD/MGCAMD/CAMD35/CS378X/CACHE) com BANTIME → ipblock.
- **ANTICASCADE**: anti-reshare por zapping excessivo (MAXZAP/WINDOW/BANTIME).
- **ECMRATELIMIT**: proteção do cartão físico (SIDTIME/MAXECM por perfil).
- **DCW CAK7 (CAK7 Merlin)**: transformação iCAM da CW (permutação de bits + checksum dos quads, algoritmo extraído do r120) para Sky DE 098D/MEO/NOS + perfil [SkyDE-098D] de exemplo.

## v1.24 (2026-08-31)
- **SILENT NOK adiado** (2.5s) em todos os protocolos: falhas respondem antes do timeout da box do cliente — elimina reconexões e storms de retries.
- GUI: fix do footer do dashboard com versão hardcoded; DBG/OFF/ON dos perfis corrigidos; status line HTTP nos divs AJAX (autorefresh e botões a funcionar em todos os browsers).
- 098D: suporte a cartões hop1/hop2 no fluxo; limpeza de avisos (MSG_ECHO 0x02 do reader → debug).

## v1.23 (2026-08-30)
- Login guard anti brute-force (5 falhas/30s por IP) + sessões persistentes + "Terminar todas as sessões" + invalidação ao mudar password.
- ECM Dedup (1 pedido único por ECM em voo) com stats na página Servers.
- NOK cache por reader+canal (~8s) — menos tráfego NOK em zappings.
- ip2country diário (iplocate) com bandeiras; ferramenta update_ip2country.py.
- Pacote dos 3 satélites (30W/13E/19.2E) em profiles/providers/channelinfo/lite.
- Filtro rigoroso de idents (ACCEPT NULL PROVIDER NO por defeito), nomes de providers corrigidos, parser SRVID com idents a 6 dígitos.
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


## v1.41 (2026-09-22)
- Feedback do cliente: hash repetido apos entrega com sucesso marca a fonte (cwbad + bad-cw cache) nos 3 protocolos - a box do cliente e o sensor
- Pagina Vigia na GUI (/watchdog): reputacao dos readers (hop/health/cwbad/canais marcados), bad-cw cache activo, cycle engine por canal (cadencia aprendida + anomalias)
- Monitor v140_monitor.py na VPS (cron 30min)


## v1.42 (2026-09-22)
- FIX: MINECMS default 10 - readers sem amostras participam no load-balance ate ganharem historial (o default 0 excluia-os para sempre no filtro de dropoff - ovo-e-galinha; a MGcamd do circuito nunca recebia pedidos apesar de online com 4 cartoes)
- FIX: Vigia "canais marcados" mostra contagem ACTIVA (o contador de badchannels era monotonico e nunca expirava)
- getchname com fallback prov 0 (wildcard): nomes de canais sem ident exacto no channelinfo passam a aparecer na Vigia/last-share; channelinfo de producao fundido com nomes do lamedb da VU (SDT real do satelite, ~27k linhas)


## v1.43 (2026-09-23)
- FIX formato: %d vs uint64_t no httpserver (Last ECM/DCW errados no x64), srv->version sempre-true, declaracao de malloc no ecmdata.c
- FIX cwbad: a coluna da CW Monitoring mostra o valor EFECTIVO com decay (igual ao health: 0 apos 30min, metade apos 10min)
- Graficos 24h na CW Monitoring: anel de 96 amostras (15min) por reader - barras verdes (ok%), cinzento (timeouts), vermelho (cwbad)
- CACHE: ADAPTIVETTL (entradas expiram pela cadencia aprendida do cycle engine) + ALIVETIME 10s; CACHE PEER do circuito em producao
- Renomeacao: pagina Vigia -> CW Monitoring
- Docs: mojibake corrigido no README/CHANGELOG/LEIA-ME (acentos e cedilhas restaurados)


## v1.44 (2026-09-25)
- CALIBRACAO DO CYCLE ENGINE (estudo do circuito):
  - clamp da cadencia aprendida por familia de CAID (18xx 30W: amostras quantizadas para 7.5s/15s - a malha esticava 20-45s e envenenava o EMA)
  - gate do stale-hold: so actua com 3+ stales em 60s (a repeticao legitima de pares do MEO passa limpa; sem freezes periodicos causados pelo motor)
- AUTO-RECONEXAO da fonte com excesso de CWs mas: DCW BADCW RECONNECT (default 15, cooldown 15min, desconecta e reconecta o reader)
- Pagina Cache: cabecalho "Cache Servers (N) - Peers: activos/total" (sem ambiguidade do contador)
- Renomeacao definitiva: pagina Vigia -> CW Monitoring (menu + titulo)
- Cache em producao: ALIVETIME 10s + ADAPTIVETTL + CACHE PEER do circuito




