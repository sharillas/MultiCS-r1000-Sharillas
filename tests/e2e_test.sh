#!/bin/bash
# Teste end-to-end: A (server) -> B (reader com emulator BISS) + cliente fake
set -e
BIN="$1"   # caminho do binario a testar
OUT="$2"   # ficheiro de log do cliente

mkdir -p /tmp/e2e/A /tmp/e2e/B
rm -rf /tmp/e2e/A/* /tmp/e2e/B/*

# ---------- B (reader) ----------
cat > /tmp/e2e/B/multics.cfg <<'EOF'
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

CONSTCW FILE: /tmp/e2e/B/Softcam.cfg
EOF

cat > /tmp/e2e/B/Softcam.cfg <<'EOF'
# Test BISS
2600:000000:1FFF:010203060405060F070809180A0B0C21 ; Test Channel
EOF

# ---------- A (server) ----------
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

N: 127.0.0.1 16001 user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14
EOF

pkill -f 'multics.x64 -C /tmp/e2e' 2>/dev/null || true
sleep 1

# B
cd /tmp/e2e/B
nohup sh -c '"$1" -v -C /tmp/e2e/B/multics.cfg 2>&1 | cat > /tmp/e2e/B.log' sh "$BIN" &
sleep 3

# A
cd /tmp/e2e/A
nohup sh -c '"$1" -v -C /tmp/e2e/A/multics.cfg 2>&1 | cat > /tmp/e2e/A.log' sh "$BIN" &
sleep 4

echo "== processos =="
pgrep -af "/tmp/e2e/" || true

echo "== cliente fake -> A:15050 =="
/tmp/e2e/fakeclient 127.0.0.1 15050 | tee "$OUT"

echo "== logs A (linhas relevantes) =="
grep -aE "ecm|server|login|DCW|decode|emu" /tmp/e2e/A.log | head -20
echo "== logs B =="
grep -aE "ecm|server|login|DCW|decode|emu" /tmp/e2e/B.log | head -20
