# MultiCS r1000 v1.28 - by Sharillas

Cardserver proxy (partilha de cards/CWs) baseado no trabalho do evileyes, reconstruído e muito expandido: GUI web moderna, Softcam BISS/CW, proteções contra CWs falsas (SKIPCWC, CWC, NAGRA protection, anti-fake XOR 0xF0, nano e0), Health scoring, Fallback cross-protocol, Timing budget, BUILD LITE, **DEDUP de ECMs**, login guard anti brute-force, NOK cache, validação de uploads sem crash, **SILENT NOK adiado** (v1.24), e o pacote anti-“cartões marcados” (v1.28): **ECM FILTER rule engine**, **DCW FILTER CWPK** com modo AUTO, **FAILBAN**, **ANTICASCADE**, **ECMRATELIMIT** e **DCW CAK7** (Sky DE 098D / MEO / NOS).

> **Versão:** v1.28 | **Licença:** Sharillas@2026

---

## Dashboard

| Light mode | Dark mode |
|---|---|
| ![Dashboard Light](screenshot/dashboard-light.png) | ![Dashboard Dark](screenshot/dashboard-dark.png) |

---

## Instalação (qualquer VPS Linux x86/x64)

Os binários são **estáticos musl** ”” correm em Debian, Ubuntu, CentOS, Rocky, Alma, Arch, Alpine”¦ sem dependências.

**Sem git** (download do asset do release):

```bash
# 1. descarrega o pacote da release (exemplo v1.28)
wget https://github.com/sharillas/MultiCS-r1000-Sharillas/releases/download/v1.28/multics-r1000-v1.28.tar.gz
# 2. extrai e instala
tar xzf multics-r1000-v1.28.tar.gz
cd multics-r1000
sudo bash install.sh            # ou: sudo bash install.sh /meu/caminho
```

O instalador:
- copia só os **binários** e as **configs de exemplo** (não precisa do repo completo)
- cria o serviço systemd `multics` (inicio automático, restart em falha)
- abre as portas no ufw (se existir)
- no fim mostra um **guia completo** (onde estão os ficheiros, como funciona, primeiros passos)

Depois abre `http://SEU_IP:5500` â†’ login (default `admin`/`admin`) â†’ **Dashboard**.

---

## Portas (default)

| Serviço | Porta |
|---|---|
| Web UI (HTTP) | 5500 |
| Telnet (gestão) | 5600 |
| CCcam (F-lines) | 16000 |
| **CCcam3 (boxes CCcam 3.0.1)** | 16001 |
| MGcamd | 21000 |
| Newcamd (por perfil) | 15000”“15025 (definido no `PORT` de cada perfil) |
| camd35 (UDP) | 7502 |
| cs378x (TCP) | 8600 |
| Cache | 5599 |

---

## Ficheiros de configuração (`/var/etc/`)

| Ficheiro | O que faz |
|---|---|
| `multics.cfg` | **Mestre**: portas, credenciais, cache, telnet, HTTP, FAILBAN, ANTICASCADE e INCLUDEs. **Pode conter tudo num só ficheiro** (manter tudo no mestre continua a funcionar). |
| `perfis.cfg` | **Perfis de saída** (portas virtuais): CAID, PROVIDERS, DCW checks, SKIPCWC, CWC, NAGRA, HEALTH, FALLBACK, TIMING, LITE, EMULATOR BISS, ICAM, filtros ECM/DCW… + **users newcamd de cada perfil** (`USER:` dentro da secção; `USER:` antes do 1º perfil = global) |
| `servidores.cfg` | **Readers/servidores** (fonte de cards): linhas `N:` (newcamd), `C:` (cccam), `L:` (radegast), `CACHE:` (peers CSP), `C: ... { cacheex_mode=3 }` |
| `clientes_cccam.cfg` | **F-lines** (clientes CCcam) |
| `clientes_mgcamd.cfg` | Utilizadores MGcamd |
| `clientes_cs378x.cfg` | Utilizadores cs378x (TCP) |
| `clientes_camd35.cfg` | Utilizadores camd35 (UDP) |
| `clientes_cache.cfg` | Utilizadores de cache (CSP) que se ligam a ti |
| `Softcam.cfg` | Chaves CONSTCW (BISS/Tandberg) — gerido pelo Softcam |
| `blocked_ips.cfg` | IPs bloqueados (gerido na GUI) |
| `CCcam.channelinfo` | Nomes de canais para a GUI (gerado pelo Update Channel Info) |
| `CCcam.providers` | Nomes de providers para a GUI |
| `CCcam.lite` | BUILD LITE: canais activos (CAID:PROVID:SID) |
| `ip2country.csv` | Base IP → país (bandeiras na GUI) |
| `multics.css` | Tema externo (dark/light) |

