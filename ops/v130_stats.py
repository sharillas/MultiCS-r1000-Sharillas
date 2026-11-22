#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# v130_stats.py - snapshot horario das stats v1.30 (por canal / LASTCW / stalecw)
# corre via cron: 0 * * * * python3 /opt/multics/v130_stats.py
import re, time, os, urllib.request, http.cookiejar, datetime

CWLOG = "/var/log/multics-cw.log"
STATS = "/var/log/multics-v130-stats.log"

now = datetime.datetime.now()
win = now - datetime.timedelta(hours=1)

channels = {}
srcs = {}
total = 0
try:
    for line in open(CWLOG, "r", errors="replace"):
        m = re.match(r"(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}) ch (\w{4}:\w{6}:\w{4}) cw \w+ \w+ src (\d+)", line)
        if not m:
            continue
        t = datetime.datetime.strptime(m.group(1), "%Y/%m/%d %H:%M:%S")
        if t < win:
            continue
        ch = m.group(2)
        src = m.group(3)
        channels[ch] = channels.get(ch, 0) + 1
        srcs[src] = srcs.get(src, 0) + 1
        total += 1
except Exception as e:
    pass

# debug: conta LAST CW resent / stalecw no anel
lastcw = stale = 0
try:
    cj = http.cookiejar.CookieJar()
    op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))
    op.open("http://127.0.0.1:5500/login", data=b"user=sharpicos&pass=114494", timeout=8)
    dbg = op.open("http://127.0.0.1:5500/debug?action=div", timeout=8).read().decode("utf-8", "replace")
    lastcw = dbg.count("LAST CW resent")
    stale = dbg.count("stalecw:")
except Exception as e:
    pass

top = " ".join("%s=%d" % (k, v) for k, v in sorted(channels.items(), key=lambda x: -x[1])[:8])
with open(STATS, "a") as f:
    f.write("%s total=%d lastcw_anel=%d stalecw_anel=%d src=%s top=[%s]\n"
            % (now.strftime("%Y/%m/%d %H:%M"), total, lastcw, stale, srcs, top))
