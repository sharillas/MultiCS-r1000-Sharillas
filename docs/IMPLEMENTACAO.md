# Implementação — o que foi feito nesta build (v1.0.10)

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

### Proteções DCW / Cache
- SKIPCWC (default ON por perfil): ignora CW idêntica à anterior do canal
- Checksum DCW, null DCW, bad DCW, nanoE0 (viaccess), filtros cache CAID 0500
- DCWCHECK2/3, CacheEX (mode 2/3), hitcache, maxhop

### CWC — CW Cycle Check (estilo OSCam, v1.0.10)
- Máquina de estados 0-4 por canal: aprendizagem do cycletime (±2s, lock 3s),
  verificação de ciclos (metade âncora), `sensitive` (countCWpart), replay
  (histórico 15 ECMs), keepcycletime (stage 4)
- Config por perfil: `ENABLE CWC`, `CWC SENSITIVE`, `CWC DROPOLD`,
  `CWC DROPBAD`, `CWC KEEPCYCLETIME` (+ `DEFAULT ...`)
- Validado e2e: bad CW cycle → DROP; old ECM replay → DROP

### Health scoring (v1.0.10)
- Score 0-1000 por reader: sucesso (ecmok/ecmnb), latência média,
  estabilidade (uptime), penalização de erros (timeouts + bad DCWs)
- Ordenação por score no loadbalance + `HEALTH DROPOFF` exclui os doentes
- Config por perfil: `ENABLE HEALTH`, `HEALTH WEIGHTS: suc lat sta err`,
  `HEALTH MINECMS`, `HEALTH DROPOFF` (+ `DEFAULT ...`); score na GUI

### Fallback cross-protocol (v1.0.10)
- `FALLBACK ORDER` define a preferência de protocolos por perfil; só os
  protocolos listados participam; `FALLBACK TIMEOUT` adia os fallbacks
  enquanto existirem servers do protocolo primário
- Validado e2e: morte do reader newcamd → failover automático para CCcam
  (cliente manteve 100% de DCWs)

### Timing budget por canal (v1.0.10)
- Estimador de cryptoperiod (EWMA) alimentado pelas mudanças de CW entregues
- Timeout de decode adaptativo: min(DCW TIMEOUT, cryptoperiod×fraction) —
  o cliente recebe o decode failed dentro do ciclo e pede o próximo ECM a tempo
- `CACHE ADAPTIVETTL`: entradas da cache expiram ao fim do cryptoperiod
- Config por perfil: `ENABLE TIMING`, `TIMING FRACTION`, `TIMING MINPERIOD`
- Validado e2e: decode failed 8008ms → 6104ms após aprender o período

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
16. **Bad DCW de reader newcamd/radegast/camd35/cs378x nunca marcava REPLY_FAIL** — com SERVER MAX ≥ 1 o ECM nunca expirava e o cliente pendurava para sempre (causa provável de vários "não puxa ecms") — cli-*.c
17. **Peer CSP com RTT sub-milissegundo nunca entrava na peerReq** (ipeer_update antes do ping++) — clustredcache.c

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

**v1.0.10** — agosto 2026