**Estrutura v1.28 (PT)**: os antigos `profiles.cfg`, `Nlines.cfg`, `users.cfg`, `Mgcamd.cfg`, `Camd35.cfg`, `Cache.cfg`, `CacheEX.cfg`, `1-Clients.cfg` foram reorganizados nos ficheiros acima. Os nomes antigos continuam aceites no editor/upload da GUI (compatibilidade).**Todos os ficheiros têm exemplos comentados completos** em `configs_exemplos/` (cada opção explicada em PT). Guia de configuração detalhado: **[docs/CONFIGS.md](docs/CONFIGS.md)**

Fluxo mínimo para funcionar:

1. Define o teu CAID no perfil (`perfis.cfg`): `[Meu Perfil]` + `PORT: 15000` + `CAID: XXXX`
2. Adiciona readers: `N:` (newcamd) ou `C:` (cccam) ”” no multics.cfg ou servidores.cfg
3. Adiciona clientes: `F:` (CCcam), `USER:`/N-lines (newcamd) nos perfis
4. Aplica na hora pela GUI (**Configs â†’ Edit** ou **Upload**) ”” sem restart
5. Vê os clientes na GUI: Newcamd / CCcam / Mgcamd”¦

---

## Features desta build (v1.28)

### Novo na v1.28 ”” pacote anti-"cartões marcados" (análise do MultiCS r120)
- **DCW FILTER (CWPK)**: blacklist de CWs de cartões marcados com 35 valores conhecidos; modos `AUTO` (recomendado: desligado por defeito, **ativa-se sozinho no 1Âº hit** e passa a bloquear ”” sem ninguém ver o log), `LOGONLY` (só log) e `DROP`; regras `EXACT` (até 8 CWs por regra), `MASK` (wildcard) e `ALLEQUAL` (fake CW)
- **ECM FILTER (rule engine)**: validação genérica do ECM por perfil ”” `PREFIX` (header whitelist), `LEN` (multi-comprimento), `BYTE <pos> INMASK <mask64>` (ex.: validação iCAM `BYTE 21 INMASK 1300010012`); modo `DROP`/`LOGONLY`
- **FAILBAN**: ban automático de IPs com eventos maus por protocolo (CCCAM/NEWCAMD/MGCAMD/CAMD35/CS378X/CACHE) com `BANTIME`
- **ANTICASCADE**: deteção de reshare por zapping excessivo (`MAXZAP`/`WINDOW`/`BANTIME`)
- **ECMRATELIMIT**: proteção do cartão físico (`SIDTIME` entre ECMs do mesmo canal, `MAXECM` por segundo)
- **DCW CAK7 (CAK7 Merlin)**: transformação iCAM da CW (permutação de bits + checksum dos quads ”” algoritmo extraído por reverse engineering do r120) para Sky DE 098D / MEO / NOS; perfil `[SkyDE-098D]` de exemplo
- Estado de tudo visível na GUI (página do perfil + DBG)

### Novo na v1.24 ”” SILENT NOK
- **SILENT NOK adiado** (2.5s): falhas respondem antes do timeout da box ”” elimina reconexões e storms de retries (CCcam/Newcamd/Camd35/cs378x/Mgcamd)

