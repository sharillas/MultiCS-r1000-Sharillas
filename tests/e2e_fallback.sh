#!/bin/bash
# Teste FALLBACK CROSS-PROTOCOL: A (proxy) -> B1 (newcamd, primario) + B2 (CCcam, fallback)
# Mata B1 a meio do teste: os pedidos seguintes devem ser servidos pelo B2 via CCcam
set -e
BIN="$1"
LOG="$2"

mkdir -p /tmp/e2e/A /tmp/e2e/B1 /tmp/e2e/B2
rm -rf /tmp/e2e/A/* /tmp/e2e/B1/* /tmp/e2e/B2/*

# ---------- B1 (newcamd reader, primario) ----------
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
2600:000000:1FFF:010203060405060F070809180A0B0C21 ; Good Channel
EOF

# ---------- B2 (CCcam server + emulator, fallback) ----------
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

CCCAM PORT: 16400
CCCAM PROFILES: 16011
F: user pass
EOF
cat > /tmp/e2e/B2/Softcam.cfg <<'EOF'
2600:000000:1FFF:010203060405060F070809180A0B0C21 ; Good Channel (via CCcam)
EOF

# ---------- A (proxy com FALLBACK) ----------
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
ENABLE FALLBACK: 1
FALLBACK ORDER: NEWCAMD CCCAM
FALLBACK TIMEOUT: 800

N: 127.0.0.1 16001 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14
C: 127.0.0.1 16400 user pass
EOF

pkill -f 'multics.x64 -C /tmp/e2e' 2>/dev/null || true
pkill -x our.x64 2>/dev/null || true
pkill -x fakecsp 2>/dev/null || true
pkill -x fakeclient 2>/dev/null || true
sleep 1

cd /tmp/e2e/B1
nohup sh -c '"$1" -v -C /tmp/e2e/B1/multics.cfg 2>&1 | cat > /tmp/e2e/B1.log' sh "$BIN" &
cd /tmp/e2e/B2
nohup sh -c '"$1" -v -C /tmp/e2e/B2/multics.cfg 2>&1 | cat > /tmp/e2e/B2.log' sh "$BIN" &
sleep 3

cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A.log' sh "$BIN" &
sleep 6

echo "== cliente fake (background) -> A:15050 =="
/tmp/e2e/fakeclient 127.0.0.1 15050 seq > "$LOG" 2>&1 &
CLIENTPID=$!

# deixar os primeiros pedidos irem ao primario (newcamd) e depois MATAR o B1
sleep 18
echo "== a matar B1 (primario) =="
pkill -f '/tmp/e2e/B1' 2>/dev/null || true
sleep 1
ps aux | grep -e '/tmp/e2e/B1' | grep -v grep | head -3 || echo "B1 morto"

wait $CLIENTPID

echo "== cliente =="
grep -a -e SUCCESS -e SEM "$LOG"
echo "== A.log: envios por protocolo =="
grep -a -e "ecm to Newcamd" -e "ecm to CCcam" -e "fallback" /tmp/e2e/A.log | head -30
echo "== B2.log (CCcam client login) =="
grep -a -e cccam -e CCcam -e login -e ecm -e dcw /tmp/e2e/B2.log | head -15
