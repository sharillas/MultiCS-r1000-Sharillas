# MultiCS r1000 v1.30 - by Sharillas

Cardserver proxy (partilha de cards/CWs) baseado no trabalho do evileyes, reconstruÃ­do e muito expandido: GUI web moderna, Softcam BISS/CW, proteÃ§Ãµes contra CWs falsas (SKIPCWC, CWC, NAGRA protection, anti-fake XOR 0xF0, nano e0), Health scoring, Fallback cross-protocol, Timing budget, BUILD LITE, **DEDUP de ECMs**, login guard anti brute-force, NOK cache, validaÃ§Ã£o de uploads sem crash, **SILENT NOK adiado** (v1.24), e o pacote anti-â€œcartÃµes marcadosâ€ (v1.29): **ECM FILTER rule engine**, **DCW FILTER CWPK** com modo AUTO, **FAILBAN**, **ANTICASCADE**, **ECMRATELIMIT** e **DCW CAK7** (Sky DE 098D / MEO / NOS).

> **VersÃ£o:** v1.30 | **LicenÃ§a:** Sharillas@2026

---
## InstalaÃ§Ã£o (qualquer VPS Linux x86/x64)

Os binÃ¡rios sÃ£o **estÃ¡ticos musl** â€â€ correm em Debian, Ubuntu, CentOS, Rocky, Alma, Arch, Alpineâ€Â¦ sem dependÃªncias.

**Sem git** (download do asset do release):

```bash
# 1. descarrega o pacote da release (exemplo v1.30)
wget https://github.com/sharillas/MultiCS-r1000-Sharillas/releases/download/v1.30/multics-r1000-v1.30.tar.gz
# 2. extrai e instala
tar xzf multics-r1000-v1.30.tar.gz
cd multics-r1000
sudo bash install.sh            # ou: sudo bash install.sh /meu/caminho
```

O instalador:
- copia sÃ³ os **binÃ¡rios** e as **configs de exemplo** (nÃ£o precisa do repo completo)
- cria o serviÃ§o systemd `multics` (inicio automÃ¡tico, restart em falha)
- abre as portas no ufw (se existir)
- no fim mostra um **guia completo** (onde estÃ£o os ficheiros, como funciona, primeiros passos)

Depois abre `http://SEU_IP:5500` Ã¢â€ â€™ login (default `admin`/`admin`) Ã¢â€ â€™ **Dashboard**.

---

## Portas (default)

| ServiÃ§o | Porta |
|---|---|
| Web UI (HTTP) | 5500 |
| Telnet (gestÃ£o) | 5600 |
| CCcam (F-lines) | 16000 |
| **CCcam3 (boxes CCcam 3.0.1)** | 16001 |
| MGcamd | 21000 |
| Newcamd (por perfil) | 15000â€â€œ15025 (definido no `PORT` de cada perfil) |
| camd35 (UDP) | 7502 |
| cs378x (TCP) | 8600 |
| Cache | 5599 |

---

## Ficheiros de configuraÃ§Ã£o (`/var/etc/`)

| Ficheiro | O que faz |
|---|---|
| `multics.cfg` | **Mestre**: portas, credenciais, cache, telnet, HTTP, FAILBAN, ANTICASCADE e INCLUDEs. **Pode conter tudo num sÃ³ ficheiro** (manter tudo no mestre continua a funcionar). |
| `perfis.cfg` | **Perfis de saÃ­da** (portas virtuais): CAID, PROVIDERS, DCW checks, SKIPCWC, CWC, NAGRA, HEALTH, FALLBACK, TIMING, LITE, EMULATOR BISS, ICAM, filtros ECM/DCWâ€¦ + **users newcamd de cada perfil** (`USER:` dentro da secÃ§Ã£o; `USER:` antes do 1Âº perfil = global) |
| `servidores.cfg` | **Readers/servidores** (fonte de cards): linhas `N:` (newcamd), `C:` (cccam), `L:` (radegast), `CACHE:` (peers CSP), `C: ... { cacheex_mode=3 }` |
| `clientes_cccam.cfg` | **F-lines** (clientes CCcam) |
| `clientes_mgcamd.cfg` | Utilizadores MGcamd |
| `clientes_cs378x.cfg` | Utilizadores cs378x (TCP) |
| `clientes_camd35.cfg` | Utilizadores camd35 (UDP) |
| `clientes_cache.cfg` | Utilizadores de cache (CSP) que se ligam a ti |
| `Softcam.cfg` | Chaves CONSTCW (BISS/Tandberg) â€” gerido pelo Softcam |
| `blocked_ips.cfg` | IPs bloqueados (gerido na GUI) |
| `CCcam.channelinfo` | Nomes de canais para a GUI (gerado pelo Update Channel Info) |
| `CCcam.providers` | Nomes de providers para a GUI |
| `CCcam.lite` | BUILD LITE: canais activos (CAID:PROVID:SID) |
| `ip2country.csv` | Base IP â†’ paÃ­s (bandeiras na GUI) |
| `multics.css` | Tema externo (dark/light) |

