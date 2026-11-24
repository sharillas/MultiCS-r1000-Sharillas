# Configuração do Cardserver — guia exato (v1.63.4)

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

### Filtro rigoroso por ident (anti fake/NOK)

- `DEFAULT ACCEPT NULL PROVIDER: NO` — pedidos com provider vazio (`000000`)
  **só passam** se o perfil listar `000000` explicitamente em `PROVIDERS`.
  Qualquer outro ident fora da lista é rejeitado de imediato ("Wrong provider").
- Lista em `PROVIDERS` **todos os idents ativos** do pacote, ex:

```cfg
[MEO]
PORT: 15002
CAID: 1814
PROVIDERS: 000000, 005211, 005221, 000007   # wildcard + idents reais

[NOS]
PORT: 15003
CAID: 1802
PROVIDERS: 000000                   # a NOS usa SEMPRE o wildcard (estudo 01/10)
```

- Os idents oficiais apurados para os 3 satélites estão em `CCcam.providers`
  (nomes com a proveniência de cada ident) — usa-os como referência.
- **Se uma box pedir um ident novo**: aparece `Wrong provider` no Debug Log
  (com o CAID:PROVID do pedido) → acrescenta esse ident à lista `PROVIDERS`
  do perfil e grava pela GUI (**Configs → Edit**) — sem restart.

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
C: 203.0.113.10 15000 user pass { nocheck=yes }
N: 203.0.113.10 3000 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14 { nocheck=yes }
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

---

## 13. Referência das opções v1.40 – v1.63

Esta secção foi escrita a partir do parser (`src/config.c`) da **v1.63.4**. Os intervalos são os que o código aplica (valores fora do intervalo são cortados). «Defeito» só aparece quando o código o fixa explicitamente. As opções de perfil aceitam o prefixo `DEFAULT` (ex.: `DEFAULT DCW AGREE: 2`) ou valem só dentro de `[perfil]`.

### 13.1 `multics.cfg` (globais)

| Opção | Valores | O que faz |
|---|---|---|
| `LOGFILE: <caminho>` | caminho | v1.49. Ficheiro de log completo (todas as linhas acima do `LOGLEVEL`). |
| `FAILBAN ENABLE:` | `YES`/`NO` | Liga o ban automático de IPs com eventos maus (CWs más). |
| `FAILBAN BANTIME:` | 60–86400 (s) | Duração do ban. |
| `FAILBAN CCCAM:` / `NEWCAMD:` / `MGCAMD:` / `CACHE:` | número de eventos; `0` = desligado | Eventos maus por IP até ser banido. **Escreve só o número** (`FAILBAN CCCAM: 20`). O parser lê apenas dígitos: `YES 20` é lido como `0`, ou seja, desligado. |
| `FAILBAN EXCLUDE:` | até 32 IPs, separados por vírgula | IPs que nunca são banidos. |
| `ANTICASCADE MAXZAP:` | 0–1000 (`0` = desligado) | Nº de zaps por IP, dentro da janela, a partir do qual há suspeita de reshare. |
| `ANTICASCADE WINDOW:` | 5–3600 (s) | Janela de contagem dos zaps. |
| `ANTICASCADE BANTIME:` | segundos | Duração do ban do IP (vai para o `ipblock`). |
| `ANTICASCADE EXCLUDE:` | IPs | IPs isentos. |

> O README e o CHANGELOG referem `CAMD35` e `CS378X` no FAILBAN, mas o código da v1.63.4 só aplica limite a CCCAM, NEWCAMD, MGCAMD e CACHE.

### 13.2 Perfil (`[perfil]` / `DEFAULT`)

