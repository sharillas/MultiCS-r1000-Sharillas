#!/usr/bin/env python3
# corrigir profiles.cfg: comentar linhas de explicacao + remover [DEFAULT] falso
import re

p = '/var/etc/profiles.cfg'
src = open(p, encoding='utf-8', errors='replace').read()

specials = {
    '[DEFAULT] define valores por defeito para todos os perfis.',
    'SKIPCWC (Skip Same CW): ignora CWs exatamente iguais ao anterior do',
    'E os servidores que alimentam este perfil (formato de leitura):',
    'SERVER: host porta user pass { protocolo e opcoes }',
    'Para CSAT/TNTSAT nano e0 activar SKIPCWC APENAS neste perfil:',
    'EXEMPLO DE PERFIL: SKY DE 098D',
}

out = []
for line in src.split('\n'):
    s = line.strip()
    if not s:
        out.append(line)
        continue
    if s in specials:
        out.append('# ' + line)
        continue
    if s[0] in '#[':
        out.append(line)
        continue
    # opcao valida: KEY: valor  (ou DEFAULT KEY: valor)
    if re.match(r'^(DEFAULT\s+)?[A-Z][A-Z0-9 _\-/]*:', s):
        out.append(line)
        continue
    # resto = explicacao -> comentar
    out.append('# ' + line)

open(p, 'w', encoding='utf-8').write('\n'.join(out))
print('ok')