### GUI web
- Páginas: Dashboard, Servers, Cache, CacheEX, Newcamd, Mgcamd, CCcam, Cs358x/Camd35, Profiles, **Packages**, Softcam, Configs
- Temas Dark/Light, login com sessões persistentes, tabelas sortáveis, bandeiras de país (base diária iplocate), menu sticky, responsivo em telemóvel (tabelas com scroll)
- **Packages**: dashboard por satélite (30W/13E/19.2E) ”” package, CAID:Ident, perfil ligado, Ecm OK, readers que servem
- **Test Channel**: define um canal de teste pela GUI (ativo na sessão ou gravado no multics.cfg) e segue-o no Debug Log
- **Servers**: coluna Cards com ident a 6 dígitos + nomes de ident e pacote (ex: `1814: 005211 MEO ID (ident real transmissao)`), cores de estado na linha, stats de **ECM Dedup**
- **Softcam**: upload SoftCam.Key com feedback, Update SoftCam.Key (download remoto + merge), Reload Keys
- **Configs**: Upload Configs + editor com todos os ficheiros, save por AJAX, caminhos resolvidos da config, botão **Terminar todas as sessões**
- **Debug**: filtros, 500 entradas, botão Download Log (txt)

### Segurança (GUI)
- **Login guard anti brute-force**: 5 falhas â†’ bloqueio 30s por IP (configurável: `HTTP LOGIN MAXFAIL` / `HTTP LOGIN LOCKTIME`), throttle global contra rajadas de IPs, reset no login com sucesso
- Cookie de sessão `HttpOnly; SameSite=Lax`; sessões persistentes em ficheiro; **todas as sessões são invalidadas quando a password muda**
- Log com `vsnprintf` (sem stack overflow com nomes longos), validação de datagramas UDP da cache, fuzz-testing do parser HTTP
- Crash handler gera core dump + serviço `multics-crashreport` regista o backtrace automaticamente

### Proteções e otimizações
- **SKIPCWC** (default ON): ignora CWs exatamente iguais à anterior no mesmo canal (fakes repetidas) ”” por perfil, com SIDLIST de exclusões
- **CWC ”” CW Cycle Check** (estilo OSCam): aprende o ciclo CW0/CW1 e descarta CWs fora do ciclo, ECMs antigos (replay) e bad cycles ”” por perfil
- **NAGRA protection** (18xx/19xx/1a0x): checksum das 4 quads, provider, ciclo com aprendizagem (6 amostras), similaridade, duplicate/conflicting/fake half ”” por perfil
- **Anti-fake embutido (não configurável)**: CWs com assinatura XOR 0xF0 no último byte e CRC nano e0 (viaccess 0500) são descartadas
- **DCW MINTIME / CYCLE_CHECK**: tempo mínimo entre CWs e alternância obrigatória da metade que muda (NDS)
- **CCcam 3.0.1** (projeto `CCcam-3.0.1-by-Sharillas`): suporte completo nos dois sentidos ””
  - **Server**: `CCCAM3 PORT: 16001` ”” boxes CCcam3 ligam-se com as mesmas F-lines; handshake encriptado **RSA+AES-GCM** (PBKDF2-SHA256, 10000 iterações)
  - **Reader**: linha `C3: host porta user pass` ”” o MultiCS lê cards de um server CCcam3
  - Crypto implementada em **C puro** (sem OpenSSL): SHA1/SHA256, HMAC, PBKDF2, RC4, AES-256 (ECB+GCM) ”” binário continua estático musl sem dependências
- **DEDUP de ECMs**: 1 pedido único ao reader por ECM em voo (clientes com o mesmo ECM ficam à espera do mesmo CW) ”” resolve o flood em canais críticos (ex: TVCine) e protege o card de throttling
- **NOK cache por reader+canal** (~8s): um reader que responde NOK num canal é saltado nos zappings seguintes (só quando há alternativa) ”” menos tráfego NOK
- **Fallback mais rápido**: intervalo adaptativo ”” se o reader atual está lento, o próximo é tentado mais cedo
- **Health scoring / Fallback cross-protocol / Timing budget**: ordenação por saúde, ordem de protocolos, budget de cryptoperíodo
- **BUILD LITE**: filtro de canais activos (CCcam.lite) ”” o perfil ignora ECMs fora da lista
- **ENABLE EMULATOR BISS** por perfil
- **DCW TIMEOUT 2500ms** recomendado nos perfis NAGRA (NOK mais rápido, cryptoperíodo ~10s)