| Opção | Valores / defeito | O que faz |
|---|---|---|
| `DCW CYCLE_ENGINE:` | `YES`/`NO` | v1.40. Motor único de ciclo por canal; substitui MINTIME / CYCLE_CHECK / CWC / STALE_CHECK / NAGRA CYCLE. **O nome é `CYCLE_ENGINE` (com underscore)**: `CYCLE ENGINE` (com espaço) não é reconhecido pelo parser e é **ignorado sem aviso no log**. |
| `DCW SILENT_NOK:` | `YES`/`NO` | Adia o NOK até perto do timeout da box (evita reconexões e tempestades de retries). Desligável desde a v1.55. |
| `DCW LASTCWONNOK:` | `YES`/`NO` | Em NOK, reenvia a última CW válida do canal (não pára o descrambler). Pode mascarar falhas reais da fonte. |
| `DCW HALFROTATE:` | `YES`/`NO` | v1.55. Filtro de rotação CW0/CW1 (anti-fake de período). |
| `DCW HALFREPEAT TTL:` | 2–8, defeito 3 | v1.55. Janela de repetições de meia-CW. |
| `DCW ARBITER:` | `YES`/`NO` | v1.55. Valida cada CW contra o reader de confiança (`arbiter=yes`). Desde a v1.55.5 o árbitro é perguntado em paralelo com a rotação (PRE-ASK). |
| `DCW ARBITER SERVE:` | `YES`/`NO` | Quando o árbitro **discorda** da CW da fonte, entrega a CW do árbitro. |
| `DCW ARBITER RATE:` | 0–100, defeito 100 | Percentagem de ECMs validados pelo árbitro. |
| `DCW ARBITER MAX:` | 0–500, defeito 0 (off) | v1.61.2 (fair-share). Tecto de validações por minuto, por cliente. |
| `DCW AGREE:` | 0–6, defeito 0 (`0`/`1` = off) | v1.55.1. Consenso: nº de fontes que têm de dar a **mesma** CW para ser aceite. |
| `SERVER ASK:` | 0–8, defeito 0 | v1.55.1. Quantos servers perguntar em paralelo por ECM (`0` = clássico: 1 server por ronda). |
| `SERVER ECMTIME:` (alias `TIMEPERECM`) | 0–5000 | Tempo por ECM no pedido ao server. |
| `ECMRATELIMIT SIDTIME:` | 0–60000 (ms) | Proteção do cartão: intervalo mínimo entre ECMs do mesmo canal. |
| `ECMRATELIMIT MAXECM:` | 0–10000 | Proteção do cartão: ECMs por segundo. |

### 13.3 Opções de reader (dentro de `{ ... }`, separadas por `;`)

| Opção | Valores | O que faz |
|---|---|---|
| `arbiter=` | `yes`/`no` | v1.55. Marca o reader como árbitro: fica fora da rotação normal e só é perguntado para validar CWs (ver `DCW ARBITER`). |
| `budget_max=` | 0–1000 (`0` = off) | v1.49 BUDGET ENGINE. Tecto manual em CWs por minuto. |
| `budget_group=` | 1–8 (`0` = só este reader) | Orçamento partilhado entre readers do mesmo cartão. |
| `budget_ask=` | 0–50 (`0` = sem tecto) | v1.50 BUDGET ASK. Tecto de **pedidos por segundo**: o cartão upstream vê no máximo N pedidos/s do saco inteiro. |
| `hop=` | número | v1.40. Hop da fonte (1 = direta, N = circuito); preferido no load-balance. |
| `priority=` | número | Prioridade do reader. |
| `providrewrite=` | `yes`/`no` | v1.48. Reescreve o provid para 0 nos pedidos a este reader. |
| `stealth=` | `yes`/`no` | v1.48.2. Server stealth (não anuncia a lista de cards). |
| `nocheck=` | `yes`/`no` | Desliga a proteção anti-loop (cliente e reader no mesmo IP). |
| `nochecksum=` | `yes`/`no` | v1.70.18. Não rejeitar as CWs **deste reader** por checksum inválido (resposta a ECM e proteção NAGRA `CHK`). CW nula, bad e fake (xor F0) continuam a ser rejeitadas. Não afeta o CacheEX. Checkbox "No CW checksum" no popup Edit. |
| `type=` | `mgcamd`/`newcamd`/`cccam` | v1.51. Grupo de apresentação na GUI (o protocolo real não muda). |
| `cacheex_mode=` / `cacheex_maxhop=` / `cacheex_forward=` | hex | Opções CacheEX do reader. |

Nas F-lines CCcam existe ainda a opção de cliente `ARBITER_MAX` (v1.61.2): tecto de validações do árbitro por minuto para esse cliente.

### 13.4 Ainda por documentar

O parser aceita também `ADMINUSER`, `ADMINPASS`, `MGUSER`, `TRACE` e `EXPIRED`, sem descrição fiável nos comentários do código. Rever antes de os documentar.

### 13.5 Opções obsoletas

- `CONSTCW FILE:` já não é lido pelo parser (aparece como `unknown option` no log em cada arranque). Não faz mal: sem ele, o emulador usa o `Softcam.cfg` que está na mesma pasta do `multics.cfg`.
- Sub-opções desconhecidas dentro de `DCW ...` são **ignoradas em silêncio**: um erro de escrita (ex.: `DCW CYCLE ENGINE`) não gera aviso no log.
