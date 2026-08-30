# Configuração do Cardserver — guia exato (v1.20)

Os ficheiros ficam em `/var/etc/` (depois do `install.sh`). Qualquer alteração pode ser feita pela **GUI (Configs → Edit ou Upload)** — aplica na hora, sem restart — ou editando os ficheiros.

> Ordem de parsing: os perfis têm de vir **antes** dos utilizadores/clientes. O `users.cfg` é incluído antes dos profiles (users globais); F-lines depois do `CCCAM PORT`.
>
> A GUI resolve os caminhos a partir da config em execução — funciona em qualquer layout de pastas (não é obrigatório `/var/etc`).

---

## 1. multics.cfg (mestre)

```cfg
######### HTTP (web UI) #########
HTTP PORT: 5500
HTTP USER: admin
HTTP PASS: admin
HTTP LOGIN MAXFAIL: 5    # falhas de login antes do bloqueio (brute-force)
HTTP LOGIN LOCKTIME: 30  # segundos de bloqueio por IP (default 5/30)

######### TELNET #########
TELNET PORT: 5600
TELNET USER: admin
TELNET PASS: troca_isto

######### CACHE #########
CACHE PORT: 5599
CACHE AUTOADD: YES, YES
CACHE FILTER: ON

######### Servidores / clientes #########
CCCAM PORT: 16000
MGCAMD PORT: 21000
CAMD35 PORT: 7502
CS378X PORT: 8600
NEWCAMD CLIENTID: 8888

INCLUDE "/var/etc/users.cfg"      # users globais (ANTES dos profiles)
INCLUDE "/var/etc/profiles.cfg"   # perfis
INCLUDE "/var/etc/1-Clients.cfg"  # F-lines (DEPOIS do CCCAM PORT)
INCLUDE "/var/etc/Nlines.cfg"     # readers N:
INCLUDE "/var/etc/Mgcamd.cfg"
INCLUDE "/var/etc/Camd35.cfg"     # camd35 + cs378x users (DEPOIS dos PORTs)
INCLUDE "/var/etc/Cache.cfg"
INCLUDE "/var/etc/CacheEX.cfg"

CONSTCW FILE: "/var/etc/Softcam.cfg"
BLOCKEDIP FILE: "/var/etc/blocked_ips.cfg"
FILE STYLESHEET: "/var/etc/multics.css"
```

---

## 2. profiles.cfg — o coração

Cada perfil = uma porta virtual newcamd onde os clientes ligam.

```cfg
[DEFAULT]
DEFAULT DCW CHECK: YES
DEFAULT DCW TIMEOUT: 3000
ENABLE SKIPCWC: YES        # default ON: ignora CWs repetidas (fakes)

[Meu Perfil]
PORT: 15001
CAID: 098D
PROVIDERS: 0
DCW SWAP: YES              # se o cartão trocar bytes da CW
DCW CHECK: YES
SERVER TIMEOUT: 1000
SERVER FIRST: 3
ENABLE NEWCAMD: YES
ENABLE CCCAM: YES
ENABLE CACHE: YES
```

- `CAID` tem de corresponder ao CAID do teu leitor/servidor de leitura.
- Um perfil por CAID (ex: 098D Sky DE, 0500 Viaccess, 0100 Seca…).
- Se um reader responde CWs de vários CAIDs, cria um perfil por CAID e partilha o reader entre eles.

---

## 3. Readers (servidores de leitura)

**Newcamd (N-line)** — `Nlines.cfg`:

```cfg
N: host.do.reader 34000 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14
```

**CCcam (C-line)** — pode ir no master ou num INCLUDE:

```cfg
C: host.do.reader 12000 user pass
```

**Restringir reader a perfis:**

```cfg
N: host 34000 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14 { profiles=15001,15002 }
```

**Topologia cliente + reader no mesmo IP externo** (a protecao anti-circular
salta o reader porque o IP dele esta na lista do ECM). Opcao `nocheck`:

```cfg
C: 130.255.78.45 15000 user pass { nocheck=yes }
N: 130.255.78.45 3000 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14 { nocheck=yes }
```

**Reader CacheEX** — `CacheEX.cfg`:

```cfg
C: cachex.peer.com 8600 user pass { cacheex_mode=3 }
```

