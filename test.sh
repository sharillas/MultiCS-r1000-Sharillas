#!/bin/bash
# MultiCS r1000 - smoke test local (correr DENTRO do WSL)
# Uso: bash test.sh [caminho_para_build] [caminho_para_Configs]
BUILD_DIR=${1:-/mnt/c/Users/smartvideodesktop01/Documents/MultiCS_Sharillas/build}
CFG_DIR=${2:-/mnt/c/Users/smartvideodesktop01/Documents/MultiCS_Sharillas/Configs}

echo "== copiar binarios e configs para /tmp/multics-test =="
rm -rf /tmp/multics-test
mkdir -p /tmp/multics-test/configs
cp $BUILD_DIR/multics.x64 /tmp/multics-test/
cp $BUILD_DIR/multics.x32 /tmp/multics-test/
cp $CFG_DIR/*.cfg $CFG_DIR/*.css $CFG_DIR/*.csv /tmp/multics-test/configs/ 2>/dev/null
mkdir -p /var/etc 2>/dev/null
cp $CFG_DIR/*.cfg $CFG_DIR/*.css $CFG_DIR/*.csv /var/etc/ 2>/dev/null || true
chmod +x /tmp/multics-test/multics.x64

echo "== arrancar multics.x64 em background =="
cd /tmp/multics-test
./multics.x64 -b -C /var/etc/multics.cfg
sleep 2

echo "== processo =="
ps aux | grep multics | grep -v grep

echo "== HTTP check (porta 5500) =="
curl -s -o /dev/null -w "HTTP %{http_code}\n" http://127.0.0.1:5500/ || echo "HTTP FALHOU"

echo "== log (se existir) =="
ls -la /tmp/multics-test/ 2>/dev/null | head -20

echo "== parar =="
pkill -f multics.x64 || true
echo "== feito =="
