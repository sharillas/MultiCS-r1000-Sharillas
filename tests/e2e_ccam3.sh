#!/bin/bash
# e2e CCcam3: (1) reader C3: -> fake server ; (2) fake client -> server CCCAM3 PORT
BIN=${1:-/opt/multics/multics.x64}
PASS=0; FAIL=0
ok() { PASS=$((PASS+1)); echo "  OK: $1"; }
bad() { FAIL=$((FAIL+1)); echo "  FAIL: $1"; }

echo "== FASE 1: READER (C3: -> fake cccam3 server) =="
pkill -f "/tmp/e2e/A/multics.cfg" 2>/dev/null
pkill -x fake_ccam3srv 2>/dev/null
pkill -x fakeclient 2>/dev/null
sleep 1
mkdir -p /tmp/e2e/A
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
ENABLE CCCAM: 1

C3: 127.0.0.1 17000 sharillas teste
EOF
rm -f /var/tmp/multics.log /tmp/e2e/fake3.log
nohup /tmp/e2e/fake_ccam3srv 17000 > /tmp/e2e/fake3.log 2>&1 &
sleep 1
cd /tmp/e2e/A
nohup $BIN -v -f -C /tmp/e2e/A/multics.cfg >/dev/null 2>&1 &
sleep 5
/tmp/e2e/fakeclient 127.0.0.1 15050 > /tmp/e2e/c1r.log 2>&1
grep -q SUCCESS /tmp/e2e/c1r.log && ok "cliente recebeu CW via reader CCcam3" || bad "cliente SEM CW via reader CCcam3"
grep -q "ack enviado (RSA_AES)" /tmp/e2e/fake3.log && ok "handshake RSA_AES no reader" || bad "handshake reader falhou"
pkill -f "/tmp/e2e/A/multics.cfg"
pkill -x fake_ccam3srv
sleep 1

echo "== FASE 2: SERVER (fake cliente -> CCCAM3 PORT -> EMU) =="
cat > /tmp/e2e/A/multics.cfg <<'EOF'
LOGLEVEL: 4
HTTP PORT: 6501
HTTP USER: admin
HTTP PASS: admin

CCCAM PORT: 16000
CCCAM3 PORT: 16001
F: cli pass

[BISS]
PORT: 15050
CAID: 2600
PROVIDERS: 000000
KEY: 01 02 03 04 05 06 07 08 09 10 11 12 13 14
USER: testuser testpass

CONSTCW FILE: /tmp/e2e/A/Softcam.cfg
EOF
echo "2600:000000:1FFF:010203060405060F070809180A0B0C21 ; Test" > /tmp/e2e/A/Softcam.cfg
rm -f /var/tmp/multics.log
cd /tmp/e2e/A
nohup $BIN -v -f -C /tmp/e2e/A/multics.cfg >/dev/null 2>&1 &
sleep 5
/tmp/e2e/fake_ccam3cli 127.0.0.1 16001 > /tmp/e2e/c2s.log 2>&1
grep -q "handshake RSA_AES OK" /tmp/e2e/c2s.log && ok "handshake RSA_AES no server" || bad "handshake server falhou"
grep -q "cw msg len=27" /tmp/e2e/c2s.log && ok "CW devolvida ao cliente ccam3" || bad "sem CW no server"
grep -q "=> cw to CCcam3 client" /var/tmp/multics.log && ok "log server: cw enviada" || bad "log server sem cw"
pkill -f "/tmp/e2e/A/multics.cfg"
sleep 1

echo "== RESULTADO: $PASS ok, $FAIL falhas =="
exit $FAIL
