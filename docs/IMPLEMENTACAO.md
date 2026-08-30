# Implementação — o que foi feito nesta build (v1.0.10.2)

Base: fork multi-cs/multics (evileyes). Compilação: Zig 0.15.2 cross-compile (Windows → Linux), binários estáticos musl x64/x32.

## Funcionalidades implementadas

### GUI / Web UI
- **Dashboard** (`/dashboard`) com: System (uptime, RAM, cached, CPU do processo), Servers, Clients, Current ECM Request, Debug Log (poll 2s + filtro por categoria), Cache Stats, Decrypted CW Log
- **Login com sessões** (`/login`, cookie `multics_session`, logout, redirect para `/dashboard`)
- **Temas Dark/Light** (default Light, guardado em localStorage, botão no header)
- **Tabelas sortáveis** no cliente (clicar no cabeçalho ordena)
- **Linhas de debug por cliente/server/perfil** (botão DBG → linha inline com stats; abre/fecha)
- **Edit Config / Edit Profiles** com save imediato (reread da config sem restart); Edit Profiles só mostra profiles.cfg
- **Páginas de servidores/clients com info em tabela** (template skin) acima da div principal
- **Icons modernos** (ON/OFF/DBG em pills CSS em vez dos PNGs antigos)
- Página Restart com template completo e tema inline (sobrevive ao restart)

### Emulator
- Upload SoftCam.Key (multipart) → parse BISS (`F`/`f`) + Tandberg (`T`) → grava `/var/etc/Softcam.cfg`
- Página Emulator: chaves, adicionar/apagar, SoftCam.cfg status, Decrypted CW Log
- `biss_updater.py` (cron) para atualização automática
- **Update SoftCam.Key** (botão GUI, `/emulator?action=updatekey`): corre `tools_update_softcam.py` (download remoto + parse, chaves manuais preservadas)
- **Reload Keys** (`/emulator?action=applykeys`): relê o Softcam.cfg do disco
- **ENABLE EMULATOR BISS** por perfil (default ON): liga/desliga o emulador em cada perfil

### Proteções DCW / Cache
- SKIPCWC (default ON por perfil): ignora CW idêntica à anterior do canal
- **CWC — CW Cycle Check** (estilo OSCam module-cw-cycle-check): aprendizagem do ciclo CW0/CW1 por canal (3 estágios), rejeita bad CW cycle, ECM antigo (replay, DROPOLD) e same CW fora da janela; DROPBAD (drop em bad cw), KEEPCYCLETIME (mantém o cycletime aprendido N minutos)
- **NAGRA protection** (caid 18xx–1a12): `NAGRA CHK` (checksum das 4 quads), `NAGRA PROV` (provider do perfil), `NAGRA CYCLE` (aprendizagem de 6 amostras → full-cw/half-cycle + similaridade SENSITIVE), rejeita bad checksum (code 2), bad provider (code 3), fake half (code 4), duplicate (code 5) e too similar (code 6); `NAGRA ONBAD` = drop ou só log
- **DCW MINTIME** (ms): rejeita mudanças de CW demasiado rápidas por canal
- **DCW CYCLE_CHECK**: a metade que muda tem de alternar (NDS)
- **DCW SKIPCWC_EXCLUDE SIDS**: sids onde o SKIPCWC não se aplica
- Checksum DCW, null DCW, bad DCW, nanoE0 (viaccess), filtros cache CAID 0500
- DCWCHECK2/3, CacheEX (mode 2/3), hitcache, maxhop

### Health scoring (por perfil)
- `ENABLE HEALTH`: pontua cada server (sucesso, latência, estabilidade, erros) com pesos configuráveis (`HEALTH WEIGHTS`)
- `HEALTH MINECMS`: amostras mínimas antes de pontuar
- `HEALTH DROPOFF`: exclui servers abaixo do score (não participam nos pedidos)

