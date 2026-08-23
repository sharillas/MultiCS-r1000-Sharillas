#!/bin/bash
# Teste NOK BISS EMU (amarelo): perfil sem chave / emulador OFF / com chave
set -e
mkdir -p /tmp/e2e/B
rm -rf /tmp/e2e/B/*

cat > /tmp/e2e/B/multics.cfg <<'EOF'
LOGLEVEL: 4
HTTP PORT: 6502
HTTP USER: admin
HTTP PASS: admin

CONSTCW FILE: /tmp/e2e/B/Softcam.cfg

[BN]
PORT: 15060
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
ENABLE CACHE: 0

[BO]
PORT: 15061
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
ENABLE CACHE: 0
ENABLE EMULATOR BISS: 0

[BK]
PORT: 15062
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
DCW TIMEOUT: 4000
USER: testuser testpass
ENABLE CACHE: 0
EOF

cat > /tmp/e2e/B/Softcam.cfg <<'EOF'
2600:000000:1fff: 01020306040506071011121314151617
EOF

pkill -x our.x64 2>/dev/null || true
pkill -x fakeclient 2>/dev/null || true
sleep 1

cd /tmp/e2e/B
nohup /tmp/e2e/our.x64 -v -C /tmp/e2e/B/multics.cfg > /tmp/e2e/B.log 2>&1 &
sleep 4

echo "== BN: BISS sem chave (EMU on, sid 2abc) -> esperado SEM imediato =="
timeout 20 /tmp/e2e/fakeclient 127.0.0.1 15060 single 2600 2abc | grep -e DCW -e SEM
sleep 3

echo "== BO: BISS emulador OFF -> esperado SEM imediato =="
timeout 20 /tmp/e2e/fakeclient 127.0.0.1 15061 single 2600 1fff | grep -e DCW -e SEM
sleep 3

echo "== BK: BISS com chave -> esperado SUCCESS =="
timeout 20 /tmp/e2e/fakeclient 127.0.0.1 15062 single 2600 1fff | grep -e DCW -e SEM

pkill -x our.x64 2>/dev/null || true
sleep 1
echo "== B.log =="
grep -a -e "BISS EMU NOK" -e "decode failed" /tmp/e2e/B.log | head -8
