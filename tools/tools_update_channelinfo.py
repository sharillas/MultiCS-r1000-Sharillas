#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================
# tools_update_channelinfo.py - MultiCS r1000 by @Sharillas
# Atualiza /var/etc/CCcam.channelinfo a partir do KingOfSat
# (feeds ATIVOS apenas) + base comunitaria (pasta srvid).
#
# Uso (na VPS):
#   python3 /opt/multics/tools_update_channelinfo.py
#   python3 /opt/multics/tools_update_channelinfo.py --positions 30W,13E,19.2E
#   python3 /opt/multics/tools_update_channelinfo.py --apply   (grava e da reload)
#
# Filtra os blocos "Occasional Feeds, data or inactive frequency"
# (regra: o marcador aplica-se a frequencia que se segue).
# ============================================================
import re
import sys
import time
import argparse
import urllib.request

SRCDIR = '/opt/multics/srvid'
OUT    = '/var/etc/CCcam.channelinfo'

# Mapeamento pacote KOS -> CAID:PROVID (ampliavel: Movistar+=1810)
PKGMAP = {
    'Nos': '1814:000000',
    'Meo': '1813:000000',
    'Movistar+': '1810:000000',
}

# Tabela curada pacote -> CAIDs (cross-check com o KOS)
# So inclui CAIDs confirmados; o resto e coberto pelo wildcard FFFE
PKGCAID = {
    # Astra 19.2E
    'HD +': ['1830', '1843'],
    'HD+': ['1830', '1843'],
    'Sky Deutschland': ['098D'],
    'Austriasat': ['0D95', '0D96'],
    'Movistar+': ['1810'],
    'BetaDigital': ['1702', '1722'],
    'ORF Digital': ['0D95', '0D96', '0648', '0650'],
    'Canal+ France': ['0500', '1811'],
    'TNTSAT': ['0500'],
    'Bis': ['0500'],
    'MTV Networks': ['0500'],
    'SKY': ['0963'],
    'Orange': ['0500'],
    'M7 Group': ['0B00', '0B01', '0B02'],
    'M7': ['0B00', '0B01', '0B02'],
    # Hotbird 13E
    'Orange Polska': ['0B01', '0500'],
    'Platforma Canal+': ['1884', '0100'],
    'Polsat Box': ['1803', '1861', '186C'],
    'Nova': ['0604', '0699'],
    'KABELIO.CH': ['4A70'],
    'SSR/SRG': ['4AFC', '0500'],
    'Radiotelevisione svizzera': ['4AFC', '0500'],
    'RAI': ['183D', '183E'],
    'TivùSat': ['183D', '183E'],
    'Globecast': ['2600'],
    'Sky Italia': ['0919', '093B', '09CD'],
    'Vivacom': ['09BD'],
    # Hispasat 30W
    'NOS': ['1814'],
    'MEO': ['1813'],
    'TDT Abertis': ['2600'],
    # Astra 2 28.2E
    'Sky UK': ['0963', '0960', '0961'],
    # Astra 3 23.5E
    'Skylink': ['0D96', '0624'],
    # Thor 0.8W
    'Digi TV': ['1802', '1880'],
    'Focus Sat': ['0B02'],
    # Astra 4A 4.8E
    'Viasat': ['090F', '093E'],
    # Eutelsat 5W
    'Fransat': ['0500'],
    'Mediaset': ['0100'],
    # Eutelsat 9E
    'Kabelkiosk': ['0B01'],
    # Eutelsat 16A
    'MaxTV': ['0604'],
    # Hellas Sat 39E
    'Bulsatcom': ['4AEE', '5581'],
    # Turksat 42E
    'Turksat': ['0D00', '0D01', '0D03'],
}

SECTIONS = ['oscam.srvid', 'oscam.srvid.new', 'oscam.srvid2', 'multi.srvid2',
            'oscam.srvid2.canalplus', 'oscam.srvid2.pl', 'oscam.srvid2.polsatbox',
            'oscam.srvid2.server', 'oscam.srvid2.xxx']

