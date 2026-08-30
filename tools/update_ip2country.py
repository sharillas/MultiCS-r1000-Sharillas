#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# update_ip2country.py - atualiza o ip2country.csv com a base diaria do
# repo iplocate/ip-address-databases (https://github.com/iplocate/ip-address-databases)
# Formato de saida (MultiCS): NETWORK/PREFIXO,CC  (IPv4; IPv6 e ignorado)
#
# Uso: python3 update_ip2country.py <ip2country.csv>
#   descarrega o zip diario, converte e grava <ip2country.csv> (backup .bak-<data>)
# Sugestao: crontab -e -> 30 5 * * * python3 /opt/multics/update_ip2country.py /var/etc/ip2country.csv
import sys, os, csv, io, zipfile, time, shutil
try:
    import urllib.request
except ImportError:
    print("python3 + urllib necessarios")
    sys.exit(1)

MEDIA = "https://media.githubusercontent.com/media/iplocate/ip-address-databases/main/ip-to-country/ip-to-country.csv.zip"
DEST = sys.argv[1] if len(sys.argv) > 1 else "/var/etc/ip2country.csv"

print("descarregar %s" % MEDIA)
buf = urllib.request.urlopen(MEDIA, timeout=120).read()
z = zipfile.ZipFile(io.BytesIO(buf))
name = [n for n in z.namelist() if n.endswith(".csv")][0]
print("ficheiro no zip: %s" % name)

tmp = DEST + ".tmp"
n = 0
with io.TextIOWrapper(z.open(name), encoding="utf-8", errors="replace") as fi, \
     open(tmp, "w", encoding="ascii", newline="\n") as fo:
    rd = csv.reader(fi)
    next(rd, None)
    for row in rd:
        if len(row) < 4 or not row[0] or not row[2]:
            continue
        net, cc = row[0], row[2].strip()
        if len(cc) != 2 or ":" in net:
            continue  # so IPv4
        fo.write("%s,%s\n" % (net, cc))
        n += 1

if n < 100000:
    print("ERRO: so %d linhas convertidas - abortar" % n)
    os.unlink(tmp)
    sys.exit(1)

if os.path.exists(DEST):
    shutil.copy2(DEST, DEST + ".bak-" + time.strftime("%Y%m%d"))
os.rename(tmp, DEST)
try:
    os.chmod(DEST, 0o666)
except Exception:
    pass
print("OK: %d linhas IPv4 gravadas em %s" % (n, DEST))
