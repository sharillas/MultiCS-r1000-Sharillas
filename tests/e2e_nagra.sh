#!/bin/bash
# Teste NAGRA PROTECTION: A (proxy, perfil CAID 1810 + ENABLE NAGRA) -> fakecsp nagra
# Esperado:
#   - CW com checksum errado -> "nagra: bad checksum" + "dcw rejected (code 2) ... (drop)" -> cliente SEM
#   - CWs normais -> OK (aprendizagem do ciclo, 6 amostras)
#   - CW too similar (>=4 bytes iguais numa metade) -> "too similar" code 6 -> SEM
#   - CW fake half (so uma metade muda) -> "fake half" code 4 -> SEM
#   - CWs normais de novo -> OK
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

[NAG]
PORT: 15050
CAID: 1813
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
ENABLE CACHE: 1
ENABLE NAGRA: 1
NAGRA SENSITIVE: 4
EOF

pkill -f 'multics.x64 -C /tmp/e2e' 2>/dev/null || true
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
pkill -x fakeclient 2>/dev/null || true
sleep 1

nohup /tmp/e2e/fakecsp 16200 nagra > /tmp/e2e/fakecsp.log 2>&1 &
sleep 1

cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A.log' sh "$BIN" &
sleep 6

echo "== cliente fake (12 pedidos, caid 1810) -> A:15050 =="
/tmp/e2e/fakeclient 127.0.0.1 15050 seq 1813 | tee "$LOG"

echo "== A.log (nagra) =="
grep -a -e "nagra" /tmp/e2e/A.log | head -25
echo "== fakecsp log =="
cat /tmp/e2e/fakecsp.log | head -14