# descricao por CAID para os cabecalhos do ficheiro
CAIDDESC = {
    '0100': 'SECA Mediaguard',
    '0500': 'Viaccess',
    '0604': 'Irdeto', '0624': 'Irdeto', '0648': 'Irdeto', '0650': 'Irdeto', '0699': 'Irdeto',
    '090F': 'NDS Videoguard (CD NL)', '0919': 'NDS Videoguard (Sky IT)', '093B': 'NDS Videoguard (Sky IT)',
    '0960': 'NDS Videoguard (Sky UK)', '0961': 'NDS Videoguard (Sky UK)', '0963': 'NDS Videoguard (Sky UK)',
    '098C': 'NDS Videoguard (Sky DE)', '098D': 'NDS Videoguard (Sky DE)',
    '09BD': 'NDS Videoguard (Vivacom)', '09CD': 'NDS Videoguard (Sky IT)',
    '0B00': 'Conax', '0B01': 'Conax', '0B02': 'Conax',
    '0D00': 'CryptoWorks (Turksat)', '0D01': 'CryptoWorks (Turksat)', '0D03': 'CryptoWorks (Turksat)',
    '0D95': 'CryptoWorks (ORF/Austriasat)', '0D96': 'CryptoWorks (Skylink)',
    '1702': 'Betacrypt (BetaDigital)', '1722': 'Betacrypt (BetaDigital)',
    '1802': 'NAGRA (Digi TV)', '1803': 'NAGRA (Polsat)', '1810': 'NAGRA (Movistar+)',
    '1811': 'NAGRA (Canal+ FR)', '1813': 'NAGRA (MEO)', '1814': 'NAGRA (NOS)',
    '1830': 'NAGRA (HD+)', '183D': 'NAGRA (RAI/TivuSat)', '183E': 'NAGRA (RAI/TivuSat)',
    '1843': 'NAGRA (HD+)', '1861': 'NAGRA (Polsat)', '186C': 'NAGRA (Polsat)',
    '1880': 'NAGRA (Digi TV)', '1884': 'NAGRA (Platforma Canal+)',
    '2600': 'BISS',
    '4A70': 'Bulcrypt (Kabelio)', '4AEE': 'Bulcrypt (Bulsatcom)', '4AFC': 'Bulcrypt (SRG)',
    '5581': 'Bulcrypt (Bulsatcom)',
    'FFFF': 'FTA (canais abertos)',
}

# nomes dos satelites para os cabecalhos
SATNAMES = {
    '30W': 'Hispasat 30W', '13E': 'Hotbird 13E', '19.2E': 'Astra 19.2E',
    '28.2E': 'Astra 28.2E', '23.5E': 'Astra 23.5E', '0.8W': 'Thor 0.8W',
    '4.8E': 'Astra 4.8E', '5W': 'Eutelsat 5W', '9E': 'Eutelsat 9E',
    '16E': 'Eutelsat 16E', '39E': 'Hellas Sat 39E', '42E': 'Turksat 42E',
    '1.9E': 'BulgariaSat 1.9E', '3.1E': 'Eutelsat 3.1E', '7E': 'Eutelsat 7E',
    '10E': 'Eutelsat 10E', '21.5E': 'Eutelsat 21.5E', '26E': 'Badr 26E',
    '31.5E': 'Astra 31.5E', '33E': 'Eutelsat 33E', '36E': 'Eutelsat 36E',
    '45E': 'Intelsat 45E', '46E': 'Azerspace 46E', '52E': 'TurkmenAlem 52E',
    '7W': 'Nilesat 7W', '8W': 'Eutelsat 8W', '12.5W': 'Eutelsat 12.5W',
    '14W': 'Express 14W', '15W': 'Telstar 15W', '22W': 'SES 22W',
    '27.5W': 'Intelsat 27.5W', '34.5W': 'Intelsat 34.5W', '37.5W': 'Telstar 37.5W',
    '40.5W': 'SES 40.5W', '43.1W': 'Intelsat 43.1W', '45W': 'Intelsat 45W',
    '47.5W': 'SES 47.5W', '55.5W': 'Intelsat 55.5W', '58W': 'Intelsat 58W',
    '61W': 'Amazonas 61W', '63W': 'Telstar 63W', '70W': 'Star One 70W',
}

