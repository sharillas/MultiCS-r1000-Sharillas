#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# v140_monitor.py - snapshot 30min da v1.40: cyc: anomalias, cwbad por reader, trafego por CAID (incl 098D)
# cron: */30 * * * * python3 /opt/multics/v140_monitor.py
import re, datetime, urllib.request, http.cookiejar
from collections import defaultdict

CWLOG = "/var/log/multics-cw.log"
STATS = "/var/log/multics-v140-stats.log"

now = datetime.datetime.now()
win = now - datetime.timedelta(minutes=30)

pat = re.compile(r"(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}) ch (\w{4}):\w{6}:\w{4} cw \w+ \w+ src (\d+) srv (\d+)")
by_caid = defaultdict(int)
by_srv = defaultdict(int)
for line in open(CWLOG, "r", errors="replace"):
    m = pat.search(line)
    if not m:
        continue
    dt = datetime.datetime.strptime(m.group(1), "%Y/%m/%d %H:%M:%S")
    if dt < win:
        continue
    by_caid[m.group(2)] += 1
    by_srv[m.group(4)] += 1

# debug: contagem de anomalias cyc: + cwbad por reader (anel)
cyc = 0
cwbad = {}
try:
    cj = http.cookiejar.CookieJar()
    op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))
    op.open("http://127.0.0.1:5500/login", data=b"user=sharpicos&pass=114494", timeout=8)
    dbg = op.open("http://127.0.0.1:5500/debug?action=div", timeout=8).read().decode("utf-8", "replace")
    cyc = dbg.count(" cyc: ")
    for m in re.finditer(r"health: server \([\d.]+:(\d+)\) score=\d+ \(ok=\d+/\d+ tmo=\d+ baddcw=\d+ cwbad=(\d+)\)", dbg):
        cwbad[m.group(1)] = int(m.group(2))
except Exception:
    pass

with open(STATS, "a") as f:
    f.write("%s caids=%s srv=%s cyc_anel=%d cwbad=%s\n"
            % (now.strftime("%Y/%m/%d %H:%M"), dict(by_caid), dict(by_srv), cyc, cwbad))
