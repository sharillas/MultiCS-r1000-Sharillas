#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# nos_estudo_stats.py - snapshot 30min do estudo NOS (circuito, caid 1802)
# cron: */30 * * * * python3 /opt/multics/nos_estudo_stats.py
import re, datetime
from collections import defaultdict

CWLOG = "/var/log/multics-cw.log"
STATS = "/var/log/multics-nos-estudo-stats.log"

now = datetime.datetime.now()
win = now - datetime.timedelta(minutes=30)

pat = re.compile(r"(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}) ch (1802:\w{6}:\w{4}) cw \w+ \w+ src (\d+) srv (\d+)")
by_ch = defaultdict(list)
by_srv = defaultdict(int)
for line in open(CWLOG, "r", errors="replace"):
    m = pat.search(line)
    if not m:
        continue
    dt = datetime.datetime.strptime(m.group(1), "%Y/%m/%d %H:%M:%S")
    if dt < win:
        continue
    by_ch[m.group(2)].append(dt)
    by_srv[m.group(4)] += 1

res = []
for ch, times in by_ch.items():
    deltas = []
    for i in range(1, len(times)):
        d = (times[i] - times[i-1]).total_seconds()
        if 0 < d < 300:
            deltas.append(d)
    med = sorted(deltas)[len(deltas)//2] if deltas else 0
    res.append("%s=%d(~%ds)" % (ch, len(times), med))

with open(STATS, "a") as f:
    f.write("%s total_1802=%d srv=%s %s\n" % (now.strftime("%Y/%m/%d %H:%M"), sum(len(v) for v in by_ch.values()), dict(by_srv), " ".join(sorted(res)[:10])))
