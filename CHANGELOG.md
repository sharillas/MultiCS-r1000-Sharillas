## v1.62 (2026-10-16)
- **Dashboard**: tiles de Uptime moderno, saude por perfil (MEO/NOS com cor do OK%), Referee (ask/match/disagree sem host); widgets Fair-Share (validacoes do arbitro por cliente no ultimo minuto) e Protecções (CW Infected, Rejected, Disagree/Discard by Referee, Verdict OK/BLACK); contadores globais de arbitro/filtros/verdict.
- **Coluna Truth na pagina Servers**: % de CWs validas por fonte (ecmok vs ecmok+cwbad), contadores RAW/WRAP (veneno de periodo), nodeid e Resp. media - a agulha do veneno no palheiro.
- **VERDICT end-to-end**: anel de CWs servidas + endpoint /verdict (caid/prov/sid/cw/ok) - a box reporta o resultado real (abriu/preto) e a fonte que serviu a CW ma e marcada (dcw_badmark + log multics-verdict.log). Agente externo na box (grab luminancia + .cwl).
- Perfil [DEFAULT] deixa de contar como perfil; Packages Info com Caid|Ident Provider|Actions e Options/Filters em pilha; fix parser de query (9 params -> MAXHEADERS-1).

## v1.61 (2026-10-14)
- **Editor popup /editline** (form com protocolo/host/porta/user/pass/deskey/options; SAVE reconstrói a linha em Servers.cfg/Clients.cfg). Botões "Edit" nas paginas de servers e clients.
- **Fair-share do arbitro**: DCW ARBITER MAX por perfil (teto de validacoes por cliente na janela) - evita que um cliente monopole o arbitro.
- Deadlines 5500/5500 + retry de validacao quando o arbitro esta ocupado (fix da contenção multi-cliente).


- **Editor linha-a-linha** (pagina Editor): para `Servers.cfg` e `Clients.cfg` - cada linha ativa aparece como linha editavel com botoes ON/OFF (toggle do comentario - respeito total pelo mecanismo disabled da v1.54), SAVE (edicao inline via base64), DEL e ADD. Tudo com reload de config imediato (sem restart).
- Versao GUI/relatorio alinhada (VERSION_STR 1.60).

## v1.55.5 (2026-10-11)
- **ARBITER PRE-ASK**: o reader arbiter e perguntado EM PARALELO com a rotacao no inicio do ECM. Como e rapido (~300ms) e e a fonte da verdade, a entrega ao cliente cai de ~3s para ~300ms - a latencia alta era o gatilho do wedge do descrambler 1802 na box (freezes de 1-frame).
- **SILENT NOK desligavel por perfil** (recomendado NO no 1802): o NOK adiado fazia a box repetir o pedido e o cache responder com CW velha - dessincronizava o descrambler.
- Estudo completo documentado: veneno de periodo (CWs checksum-validas de outros criptoperiodos), filtros por comparacao #SUM, protocolo ponta-a-ponta.

## v1.55.4
- **ENABLE CACHE: NO no perfil 1802**: o cache servia CWs de periodos anteriores em re-requests do cliente (resposta 0ms) - causa raiz de freezes residuais.
- DCW TIMEOUT mais generoso no NOS (2500) para o ciclo pergunta+validacao.

## v1.55.3
- Arbitro sem resposta em 2.5s: o candidato NAO validado e DESCARTADO (marca a fonte) em vez de entregue - nunca entregar CWs nao confirmadas (um par errado dessincroniza a box).

## v1.55.2
- **STUCK RECOVERY**: busy preso de cliente (ECM orfao) > 10s -> reset automatico e aceite do pedido novo (auto-recuperacao; fim dos blackouts prolongados).
- Fix checktime=0 ("nunca re-verificar") no ramo de timeout do arbitro - o ECM ficava orfao para sempre e o busy do cliente nunca limpava.
- Stash re-entrado salta os filtros (falso positivo da meia carregada contra a propria cadeia).

## v1.55.1
- **DCW AGREE**: consenso - a mesma CW tem de vir de N fontes (`DCW AGREE: 2`); com arbitro ativo o arbitro e o consenso.
- **SERVER ASK**: fan-out - perguntar a N servers em paralelo por ECM (`SERVER ASK: 3`).