# posicoes por CAID (preenchido durante o harvest do KingOfSat)
CAIDPOS = {}

def caidpos_add(caid, pos):
    if caid and pos:
        CAIDPOS.setdefault(caid, set()).add(pos.strip())

stats = {'ativos': 0, 'inativos': 0, 'fta': 0, 'mapeados': 0, 'biss': 0, 'caid_lite': 0}

# mapa SID -> {CAID:PROVID} (das listas comunitarias multi-CAID)
SIDMAP = {}
# relatorio: pacote -> CAIDs confirmados (tabela curada)
PKGREP = {}

def sidmap_add(sid, caprov):
    if sid not in SIDMAP:
        SIDMAP[sid] = {}
    SIDMAP[sid][caprov] = 1

def sanitize(s):
    return re.sub(r'[^\x20-\x7e]', '?', re.sub(r'\s+', ' ', s)).strip()

def fetch(url):
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read().decode('utf-8', 'replace')

def add_chan(chans, order, key, name):
    name = sanitize(name)
    if not name or name == 'fta':
        return
    if key not in chans:
        chans[key] = name
        order.append(key)

def load_community(chans, order, srcdir):
    import os
    for fn in SECTIONS:
        f = os.path.join(srcdir, fn)
        if not os.path.isfile(f):
            continue
        with open(f, 'r', encoding='utf-8', errors='replace') as fh:
            for line in fh:
                line = line.rstrip('\n')
                m = re.match(r'\s*[0-9A-Fa-f]{4}:\s*(?:0000,)?([0-9A-Fa-f,@]+)\|([^|\r\n]*)(?:\|([^|\r\n]*))?', line)
                if m:
                    sid = line.split(':')[0].strip().upper().zfill(4)
                    canal = m.group(2).strip()
                    for c in m.group(1).split(','):
                        c = c.strip()
                        m2 = re.match(r'^([0-9A-Fa-f]{4})@', c)
                        if m2:
                            caid = m2.group(1).upper()
                            for pr in re.findall(r'@([0-9A-Fa-f]{6})', c):
                                add_chan(chans, order, '%s:%s:%s' % (caid, pr.upper(), sid), canal)
                                sidmap_add(sid, '%s:%s' % (caid, pr.upper()))
                        elif re.match(r'^[0-9A-Fa-f]{4}$', c):
                            add_chan(chans, order, '%s:000000:%s' % (c.upper(), sid), canal)
                            sidmap_add(sid, '%s:000000' % c.upper())
                    continue
                m = re.match(r'\s*([0-9A-Fa-f]{4}):([0-9A-Fa-f]{6}):([0-9A-Fa-f]{4})', line)
                if m:
                    ca, pr, si = m.group(1).upper(), m.group(2).upper(), m.group(3).upper()
                    sidmap_add(si, '%s:%s' % (ca, pr))
                    nm = ''
                    mq = re.search(r'"([^"]*)"', line)
                    if mq:
                        nm = mq.group(1)
                    else:
                        mp = re.search(r'\|([^"|\r\n]+)', line)
                        if mp:
                            nm = mp.group(1)
                    add_chan(chans, order, '%s:%s:%s' % (ca, pr, si), nm)
                    continue
                m = re.match(r'\s*([0-9A-Fa-f]{4}):([0-9A-Fa-f]{4})\|([^"|\r\n]+)', line)
                if m:
                    add_chan(chans, order, '%s:000000:%s' % (m.group(2).upper(), m.group(1).upper()), m.group(3))
                    sidmap_add(m.group(1).upper(), '%s:000000' % m.group(2).upper())
        # oscam.srvid classico: CAIDLIST:SID|provider|canal|tipo
        if fn == 'oscam.srvid':
            with open(f, 'r', encoding='utf-8', errors='replace') as fh:
                for line in fh:
                    m = re.match(r'\s*([0-9A-Fa-f,]+):([0-9A-Fa-f]{4})\|([^|]*)\|([^|]*)\|', line)
                    if m:
                        for c in m.group(1).split(','):
                            if len(c) == 4:
                                add_chan(chans, order, '%s:000000:%s' % (c.upper(), m.group(2).upper()), m.group(4))
                                sidmap_add(m.group(2).upper(), '%s:000000' % c.upper())
    print('base comunitaria: %d canais | sidmap: %d SIDs com CAID real' % (len(order), len(SIDMAP)))