**Estrutura v1.29 (PT)**: os antigos `profiles.cfg`, `Nlines.cfg`, `users.cfg`, `Mgcamd.cfg`, `Camd35.cfg`, `Cache.cfg`, `CacheEX.cfg`, `1-Clients.cfg` foram reorganizados nos ficheiros acima. Os nomes antigos continuam aceites no editor/upload da GUI (compatibilidade).**Todos os ficheiros tÃªm exemplos comentados completos** em `configs_exemplos/` (cada opÃ§Ã£o explicada em PT). Guia de configuraÃ§Ã£o detalhado: **[docs/CONFIGS.md](docs/CONFIGS.md)**

Fluxo mÃ­nimo para funcionar:

1. Define o teu CAID no perfil (`perfis.cfg`): `[Meu Perfil]` + `PORT: 15000` + `CAID: XXXX`
2. Adiciona readers: `N:` (newcamd) ou `C:` (cccam) â€â€ no multics.cfg ou servidores.cfg
3. Adiciona clientes: `F:` (CCcam), `USER:`/N-lines (newcamd) nos perfis
4. Aplica na hora pela GUI (**Configs Ã¢â€ â€™ Edit** ou **Upload**) â€â€ sem restart
5. VÃª os clientes na GUI: Newcamd / CCcam / Mgcamdâ€Â¦

---

## Features desta build (v1.30)

### Novo na v1.30 â€â€ anti-freeze real do circuito (LASTCW + STALE_CHECK)
- **FIX LASTCWONNOK**: a janela de CWs por canal Ã© agora sempre actualizada â€â€ o reenvio da Ãºltima CW em NOK/timeout funciona mesmo sem `DCW MINTIME`/`CYCLE_CHECK` (antes o NOK ia sempre ao cliente)
- **FIX CACHE STATIC**: revertido para NO nos perfis (respondia com a CW velha e descartava a fresca do cartÃ£o â€â€ congelava atÃ© restart)
- **DCW STALE_CHECK**: hash novo + CW igual Ã s Ãºltimas 2 entregues = stale â€â€ segura 1x por fonte e pede outra (2Âª vez entrega)
- **DCW RETRY: 2** nos perfis de circuito + perfil **[SkyDE-098D] ICAM activado**

### Novo na v1.29 â€â€ pacote anti-"cartÃµes marcados" (anÃ¡lise do MultiCS r120)
- **DCW FILTER (CWPK)**: blacklist de CWs de cartÃµes marcados com 35 valores conhecidos; modos `AUTO` (recomendado: desligado por defeito, **ativa-se sozinho no 1Ã‚Âº hit** e passa a bloquear â€â€ sem ninguÃ©m ver o log), `LOGONLY` (sÃ³ log) e `DROP`; regras `EXACT` (atÃ© 8 CWs por regra), `MASK` (wildcard) e `ALLEQUAL` (fake CW)
- **ECM FILTER (rule engine)**: validaÃ§Ã£o genÃ©rica do ECM por perfil â€â€ `PREFIX` (header whitelist), `LEN` (multi-comprimento), `BYTE <pos> INMASK <mask64>` (ex.: validaÃ§Ã£o iCAM `BYTE 21 INMASK 1300010012`); modo `DROP`/`LOGONLY`
- **FAILBAN**: ban automÃ¡tico de IPs com eventos maus por protocolo (CCCAM/NEWCAMD/MGCAMD/CAMD35/CS378X/CACHE) com `BANTIME`
- **ANTICASCADE**: deteÃ§Ã£o de reshare por zapping excessivo (`MAXZAP`/`WINDOW`/`BANTIME`)
- **ECMRATELIMIT**: proteÃ§Ã£o do cartÃ£o fÃ­sico (`SIDTIME` entre ECMs do mesmo canal, `MAXECM` por segundo)
- **DCW CAK7 (CAK7 Merlin)**: transformaÃ§Ã£o iCAM da CW (permutaÃ§Ã£o de bits + checksum dos quads â€â€ algoritmo extraÃ­do por reverse engineering do r120) para Sky DE 098D / MEO / NOS; perfil `[SkyDE-098D]` de exemplo
- Estado de tudo visÃ­vel na GUI (pÃ¡gina do perfil + DBG)

