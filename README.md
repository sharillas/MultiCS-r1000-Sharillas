# MultiCS r1000 v1.20 - by Sharillas

Cardserver proxy com GUI web moderna, Softcam BISS/CW, proteÃ§Ã£o SKIPCWC contra CWs falsas, CWC (CW Cycle Check), NAGRA protection, Health scoring, Fallback cross-protocol, Timing budget, BUILD LITE, login com sessÃµes, temas Dark/Light e dezenas de fixes.

> **VersÃ£o:** v1.20 | **LicenÃ§a:** Sharillas@2026

---

## Dashboard

| Light mode | Dark mode |
|---|---|
| ![Dashboard Light](screenshot/dashboard-light.png) | ![Dashboard Dark](screenshot/dashboard-dark.png) |

---

## InstalaÃ§Ã£o (qualquer VPS Linux x86/x64)

Os binÃ¡rios sÃ£o **estÃ¡ticos musl** â€” correm em Debian, Ubuntu, CentOS, Rocky, Alma, Arch, Alpineâ€¦ sem dependÃªncias.

```bash
git clone https://github.com/sharillas/MultiCS-r1000-Sharillas.git
cd MultiCS-r1000-Sharillas
sudo bash install.sh
```

O instalador:
- detecta a arquitetura (x64/x32) e o sistema de serviÃ§os (systemd â†’ init.d â†’ crontab @reboot)
- instala binÃ¡rios em `/opt/multics` e configs em `/var/etc`
- cria o serviÃ§o `multics` (auto-start no boot)
- abre a porta da web UI no firewall (ufw/firewalld)
- faz smoke test Ã  web UI

Opcional:

```bash
# pasta diferente para os binÃ¡rios
sudo bash install.sh /meu/caminho

# credenciais web personalizadas
sudo HTTP_USER=admin HTTP_PASS=minhasenha bash install.sh
```

Depois abre `http://SEU_IP:5500` â†’ login (default `admin`/`admin`) â†’ **Dashboard**.

---

## Portas

| ServiÃ§o | Porta |
|---|---|
| Web UI (HTTP) | 5500 |
| Telnet (gestÃ£o) | 5600 |
| CCcam (F-lines) | 16000 |
| MGcamd | 21000 |
| Newcamd (perfil) | 15001+ (definido no PORT de cada perfil) |
| camd35 | 7502 |
| cs378x | 8600 |
| Cache | 5599 |

---

## ConfiguraÃ§Ã£o do cardserver (resumo)

Os configs vivem em `/var/etc/`:

| Ficheiro | O que faz |
|---|---|
| `multics.cfg` | **Mestre**: portas, credenciais, cache, telnet, HTTP e INCLUDEs |
| `profiles.cfg` | **Perfis de saÃ­da** (portas virtuais newcamd): CAID, PROVIDERS, DCW checks, SKIPCWCâ€¦ |
| `1-Clients.cfg` | **F-lines** (clientes CCcam) |
| `Nlines.cfg` | **N-lines** (readers newcamd) |
| `users.cfg` | Utilizadores globais (antes dos profiles!) |
| `Mgcamd.cfg` / `Camd35.cfg` | Utilizadores MGcamd / camd35 + cs378x |
| `Cache.cfg` | Peers de cache |
| `CacheEX.cfg` | Readers CacheEX (`C: ... { cacheex_mode=3 }`) |
| `Softcam.cfg` | Chaves CONSTCW (BISS/Tandberg) â€” gerido pelo Emulator |

Guia detalhado com exemplos reais: **[docs/CONFIGS.md](docs/CONFIGS.md)**

Fluxo mÃ­nimo para funcionar:

1. Define o teu CAID no perfil (`profiles.cfg`): `[Meu Perfil]` + `PORT: 15001` + `CAID: XXXX`
2. Adiciona readers: `N:` (newcamd) ou `C:` (cccam) com o teu servidor
3. Adiciona clientes: `F:` (CCcam) ou `USER:`/N-lines nos perfis
4. `systemctl restart multics` (ou usa **Edit Config** â€” aplica na hora)
5. VÃª os clientes na GUI: Newcamd / CCcam / Mgcamdâ€¦

---

## Features desta build

