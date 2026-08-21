#!/bin/bash
# ============================================================
# MultiCS r1000 v1.0.9 by Sharillas - Instalador Universal
# Funciona em: Debian/Ubuntu, CentOS/Rocky/Alma, Arch, Alpine
# (binarios estaticos musl - nao precisam de libs do sistema)
#
# Uso:
#   bash install.sh                    # instala em /opt/multics, cfg em /var/etc
#   bash install.sh /meu/dir           # instala os binarios em /meu/dir
#   HTTP_USER=meuuser HTTP_PASS=senha bash install.sh   # credenciais web
# ============================================================
set -e

BIN_DIR="${1:-/opt/multics}"
CFG_DIR="${CFG_DIR:-/var/etc}"
HTTP_USER="${HTTP_USER:-admin}"
HTTP_PASS="${HTTP_PASS:-admin}"
PORT_HTTP=5500

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ "$(id -u)" -ne 0 ]; then
  echo "ERRO: executa como root (sudo bash install.sh)"
  exit 1
fi

# ---- escolher binario conforme arquitetura ----
ARCH="$(uname -m)"
case "$ARCH" in
  x86_64|amd64) BIN="$SCRIPT_DIR/build/multics.x64" ;;
  i386|i486|i586|i686) BIN="$SCRIPT_DIR/build/multics.x32" ;;
  *) echo "ERRO: arquitetura $ARCH nao suportada (x86_64 ou i386)"; exit 1 ;;
esac

if [ ! -f "$BIN" ]; then
  echo "ERRO: binario nao encontrado em $BIN"
  echo "Clona o repo completo: git clone https://github.com/sharillas/MultiCS-r1000-Sharillas.git"
  exit 1
fi

echo "== MultiCS r1000 v1.0.9 - instalacao =="
echo "  binarios -> $BIN_DIR"
echo "  configs  -> $CFG_DIR"
echo "  web UI   -> porta $PORT_HTTP (user: $HTTP_USER / pass: $HTTP_PASS)"

# ---- binarios ----
mkdir -p "$BIN_DIR"
install -m 755 "$SCRIPT_DIR/build/multics.x64" "$BIN_DIR/multics.x64" 2>/dev/null || cp -f "$SCRIPT_DIR/build/multics.x64" "$BIN_DIR/multics.x64"
install -m 755 "$SCRIPT_DIR/build/multics.x32" "$BIN_DIR/multics.x32" 2>/dev/null || cp -f "$SCRIPT_DIR/build/multics.x32" "$BIN_DIR/multics.x32"

# ---- configs (NUNCA sobrescrever os existentes) ----
mkdir -p "$CFG_DIR"
BACKUP_TS=$(date +%Y%m%d_%H%M%S)
FIRST_INSTALL=1
if [ -f "$CFG_DIR/multics.cfg" ]; then FIRST_INSTALL=0; fi
if [ "$FIRST_INSTALL" = "0" ]; then
  # instalacao anterior detectada: copia apenas os ficheiros em falta
  for f in "$SCRIPT_DIR"/configs_exemplos/*; do
    base=$(basename "$f")
    if [ ! -e "$CFG_DIR/$base" ]; then
      cp -f "$f" "$CFG_DIR/"
      echo "  novo: $base"
    fi
  done
else
  # primeira instalacao: copia tudo
  cp -f "$SCRIPT_DIR"/configs_exemplos/* "$CFG_DIR/" 2>/dev/null || true
fi
chmod -R 775 "$CFG_DIR"

# ---- credenciais web ----
if [ -n "$HTTP_USER" ]; then
  sed -i "s/^HTTP USER:.*/HTTP USER: $HTTP_USER/" "$CFG_DIR/multics.cfg" 2>/dev/null || true
  sed -i "s/^HTTP PASS:.*/HTTP PASS: $HTTP_PASS/" "$CFG_DIR/multics.cfg" 2>/dev/null || true
