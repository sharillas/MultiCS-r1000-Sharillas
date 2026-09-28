# O que falta para a nossa ser a melhor (backlog v1.45+)

Documento de trabalho — pontos identificados na análise das builds do circuito.
Nada disto é implementado sem discussão prévia. Standby para a próxima versão.

## 1. CacheEX — fechar a lacuna com a linhagem dele

| Ponto | O que é | Fonte |
|---|---|---|
| CACHEEX VALIDECMTIME configurável | A validade do hit no cacheex (nós temos o 7000ms fixo; o r84 SkyDEv15 tem-na configurável) | r84v15 |
| cacheex_forward refinado (mode 1/2) | O forward do r84v15 distingue o reenvio simples (1) do "extrasrv" (2) — nós restaurámos o simples | r84v15 |
| Protocolo CSP no CACHE UDP | O modo compat-oscam dos peers cache (o `csp=yes` das linhas dele) | malha |

## 2. Anti-lixo por ident — automático

- Hoje o filtro dos idents (ex.: sem o `000007` do MEO) = manual nos `shares=`/`PROVIDERS`
- Proposta: o motor aprende os idents bons por CAID (como já aprende a cadência) e marca os idents que trazem lixo — filtro automático
- O "bad dcw ch 17a1" que o log apanhou a vir pelo cacheex = o caso de uso

## 3. O motor de ciclo NA FONTE (o argumento para o colega)

- A nossa build = a única com o CYCLE ENGINE + o HEALTH + o MINECMS
- Posta na fonte (o multics principal dele), filtra o caos da malha para TODOS os amigos
- É o argumento central do benchmark: mesma posição, números melhores

## 4. Identificação limpa nos protocolos (em curso)

- CCcam: flag WHO removido + nodeid aleatório (feito — a validar no lado dele)
- Newcamd/Mgcamd: provid multics removido (feito)
- Pendente: confirmar que o servidor r84 dele mostra as versões limpas

## 5. Analisar o oscam4 (a mod dele)

- O oscam4 = um oscam com cs378x/cacheex/camd35 embutidos (a malha dele inclui oscams?)
- Vale estudar: que papel tem + se há features úteis (o CAK7 dele, os readers)

## 6. Benchmark A/B formal (nossa vs r84 SkyDEv15 na fonte)

- X dias cada build no multics principal dele, mesmas métricas:
  - freezes na VU (watchdog), anomalias `cyc:`, cwbad, tempos `found`, hits do cacheex
- Pré-requisito: relançar o watchdog da VU (morreu com a limpeza do TMP)

## 7. GUI — retoques pendentes

- Tooltip dos grafos da CW Monitoring (feito) — validar o render
- A coluna "Profile/Hits" dos peers do cache (movida) — validar com dados reais
- Os botões ON/OFF/DBG das páginas cs378x/camd35 (herdados da v1.30 — testar)

## 8. Observações da análise (não-acções, para a discussão)

- O CACHE STATIC da linhagem dele = o que NÓS removemos por servir CWs velhas — manter removido
- O multics1 dele = um relay redundante (poderia ser consolidado no principal)
- O oscam--ok/oscam4 = a peça emu (BISS/constcw) — fora do nosso âmbito
