# Testes end-to-end (VPS)

Ferramentas de teste do MultiCS r1000. São compiladas na VPS com gcc
(não fazem parte do build do multics).

## Ficheiros

- `fakeclient.c` — cliente newcamd fake (handshake DES completo).
  Modos: `fakeclient HOST PORT` (1 pedido), `... seq` (14 pedidos),
  `... seqreplay` (12 pedidos com replay de hash no pedido 9).
  Compilar: `gcc -O2 -std=gnu90 -o fakeclient fakeclient.c des.c md5.c`
  (usar o des.c/md5.c do src).
- `fakecsp.c` — servidor CSP fake (UDP): responde a PINGREQ e
  TYPE_REQUEST com CWs scriptadas (ciclo de meias-CWs + bad cycles).
  Modos: `fakecsp PORT` (ciclo + bad no idx 9), `fakecsp PORT replay`
  (8 ciclos validos e depois bad cycles).
- `e2e_test.sh` — teste base: A (proxy) -> B (reader com emulator
  constcw) + fakeclient. Uso: `bash e2e_test.sh <binario> <log>`.
- `e2e_fallback.sh` — teste FALLBACK CROSS-PROTOCOL: A com primario
  newcamd (B1) + fallback CCcam (B2). Mata B1 a meio: os pedidos
  seguintes devem ser servidos via CCcam ("-> ecm to CCcam server").
  Uso: `bash e2e_fallback.sh <binario> <log>`.
- `cc_test.c` — cliente TCP cru para testar o handshake CCcam (16 bytes
  de seed). Compilar: `gcc -O2 -o cc_test cc_test.c`.
- `e2e_timing.sh` — teste TIMING BUDGET: A com TIMING ativo + fakecsp
  em modo `timing` (CW muda a cada 10s, silencio apos 8 pedidos).
  Esperado: "timing: ch ... period=..." (cryptoperiod aprendido) e os
  decode failed passam a chegar dentro do budget (period*fraction) e
  nao do DCW TIMEOUT. Uso: `bash e2e_timing.sh <binario> <log>`.
- `e2e_health.sh` — teste HEALTH SCORING: A (proxy) com 2 readers
  newcamd (B1 bom + B2 com CW de checksum errado). Esperado: depois
  de amostras o server mau cai abaixo de DROPOFF e e excluido
  (`health=... abaixo de dropoff`), o bom assume todos os pedidos.
  Uso: `bash e2e_health.sh <binario> <log>`.
- `e2e_cwc.sh` — teste CWC (2 fases):
  - Fase 1: bad CW cycle -> espera-se "cwc: bad CW cycle -> DROP"
  - Fase 2: old ECM replay -> espera-se "cwc: old ECM (replay) -> DROP"
  Uso: `bash e2e_cwc.sh <binario> <log>`.
- `fake_client.py` — versao python antiga (login sem handshake, obsoleta).

## Notas importantes descobertas durante o desenvolvimento

1. A KEY do config e HEX: `KEY: 01 02 ... 10 11` = bytes 0x01..0x11
   (nao decimais).
2. ECM tem de ter >= 20 bytes (accept_ecmlen) e o header de tamanho
   embutido (byte1/byte2) quando CHECK ECM LENGTH esta ativo.
3. DCW tem estrutura de checksum: bytes 3,7,11,15 = soma dos 3
   anteriores (acceptDCW valida).
4. CSP: o peer so recebe pedidos depois de online (PINGRPL) E de
   enviar CARD_LIST com caprov que intersecte um perfil local.
   Bug corrigido no multics: com RTT sub-milissegundo o peer nunca
   entrava na peerReq list (ipeer_update antes do ping++).
5. CACHE FILTER: 1 (default) bloqueia DCWs de CSP sem cwcycle
   (NO_CYCLE) — para CSP simples usar CACHE FILTER: 0.
6. Bug corrigido no multics: num bad DCW de um reader newcamd/radegast/
   camd35/cs378x, o flag ECM_SRV_REQUEST nunca era mudado para
   REPLY_FAIL -> com SERVER MAX >= 1 o ECM nunca expirava e o cliente
   pendurava (decode failed nunca chegava).
7. O servidor CCcam do multics configura-se ao NIVEL TOPO (fora de
   seccoes): `CCCAM PORT: 16400` + `CCCAM PROFILES: 16011` +
   `F: user pass`. Qualquer `[nome]` no config cria um PERFIL newcamd -
   nao usar `[CCcam]` para o servidor CCcam!
8. TIMING: o timeout adaptativo so se aplica quando os checktimes do
   ECM (th-ecm) usam a mesma funcao dcwtimeout() - senao o ECM acorda
   no timeout total e o budget nunca e avaliado.
