#!/bin/bash
# Teste HEALTH SCORING: A (proxy, HEALTH ativo) -> B1 (bom) + B2 (CW com checksum errado)
# Esperado: no inicio pedidos alternam (alguns falham); apos amostras o server mau
#           cai abaixo de DROPOFF e e excluido -> todos os pedidos passam pelo bom
set -e
BIN="$1"
LOG="$2"

mkdir -p /tmp/e2e/A /tmp/e2e/B1 /tmp/e2e/B2
rm -rf /tmp/e2e/A/* /tmp/e2e/B1/* /tmp/e2e/B2/*

# ---------- B1 (bom: constcw valido) ----------
cat > /tmp/e2e/B1/multics.cfg <<'EOF'
LOGLEVEL: 4
HTTP PORT: 6500
HTTP USER: admin
HTTP PASS: admin

[BISS]
PORT: 16001
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
USER: user pass

CONSTCW FILE: /tmp/e2e/B1/Softcam.cfg
EOF
cat > /tmp/e2e/B1/Softcam.cfg <<'EOF'
# CW com checksum valido
2600:000000:1FFF:010203060405060F070809180A0B0C21 ; Good Channel
EOF

# ---------- B2 (mau: constcw com checksum ERRADO) ----------
cat > /tmp/e2e/B2/multics.cfg <<'EOF'
LOGLEVEL: 4
HTTP PORT: 6502
HTTP USER: admin
HTTP PASS: admin

[BISS]
PORT: 16011
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
USER: user pass

CONSTCW FILE: /tmp/e2e/B2/Softcam.cfg
EOF
cat > /tmp/e2e/B2/Softcam.cfg <<'EOF'
# CW com checksum ERRADO (ultimo byte errado)
2600:000000:1FFF:010203060405060F070809180A0B0C22 ; Bad Channel
EOF

# ---------- A (proxy com HEALTH) ----------
cat > /tmp/e2e/A/multics.cfg <<'EOF'
LOGLEVEL: 4
HTTP PORT: 6501
HTTP USER: admin
HTTP PASS: admin

[TEST]
PORT: 15050
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
SERVER MAX: 1
SERVER INTERVAL: 1000
ENABLE HEALTH: 1
HEALTH WEIGHTS: 40 30 10 20
HEALTH MINECMS: 5
HEALTH DROPOFF: 300

N: 127.0.0.1 16001 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14
N: 127.0.0.1 16011 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14
EOF

pkill -f "multics.x64 -C /tmp/e2e" 2>/dev/null || true
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
sleep 1

cd /tmp/e2e/B1
nohup sh -c '"$1" -v -C /tmp/e2e/B1/multics.cfg 2>&1 | cat > /tmp/e2e/B1.log' sh "$BIN" &
cd /tmp/e2e/B2
nohup sh -c '"$1" -v -C /tmp/e2e/B2/multics.cfg 2>&1 | cat > /tmp/e2e/B2.log' sh "$BIN" &
sleep 3

cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A.log' sh "$BIN" &
sleep 6

echo "== cliente fake -> A:15050 =="
/tmp/e2e/fakeclient 127.0.0.1 15050 seq | tee "$LOG"

echo "== A.log (health) =="
grep -a -e health -e "server (" -e decode /tmp/e2e/A.log | head -60
