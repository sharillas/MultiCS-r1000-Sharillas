#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================
# tools_update_softcam.py - MultiCS r1000 by @Sharillas
# Descarrega o SoftCam.Key mais recente (MOHAMED19OS/SoftCam_Emu)
# e envia para o /emulator da GUI (POST multipart = o mesmo que o
# botao Convert & Load). O parse e feito pelo C, que grava no
# Softcam.cfg da config em execucao (qualquer layout de pastas).
#
# Uso: python3 tools_update_softcam.py [--port 5500] [--user admin] [--password admin]
# ============================================================
import sys
import argparse
import base64
import urllib.request
import uuid

URL = 'https://raw.githubusercontent.com/sharillas/SoftCam_Emu/main/SoftCam.Key'

def strip_banner(data):
    # remove o cabecalho do autor (linhas #### ... ####) - fica so o conteudo
    lines = []
    in_banner = False
    for raw in data.decode('utf-8', 'replace').splitlines():
        if raw.startswith('####'):
            in_banner = not in_banner
            continue
        if in_banner:
            continue
        lines.append(raw)
    return ('\n'.join(lines) + '\n').encode('ascii', 'replace')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, default=5500)
    ap.add_argument('--user', default='admin')
    ap.add_argument('--password', default='admin')
    args = ap.parse_args()

    try:
        req = urllib.request.Request(URL, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=60) as r:
            data = r.read()
        data = strip_banner(data)
        print('SoftCam.Key descarregado: %d bytes (banner do autor removido)' % len(data))
    except Exception as e:
        print('download falhou: %s' % e)
        sys.exit(1)

    # POST multipart para o /emulator (igual ao upload do browser)
    boundary = '----multics%s' % uuid.uuid4().hex
    body = (b'--' + boundary.encode() +
            b'\r\nContent-Disposition: form-data; name="softcamkey"; filename="SoftCam.Key"\r\n'
            b'Content-Type: text/plain\r\n\r\n' + data +
            b'\r\n--' + boundary.encode() + b'--\r\n')
    cred = base64.b64encode(('%s:%s' % (args.user, args.password)).encode()).decode()
    req = urllib.request.Request(
        'http://127.0.0.1:%d/emulator' % args.port,
        data=body,
        headers={
            'Authorization': 'Basic %s' % cred,
            'Content-Type': 'multipart/form-data; boundary=%s' % boundary,
        })
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            resp = r.read().decode('utf-8', 'replace')
            print('apply: OK (redirect para /emulator)')
    except Exception as e:
        print('apply falhou: %s' % e)
        sys.exit(1)

if __name__ == '__main__':
    main()
