# Análise do oscam do colega — o mapa real (corrigido 30/09)

**Correcções importantes à análise inicial:** o binário `oscam4` **não está a
correr** — é um backup na pasta. O oscam em execução = o **OSCam 2.00 build
11961** normal (o `/emu/oscam/oscam`, o systemd, o reinício diário às 03:00).
O webif dele = o digest auth (o curl precisa de `--digest`).

## O que o oscam dele realmente faz (o conf + o log ao vivo)

| Peça | Valor real |
|---|---|
| Readers | `cccam-virgi` (virgiliosantos, 1814+1802 — o **cartão 000007**), `Marreta` (78.111.73.14, 1814+1802), `virgilio-alemaes` (bento44: 098D/1830/1843), `cccam-vps15000` (lê o multics), `multics-mgcamd` (desligado) |
| **Cacheex** | reader `cacheexcastptvps` — protocolo **cs378x** → `130.255.78.45,7777` = o **multics1** (não o principal!), user local1/teste1, `ident = 1814:005211,000007`, lb_weight 150 |
| Contas | dvbapi local + testee3hd, teste12, vps, caspt2026 (a VU+4K dele) |
| Webif | 8888, user/pass `caspt1974*` (o `*` = literal), digest auth |

**Fluxo:** as linhas quase-directas → oscam → cs378x cacheex → **multics1
(7777)** → multics principal → malha → nós. O oscam também **serve o ident
000007** (o `ident = 1814:005211,000007` no reader cacheex) — o lixo do 000007
entra na malha por aqui.

## Benchmark (o log ao vivo, 30/09)

```
(ecm) vps (1814@005211/.../13AC): found (56 ms)  by cccam-virgi   ← SIC Notícias
(ecm) vps (1814@000007/.../0450): found (42 ms)  by cccam-virgi   ← TVI ID_SAT
(ecm) vps (1814@000007/.../0450): found (1 ms)   by cacheexcastptvps ← a CW veio do multics1
```

- A fonte dele = **42ms** com a cadência perfeita de 15s; nós recebemos a
  ~1000ms (o found med 827-1038ms na VU) — os ~960ms = a latência do circuito
  (multics1 → multics → VU + o cacheex). É o alvo das próximas optimizações.
- O oscam dele = sob ataque constante de um scanner (85.138.253.129) — só
  ruído, nada entra.
- O log persistente do oscam dele = partido (`logfile = emu/oscam/oscam.log`
  relativo + o daemon `-b`) — a visão = o webif/live log.

## Implicações para o nosso lado

1. **A5 (o CSP) = fora** — o oscam dele fala cs378x, que nós já temos. O peer
   directo dele→nós = quando o colega quiser (ele recusou por agora).
2. **O 000007 = o cccam-virgi** (o cartão do colega) — a nossa decisão de o
   filtrar = validada.
3. As implementações pendentes (o cycletime nos pushes, o CAK7, o CWPK) —
   ver `IMPLEMENTACOES_STANDBY.md`.

# O estudo do 000007 (30/09) — as conclusões que sustentam a v1.47

## Pergunta 1: o 000007 é só para certos canais?

**Não.** Os 70 SIDs do 000007 aparecem TODOS também no 005211/000000 (zero
exclusivos). O 000007 é o **ident alternativo dos mesmos canais** — mas com
**scrambling independente**.

## Pergunta 2: há canais que pedem ambos os idents?

A nível do **canal** sim (70/70); a nível do **receptor** não — cada PMT tem um
ident (a TVI da nossa VU = só 005211 no PMT; a TVI do colega = o 000007).

## Pergunta 3: é o problema dos freezes?

**Sim** — dois achados:

1. **As streams são independentes**: o Jaccard por janela de 1h entre o 000007
   e o 005211 = **0.009** (histórico) / **0.014** (dia 1 do teste com as shares
   abertas) ≈ 0. As chaves nunca coincidem (o controlo SID-diferente = 0/184).
2. **O multics cruzava as caches entre providers do mesmo SID**: o
   `cache_fetch_samechannel` ("same channel with different hash and provider")
   tinha o check do provid **comentado**; o `cache_check_cw`/`cache_check_samecw`
   não comparavam o provid. A chave do 000007 ia parar à cache do 005211 (e
   vice-versa) = chave errada = freeze. As 37 "meias-chaves" coincidentes nos
   dados (impossíveis com as streams independentes) = a assinatura da mistura.

## O ident 1814:000000

O mais activo (404 SIDs). É o **wildcard/prov 0** — os pedidos sem provider
específico. Também tem chaves próprias (Jaccard ≈ 0.01 vs os outros). No
filtro v1.47 o 0 mantém-se como wildcard (o cross só é barrado quando AMBOS os
providers são não-zero e diferentes).

## O teste de 1 dia (30/09 00:55 → 01/10)

- Shares abertas (o perfil, o CACHE PEER, o cs378x/camd35) + o multics
  reiniciado.
- 709 pushes/linhas do 000007 no dia (o colega a pedir via a malha) — cadência
  esporádica (os medgaps 105-8900s = os pedidos on-demand, não a stream).
- Zero eventos B8 (o "ident marcado como mau") — o 000007 não dispara o filtro
  de anomalias.
- O teste na VU: o PMT da TVI = só 005211 (o I:/P: à força = o ecrã preto, a
  revertido). O oscam da VU estava crashado desde 28/09 — o reiniciado.

## O fix (v1.47 — deployado 30/09 23:13)

- `CACHE STRICTPROVID: YES` (default YES) — o isolamento por provider nas
  caches: `cache_fetch`, `cache_fetch_cycle`, `cache_check_cw` (o novo
  `ecm->provid` no chamador), `cache_check_samecw`,
  `cache_fetch_samechannel` (o check reactivado).
- Regra: o cross só quando um dos provid = 0 (o wildcard mantém-se).