## v1.55
- **DCW HALFROTATE / HALFREPEAT TTL**: filtro de rotacao CW0/CW1 por canal+FONTE - rejeita pares sem meia em comum com o anterior e meias repetidas (o fingerprint das CWs envenenadas de periodo).
- **DCW ARBITER**: reader de confianca (`arbiter=yes` na linha, fora da rotacao normal) valida a CW de cada fonte; `DCW ARBITER SERVE: YES` entrega a CW do arbitro em discordancia; `DCW ARBITER RATE` limita a % de ECMs validados (protecao do cartao do arbitro). Degradacao graciosa quando o arbitro desaparece.
- Fonte do problema NOS 1802 identificada e neutralizada: cards marcados do saco devolvem CWs de criptoperiodos errados (checksum valido); o arbitro (fonte limpa) corrige.

## v1.54
- **Fix OFF/ON da GUI**: linhas comentadas (`#N:`/`#C:`/...) sao lidas como readers DISABLED (FLAG_DISABLE) - o OFF ja nao faz a linha desaparecer e o ON volta sempre. Exemplos de documentacao (`#    N: ...`) continuam ignorados.

## v1.53
- **Fix passwords com '#"**: o '#' so e comentario no inicio da linha ou apos espaco/tab - linhas C: com passwords a conter '#' (ex: `FiL#nW0$A9u0B`) passam a ser lidas corretamente.

## v1.52
- Fix badident (provider do ECM mal identificado), re-poll curto (300ms) quando as fontes estao ocupadas, fix da race timeout-vs-rotacao (o 2o/3o server nunca era perguntado), CAK7 INVERSE.

## v1.51
- Seccoes find-or-create (re-declarar [perfil] noutro ficheiro reutiliza o existente), grupo de apresentacao na GUI por reader, values de budget separados.

## v1.50
- **BUDGET ASK**: teto de pedidos/s por reader (anti rate-limit upstream por pedido, nao so por CW).
## v1.48 (2026-10-03)
- **DEADCHAN**: canais mortos (sem resposta ha N minutos) deixam de martelar os readers - NOK rapido ao cliente + 1 probe de recuperacao a cada RETRY segundos. Config: `DCW DEADCHAN: YES`, `DCW DEADCHAN MINTIME: 5`, `DCW DEADCHAN RETRY: 60`. Lista na pagina CW Monitoring.
- **PACING**: intervalo minimo entre pedidos do MESMO canal ao MESMO reader (`DCW PACING: 2000`) - anti rate-limit upstream (1 pedido por janela em vez do storm de retries).
- **VERDICT**: endpoint HTTP `/verdict` + anel das ultimas 128 CWs servidas - a box reporta se a CW abriu (OK) ou ficou preta (BLACK); BLACK marca a fonte exacta (dcw_badmark) e regista em `/var/log/multics-verdict.log`. Agente de exemplo para boxes Enigma2: le /tmp/ecm.info + estado do decoder vmpeg1 e reporta de 8 em 8s.
- **RAWLOG**: `DCW RAWLOG: YES` captura a CW crua PRE-transformacao em `/var/log/multics-raw.log` com classificacao RAW/WRAP pelos checksums de quad + detector de periodos de graca 1802 (eventos GRACE ON/OFF no dashboard).
- **Oráculo CWPK (telnet)**: comando `CWPKTEST <32hex>` testa uma chave candidata (3DES-EDE, 4 variantes de layout) contra as CWs 1802 WRAP capturadas - validacao instantanea quando aparecer um candidato (dump/leak/EMM).
- **CAK7 INVERSE**: `DCW CAK7 INVERSE: YES` usa a permutacao inversa (para fontes que entregam a CW ja icam-scrambled nos premium).
- **Divergencia entre readers**: contador por reader de CWs diferentes da aceite (coluna Div na CW Monitoring) - deteta readers a produzir CW errada.
- **Nanos por canal**: seccao na CW Monitoring com os nanos (ECM[2]) vistos por canal - deteta streams especiais (ex.: sport com nano 67 vs 66 nos basicos).
- **PROVID REWRITE**: opcao de reader `providrewrite=YES` reescreve o provid para 0 nos pedidos (rota do cartao real em servers que encaminham o ident para cacheex).
- Fix: probe de recuperacao do DEADCHAN (antes o canal marcado nunca voltava); TESTCHANNEL ja existia na GUI (pagina /testchannel).

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
