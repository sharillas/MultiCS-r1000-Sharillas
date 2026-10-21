# MultiCS r1000 v1.30 — Tutorial Plug & Play

> Cardserver proxy multiprotocolo (CCcam / Newcamd / Mgcamd) Cache / CacheEX) com tema "Stats Tiles" e painéis de estudo de CWs.
> **Importante**: NUNCA colocar IPs, utilizadores, passwords ou linhas reais na repo — usa este tutorial com valores fictícios e guarda os teus segredos localmente (`deploy.secrets.ps1` está no .gitignore).

---

## 1. Instalação (5 minutos)

1. Descarrega os binários (`multics.x64` / `multics.x32`, musl estático) para `/opt/multics/`.
2. Copia a pasta `configs_exemplos/` para `/var/etc/` (ou usa `-C /var/etc/multics.cfg`).
3. `chmod 755 /opt/multics/multics.x64`
4. systemd (adaptar caminhos):

```ini
[Unit]
Description=MultiCS r1000 by Sharillas (cardserver proxy)
After=network-online.target

[Service]
ExecStart=/opt/multics/multics.x64 -f -C /var/etc/multics.cfg
Restart=always

[Install]
WantedBy=multi-user.target
```

5. `systemctl enable --now multics` → abre `http://IP-DA-VPS:5500` (user/pass definidos no multics.cfg).

---

## 2. multics.cfg (mestre)

```ini
LOGLEVEL: 3                    # 0-5 (4+ = muito verboso, só para debug)
HTTP PORT: 5500
HTTP USER: admin
HTTP PASS: muda-me
TELNET PORT: 5600
CCCAM PORT: 16000
MGCAMD PORT: 21000
CACHE PORT: 5599

FILE CHANNELINFO: "/var/etc/CCcam.channelinfo"
FILE PROVIDERINFO: "/var/etc/CCcam.providers"
FILE IP2COUNTRY: "/var/etc/ip2country.csv"
FILE STYLESHEET: "/var/etc/multics.css"

INCLUDE "/var/etc/profiles.cfg"
INCLUDE "/var/etc/servidores.cfg"
INCLUDE "/var/etc/clientes_cccam.cfg"
```

---

## 3. profiles.cfg — os pacotes (CAID por perfil)

Cada secção `[Nome]` = um pacote:

```ini
[MEO]
PORT: 15002
CAID: 1814
PROVIDERS: 000000, 005211
USER: cliente1 senha1 { name="Box cliente" }
```

### 3.1 Opções DCW (entrega das CWs)

| Opção | Default | O que faz |
|---|---|---|
| `DCW TIMEOUT: 2500` | 3000 | tempo máximo para obter a CW (ms) |
| `DCW MAXFAILED: 0` | 0 | falhas seguidas antes de desistir |
| `DCW RETRY: 3` | 3 | tentativas por ciclo |
| `DCW CHECK: NO` | NO | validação anti-fake de CW (opt-in) |
| `DCW HALFNULLED: NO` | NO | aceita CW half-null (NDS) |
| `DCW SWAP: YES` | NO | troca CW0/CW1 (NDS) |
| `DCW SILENT_NOK: YES` | NO | atrasa o NOK 2.5s (não pára o descrambler do cliente) |
| `DCW LASTCWONNOK: YES` | NO | em NOK/timeout reenvia a última CW válida do canal (janela de 2) — **anti-freeze do circuito** |
| `DCW CYCLE ENGINE: YES` | NO | **(v1.40)** motor único de ciclo por canal: aprende a cadência e a alternância CW0/CW1; segura CWs stale (hold 1x) e marca anomalias na fonte (cwbad + bad-cw cache). Substitui MINTIME/CYCLE_CHECK/CWC/STALE_CHECK/TIMING |
| `DCW BADCW TTL: 10` | 10 | **(v1.40)** minutos que um reader é saltado num canal onde entregou CW má (continua a servir os outros canais) |
| `DCW LOG: YES` | NO | regista as CWs em hex em `/var/log/multics-cw.log` com o id da fonte (`srv N`) |
| `DCW CAK7: YES` | NO | transformação CAK7 Merlin (canais que exigem) |
| `SERVERS: 1,5` | (todos) | **(v1.40)** lista explícita de readers que o perfil pode usar — fim do "saco de relé" |

### 3.2 Opções ECM (aceitação do pedido)

| Opção | Default | O que faz |
|---|---|---|
| `ECM CHECK: NO` | NO | validação do ECM |
| `ECM CHECK LENGTH: YES` | YES | valida o tamanho declarado |
| `ECM FILTER: YES` | NO | regras de drop/log (ECM RULE, ECM FILTER MODE) |

### 3.3 SID (filtros por canal)

```ini
SID LIST: 0EDA            # lista de SIDs (com prefixo ! = denylist)
SID DENYLIST: YES         # IMPORTANTE: escrever DEPOIS do SID LIST
SID FILE: CHANNELINFO     # aceitar só os canais do CCcam.channelinfo
```