### Fallback cross-protocol (por perfil)
- `FALLBACK ORDER`: ordem de preferência de protocolos (NEWCAMD/CCCAM/CS378X/CAMD35/RADEGAST)
- `FALLBACK TIMEOUT`: adia protocolos de fallback enquanto houver servers do preferido
- Fix crítico: bad DCW em cli-newcamd/radegast/camd35/cs378x agora marca `ECM_SRV_REPLY_FAIL` (antes os ECMs ficavam pendurados com SERVER MAX≥1)

### Timing budget (por perfil)
- `ENABLE TIMING`: estima o cryptoperiod (EWMA por canal) e define o budget de decode
- `TIMING FRACTION` (% do período) e `TIMING MINPERIOD` (só aplica acima deste)
- `CACHE ADAPTIVETTL`: entradas da cache expiram ao fim do cryptoperiod estimado

### BUILD LITE
- `LITE FILE:` (default `/var/etc/CCcam.lite`): lista de canais activos `caid:provid:sid` (wildcards `FFFE`/provid 0)
- `ENABLE LITE` por perfil: ignora ECMs fora da lista (`Ignored (lite)`) — sem lista carregada o filtro deixa passar tudo (seguro por omissão)

### Ferramentas GUI
- **Update Channel Info** (`/editor?action=updatechinfo`): corre `tools_update_channelinfo.py --apply` (KingOfSat → CCcam.channelinfo + reload)
- **Load Channel Info** (`/editor?action=reloadchinfo`): relê o CCcam.channelinfo do disco sem restart
- Botões com verificação de existência da ferramenta (`Ferramenta nao encontrada`)

### Robustez
- **Restart fiável**: exec direto (mesmo PID — compatível com systemd/crontab), caminho via `/proc/self/exe` + fallback, fds fechados antes do exec, delay 2s para a resposta HTTP sair
- **Parsing de configs** robusto: paths com/sem aspas (`parse_path`), `CONSTCW FILE:` e `BLOCKEDIP FILE:` como top-level, `version=` em F-lines, valores com aspas nas opções, erros de parse com nome do ficheiro
- HEAD suportado, Cache-Control em todas as respostas (sem caches erradas de 302)
- Configs de exemplo sem erros de parsing

## Bugs corrigidos (desde a base)

1. **Segfault no arranque** sem profiles (`get_cache_caids` deref NULL) — config.c
2. **Login da Web UI** — o POST era lido com o header HTTP no buffer (`parse_http_request`) — httpserver.c
3. **`-C` espera o ficheiro** de config (não o diretório) — main.c/documentação
4. **Overflow signed em `hashCode`** com usernames longos (UB em -O3) — ecmdata.c
5. **`cc_version[]` sem NULL terminator** — loop lia fora do array — config.c
6. **Socket não fechado na página /login** — browser "a pensar" — httpserver.c
7. **AJAX `action=div` sem headers HTTP** — autorefresh não funcionava — httpserver.c
8. **`http_send_text` sem headers** — botões ON/OFF sem resposta — httpserver.c
9. **Emulator não gravava** — `CONSTCW FILE:` não era parseado (só `FILE CONSTCW:`) — config.c
10. **Editor não gravava** — parser multipart procurava a boundary errada (depois da correção do POST) — httpserver.c
11. **Restart matava o processo** em systemd (fork+exec mudava o PID) — main.c
12. **Google Fonts bloqueavam o render** (removidos) e **fade-in removido** (páginas "em branco")
13. **Caches de 302** (browser preso no /login) — Cache-Control no-store em tudo
14. **Badge Newcamd/Cs378x** contava listas erradas — httpserver.c
15. **i18n/parsing dos configs de exemplo** — ordem de INCLUDEs corrigida, sintaxe de users camd35/cs378x/cacheex corrigida

## Estrutura

