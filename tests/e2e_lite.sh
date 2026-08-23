#!/bin/bash
# Teste BUILD LITE + ENABLE EMULATOR BISS
# Perfis:
#   NL  (15050) ENABLE LITE: 1  -> so canais da lista CCcam.lite passam
#   EO  (15051) ENABLE EMULATOR BISS: 0 -> constcw ignorado -> SEM
#   E1  (15052) ENABLE EMULATOR BISS: 1 -> constcw responde -> SUCCESS
# Esperado:
#   NL sid 1fff (na lista)  -> SUCCESS (via CSP)
#   NL sid 2abc (fora)      -> "Ignored (lite)" -> SEM
#   NL sid 2b01 (FFFE wild) -> SUCCESS (via CSP)
#   EO sid 1fff             -> SEM (emulador desligado)
#   E1 sid 1fff             -> SUCCESS (constcw)
set -e
BIN="$1"
LOG="$2"

mkdir -p /tmp/e2e/A
rm -rf /tmp/e2e/A/*

cat > /tmp/e2e/A/multics.cfg <<'EOF'
LOGLEVEL: 4
HTTP PORT: 6501
HTTP USER: admin
HTTP PASS: admin

CACHE PORT: 16100
CACHE PEER: 127.0.0.1 16200
CACHE ALIVETIME: 30
CACHE TIMEOUT: 500
CACHE FILTER: 0
CACHE THRESHOLD: 1

CONSTCW FILE: /tmp/e2e/A/Softcam.cfg
LITE FILE: /tmp/e2e/A/CCcam.lite

[NL]
PORT: 15050
CAID: 1813
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
ENABLE CACHE: 1
ENABLE LITE: 1

[EO]
PORT: 15051
CAID: 1813
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
ENABLE CACHE: 0
ENABLE EMULATOR BISS: 0

[E1]
PORT: 15052
CAID: 1813
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
ENABLE CACHE: 0
ENABLE EMULATOR BISS: 1
EOF

cat > /tmp/e2e/A/CCcam.lite <<'EOF'
# lista de canais activos
1813:000000:1fff
FFFE:000000:2b01
EOF

cat > /tmp/e2e/A/Softcam.cfg <<'EOF'
# constcw de teste (sid 3ccc - usado so nos perfis EO/E1)
1813:000000:3ccc: 01020306040506071011121314151617
EOF

pkill -f 'multics.x64 -C /tmp/e2e' 2>/dev/null || true
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
pkill -x fakeclient 2>/dev/null || true
sleep 1

nohup /tmp/e2e/fakecsp 16200 normal 1813 > /tmp/e2e/fakecsp.log 2>&1 &
sleep 1

cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A.log' sh "$BIN" &
sleep 5

echo "== 1) NL: sid 1fff (na lista) -> esperado SUCCESS (CSP) =="
timeout 25 /tmp/e2e/fakeclient 127.0.0.1 15050 single 1813 1fff | grep -e DCW -e SEM
sleep 3

echo "== 2) NL: sid 2abc (fora da lista) -> esperado SEM (Ignored lite) =="
timeout 25 /tmp/e2e/fakeclient 127.0.0.1 15050 single 1813 2abc | grep -e DCW -e SEM
sleep 3

echo "== 3) NL: sid 2b01 (FFFE wildcard) -> esperado SUCCESS (CSP) =="
timeout 25 /tmp/e2e/fakeclient 127.0.0.1 15050 single 1813 2b01 | grep -e DCW -e SEM
sleep 3

echo "== 4) EO: emulador OFF -> esperado SEM =="
timeout 25 /tmp/e2e/fakeclient 127.0.0.1 15051 single 1813 3ccc | grep -e DCW -e SEM
sleep 3

echo "== 5) E1: emulador ON (constcw) -> esperado SUCCESS =="
timeout 25 /tmp/e2e/fakeclient 127.0.0.1 15052 single 1813 3ccc | grep -e DCW -e SEM

echo "== A.log (lite/emu) =="
pkill -x our.x64 2>/dev/null || true
sleep 1
grep -a -e "lite" -e "Ignored" -e "decode failed" /tmp/e2e/A.log | head -20
pkill -x fakecsp 2>/dev/null || true
