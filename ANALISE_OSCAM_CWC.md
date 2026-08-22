# Análise OSCam — CW Cycle Check (para implementação futura)

Fonte: https://git.streamboard.tv/common/oscam — módulo `module-cw-cycle-check.c`

## O que o OSCam faz

Estado por canal (caid:provid:sid:chid) numa lista:
- última CW, timestamp, máquina de estados (stage 0→3 + 4), cycletime aprendido
  (intervalo entre mudanças de CW), dyncycletime (jitter), nextcyclecw
  (qual metade muda a seguir: CW0/CW1), histórico dos últimos 15 ECMs
  (md5/csp_hash + CW associada).

Aprendizagem (stage 0-2): mede o tempo entre CWs; intervalo estável (±2s)
-> stage 3 (verificar). Stage 4 = janela "keep" (só verifica qual metade cicla).

Verificações em stage 3:
1. Same CW demasiado tarde (fora de cycletime - dyncycletime) -> drop (replay/fake)
2. Ciclo inválido: as DUAS metades mudaram ao mesmo tempo -> bad cycle
3. countCWpart + cwcycle_sensitive: a metade que NÃO devia mudar tem
   demasiados bytes iguais à anterior -> inválida ("too like old one")
4. ECM antigo (replay): hash do ECM nos últimos 15 -> old ECM -> drop
5. Exceções: allowbadfromffb (fallback readers), isenção CAIDs half-cycle
   (NDS/videoguard), usecwcfromce (salta aprendizagem se o peer CacheEX
   envia cycletime/nextcycle no protocolo)

Comentário-chave do código:
```
D41A1A08B01DAD7A 0F1D0A36AF9777BD found -> ok
E9151917B01DAD7A 0F1D0A36AF9777BD found last -> wrong (freeze), mas para cwc é ok
7730F59C6653A55E D3822A7F133D3C8C cwc bad -> mas cw é certa, cwc fora de passo
```
Ou seja: distingue "CW certa mas ciclo errado" de "CW certa e ciclo certo".

Config (globals.h): cwcycle_check_enable, cwcycle_check_caidtab,
maxcyclelist, onbadcycle, cwcycle_dropold, cwcycle_sensitive,
cwcycle_allowbadfromffb, cwcycle_usecwcfromce, keepcycletime.
Stats por cliente: cwcycledchecked/ok/nok/ign.

## Plano para o MultiCS (quando implementarmos)

1. Módulo CWC no `cache_setdcw` (clustredcache.c) — chokepoint único que
   cobre CSP, cacheex e clientes:
   - lista por canal com a mesma máquina de estados 0-4
   - checks: same-CW-tarde, ciclo inválido, similaridade da metade fixa,
     replay de ECM antigo (já temos pcache->hash/ecmd5)
   - seed do stage 3 via byte cwcycle do protocolo CacheEX
   - config por perfil: CWC ENABLE / CWC CAID / CWC SENSITIVE / CWC DROPOLD /
     CWC KEEPCYCLETIME
   - stats na GUI (checked/ok/bad)
2. Complementos: regra de timing (CW igual fora da janela = drop) e
   histórico de ECMs (replay) — hoje só comparamos igualdade de CW.
3. Não copiar: verificação por cliente do oscam; no multics basta por canal.

Estimativa: 2-3 dias (struct + máquina de estados + parse + GUI).