---

## 4. Clientes

**CCcam (F-lines)** — `1-Clients.cfg`:

```cfg
F: cliente1 senha1                      # todos os perfis, sem reshare
F: cliente2 senha2 2                    # 2 reshares
F: cliente3 senha3 { profiles=15001 }   # só o perfil 15001
F: cliente4 senha4 { shares=0:0:0,098D:000000:1; name="Box Sala"; host=IP }  # limites
```

**Newcamd** — os clientes ligam à porta do perfil (ex: `15001`) com user/pass definidos em:

```cfg
# users.cfg (globais, antes dos profiles):
USER: global1 passglobal

# ou por perfil (dentro de um [perfil] no profiles.cfg):
USER: user1 pass1
```

**MGcamd** — `Mgcamd.cfg`:

```cfg
MG: usermg passmg
```

**camd35 / cs378x** — `Camd35.cfg`:

```cfg
CAMD35 USER: userc35 passc35
CS378X USER: usercsx passcsx
```

---

## 5. Emulator (BISS/Tandberg)

1. GUI → **Emulator** → escolhe o `SoftCam.Key` → **Convert & Load**
2. As chaves ficam em `/var/etc/Softcam.cfg` (formato `CAID:PROVID:SID:CW32HEX`)
3. Perfil para chaves constantes (BISS = CAID 2600):

```cfg
[Profile_BISS]
PORT: 15025
CAID: 2600
ENABLE EMULATOR BISS: YES
```

4. Atualização automática (opcional, crontab a cada 6h):

```cron
0 */6 * * * root /usr/bin/python3 /var/etc/biss_updater.py >> /var/log/biss_updater.log 2>&1
```

---

## 6. SKIPCWC (proteção contra CWs falsas)

- **Default ON** em todos os perfis.
- Desligar por perfil: `ENABLE SKIPCWC: NO` dentro do `[perfil]`.
- O que faz: ignora CW **exatamente igual** à anterior no mesmo canal (não re-cacheia, não re-propaga, não conta). A primeira CW de cada ciclo passa sempre.

---

## 7. Cache / partilha de CWs

```cfg
# Cache.cfg — peer que partilha CWs:
CACHE: peer.host.com 5599 user pass { sendreq=yes; sendrep=yes; autoadd=yes }
```

O cache troca CWs entre servers; o `CACHE AUTOADD` aceita peers dinâmicos.

---

## 8. Proteções por perfil (CWC, NAGRA, Health, Fallback, Timing, DCW, LITE)

Tudo desligado por omissão (exceto SKIPCWC e ENABLE EMULATOR BISS, ON). Ativa-se por perfil ou com `DEFAULT ...` no topo do profiles.cfg.

```cfg
## CW Cycle Check (estilo OSCam) — protege contra cws fakes/replay
ENABLE CWC: 1              # no perfil afetado
CWC SENSITIVE: 3           # bytes iguais na metade que NAO devia mudar (0=off)
CWC DROPOLD: 1             # drop ECM antigo (replay) e same CW fora da janela
CWC DROPBAD: 1             # drop bad CW cycle (ou ONBAD: alias)
CWC KEEPCYCLETIME: 5       # minutos que mantem o cycletime sem re-aprender

## NAGRA protection (caid 18xx/19xx/1a0x) — checksum, ciclo e similaridade
ENABLE NAGRA: 1
NAGRA CHK: 1               # checksum das 4 quads
NAGRA PROV: 0              # validar provider contra o perfil
NAGRA CYCLE: 1             # ciclo + similaridade (aprendizagem de 6 amostras)
NAGRA ONBAD: 1             # 1=drop em bad dcw, 0=so log
NAGRA SENSITIVE: 4         # bytes iguais a CW anterior (0=off, 1-8)

## Health scoring — ordena/exclui servers por saude
ENABLE HEALTH: 1
HEALTH WEIGHTS: 40 30 10 20   # sucesso latencia estabilidade erros (%)
HEALTH MINECMS: 20            # amostras minimas antes de pontuar
HEALTH DROPOFF: 200           # score minimo (0=off)

## Fallback cross-protocol
ENABLE FALLBACK: 1
FALLBACK ORDER: NEWCAMD CCCAM CS378X CAMD35 RADEGAST
FALLBACK TIMEOUT: 800       # ms antes de permitir protocolos fallback

## Timing budget (cryptoperiod adaptativo)
ENABLE TIMING: 1
TIMING FRACTION: 60         # % do cryptoperiod como budget
TIMING MINPERIOD: 3000      # so aplica acima deste periodo (ms)
#CACHE ADAPTIVETTL: 1       # TTL da cache = cryptoperiod

## DCW proteccoes por canal
DCW MINTIME: 3000           # tempo minimo entre CWs (ms, 0=off)
DCW CYCLE_CHECK: 1          # a metade que muda tem de alternar (NDS)
DCW SKIPCWC_EXCLUDE_SIDS_ACTIVE: 1
DCW SKIPCWC_EXCLUDE_SIDLIST: 14B7,14B4   # sids onde o skipcwc nao se aplica

## Emulador por perfil
ENABLE EMULATOR BISS: 1     # default ON; 0 desliga o constcw/BISS no perfil

## BUILD LITE — filtro de canais activos
#LITE FILE: /var/etc/CCcam.lite   # top-level no multics.cfg (caid:provid:sid)
ENABLE LITE: 1              # no perfil: ignora ECMs fora da lista
```

