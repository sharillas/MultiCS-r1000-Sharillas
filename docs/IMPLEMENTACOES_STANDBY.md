# Lista total de implementações em standby

(Compilada a 28/09 — nada se implementa sem discussão prévia.)

## A. CacheEX — a afinação a 100% (prioridade máxima, do pedido do colega)

| # | Implementação | Fonte/porquê |
|---|---|---|
| 1 | **O cycletime dos pushes cacheex → alimentar o motor**: ler o `cycletime` que o oscam4 empurra (o "CWC CE") e usá-lo como cadência real do canal no `dcwchan` — o motor deixa de depender só do aprendizado local | oscam4 |
| 2 | **Validação anti-lixo dos hits cacheex**: as CWs recebidas por cacheex passam os mesmos filtros do motor (estrutura/cadência/alternância) antes de aceites — o caso "bad dcw ch 17a1" | oscam4 (`cacheex_cw_check_for_push`) |
| 3 | **CACHEEX VALIDECMTIME configurável** (hoje 7000ms fixo) | r84 SkyDEv15 |
| 4 | **cacheex_forward refinado** (os modos 1/2, o "extrasrv" do r84v15) | r84 SkyDEv15 |
| 5 | **CSP no CACHE**: testar/activar o peer directo com o oscam4 (`csp=yes`) — via adicional das CWs limpas | oscam4 (`csp`) |
| 6 | **CAK7**: validar a compatibilidade da transformação com a do oscam4 | oscam4 |
| 7 | **CWPK**: decidir se re-adicionamos (removido na v1.40; o oscam4 usa-o) | discussão |

## B. Anti-lixo por ident — automático

| # | Implementação |
|---|---|
| 8 | O motor aprende os **idents bons por CAID** e marca os que trazem lixo (o 000007) — filtro automático em vez do `shares=` manual |

## B2. STRICT PROVIDER — v1.47 (IMPLEMENTADO e compilado — standby, não deployado)

**Conclusão do estudo (30/09):** os idents MEO são streams com chaves PRÓPRIAS —
`1814:005211`, `1814:000007` (ID_SAT) e `1814:000000` (wildcard/prov 0) têm
Jaccard por hora ≈ 0 (chaves independentes). Os 70 SIDs do 000007 existem todos
também no 005211 — o 000007 não é "só para certos canais"; é o ident alternativo
dos MESMOS canais com scrambling separada. O receptor pede UM ident (o do PMT).

**O mecanismo dos freezes:** o multics cruzava as CWs entre providers do mesmo
SID (o `cache_fetch_samechannel` "same channel with different hash and provider"
com o check do provid COMENTADO + os `cache_check_cw/samecw` sem provid) — a
chave do 000007 ia parar à cache do 005211 (e vice-versa) = chave errada = freeze.

**O que a v1.47 faz (feito e compilado, x64+x32):**
- Nova opção `CACHE STRICTPROVID: YES/NO` (default **YES**)
- O isolamento nos 4 pontos: `cache_fetch` (o hit principal), `cache_fetch_cycle`
  (já tinha), `cache_check_cw` (assinatura + `ecm->provid` no chamador setdcw.c),
  `cache_check_samecw`, `cache_fetch_samechannel` (o comentado reactivado)
- A regra: cross-provider só quando um dos dois provid = 0 (o wildcard — o caso
  "csp luck of provid" mantém-se)
- `VERSION_STR` = 1.47

**Deploy:** quando aprovar — o build está em `build\multics.x64` (a config da
produção pode levar `CACHE STRICTPROVID: YES` explícito; sem a linha o default
já é YES).

## C. GUI — retoques pendentes

| # | Implementação |
|---|---|
| 9 | Validar o render dos grafos da CW Monitoring (tooltips) com dados reais |
| 10 | Testar os botões ON/OFF/DBG das páginas cs378x/camd35 (herdados da v1.30) |
| 11 | Validar a coluna Profile/Hits dos peers do cache com dados reais |

## D. Identificação nos protocolos (trabalho feito — validação pendente)

| # | Implementação |
|---|---|
| 12 | Confirmar no servidor do colega que as nossas linhas mostram as versões limpas (CCcam v2.3.0, sem "MCS") |

## E. Fecho da v1.45 (o acumulado todo — pronto a fechar quando mandares)

- CacheEX cs378x/camd35 restaurados (client + server, mode 2/3)
- Fix do AES (o loop das rondas mutilado na v1.40 — passa o vector RFC)
- Fix do CPU (as pipes cs378x em falta — busy-spin de 93%)
- O forward do pull + o fallback parse
- Calibração do motor (clamp de cadência 18xx + gate de stales 3/60s)
- Auto-recuperação (DCW BADCW RECONNECT)
- GUI: páginas Cs378x/Camd35, CacheEX com os novos tipos, cache header, sticky headers, grafos
- Identificação limpa (WHO flag + nodeid aleatório + provid multics removidos)
- Configs: clientes_cs378x.cfg/camd35.cfg + servidores.cfg + AUTOADD NO + shares por ident

## F. Operacional

| # | Tarefa |
|---|---|
| 13 | Watchdog da VU: o processo sobreviveu à limpeza do TMP — mas o ficheiro .py está no TMP; se o PC reiniciar, relançar (recriar o script em local seguro) |
| 14 | A verificação sem benchmark (a via alternativa): medir tudo do NOSSO lado — freezes na VU, anomalias, cwbad, hits cacheex, tempos `found` — a observação contínua já montada |