### Robustez e gestão
- **Upload validado**: deteta erros de parse por ficheiro/linha, comenta automaticamente as linhas más, aplica defaults, e em último recurso faz rollback do backup (`.bak-<timestamp>`) ”” nunca crasha nem fica sem arrancar
- Escrita atómica (tmp+rename) com mensagens de erro claras (permissões, etc.)
- Opções legadas de builds antigas aceites como no-op (compatibilidade)
- Serviço systemd `User=root` + `chmod 666` nas configs + log em ficheiro com rotação (`-f` â†’ `/var/tmp/multics.log`)
- Restart fiável em qualquer setup (systemd/crontab/paths diferentes)
- SIG_HANDLER: registos de crash com RIP/CR2 no log + **core dump** para análise post-mortem
- **ip2country diário**: base do repo iplocate/ip-address-databases via `tools/update_ip2country.py` (cron: `30 5 * * * python3 /opt/multics/update_ip2country.py /var/etc/ip2country.csv`)
- **CI GitHub Actions**: build musl automático em cada push
- **Assinaturas de protocolo** na coluna info server: `Newcamd v6.06`, `Mgcamd v1.46`, `Camd35 v0.3.x`, `Cs738x TCP v0.3.x`, `Cs357x UDP v0.3.x`, `Csp-Cache | Mcs1000`, `Cache EX | Mcs1000` (CCcam mantém versão + build + nodeid da config)
- Ferramentas GUI: **Update Channel Info** (KingOfSat â†’ CCcam.channelinfo + CCcam.lite) e **Update SoftCam.Key** ”” resolvidas junto do binário

Lista completa de fixes e implementação: **[docs/IMPLEMENTACAO.md](docs/IMPLEMENTACAO.md)**

---

## Desenvolvimento (Windows)

```powershell
# compilar (cross-compile Zig 0.15.2, sem Linux)
powershell -ExecutionPolicy Bypass -File build.ps1

# empacotar (dist/)
powershell -ExecutionPolicy Bypass -File package.ps1

# deploy para a VPS (credenciais em deploy.secrets.ps1 - gitignored)
powershell -ExecutionPolicy Bypass -File deploy.ps1

# regenerar CSS/JS embutido (httpstyle.c) depois de mudar Configs/multics.css
python tools_generate_httpstyle.py
```

Build flags (equivalentes ao Makefile original):
`-DCHECK_NEXTDCW -DSID_FILTER -DNEWCACHE -DCCCAM_CLI -DRADEGAST_CLI -DCAMD35_CLI -DCS378X_CLI -DHTTP_SRV -DTELNET -DMGCAMD_SRV -DCCCAM_SRV -DCAMD35_SRV -DCS378X_SRV -DSRV_CSCACHE -DEXPIREDATE -DDCWSWAP -DCACHEEX -DIPLIST -DTESTCHANNEL -DTHREAD_DCW -DEPOLL_NEWCAMD -DEPOLL_CCCAM -DEPOLL_MGCAMD -DEPOLL_ECM -DPEERLIST -DECMLIST -DEPOLL_FREECCCAM -DSIG_HANDLER -DCLI_CSCACHE -std=gnu90 -O3 -fpack-struct`

---

## Segurança

- **Muda já** o `HTTP USER/PASS` e `TELNET USER/PASS` no `multics.cfg`
- Login guard ativo por defeito (5 falhas/30s por IP) ”” ajustável em `HTTP LOGIN MAXFAIL` / `HTTP LOGIN LOCKTIME`
- Expõe só as portas que precisas (a web UI 5500 é a mais sensível)
- Credenciais de deploy vivem **fora do repo** (`deploy.secrets.ps1`, gitignored)
- Binários estáticos: sem dependências da libc da VPS

---

## Créditos

Base: (evileyes). Mod, GUI e fixes: **Sharillas@2026** ”” v1.24
