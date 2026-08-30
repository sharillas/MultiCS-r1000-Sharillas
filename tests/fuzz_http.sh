#!/bin/bash
# FUZZ HTTP: martela a GUI com pedidos malformados e verifica que o daemon
# nao crasha nem fica pendurado. Uso: fuzz_http.sh <host> <porta>
# (corre contra uma instancia de TESTE - nao contra producao)
H=${1:-127.0.0.1}
P=${2:-6501}
B="http://$H:$P"
FAILS=0

req() { # nome, metodo, alvo
  timeout 5 curl -s -m 5 -o /dev/null -X "$2" "$B$3" -H "$4" --data-binary "$5" 2>/dev/null
  return 0
}

echo "== fuzz basico (metodos errados/garbage) =="
for m in "ABCDEFG" "GET\0x00" "%%%%%" "OPTIONS" "TRACE"; do
  printf 'X' | timeout 5 nc $H $P 2>/dev/null | head -1 | grep -qi '400\|404\|301\|200' && echo "  $m: respondeu" || echo "  $m: sem resposta (ok)"
done

echo "== caminhos malformados =="
for path in "/"$'\x01'"/" "/%%%ZZ" "/login?%00" "/..%2f..%2f..%2fetc%2fpasswd" "/profile?id=%s%s%s" "/debug?action=%n" "/"$(python3 -c "print('A'*8000)") ; do
  timeout 5 curl -s -m 5 -o /dev/null -w "%{http_code} " "$B$path" 2>/dev/null
done; echo

echo "== headers malformados =="
timeout 5 curl -s -m 5 -o /dev/null -H "User-Agent: $(python3 -c "print('U'*5000)")" "$B/" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null -H "Content-Length: -9999999" -X POST "$B/login" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null -H "Content-Length: 999999999999" -X POST "$B/login" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null -H "Content-Length: abc" -X POST "$B/login" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null -H "Transfer-Encoding: chunked" --data-binary $'5\r\nabcde\r\n0\r\n\r\n' "$B/login" 2>/dev/null

echo "== POSTs truncados/gigantes =="
timeout 5 curl -s -m 5 -o /dev/null -X POST "$B/login" --data-binary "user=" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null -X POST "$B/login" --data-binary "$(python3 -c "print('a='+'b'*1000000)")" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null -X POST "$B/emulator" -F "softcamkey=@/dev/urandom;filename=test.key" 2>/dev/null

echo "== GETs com parametros estranhos =="
timeout 5 curl -s -m 5 -o /dev/null "$B/servers?action=%00%01%ff" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null "$B/server?id=99999999999999999" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null "$B/profile?id=-1" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null "$B/profile?id=999999999" 2>/dev/null
timeout 5 curl -s -m 5 -o /dev/null "$B/testchannel?caid=ZZZZ&prid=ZZZZ&sid=ZZZZ" 2>/dev/null

echo "== verificacao final =="
timeout 5 curl -s -m 5 -o /dev/null -w "GUI responde: %{http_code}\n" "$B/"
ps -p $(pgrep -f "multics.x64 -C /tmp/e2e" | head -1) >/dev/null 2>&1 && echo "daemon VIVO" || echo "daemon MORTO (ver log!)"
