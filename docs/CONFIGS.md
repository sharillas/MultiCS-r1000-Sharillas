# Configuração do Cardserver — guia exato

Os ficheiros ficam em `/var/etc/` (depois do `install.sh`). Qualquer alteração pode ser feita pela **GUI (Edit Config / Edit Profiles)** — aplica na hora — ou editando os ficheiros e reiniciando.

> Ordem de parsing: os perfis têm de vir **antes** dos utilizadores/clientes. O `users.cfg` é incluído antes dos profiles (users globais); F-lines depois do `CCCAM PORT`.

---

## 1. multics.cfg (mestre)

```cfg
######### HTTP (web UI) #########
HTTP PORT: 5500
HTTP USER: admin
HTTP PASS: admin

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
PORT: 15038
CAID: 2600
ENABLE SOFTCAM: YES
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

## 8. Verificação rápida

```bash
systemctl restart multics && sleep 3
journalctl -u multics -n 30          # sem "config(...) error" = parsing limpo
ss -tlnp | grep multics              # portas a escutar
```

Na GUI: Dashboard (estado geral) → Servers (readers) → Newcamd/CCcam (clientes) → Debug (paths e log).
