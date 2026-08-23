#!/bin/bash
# Teste do FILTRO EMBUTIDO: fake cw com ultimo byte XOR 0xF0
# fakecsp modo xorf0: valido, FAKE, valido, valido
# Esperado: S SEM S S (o fake e rejeitado e o ECM espera o proximo valido)
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
pkill -x fakeclient 2>/dev/null || true
sleep 1
mkdir -p /tmp/e2e/X
rm -rf /tmp/e2e/X/*
cat > /tmp/e2e/X/multics.cfg <<'EOF'
LOGLEVEL: 4
HTTP PORT: 6505
HTTP USER: admin
HTTP PASS: admin

CACHE PORT: 16105
CACHE PEER: 127.0.0.1 16205
CACHE ALIVETIME: 30
CACHE TIMEOUT: 500
CACHE FILTER: 0
CACHE THRESHOLD: 1

[XF]
PORT: 15080
CAID: 1813
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 1000
USER: testuser testpass
ENABLE CACHE: 1
EOF
cd /tmp/e2e/X
nohup /tmp/e2e/fakecsp 16205 xorf0 1813 > /tmp/e2e/fakecsp.log 2>&1 &
sleep 1
nohup script -qefc '/tmp/e2e/our.x64 -v -C /tmp/e2e/X/multics.cfg' /tmp/e2e/X.log > /dev/null 2>&1 &
sleep 5
echo "== cliente (fast: 10 pedidos a cada 2s) =="
timeout 60 /tmp/e2e/fakeclient 127.0.0.1 15080 fast 1813 1fff | grep -e DCW -e SEM
pkill -x our.x64 2>/dev/null || true
sleep 1
echo "== log fake =="
grep -a "fake cw" /tmp/e2e/X.log | head -4
pkill -x fakecsp 2>/dev/null || true
