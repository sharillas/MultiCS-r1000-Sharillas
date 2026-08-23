#!/bin/bash
# Teste DCW MINTIME + DCW CYCLE_CHECK (por perfil)
# fakecsp modo "nds": cw muda a cada 2s no padrao A B C D E F G
#   A=ambas | B=half0 | C=half1 | D=half1 (repetida) | E=half0 | F=half0 (repetida) | G=half1
# Perfis:
#   MT (15050) DCW MINTIME: 3000  -> mudancas < 3s sao rejeitadas
#   CC (15051) DCW CYCLE_CHECK: 1 -> a metade que muda tem de alternar
# Esperado (cliente fast: 10 pedidos a cada 2s):
#   MT: r0 S | r1 B(2s<3s) SEM | r2 C(4s) S | r3 D SEM | r4 E S | r5 F SEM | r6 G S | ...
#   CC: r0 A S | r1 B(half0) S | r2 C(half1) S | r3 D(half1 rep) SEM | r4 E(half0) S | r5 F(half0 rep) SEM | r6 G S
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

[MT]
PORT: 15050
CAID: 1813
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 1000
USER: testuser testpass
ENABLE CACHE: 1
DCW MINTIME: 3000

[CC]
PORT: 15051
CAID: 1813
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 1000
USER: testuser testpass
ENABLE CACHE: 1
DCW CYCLE_CHECK: 1
EOF

pkill -f 'multics.x64 -C /tmp/e2e' 2>/dev/null || true
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
pkill -x fakeclient 2>/dev/null || true
sleep 1

nohup /tmp/e2e/fakecsp 16200 nds > /tmp/e2e/fakecsp.log 2>&1 &
sleep 1

cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A.log' sh "$BIN" &
sleep 5

echo "== MT: DCW MINTIME 3000 (esperado S SEM S SEM S SEM S ...) =="
timeout 60 /tmp/e2e/fakeclient 127.0.0.1 15050 fast 1813 1fff | grep -e DCW -e SEM | sed 's/^/  MT /'
sleep 4

echo "== CC: DCW CYCLE_CHECK (esperado S S SEM S SEM S SEM S S S) =="
timeout 60 /tmp/e2e/fakeclient 127.0.0.1 15051 fast 1813 1fff 40 | grep -e DCW -e SEM | sed 's/^/  CC /'

echo "== A.log (mintime/cycle_check) =="
pkill -x our.x64 2>/dev/null || true
sleep 1
grep -a -e "mintime" -e "cycle_check" /tmp/e2e/A.log | head -14
pkill -x fakecsp 2>/dev/null || true
