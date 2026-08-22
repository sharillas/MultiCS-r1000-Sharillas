#!/bin/bash
# Teste TIMING BUDGET: A (proxy, TIMING ativo) -> fakecsp (CW muda a cada 10s,
# silencio apos 8 pedidos)
# Esperado: o cryptoperiod e aprendido (~10-12s) e os decode failed passam a
# chegar ao cliente dentro do budget (period*fraction) e nao do DCW TIMEOUT (8s)
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
CACHE ADAPTIVETTL: 1

[TEST]
PORT: 15050
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 8000
USER: testuser testpass
ENABLE CACHE: 1
ENABLE TIMING: 1
TIMING FRACTION: 50
TIMING MINPERIOD: 3000
EOF

pkill -f 'multics.x64 -C /tmp/e2e' 2>/dev/null || true
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
pkill -x fakeclient 2>/dev/null || true
sleep 1

cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A.log' sh "$BIN" &
sleep 4

nohup /tmp/e2e/fakecsp 16200 timing > /tmp/e2e/fakecsp.log 2>&1 &
sleep 6

echo "== cliente fake (12 pedidos) -> A:15050 =="
/tmp/e2e/fakeclient 127.0.0.1 15050 seq | tee "$LOG"

echo "== A.log (timing) =="
grep -a -e "timing:" -e "decode failed" /tmp/e2e/A.log | head -30
echo "== fakecsp log =="
cat /tmp/e2e/fakecsp.log
