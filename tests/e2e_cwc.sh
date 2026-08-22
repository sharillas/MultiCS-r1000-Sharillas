#!/bin/bash
# Teste CWC (CW Cycle Check): A (proxy, CWC ativo) -> fakecsp (CWs scriptadas)
# Esperado: DCWs OK durante aprendizagem/ciclos validos,
#           DROP em bad cycle (req ~9) e old ECM (req ~11)
set -e
BIN="$1"
LOG="$2"

mkdir -p /tmp/e2e/A
rm -rf /tmp/e2e/A/*

write_cfg() {
cat > /tmp/e2e/A/multics.cfg <<'EOF'
LOGLEVEL: 4
HTTP PORT: 6501
HTTP USER: admin
HTTP PASS: admin

CACHE PORT: 16100
CACHE PEER: 127.0.0.1 16200
CACHE ALIVETIME: 15
CACHE TIMEOUT: 500
CACHE FILTER: 0
CACHE THRESHOLD: 1

[TEST]
PORT: 15050
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
ENABLE CACHE: 1
ENABLE CWC: 1
CWC SENSITIVE: 3
CWC DROPOLD: 1
CWC DROPBAD: 1
CWC KEEPCYCLETIME: 5
EOF
}

write_cfg

pkill -f "multics.x64 -C /tmp/e2e" 2>/dev/null || true
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
sleep 1

# A
cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A.log' sh "$BIN" &
sleep 4

# fakecsp
nohup /tmp/e2e/fakecsp 16200 > /tmp/e2e/fakecsp.log 2>&1 &
sleep 6

echo "== cliente fake (sequencia) -> A:15050 =="
/tmp/e2e/fakeclient 127.0.0.1 15050 seq | tee "$LOG"

echo "== fakecsp log =="
cat /tmp/e2e/fakecsp.log
echo "== A.log (cwc/ecm) =="
grep -a -e cwc -e "decode" -e "stage" /tmp/e2e/A.log

# ---------------- FASE 2: REPLAY ----------------
echo "===== FASE 2: REPLAY ====="
pkill -f "multics.x64 -C /tmp/e2e" 2>/dev/null || true
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
sleep 1
rm -rf /tmp/e2e/A/*
write_cfg

cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A2.log' sh "$BIN" &
sleep 4
nohup /tmp/e2e/fakecsp 16200 replay > /tmp/e2e/fakecsp2.log 2>&1 &
sleep 6

/tmp/e2e/fakeclient 127.0.0.1 15050 seqreplay | tee "${LOG}.replay"

echo "== fakecsp2 log =="
cat /tmp/e2e/fakecsp2.log
echo "== A2.log (cwc) =="
grep -a -e cwc -e DROP -e stage /tmp/e2e/A2.log
