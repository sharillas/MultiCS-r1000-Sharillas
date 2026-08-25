#!/bin/bash
# MultiCS r1000 v1.20 - instalador VPS (Debian/Ubuntu)
# Uso: sudo bash install.sh [/caminho/instalacao]
DIR=${1:-/opt/multics}
if [ "$(id -u)" -ne 0 ]; then echo "executa como root"; exit 1; fi
mkdir -p $DIR/configs /var/etc
cp bin/multics.x64 bin/multics.x32 $DIR/
cp bin/tools_update_*.py $DIR/ 2>/dev/null
cp configs/* $DIR/configs/
cp configs/* /var/etc/
chmod 755 $DIR/multics.x64 $DIR/multics.x32 $DIR/tools_update_*.py
chmod 666 /var/etc/*.cfg /var/etc/CCcam.* /var/etc/ip2country.csv /var/etc/multics.css 2>/dev/null
# servico systemd a correr como ROOT (a GUI salva/uploads/restart sem problemas
# de permissoes mesmo que a comunidade mude os ficheiros para 755)
cat > /etc/systemd/system/multics.service <<EOF
[Unit]
Description=MultiCS r1000 by Sharillas (cardserver proxy)
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=$DIR
ExecStart=$DIR/multics.x64 -C /var/etc/multics.cfg
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
systemctl daemon-reload
systemctl enable multics 2>/dev/null
# firewall: abrir TODAS as portas de partilha (bloquear depois o que nao se quiser)
if command -v ufw >/dev/null 2>&1; then
  ufw allow 5500/tcp       # GUI
  ufw allow 16000/tcp      # CCcam (F-lines)
  ufw allow 16001/tcp      # CCcam3 (boxes CCcam3)
  ufw allow 21000/tcp      # MGcamd
  ufw allow 8600/tcp       # cs378x
  ufw allow 7502/udp       # camd35
  ufw allow 7502/tcp
  ufw allow 5599/udp       # cache
  ufw allow 5599/tcp
  ufw allow 15000:15025/tcp  # perfis newcamd
fi
# TELNET (5600) fica FECHADO por seguranca (gestao em texto puro).
# Se quiseres abrir: ufw allow 5600/tcp
echo "Instalado em $DIR (configs em /var/etc, tools junto do binario)"
echo "Arranque: systemctl start multics"
echo "Web UI:   http://IP:5500  (user/pass definidos em multics.cfg)"