### Novo na v1.24 â€â€ SILENT NOK
- **SILENT NOK adiado** (2.5s): falhas respondem antes do timeout da box â€â€ elimina reconexÃµes e storms de retries (CCcam/Newcamd/Camd35/cs378x/Mgcamd)

### GUI web
- PÃ¡ginas: Dashboard, Servers, Cache, CacheEX, Newcamd, Mgcamd, CCcam, Cs358x/Camd35, Profiles, **Packages**, Softcam, Configs
- Temas Dark/Light, login com sessÃµes persistentes, tabelas sortÃ¡veis, bandeiras de paÃ­s (base diÃ¡ria iplocate), menu sticky, responsivo em telemÃ³vel (tabelas com scroll)
- **Packages**: dashboard por satÃ©lite (30W/13E/19.2E) â€â€ package, CAID:Ident, perfil ligado, Ecm OK, readers que servem
- **Test Channel**: define um canal de teste pela GUI (ativo na sessÃ£o ou gravado no multics.cfg) e segue-o no Debug Log
- **Servers**: coluna Cards com ident a 6 dÃ­gitos + nomes de ident e pacote (ex: `1814: 005211 MEO ID (ident real transmissao)`), cores de estado na linha, stats de **ECM Dedup**
- **Softcam**: upload SoftCam.Key com feedback, Update SoftCam.Key (download remoto + merge), Reload Keys
- **Configs**: Upload Configs + editor com todos os ficheiros, save por AJAX, caminhos resolvidos da config, botÃ£o **Terminar todas as sessÃµes**
- **Debug**: filtros, 500 entradas, botÃ£o Download Log (txt)

### SeguranÃ§a (GUI)
- **Login guard anti brute-force**: 5 falhas Ã¢â€ â€™ bloqueio 30s por IP (configurÃ¡vel: `HTTP LOGIN MAXFAIL` / `HTTP LOGIN LOCKTIME`), throttle global contra rajadas de IPs, reset no login com sucesso
- Cookie de sessÃ£o `HttpOnly; SameSite=Lax`; sessÃµes persistentes em ficheiro; **todas as sessÃµes sÃ£o invalidadas quando a password muda**
- Log com `vsnprintf` (sem stack overflow com nomes longos), validaÃ§Ã£o de datagramas UDP da cache, fuzz-testing do parser HTTP
- Crash handler gera core dump + serviÃ§o `multics-crashreport` regista o backtrace automaticamente

### ProteÃ§Ãµes e otimizaÃ§Ãµes
- **SKIPCWC** (default ON): ignora CWs exatamente iguais Ã  anterior no mesmo canal (fakes repetidas) â€â€ por perfil, com SIDLIST de exclusÃµes
- **CWC â€â€ CW Cycle Check** (estilo OSCam): aprende o ciclo CW0/CW1 e descarta CWs fora do ciclo, ECMs antigos (replay) e bad cycles â€â€ por perfil
- **NAGRA protection** (18xx/19xx/1a0x): checksum das 4 quads, provider, ciclo com aprendizagem (6 amostras), similaridade, duplicate/conflicting/fake half â€â€ por perfil
- **Anti-fake embutido (nÃ£o configurÃ¡vel)**: CWs com assinatura XOR 0xF0 no Ãºltimo byte e CRC nano e0 (viaccess 0500) sÃ£o descartadas
- **DCW MINTIME / CYCLE_CHECK**: tempo mÃ­nimo entre CWs e alternÃ¢ncia obrigatÃ³ria da metade que muda (NDS)
- **CCcam 3.0.1** (projeto `CCcam-3.0.1-by-Sharillas`): suporte completo nos dois sentidos â€â€
  - **Server**: `CCCAM3 PORT: 16001` â€â€ boxes CCcam3 ligam-se com as mesmas F-lines; handshake encriptado **RSA+AES-GCM** (PBKDF2-SHA256, 10000 iteraÃ§Ãµes)
  - **Reader**: linha `C3: host porta user pass` â€â€ o MultiCS lÃª cards de um server CCcam3
  - Crypto implementada em **C puro** (sem OpenSSL): SHA1/SHA256, HMAC, PBKDF2, RC4, AES-256 (ECB+GCM) â€â€ binÃ¡rio continua estÃ¡tico musl sem dependÃªncias