```
src/                 código C (main.c, httpserver.c, config.c, ...)
build/               binários compilados (x64/x32 estáticos musl)
configs_exemplos/    configs prontos (multics.cfg + perfis + users + readers)
install.sh           instalador universal (systemd/init.d/crontab)
build.ps1            build Windows (Zig)
package.ps1          gera pacote dist/
deploy.ps1           deploy scp+ssh
tools_generate_httpstyle.py   gera src/httpstyle.c (CSS/JS embutidos)
```

## Toolchain

- Zig 0.15.2 portátil (C:\TMP\opencode\zig-x86_64-windows-0.15.2)
- `-std=gnu90 -O3 -fpack-struct` + flags do Makefile original
- Python 3 para gerar httpstyle.c

## Versão

**v1.21** — agosto 2026

- v1.0.10: GUI, emulator, SKIPCWC, restart fiável, parsing robusto
- v1.0.10.2: CWC, NAGRA, Health, Fallback, Timing, DCW MINTIME/CYCLE_CHECK, BUILD LITE, ENABLE EMULATOR BISS, ferramentas GUI, fix REPLY_FAIL (não puxa ecms), filtro embutido fake CW (XOR 0xF0) + CRC nano e0
- v1.0.10.3 (port r82): SIG_HANDLER com registos (RIP/CR2) no log de crash, CLI_CSCACHE/SRV_CSCACHE ativos (CWs de/para clientes e readers newcamd — negociação CH/OK via keepalive), lite/channelinfo expandidos a todos os satélites (30W/13E/19.2E/23.5E/28.2E/0.8W/4.8E/5W/9E/16E/39E/42E/...), perfis por CAID com filtros por tipo de encryptação (NAGRA 18xx, Viaccess 0500, NDS 09xx, SECA/Conax/CryptoWorks/Irdeto, BISS 2600), página Configs com **Upload Configs** (whitelist de ficheiros de /var/etc, backup automático `.bak-<timestamp>`, reload após upload; ficheiro único com todas as secções carrega-se como multics.cfg)
- v1.10.9 (final stage): editor com **todos** os ficheiros da whitelist (edição direta de /var/etc), ficheiros de exemplo com documentação completa (novos CCcam.lite e ip2country.csv), Blocked since em **DD:HH:MM:SS**, **validação de upload** (deteta erros de parse por ficheiro/linha, comenta automaticamente as linhas más, aplica defaults, rollback `.bak` em último recurso — nunca crasha), opções legadas aceites como no-op, top menu sticky, botões tipo menu com info por baixo, cores de estado offline/online/busy fixas nas tabelas, **fix pipeline BISS NOK** (não mata o fluxo quando há readers/cache), **DCW TIMEOUT 2500ms nos perfis NAGRA** (NOK mais rápido, cryptoperíodo ~10s)
- v1.20 (release estável): caminhos resolvidos a partir da config (upload/editor/tools funcionam em qualquer layout — /var/etc, /emu/multics...), **save do editor via AJAX** (sem navegação/redirect, feedback Guardado/ERRO inline), escrita atómica (tmp+rename) sem perder o ficheiro anterior, mensagens de erro claras para permissões, serviço systemd com **User=root** + chmod 666 nas configs (GUI salva/uploads/update/restart mesmo com ficheiros em 755), ferramentas de update junto do binário, cache-busting no CSS/JS, log com 500 entradas + Download Log na página Debug
- v1.20 (final): **DEDUP de ECMs** — 1 pedido único ao reader por ECM em voo (followers partilham o CW do leader; falha em cadeia; timeout de segurança; tabela por canal na GUI — fix do flood em canais críticos tipo TVCine), **fallback adaptativo** (intervalo a metade quando o reader está lento, min 200ms), **nomes de pacote por CAID na coluna Cards** (NOS ID 30W Portugal, Digi TV 0.8W Hungria...), **assinaturas de protocolo** na info server (Newcamd v6.06, Mgcamd v1.46, Camd35 v0.3.x, Cs738x TCP/Cs357x UDP v0.3.x, Csp-Cache | Mcs1000, Cache EX | Mcs1000; CCcam mantém versão+build+nodeid da config), Softcam com nomes sem `;` colado + upload AJAX com feedback, botões AJAX que não desaparecem após clique (imgrequest sem onclick null, opacidade volta a 1.0), btnrequest com feedback inline (Update SoftCam.Key / Reload Keys), tools_update_softcam.py reescrita (POST multipart ao /emulator com --port), emu com fallback `Softcam.cfg` junto do multics.cfg quando CONSTCW FILE não está definido
- v1.20 (CCcam 3.0.1): suporte ao protocolo **CCcam 3** (projeto CCcam-3.0.1-by-Sharillas) nos dois sentidos — **server** `CCCAM3 PORT` (boxes CCcam3 autenticam com F-lines, handshake RSA_AES: PBKDF2-HMAC-SHA256 10000 + AES-256-GCM; legacy SHA1 suportado) e **reader** `C3:` (login v301 → RSA_AES, ECM/CW com framing próprio); cripto implementada em C puro em `src/ccam3_crypto.c` (SHA1/SHA256/HMAC/PBKDF2/RC4/AES-256 ECB+GCM, validada contra vetores FIPS-197/NIST/OpenSSL) — sem OpenSSL, binário continua musl estático; testes e2e `e2e_ccam3.sh` (reader+fake server, fake client+server, RSA_AES nos dois sentidos)
- v1.21 (fixes produção + CAIDs PT/13E/19.2E): **fix crítico dcwtimeout()** — TIMING ativo sem MINPERIOD/FRACTION devolvia timeout 0 e TODOS os ECMs falhavam em ~10ms (guard no código: nunca devolve 0, budget só com período observado; config de produção agora define `DEFAULT TIMING MINPERIOD/FRACTION`); **fix recvtime no RETRY** (CCcam/Newcamd/Mgcamd/Camd35/Cs378x — re-pedidos do mesmo canal falhavam instantaneamente com o timeout antigo); **fix SIGFPE (divisão por zero)** em `srv_healthscore` com ecmnb=0 (crash-loop em produção a cada ~15s com o browser na GUI) e em 2 células da GUI (coluna Cards e secção ECM Dedup com contadores a zero); **fix spin 100% CPU no /dashboard** (walks da lista circular `ecmdata` sem guard — cada autorefresh criava uma thread infinita); **nova opção `{ nocheck=yes }` nos readers** (topologia cliente+readers no mesmo IP externo, ex: 130.255.78.45 — a proteção anti-circular já não bloqueia a partilha); **identificações CAID oficiais** aplicadas em profiles/CCcam.providers/CCcam.channelinfo/cards GUI: MEO 1814 (005211/005221/000007), NOS 1802 (004801/004901), Canal+ Polónia 1813:000068, Tivùsat 183D/183E/1856, Polsat Box 1803, packages 19.2E (Movistar+, Canal+ France, TNT SAT 1818, Sky DE 098C/098D, HD+ 1830-188A, Canal Digitaal, TV Vlaanderen, Austriasat) + listas completas de canais; **parser de CCcam.providers aceita o formato SRVID `CAID:PROVID|NOME`** (antes só 8hex "nome" — idents nunca apareciam na GUI); **coluna Cards/Providers com ident a 6 dígitos + nomes de ident** (ex: `1814: 005211 MEO ID (ident real transmissao)`); **perfis internos DEFAULT/BISS Emu escondidos** da lista de profiles da GUI; **profiles reescritos só com os packages ativos 30W/13E/19.2E** (eliminados Sky UK/IT, Viaccess/SECA genéricos, Conax, CryptoWorks, Irdeto, BetaDigital, Bulsatcom, Turksat...); Softcam.cfg com a lista dos 3 satélites (BISS Abertis/EBU/feeds; chaves antigas do updater preservadas comentadas); cards da GUI com **ident prioritário sem duplicação** de nomes (1884/0500:050800/186A:003411 corrigidos)