def harvest_kos(chans, order, positions, ftapositions, lite):
    for pos in positions:
        pos = pos.strip()
        if not pos:
            continue
        print('a descarregar pos-%s ...' % pos)
        html = fetch('https://en.kingofsat.net/pos-%s' % pos)
        events = []
        for m in re.finditer(r'(?i)Occasional Feeds, data or inactive frequency', html):
            events.append((m.start(), 'MARK', ''))
        for m in re.finditer(r'<tr data-frequency-id=', html):
            events.append((m.start(), 'FREQ', ''))
        for m in re.finditer(r'(?s)<tr data-channel-id="[^"]*">.*?</tr>', html):
            events.append((m.start(), 'CHAN', m.group(0)))
        events.sort(key=lambda x: x[0])
        s0 = dict(stats)
        pending = False
        freq_inactive = False
        for idx, t, val in events:
            if t == 'MARK':
                pending = True
            elif t == 'FREQ':
                freq_inactive = pending
                pending = False
            else:  # CHAN
                r = val
                nm = re.search(r'(?s)<td class="ch">.*?<a[^>]*>([^<]+)</a>', r)
                if not nm:
                    continue
                name = nm.group(1).strip()
                pkgs = [x.strip() for x in re.findall(r'(?s)<a class="bq" href="pack-[^"]*">([^<]+)</a>', r)]
                enc = ''
                me = re.search(r'<td class="cr">([^<]*)</td>', r)
                if me:
                    enc = me.group(1).strip()
                else:
                    me = re.search(r'<td class="cl">([^<]*)</td>', r)
                    if me:
                        enc = me.group(1).strip()
                ms = re.search(r'<td class="s">([0-9]{1,5})</td>', r)
                if not ms:
                    continue
                sid = '%04X' % int(ms.group(1))
                if freq_inactive:
                    if enc in ('', 'Clear'):
                        stats['fta'] += 1
                    else:
                        caids_i = [PKGMAP[p] for p in pkgs if p in PKGMAP]
                        if caids_i:
                            stats['inativos'] += 1
                    continue  # SKIP feeds inativos!
                if enc in ('', 'Clear'):
                    # canal aberto (FTA): marca na lista FFFF para skip rapido
                    stats['fta'] += 1
                    if pos in ftapositions:
                        add_chan(chans, order, 'FFFF:000000:%s' % sid, 'FTA')
                    continue
                if enc == 'BISS':
                    # TDT/feeds BISS: emulador (2600) + lista lite
                    stats['biss'] += 1
                    add_chan(chans, order, '2600:000000:%s' % sid, name)
                    lite['2600:000000:%s' % sid] = 1
                    caidpos_add('2600', pos)
                    continue
                # canal codificado (qualquer CAS): entra na lite com wildcard
                # FFFE = qualquer CAID + CAIDs reais (tabela curada PKGCAID)
                lite['FFFE:000000:%s' % sid] = 1
                stats['ativos'] += 1
                cands = SIDMAP.get(sid, {})
                for caprov in cands.keys():
                    if caprov.startswith('FFFF') or caprov.startswith('FFFE'):
                        continue
                    # nome KOS correto para o canal em TODOS os CAIDs candidatos
                    add_chan(chans, order, '%s:%s' % (caprov, sid), name)
                    caidpos_add(caprov.split(':')[0], pos)
                # entradas especificas da tabela curada (CAIDs confirmados)
                for pkg in pkgs:
                    if pkg in PKGCAID:
                        for caid in PKGCAID[pkg]:
                            lite['%s:000000:%s' % (caid.upper(), sid)] = 1
                            caidpos_add(caid.upper(), pos)
                        for caid in PKGCAID[pkg]:
                            PKGREP.setdefault(pkg, set())
                            PKGREP[pkg].add(caid.upper())
                    elif pkg not in PKGMAP:
                        PKGREP.setdefault(pkg + ' (wildcard)', set())
                caids = [PKGMAP[p] for p in pkgs if p in PKGMAP]
                for caprov in caids:
                    add_chan(chans, order, '%s:%s' % (caprov, sid), name)
                    lite['%s:%s' % (caprov, sid)] = 1
                    caidpos_add(caprov.split(':')[0], pos)
                    stats['mapeados'] += 1
        print('  pos-%s : ativos=%d inativos(saltados)=%d fta=%d mapeados=%d'
              % (pos, stats['ativos'] - s0['ativos'], stats['inativos'] - s0['inativos'],
                 stats['fta'] - s0['fta'], stats['mapeados'] - s0['mapeados']))