### 3.4 CACHE (a arma anti-freeze)

| Opção | Default | O que faz |
|---|---|---|
| `CACHE TIMEOUT: 6000` | 2000 | janela de validade da cache (ms) — v1.29 subiu para 6000 |
| `CACHE SENDREQ/SENDREP` | YES | participar no anel de cache |

### 3.5 NAGRA protection (18xx)

| Opção | Default | O que faz |
|---|---|---|
| `ENABLE NAGRA: YES` | YES | validação estrutural (checksum + provider) |
| `NAGRA ONBAD: NO` | **NO (v1.29)** | **log only por omissão** — `YES` = drop (modo agressivo, opt-in) |
| checksum gate (v1.29) | automático | CW com checksum inválido **não é entregue** — espera outra fonte |

### 3.6 HEALTH / FALLBACK

```ini
ENABLE HEALTH: YES      # score por reader (sucesso/latência/estabilidade + cwbad)
HEALTH DROPOFF: 200     # readers abaixo disto ficam fora do pedido
FALLBACK ORDER: NEWCAMD CCCAM MGCAMD
FALLBACK TIMEOUT: 800   # ms antes de abrir aos protocolos seguintes
```

> **v1.40/v1.41**: o anti-fake agora tem 2 camadas — a validação estrutural sempre-on + a
> **reputação por fonte** (cwbad + bad-cw cache por canal + feedback do cliente: hash repetido
> após entrega = a CW não abriu). Os readers podem ser marcados com `hop=1` (directa) / `hop=N`
> (circuito) e o load-balance prefere a directa.

### 3.7 Protecções do servidor (não afectam a entrega)

```ini
FAILBAN: YES            # ban por CWs más repetidas
ANTICASCADE MAXZAP: 60  # zaps máximos por IP/janela (0 = off)
ANTICASCADE WINDOW: 300
ANTICASCADE BANTIME: 900
ANTICASCADE EXCLUDE: 10.0.0.5
```

---

## 4. servidores.cfg — os readers (fontes)

```ini
N: IP_DO_PARCEIRO 34000 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14 { nocheck=yes; name="Linha MEO" }
C: IP_DO_PARCEIRO 12000 user pass { nocheck=yes; name="Colega CCcam" }
```

- `name="..."` → aparece no "from" do last-used-share e no debug (v1.29).
- `nocheck=yes` → topologia cliente+readers no mesmo IP (anti-loop off).
- `priority=N` → peso no load-balance.

---

## 5. clientes_cccam.cfg — as F-lines (os teus clientes)

```ini
F: user1 pass1 { name="Box do Zé"; host=89.154.1.2; profiles=15002,15003; expire=2026-12-31 }
```

A box do cliente liga com: `C: TEU_IP 16000 user1 pass1`

---

## 6. CCcam.channelinfo — nomes dos canais

Formato: `CAID:PROVID:SID "NOME [PACOTE 30W]"` — alimenta o feed, o last-used-share e a GUI. A GUI tem "Update Channel Info" (KingOfSat) e "Load Channel Info" (rele o teu ficheiro sem restart).

---

## 7. O feed ECM/CW (estudo de CWs)

- Em **Servers** e nas páginas de **clientes**, clica **DBG** numa linha → abre painel por baixo com a info + **tabela live** (age | canal | ECM hex | CW0 CW1 | ms | WAIT/OK/NOK | fonte), actualização de 2s.
- `DCW LOG: YES` no perfil → todas as CWs entregues ficam em `/var/log/multics-cw.log` (com timestamp, canal e fonte).
- O painel sobrevive ao auto-refresh; tema claro/escuro com `?theme=light|dark`.

---

## 8. Resolução de problemas

| Sintoma | Causa provável | Acção |
|---|---|---|
| Canal abre e congela | fonte a entregar CWs erradas/stale | activar `DCW LASTCWONNOK` + `DCW CYCLE ENGINE` + `DCW SILENT_NOK` — o motor marca a fonte (cwbad) e salta-a no canal (bad-cw cache) |
| Canal não abre | CW lixo da fonte | ver o debug: `cwlr: CW lixo (checksum)` ou `cyc:` — a build segura/marca a CW má e a fonte perde prioridade |
| "Channel denied" | SID DENYLIST mal ordenado | `SID LIST` primeiro, `SID DENYLIST: YES` DEPOIS |
| Botões não respondem | cache do browser | Ctrl+F5 (customjs tem versão no URL) |
| Flag de país errada | lista ip2country estragada em memória | restart (carrega o ficheiro de novo) |

---

## 9. Notas de segurança

- Não partilhar linhas reais (N:/C:/F:) na repo — usa placeholders.
- Rodar as credenciais regularmente; prender F-lines com `host=`.
- `deploy.secrets.ps1` é LOCAL e gitignored.
