#!/bin/bash
# ============================================================
# MultiCS r1000 v1.26 by Sharillas - INSTALADOR (VPS Debian/Ubuntu)
#
# NAO PRECISA DE GIT - este script copia so o binario e as
# configs de exemplo para a VPS.
#
# USO (na VPS):
#   1. envia o pacote:  scp multics-r1000-v1.26.tar.gz root@IP:~
#   2. na VPS:          tar xzf multics-r1000-v1.26.tar.gz
#   3.                  cd multics-r1000
#   4.                  sudo bash install.sh
#
# USO (com a pasta ja extraida):
#   sudo bash install.sh [/caminho/instalacao]
# ============================================================
DIR=${1:-/opt/multics}

if [ "$(id -u)" -ne 0 ]; then echo "executa como root: sudo bash install.sh"; exit 1; fi
if [ ! -f bin/multics.x64 ]; then
  echo "ERRO: nao encontrei bin/multics.x64 - corre o script de dentro da pasta do pacote"
  exit 1
fi

echo "== 1/4: copiar binarios =="
mkdir -p $DIR
cp bin/multics.x64 bin/multics.x32 $DIR/
chmod 755 $DIR/multics.x64 $DIR/multics.x32

echo "== 2/4: copiar configs para /var/etc/ =="
mkdir -p /var/etc
cp -n configs/* /var/etc/ 2>/dev/null   # -n: nao sobrescreve ficheiros ja existentes
chmod 666 /var/etc/*.cfg /var/etc/CCcam.* /var/etc/ip2country.csv /var/etc/multics.css 2>/dev/null
echo "   (usa -n: se ja tinhas configs tuas, ficam intactas)"

echo "== 3/4: servico systemd (inicio automatico) =="
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
LimitCORE=infinity

[Install]
WantedBy=multi-user.target
EOF
systemctl daemon-reload
systemctl enable multics 2>/dev/null

echo "== 4/4: firewall =="
if command -v ufw >/dev/null 2>&1; then
  ufw allow 5500/tcp >/dev/null    # GUI (gestao web)
  ufw allow 16000/tcp >/dev/null   # CCcam (F-lines dos teus clientes)
  ufw allow 16001/tcp >/dev/null   # CCcam3
  ufw allow 21000/tcp >/dev/null   # MGcamd
  ufw allow 8600/tcp >/dev/null    # cs378x
  ufw allow 7502/udp >/dev/null    # camd35 (UDP)
  ufw allow 15000:15025/tcp >/dev/null  # perfis newcamd
  echo "   portas abertas via ufw"
else
  echo "   (ufw nao instalado - abre as portas manualmente se usares firewall)"
fi

systemctl restart multics 2>/dev/null

# ============================================================
# GUIA DO PROJETO (leigo)
# ============================================================
cat <<GUIDE

=============================================================
  INSTALACAO CONCLUIDA - MultiCS r1000 v1.26
=============================================================

O QUE E ISTO
  Um "cardserver proxy" MultiCS: recebe pedidos de descodificacao
  (ECM) dos teus clientes e reencaminha-os para os servidores onde
  estao os cartoes ("readers"); devolve as chaves (CW) aos clientes.

ONDE ESTA TUDO
  Binarios:          $DIR/multics.x64 e multics.x32
  Configs (editas):  /var/etc/  (fica tudo aqui)
    multics.cfg        - config principal (portas, users da GUI, falban,
                         anticascade). Pode conter TUDO num so ficheiro.
    perfis.cfg         - PERFIS: os "pacotes" que das aos clientes
                         (cada perfil = um CAID: MEO 1814, NOS 1802, etc.)
                         + users newcamd de cada perfil
    servidores.cfg     - READERS: linhas N: C: L: + cache + cacheex
                         (a fonte dos cards)
    clientes_cccam.cfg - F-lines (clientes CCcam que TU crias)
    clientes_mgcamd.cfg- users MGcamd que TU crias
    clientes_cs378x.cfg- users cs378x que TU crias
    clientes_camd35.cfg- users camd35 (UDP) que TU crias
    clientes_cache.cfg - users de cache (CSP) que TU crias
    CCcam.channelinfo  - nomes dos canais (GUI e logs)
    CCcam.providers    - nomes dos idents/providers (GUI e logs)
    CCcam.lite         - lista de canais permitidos (se ativares)
    Softcam.cfg        - chaves BISS/constcw do emulador
    ip2country.csv     - pais por IP (bandeiras na GUI)
    blocked_ips.cfg    - IPs banidos (failban/anticascade/GUI)
  (multics.cfg inclui os restantes via INCLUDE - quem preferir
   pode manter TUDO no multics.cfg e apagar os INCLUDEs)

COMO FUNCIONA (resumo)
  1. Os TEUS clientes ligam-se a ti: CCcam (porta 16000), newcamd,
     MGcamd, camd35 - com as contas que crias nos ficheiros acima.
  2. O pedido do cliente e aceite se casar com um PERFIL (profiles.cfg).
  3. O pedido vai para os teus READERS (linhas N:/C: em Nlines.cfg).
  4. O reader responde a CW -> validada (NAGRA/CWPK/icam/filtros) ->
     entregue ao cliente. Se nenhum reader responder, NOK ao cliente.

COMO ARRANCAR/PARAR
  systemctl start multics    (arrancar)
  systemctl stop multics     (parar)
  systemctl status multics   (ver estado)
  tail -f /var/tmp/multics.log   (ver o log em tempo real)

GUI (GESTAO WEB)
  http://IP-DA-VPS:5500
  user/pass: definidos em multics.cfg (HTTP USER / HTTP PASS)
  No ecra Servers ves os readers e o estado de cada um;
  em Profiles ves os pacotes e podes ligar/desligar (OFF/ON).

PRIMEIROS PASSOS
  1. Edita multics.cfg -> muda HTTP USER e HTTP PASS.
  2. Poe as tuas linhas de readers em servidores.cfg
     (N: ip porta user pass 01 02 03 04 05 06 07 08 09 10 11 12 13 14).
  3. Cria os teus clientes (clientes_cccam.cfg: F: user pass, etc.).
  4. systemctl restart multics e ve a GUI.
  5. Se um pacote nao abrir: ver o log (tail -f) - a mensagem diz
     se foi "Wrong provider" (ident em falta) ou NOK do reader.

BACKUP RECOMENDADO
  Guarda uma copia de /var/etc/ depois de configurado:
    tar czf multics-configs-backup.tar.gz /var/etc/
=============================================================
GUIDE