def caid_header(caid, satnames):
    desc = CAIDDESC.get(caid, '')
    if caid == 'FFFE':
        desc = 'Canais codificados (qualquer CAID)'
    elif caid == '0000':
        desc = 'Sem CAID (listas comunitarias)'
    elif not desc:
        desc = ''
    line = desc + (' ' if desc else '') + 'CAID: %s' % caid
    if satnames:
        line += ' | %s' % ', '.join(satnames)
    return [
        '############################',
        '#  ' + line,
        '############################',
    ]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--positions',
                    default='30W,13E,19.2E,28.2E,23.5E,0.8W,4.8E,1.9E,3.1E,5W,7E,9E,10E,16E,21.5E,26E,31.5E,33E,36E,39E,42E,45E,46E,52E,7W,8W,12.5W,14W,15W,22W,27.5W,34.5W,37.5W,40.5W,43.1W,45W,47.5W,55.5W,58W,61W,63W,70W',
                    help='posicoes a incluir (default: todas as principais)')
    ap.add_argument('--ftapositions', default='30W', help='posicoes cujos canais FTA entram na lista de skip (FFFF)')
    ap.add_argument('--srcdir', default=SRCDIR)
    ap.add_argument('--out', default=OUT)
    ap.add_argument('--apply', action='store_true', help='gravar + dar reload via editor HTTP (admin:admin)')
    args = ap.parse_args()

    chans = {}
    order = []
    lite = {}
    load_community(chans, order, args.srcdir)
    harvest_kos(chans, order, args.positions.split(','), [p.strip() for p in args.ftapositions.split(',')], lite)

    hdr = [
        '# ============================================================',
        '# CCcam.channelinfo - NOMES DOS CANAIS (GUI e logs)',
        '# ============================================================',
        '# So nomes - NAO controla o que abre (isso e no profiles.cfg).',
        '# Formato por linha:  CAID:PROVID:SID "NOME DO CANAL"',
        '# Exemplos:',
        '#    1814:005211:0065 "RTP 1 HD"',
        '#    1803:000000:07d4 "Polsat"',
        '# FFFF:000000:SID "FTA" = canal aberto (skip rapido, sem decode)',
        '# ============================================================',
        '# CCcam Channel Info by @Sharillas',
        '# %s' % time.strftime('%Y-%m-%d/%H:%M:%S'),
        '# Consolidado de: KingOfSat feeds ATIVOS (%s) + listas comunitarias' % args.positions,
        '# Ficheiro agrupado por CAID (cabecalhos por seccao)',
        '# Feed inativo (Occasional/data/inactive) e excluido automaticamente',
        '# ESTE FICHEIRO E GERADO pela GUI (Update Channel Info).',
        '# Edicoes manuais sao apagadas na proxima atualizacao.',
        '# ============================================================',
    ]
    # agrupar por CAID com cabecalhos (ignora caids invalidos < 0x0100)
    bycaid = {}
    for k in order:
        caid = k.split(':')[0]
        if caid in ('FFFF', 'FFFE'):
            pass
        elif int(caid, 16) < 0x100:
            continue
        bycaid.setdefault(caid, []).append(k)
    lines = list(hdr)
    for caid in sorted(bycaid.keys()):
        satnames = [SATNAMES.get(p, p) for p in sorted(CAIDPOS.get(caid, []))]
        lines.append('')
        lines += caid_header(caid, satnames)
        for k in bycaid[caid]:
            lines.append('%s "%s"' % (k, chans[k]))
    with open(args.out, 'w', encoding='ascii', errors='replace') as fh:
        fh.write('\n'.join(lines) + '\n')
    print('')
    print('FINAL: %d canais -> %s' % (len(order), args.out))
    print('feeds inativos removidos nesta execucao: %d | fta ignorados: %d | biss: %d | caid real (sidmap): %d'
          % (stats['inativos'], stats['fta'], stats['biss'], stats['caid_lite']))
    # relatorio pacote -> CAIDs (tabela curada PKGCAID)
    print('')
    print('=== RELATORIO PACOTE -> CAIDs (tabela curada, cross-check KOS) ===')
    for pkg in sorted(PKGREP.keys()):
        if pkg.endswith(' (wildcard)'):
            print('%-28s : (sem CAID confirmado - coberto pelo wildcard FFFE)' % pkg)
        else:
            print('%-28s : %s' % (pkg, ', '.join(sorted(PKGREP[pkg]))))
    # lista LITE (satelites permitidos) - agrupada por CAID
    import os
    liteout = os.path.join(os.path.dirname(args.out) or '.', 'CCcam.lite')
    with open(liteout, 'w', encoding='ascii', errors='replace') as fh:
        fh.write('# ============================================================\n')
        fh.write('# CCcam.lite - satelites permitidos (build LITE) - by @Sharillas\n')
        fh.write('# %s\n' % time.strftime('%Y-%m-%d/%H:%M:%S'))
        fh.write('# Formato: CAID:PROVID:SID\n')
        fh.write('# FFFE:000000:SID = canal codificado de posicao permitida (qualquer CAID)\n')
        fh.write('# Posicoes: %s\n' % args.positions)
        fh.write('# ============================================================\n')
        byc = {}
        for k in lite.keys():
            caid = k.split(':')[0]
            byc.setdefault(caid, []).append(k)
        for caid in sorted(byc.keys()):
            satnames = [SATNAMES.get(p, p) for p in sorted(CAIDPOS.get(caid, []))]
            fh.write('\n')
            for hl in caid_header(caid, satnames):
                fh.write(hl + '\n')
            for k in sorted(byc[caid]):
                fh.write('%s\n' % k)
    print('LITE: %d canais permitidos -> %s' % (len(lite), liteout))
    if args.apply:
        import base64
        cred = base64.b64encode(b'admin:admin').decode()
        req = urllib.request.Request('http://127.0.0.1:5500/editor?action=reread',
                                     headers={'Authorization': 'Basic %s' % cred})
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                resp = r.read().decode('utf-8', 'replace')
                print('reload: %s' % ('OK' if 'OK reread' in resp else 'resposta sem confirmacao'))
        except Exception as e:
            print('reload falhou: %s - faz reload manual (editor save ou restart)' % e)

if __name__ == '__main__':
    main()