fi

# ---- firewall (se ufw ou firewalld) ----
if command -v ufw >/dev/null 2>&1; then
  ufw allow $PORT_HTTP/tcp >/dev/null 2>&1 || true
  echo "  ufw: porta $PORT_HTTP aberta"
fi
if command -v firewall-cmd >/dev/null 2>&1; then
  firewall-cmd --permanent --add-port=$PORT_HTTP/tcp >/dev/null 2>&1 || true
  firewall-cmd --reload >/dev/null 2>&1 || true
  echo "  firewalld: porta $PORT_HTTP aberta"
fi

# ---- servico ----
if command -v systemctl >/dev/null 2>&1 && [ -d /run/systemd/system ]; then
  echo "== systemd detectado =="
  cat > /etc/systemd/system/multics.service <<EOF
[Unit]
Description=MultiCS r1000 by Sharillas (cardserver proxy)
After=network.target

[Service]
Type=simple
ExecStart=$BIN_DIR/multics.x64 -C $CFG_DIR/multics.cfg
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
  systemctl daemon-reload
  systemctl enable multics >/dev/null 2>&1 || true
  systemctl restart multics
  echo "  servico: systemctl start/stop/status multics"
elif [ -d /etc/init.d ]; then
  echo "== init.d detectado =="
  cat > /etc/init.d/multics <<EOF
#!/bin/sh
### BEGIN INIT INFO
# Provides:          multics
# Required-Start:    \$network \$remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
### END INIT INFO
BIN=$BIN_DIR/multics.x64
CFG=$CFG_DIR/multics.cfg
case "\$1" in
  start)  nohup \$BIN -C \$CFG >/var/log/multics.log 2>&1 & echo "multics started" ;;
  stop)   pkill -x multics.x64 || true ;;
  restart) \$0 stop; sleep 1; \$0 start ;;
  status) pgrep -x multics.x64 >/dev/null && echo "running" || echo "stopped" ;;
esac
exit 0
EOF
  chmod +x /etc/init.d/multics
  if command -v update-rc.d >/dev/null 2>&1; then update-rc.d multics defaults >/dev/null 2>&1 || true; fi
  if command -v chkconfig >/dev/null 2>&1; then chkconfig --add multics >/dev/null 2>&1 || true; fi
  /etc/init.d/multics restart || true
  echo "  servico: /etc/init.d/multics start|stop|status"
else
  echo "== sem systemd/init.d: fallback crontab @reboot =="
  ( crontab -l 2>/dev/null | grep -v multics.x64; echo "@reboot $BIN_DIR/multics.x64 -C $CFG_DIR/multics.cfg" ) | crontab -
  nohup "$BIN_DIR/multics.x64" -C "$CFG_DIR/multics.cfg" >/var/log/multics.log 2>&1 &
  echo "  arranque via crontab @reboot"
fi

# ---- smoke test ----
sleep 3
if curl -s -o /dev/null http://127.0.0.1:$PORT_HTTP/login 2>/dev/null; then
  echo "== OK: web UI a responder em http://SEU_IP:$PORT_HTTP =="
else
  echo "== ATENCAO: web UI ainda sem resposta (aguarda 2-3s no primeiro arranque) =="
fi

echo ""
echo "Proximos passos:"
echo "  1. Abre http://SEU_IP:$PORT_HTTP  (login: $HTTP_USER / $HTTP_PASS - MUDA ISTO!)"
echo "  2. Configura readers (N:/C:) e clientes (F:/users) - ve docs/CONFIGS.md"
echo "  3. Logs: journalctl -u multics -f   (ou tail -f /var/log/multics.log)"
echo ""
echo "Portas do cardserver (abre as que precisares na firewall):"
echo "  HTTP 5500 | CCcam 16000 | MGcamd 21000 | Newcamd profiles (15001+)"
echo "  camd35 7502 | cs378x 8600 | cache 5599"
