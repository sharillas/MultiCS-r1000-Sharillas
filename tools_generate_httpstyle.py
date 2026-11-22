#!/usr/bin/env python3
# Regenerates src/httpstyle.c from gui_embedded_style.css + gui_embedded_customjs.js (run from the repo root:
#   python tools_generate_httpstyle.py gui_embedded_style.css gui_embedded_customjs.js src/httpstyle.c)
# usage: gen_httpstyle.py <style.css> <customjs.js> <out httpstyle.c>
import sys
css, js, out = sys.argv[1:4]
def esc(s):
    return s.replace('\\','\\\\').replace('"','\\"').replace('\t','\\t')
c = open(css, encoding='utf-8', newline='').read().replace('\r\n','\n')
j = open(js, encoding='utf-8', newline='').read().replace('\r\n','\n')
for name, t in (('css', c), ('js', j)):
    bad = [ch for ch in t if ord(ch) > 126 or (ord(ch) < 32 and ch not in '\n\t')]
    if bad:
        sys.exit('non-ASCII/control char in %s: %r' % (name, bad[:5]))
lines = c.split('\n')
if lines and lines[-1] == '': lines.pop()
o = ['char style_css[] =']
for l in lines:
    o.append('\t"' + esc(l) + '\\n"')
o.append('\t"";')
o.append('')
o.append('char java_file[] = "' + esc(j).replace('\n','\\n') + '";')
o.append('')
open(out, 'w', encoding='ascii', newline='').write('\r\n'.join(o))
print('ok', len(lines), 'css lines;', len(j), 'js chars')
