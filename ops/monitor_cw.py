#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# monitor_cw.py - tabela de aprendizagem CAK7 (MultiCS)
# Le /var/log/multics-cw.log + o debug da GUI e escreve /var/log/multics-cw-report.txt
import io, re, time, urllib.request, http.cookiejar, os

CWLOG = '/var/log/multics-cw.log'
OUT = '/var/log/multics-cw-report.txt'
GUI = 'http://127.0.0.1:5500'

# 1) CWs do ficheiro de aprendizagem
cws = {}
if os.path.exists(CWLOG):
    for line in io.open(CWLOG, encoding='utf-8', errors='replace'):
        m = re.search(r'ch (\w{4}):(\w{6}):(\w{4}) cw (\w{16}) (\w{16}) src (\d+)', line)
        if m:
            caid, prov, sid, cw0, cw1, src = m.groups()
            cws[sid.lower()] = (caid, prov, cw0, cw1, line.strip()[:16])

# 2) veredictos do debug (ring) - forcar filtro ALL primeiro
cj = http.cookiejar.CookieJar()
op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))
verdicts = {}
try:
    op.open(urllib.request.Request(GUI + '/login', data=b'user=sharpicos&pass=114494', headers={'User-Agent': 'm'}), timeout=8)
    op.open(urllib.request.Request(GUI + '/debug?action=debug&value=ALL', headers={'User-Agent': 'm'}), timeout=8)
    d = op.open(urllib.request.Request(GUI + '/debug?action=div', headers={'User-Agent': 'm'}), timeout=8).read().decode('utf-8', 'replace')
    for m in re.finditer(r"=> cw to (?:CCcam |Newcamd |Mgcamd |Camd35 |cs378x )?client '([^']+)' ch (\w{4}):(\w{6}):(\w{4}) \((\d+)ms\)(?: CW: (\w{16}) (\w{16}))?", d):
        user, caid, prov, sid, ms, cw0, cw1 = m.groups()
        if user == 'vuzero':
            verdicts[sid.lower()] = (caid, prov, 'FOUND', ms)
    for m in re.finditer(r"decode failed to [^']*'vuzero' ch (\w{4}):(\w{6}):(\w{4}), ([^)]+?)\)", d):
        sid, caid, prov, reason = m.groups()
        if sid.lower() not in verdicts:
            verdicts[sid.lower()] = (caid, prov, 'NOK: ' + reason.strip())
except Exception:
    pass

out = io.open(OUT, 'w', encoding='utf-8')
out.write('=== MultiCS CW Learning Report ===\n')
out.write('gerado: %s\n\n' % time.strftime('%Y-%m-%d %H:%M:%S'))
out.write('sid   | caid:provid   | veredicto | cw0 / cw1 (do DCW LOG)\n')
out.write('------+---------------+-----------+----------------------------------------\n')
all_sids = sorted(set(list(cws.keys()) + list(verdicts.keys())))
for sid in all_sids:
    caid, prov, cw0, cw1, t = cws.get(sid, ('', '', '', '', ''))
    v = verdicts.get(sid, ())
    vtxt = v[2] if len(v) >= 3 else 'sem veredicto'
    out.write('%s | %s:%s | %s | %s %s\n' % (sid, caid or (v[0] if v else '?'), prov or (v[1] if v else '?'), vtxt, cw0, cw1))
out.write('\nCWs capturadas: %d | SIDs com veredicto: %d\n' % (len(cws), len(verdicts)))
out.close()
print('report escrito:', OUT)