Notas:
- `NAGRA` aplica só a caids 18xx–1a12 (MEO/NOS/...); caids fora do range passam sem checks.
- `LITE` sem lista carregada (ficheiro em falta/vazio) deixa passar tudo — seguro.
- O `CCcam.lite` é gerado/atualizado pela ferramenta (GUI → Update Channel Info) ou manualmente.

---

## 9. DEDUP de ECMs (automático, sem config)

Um pedido **único** ao reader por ECM em voo: o 1º cliente com um ECM gera o pedido; os seguintes com o mesmo hash ficam à espera e recebem o mesmo CW. Reduz o tráfego no card em ~90% em canais com muitos clientes (ex: TVCine) e evita o throttling do card.

- Ver na GUI: página **Servers → seccção "ECM Dedup"** (únicos vs repetidos evitados + top de canais)
- Log: `ecm: DEDUP join ch …`

---

## 10. Upload e edição de configs (GUI → Configs)

- **Upload Configs**: envia qualquer um dos 15 ficheiros para o caminho real da config (multics.cfg, profiles.cfg, CCcam.channelinfo, CCcam.lite, Nlines.cfg, users.cfg, Mgcamd.cfg, Camd35.cfg, Cache.cfg, CacheEX.cfg, 1-Clients.cfg, Softcam.cfg, blocked_ips.cfg, ip2country.csv, multics.css)
- Faz **backup automático** (`.bak-<timestamp>`) e recarrega a config
- **Validação**: linhas com erro de parse são **comentadas automaticamente** e as opções default ficam activas; se não der para corrigir, faz **rollback** do backup (o ficheiro enviado fica em `.invalid-*`) — nunca crasha
- **Edit**: todos os ficheiros editáveis na GUI, save por AJAX com feedback "Guardado com sucesso!"
- Um ficheiro único com todas as secções (servers, perfis, clientes) carrega-se como `multics.cfg`

---

## 11. ip2country (bandeiras na GUI)

Base **diária** do repo [iplocate/ip-address-databases](https://github.com/iplocate/ip-address-databases):

```bash
# manual
python3 /opt/multics/update_ip2country.py /var/etc/ip2country.csv
# diário (cron)
30 5 * * * python3 /opt/multics/update_ip2country.py /var/etc/ip2country.csv
```

- Formato aceite pelo MultiCS: `NETWORK/PREFIXO,CC` (ex: `130.255.0.0/16,PT`) ou `START,END,CC`
- **Override manual**: acrescenta a linha no **fim** do ficheiro — a última linha tem prioridade na pesquisa

---

## 12. Verificação rápida
```bash
systemctl restart multics && sleep 3
journalctl -u multics -n 30          # sem "config(...) error" = parsing limpo
ss -tlnp | grep multics              # portas a escutar
```

Na GUI: Dashboard (estado geral) → Servers (readers + dedup) → Newcamd/CCcam (clientes) → Debug (paths e log, Download Log).
