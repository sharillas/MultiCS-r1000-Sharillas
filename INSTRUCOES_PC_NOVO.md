# MultiCS r1000 v1.20 by Sharillas â€” usar noutro PC (opencode)

## O que estÃ¡ neste ZIP

- **src/** â€” cÃ³digo C completo (Ãºltima versÃ£o, todos os fixes)
- **build/** â€” binÃ¡rios compilados (x64/x32 estÃ¡ticos musl)
- **tests/** â€” suite de testes end-to-end (e2e_*.sh + fakeclient/fakecsp)
- **configs_exemplos/** â€” configs prontas (sem erros de parsing)
- **github_repo/** â€” repositÃ³rio enviado para https://github.com/sharillas/MultiCS-r1000-Sharillas
  (README, docs/CONFIGS.md, docs/IMPLEMENTACAO.md, install.sh, screenshots)
- **ANALISE_OSCAM_CWC.md** â€” anÃ¡lise do CW Cycle Check do OSCam
- **build.ps1 / package.ps1 / deploy.ps1 / test.sh** â€” toolchain de desenvolvimento
- **dist/** â€” pacotes de release (zip/tar.gz)
- **VPS.txt** â€” credenciais da VPS de testes

## PrÃ©-requisitos no PC novo

1. **Zig 0.15.2** (para compilar): descarregar em https://ziglang.org/download/0.15.2/
   e extrair para **C:\TMP\opencode\zig-x86_64-windows-0.15.2\**
   (caminho estÃ¡ no topo do build.ps1 â€” se mudares, ajusta lÃ¡)
2. **Python 3** (para tools_generate_httpstyle.py â€” regenerar o CSS/JS embutido)
3. **git** (para o repo)

## Comandos Ãºteis

```powershell
# compilar (x64 + x32)
powershell -ExecutionPolicy Bypass -File build.ps1

# regenerar CSS/JS embutido depois de mudar multics.css/customjs
python tools_generate_httpstyle.py

# empacotar (dist/)
powershell -ExecutionPolicy Bypass -File package.ps1

# deploy VPS
powershell -ExecutionPolicy Bypass -File deploy.ps1 -Host "IP_VPS"
```

## Estado atual (v1.20)

- Build = VPS = GitHub
- Features: GUI/light-dark, Emulator (SoftCam.Key + Update SoftCam.Key remoto),
  SKIPCWC, CWC (CW Cycle Check OSCam), NAGRA protection (18xx/19xx/1a0x),
  Health scoring, Fallback cross-protocol, Timing budget (ADAPTIVETTL),
  DCW MINTIME / DCW CYCLE_CHECK, DCW SKIPCWC_EXCLUDE SIDS,
  BUILD LITE (CCcam.lite), ENABLE EMULATOR BISS por perfil,
  ferramentas GUI (Update/Load Channel Info, Update SoftCam.Key),
  restart fiÃ¡vel, debug rows, editor com save imediato, 0 erros de parsing
- Fix crÃ­tico v1.0.10.3: bad DCW de readers marca ECM_SRV_REPLY_FAIL
  (corrige o "nÃ£o puxa ecms" da build antiga)
- v1.20: editor com todos os ficheiros (whitelist), validaÃ§Ã£o de upload
  (comenta linhas mÃ¡s, defaults, rollback .bak â€” sem crash), Blocked since
  DD:HH:MM:SS, top menu sticky, fix pipeline BISS NOK (tenta readers/cache),
  DCW TIMEOUT 2500ms nos perfis NAGRA, exemplos comentados completos
- Testes e2e validados na VPS: base, cwc, health, fallback, timing, nagra, lite, dcw

## Contexto importante (build antiga do PC antigo)

- A build antiga (v15-v18b/v20, binÃ¡rio b3bc2878) foi feita NOUTRO PC e tinha
  NAGRA/LITE/EMULATOR BISS/tools implementados lÃ¡; essas implementaÃ§Ãµes foram
  re-escritas/portadas para esta build (v1.20) â€” ver Prompt_old_Build.md
  e a engenharia reversa em C:\TMP\opencode\old_multics.x64
- Os ficheiros de produÃ§Ã£o da VPS (/var/etc) sÃ£o a fonte de verdade das configs:
  multics.cfg, profiles.cfg (DEFAULT ENABLE CWC/HEALTH/TIMING/FALLBACK/NAGRA),
  CCcam.providers, CCcam.channelinfo, Softcam.cfg, CCcam.lite
- Ferramentas na VPS: /opt/multics/tools_update_channelinfo.py e
  tools_update_softcam.py (invocadas pelos botÃµes da GUI)