- **GUI moderna**: Dashboard com estatÃ­sticas, tabelas sortÃ¡veis, temas Dark/Light, login com sessÃµes, logout
- **Emulator**: upload SoftCam.Key (Convert & Load), chaves BISS/Tandberg, pÃ¡gina prÃ³pria, botÃµes **Update SoftCam.Key** (download remoto) e **Reload Keys**
- **SKIPCWC** (default ON): ignora CWs idÃªnticas repetidas (fakes) â€” configurÃ¡vel por perfil
- **CWC â€” CW Cycle Check** (estilo OSCam): aprende o ciclo CW0/CW1 de cada canal e descarta CWs fora do ciclo, ECMs antigos (replay) e bad CW cycles â€” por perfil
- **NAGRA protection** (18xx/19xx/1a0x): checksum das 4 quads, provider, aprendizagem do ciclo (6 amostras), similaridade, duplicate/conflicting/fake half â€” por perfil
- **Health scoring**: ordena os servers por saÃºde (sucesso, latÃªncia, estabilidade, erros) e exclui os doentes (DROPOFF) â€” por perfil
- **Fallback cross-protocol**: ordem de preferÃªncia de protocolos por perfil (NEWCAMD/CCCAM/CAMD35/CS378X/RADEGAST) com timeout
- **Timing budget**: usa o cryptoperiod estimado para falhar cedo e dar tempo ao cliente de pedir o prÃ³ximo ECM (ADAPTIVETTL) â€” por perfil
- **DCW MINTIME / DCW CYCLE_CHECK**: tempo mÃ­nimo entre CWs e alternÃ¢ncia obrigatÃ³ria da metade que muda (NDS) â€” por perfil
- **BUILD LITE**: filtro de canais activos (CCcam.lite) â€” o perfil ignora ECMs fora da lista (`Ignored (lite)`)
- **ENABLE EMULATOR BISS** por perfil (liga/desliga o emulador em cada perfil)
- **Ferramentas GUI**: Update/Load Channel Info (KingOfSat â†’ CCcam.channelinfo) e Update SoftCam.Key na web UI
- **Edit Config / Edit Profiles** com aplicaÃ§Ã£o imediata (sem restart)
- **Restart fiÃ¡vel** em qualquer setup (systemd, crontab, paths diferentes)
- **Linhas de debug por cliente** (botÃ£o DBG em todas as tabelas)
- **ProteÃ§Ãµes de DCW**: checksum, null DCW, bad DCW, nanoE0 (viaccess), filtros cache CAID 0500
- Zero erros de parsing com os configs de exemplo

Lista completa de fixes e implementaÃ§Ã£o: **[docs/IMPLEMENTACAO.md](docs/IMPLEMENTACAO.md)**

---

## Desenvolvimento (Windows)

```powershell
# compilar (cross-compile Zig 0.15.2, sem Linux)
powershell -ExecutionPolicy Bypass -File build.ps1

# empacotar (dist/)
powershell -ExecutionPolicy Bypass -File package.ps1

# deploy para VPS (scp+ssh)
powershell -ExecutionPolicy Bypass -File deploy.ps1 -Host "IP_VPS"

# teste local WSL
bash test.sh
```

Build flags (equivalentes ao Makefile original):
`-DCHECK_NEXTDCW -DSID_FILTER -DNEWCACHE -DCCCAM_CLI -DRADEGAST_CLI -DCAMD35_CLI -DCS378X_CLI -DHTTP_SRV -DTELNET -DMGCAMD_SRV -DCCCAM_SRV -DCAMD35_SRV -DCS378X_SRV -DSRV_CSCACHE -DEXPIREDATE -DDCWSWAP -DCACHEEX -DIPLIST -DTESTCHANNEL -DTHREAD_DCW -DEPOLL_NEWCAMD -DEPOLL_CCCAM -DEPOLL_MGCAMD -DEPOLL_ECM -DPEERLIST -DECMLIST -DEPOLL_FREECCCAM -std=gnu90 -O3 -fpack-struct`

---

## SeguranÃ§a

- **Muda jÃ¡** o `HTTP USER/PASS` e `TELNET USER/PASS` no `multics.cfg`
- ExpÃµe sÃ³ as portas que precisas (a web UI 5500 Ã© a mais sensÃ­vel)
- BinÃ¡rios estÃ¡ticos: sem dependÃªncias da libc da VPS

---

## CrÃ©ditos

Base: (evileyes). Mod, GUI e fixes: **Sharillas@2026** â€” v1.20