- **DEDUP de ECMs**: 1 pedido Ãºnico ao reader por ECM em voo (clientes com o mesmo ECM ficam Ã  espera do mesmo CW) â€â€ resolve o flood em canais crÃ­ticos (ex: TVCine) e protege o card de throttling
- **NOK cache por reader+canal** (~8s): um reader que responde NOK num canal Ã© saltado nos zappings seguintes (sÃ³ quando hÃ¡ alternativa) â€â€ menos trÃ¡fego NOK
- **Fallback mais rÃ¡pido**: intervalo adaptativo â€â€ se o reader atual estÃ¡ lento, o prÃ³ximo Ã© tentado mais cedo
- **Health scoring / Fallback cross-protocol / Timing budget**: ordenaÃ§Ã£o por saÃºde, ordem de protocolos, budget de cryptoperÃ­odo
- **BUILD LITE**: filtro de canais activos (CCcam.lite) â€â€ o perfil ignora ECMs fora da lista
- **ENABLE EMULATOR BISS** por perfil
- **DCW TIMEOUT 2500ms** recomendado nos perfis NAGRA (NOK mais rÃ¡pido, cryptoperÃ­odo ~10s)

### Robustez e gestÃ£o
- **Upload validado**: deteta erros de parse por ficheiro/linha, comenta automaticamente as linhas mÃ¡s, aplica defaults, e em Ãºltimo recurso faz rollback do backup (`.bak-<timestamp>`) â€â€ nunca crasha nem fica sem arrancar
- Escrita atÃ³mica (tmp+rename) com mensagens de erro claras (permissÃµes, etc.)
- OpÃ§Ãµes legadas de builds antigas aceites como no-op (compatibilidade)
- ServiÃ§o systemd `User=root` + `chmod 666` nas configs + log em ficheiro com rotaÃ§Ã£o (`-f` Ã¢â€ â€™ `/var/tmp/multics.log`)
- Restart fiÃ¡vel em qualquer setup (systemd/crontab/paths diferentes)
- SIG_HANDLER: registos de crash com RIP/CR2 no log + **core dump** para anÃ¡lise post-mortem
- **ip2country diÃ¡rio**: base do repo iplocate/ip-address-databases via `tools/update_ip2country.py` (cron: `30 5 * * * python3 /opt/multics/update_ip2country.py /var/etc/ip2country.csv`)
- **CI GitHub Actions**: build musl automÃ¡tico em cada push
- **Assinaturas de protocolo** na coluna info server: `Newcamd v6.06`, `Mgcamd v1.46`, `Camd35 v0.3.x`, `Cs738x TCP v0.3.x`, `Cs357x UDP v0.3.x`, `Csp-Cache | Mcs1000`, `Cache EX | Mcs1000` (CCcam mantÃ©m versÃ£o + build + nodeid da config)
- Ferramentas GUI: **Update Channel Info** (KingOfSat Ã¢â€ â€™ CCcam.channelinfo + CCcam.lite) e **Update SoftCam.Key** â€â€ resolvidas junto do binÃ¡rio

Lista completa de fixes e implementaÃ§Ã£o: **[docs/IMPLEMENTACAO.md](docs/IMPLEMENTACAO.md)**

---

## Desenvolvimento (Windows)

```powershell
# compilar (cross-compile Zig 0.15.2, sem Linux)
powershell -ExecutionPolicy Bypass -File build.ps1

# regenerar CSS/JS embutido (httpstyle.c) depois de mudar Configs/multics.css
python tools_generate_httpstyle.py
```

Build flags (equivalentes ao Makefile original):
`-DCHECK_NEXTDCW -DSID_FILTER -DNEWCACHE -DCCCAM_CLI -DRADEGAST_CLI -DCAMD35_CLI -DCS378X_CLI -DHTTP_SRV -DTELNET -DMGCAMD_SRV -DCCCAM_SRV -DCAMD35_SRV -DCS378X_SRV -DSRV_CSCACHE -DEXPIREDATE -DDCWSWAP -DCACHEEX -DIPLIST -DTESTCHANNEL -DTHREAD_DCW -DEPOLL_NEWCAMD -DEPOLL_CCCAM -DEPOLL_MGCAMD -DEPOLL_ECM -DPEERLIST -DECMLIST -DEPOLL_FREECCCAM -DSIG_HANDLER -DCLI_CSCACHE -std=gnu90 -O3 -fpack-struct`

---

## SeguranÃ§a

- **Muda jÃ¡** o `HTTP USER/PASS` e `TELNET USER/PASS` no `multics.cfg`
- Login guard ativo por defeito (5 falhas/30s por IP) â€â€ ajustÃ¡vel em `HTTP LOGIN MAXFAIL` / `HTTP LOGIN LOCKTIME`
- ExpÃµe sÃ³ as portas que precisas (a web UI 5500 Ã© a mais sensÃ­vel)
- Credenciais de deploy vivem **fora do repo** (`deploy.secrets.ps1`, gitignored)
- BinÃ¡rios estÃ¡ticos: sem dependÃªncias da libc da VPS

---

## CrÃ©ditos

Base: (evileyes). Mod, GUI e fixes: **Sharillas@2026** â€â€ v1.24
