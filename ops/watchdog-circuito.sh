#!/bin/bash
# watchdog-circuito.sh - vigia SILENCIOSO do multics do circuito (VPS do colega)
# Corre na NOSSA VPS via cron. SO LEITURA: ssh -> GUI local dele (12000) + tail
# do DCW log -> extrai anomalias cyc:/fluxo ECM -> log local.
# NAO MEXE em nada do lado dele (nenhum ficheiro/config/servico).

LOG=/var/log/watchdog-circuito.log
CWL=/var/log/watchdog-circuito-cw.log
SSH_USER=caspt
SSH_HOST=130.255.78.45
SSH_PORT=29935
# SSH_PASS vem de /etc/watchdog-circuito.env (fora do repo)
GUI_USER=cas1974
[ -r /etc/watchdog-circuito.env ] && . /etc/watchdog-circuito.env
# GUI_PASS vem de /etc/watchdog-circuito.env (fora do repo)

REMOTE_DEBUG='
curl -s -m 8 -c /tmp/.wck -b /tmp/.wck -d "user='"$GUI_USER"'&pass='"$GUI_PASS"'" http://127.0.0.1:12000/login -o /dev/null
curl -s -m 8 -c /tmp/.wck -b /tmp/.wck "http://127.0.0.1:12000/debug?action=div"
'

REMOTE_CW='sudo -n tail -n 3000 /var/log/multics-cw.log 2>/dev/null'

# 1) debug ring (anomalias + fluxo)
snap=$(sshpass -p "$SSH_PASS" ssh -p $SSH_PORT -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o LogLevel=ERROR "$SSH_USER@$SSH_HOST" "$REMOTE_DEBUG" 2>/dev/null)

if [ -z "$snap" ]; then
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] SEM RESPOSTA (ssh/gui indisponivel)" >> "$LOG"
else
  echo "$snap" | sed 's/<br>/\n/g; s/<[^>]*>//g; s/&quot;/"/g; s/&gt;/>/g; s/&lt;/</g; s/&amp;/\&/g' > /tmp/.wck_raw
  interesting=$(grep -E 'cyc:|badmark|reconexao forcada|-> ecm to|<= cw from' /tmp/.wck_raw | tail -n 40)
  if [ -n "$interesting" ]; then
    echo "=== [$(date '+%Y-%m-%d %H:%M:%S')] ===" >> "$LOG"
    echo "$interesting" >> "$LOG"
  fi
fi

# 2) DCW log dele (entregas por peer - correlacao do 0066 e afins)
cwsnap=$(sshpass -p "$SSH_PASS" ssh -p $SSH_PORT -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o LogLevel=ERROR "$SSH_USER@$SSH_HOST" "$REMOTE_CW" 2>/dev/null)
if [ -n "$cwsnap" ]; then
  # guarda tudo numa janela de 2min (novas linhas desde a ultima run)
  newcws=$(echo "$cwsnap" | awk -v cutoff="$(date -u -d '3 minutes ago' '+%Y/%m/%d %H:%M' 2>/dev/null || date -u -d '-3 min' '+%Y/%m/%d %H:%M')" '$1$2 >= cutoff')
  if [ -n "$newcws" ]; then
    echo "$newcws" >> "$CWL"
  fi
  size=$(stat -c %s "$CWL" 2>/dev/null || echo 0)
  if [ "$size" -gt 3145728 ]; then
    tail -c 1572864 "$CWL" > /tmp/.wck_rot && mv /tmp/.wck_rot "$CWL"
  fi
fi

# rota do log principal
size=$(stat -c %s "$LOG" 2>/dev/null || echo 0)
if [ "$size" -gt 2097152 ]; then
  tail -c 1048576 "$LOG" > /tmp/.wck_rot2 && mv /tmp/.wck_rot2 "$LOG"
fi
