# Changelog - MultiCS r1000 by Sharillas
Todas as alterações notáveis desde a v1.20, por versão.

## v1.26 (2026-08-31)
- **Estrutura de ficheiros PT**: `profiles.cfg` (perfis + users newcamd), `servidores.cfg` (N:/C:/L: + cache + cacheex), `clientes_cccam/mgcamd/cs378x/camd35/cache.cfg` (users por protocolo). Compatibilidade total: manter tudo no `multics.cfg` continua a funcionar; o parser é o mesmo (INCLUDEs).
- **CWPK LEARNING**: o filtro DCW FILTER aprende novas regras em runtime — 5 CWs más iguais do mesmo IP → regra EXACT adicionada (cap 16, FIFO, log + evento no dashboard). `DCW FILTER LEARN: YES` por perfil.
- **Eventos recentes no Dashboard**: secção "Proteções & Eventos" com uptime do processo, ECMs totais (OK/NOK), regras CWPK aprendidas e o anel dos últimos eventos (AUTO ativado, FAILBAN, ANTICASCADE, LEARNING).
- **Packages**: coluna "Filtros" com badges por pacote (ICAM, CWPK AUTO/ATIVO/DROP/LOGONLY, LEARN, ECM FILTER).
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
- **DCW ICAM**: transformação iCAM da CW (permutação de bits + checksum dos quads, algoritmo extraído do r120) para Sky DE 098D/MEO/NOS + perfil [SkyDE-098D] de exemplo.

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
