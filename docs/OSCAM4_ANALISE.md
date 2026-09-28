# Análise do oscam4 — o que temos de implementar

O oscam4 = um **oscam profundamente modificado** (não é o oscam normal) — é a
ponte entre as linhas quase-directas e a malha cacheex. O que tem lá dentro:

## O que o oscam4 tem (strings do binário)

| Feature | O que é |
|---|---|
| `cs378x` + `cs357x` | O protocolo cacheex da família multics — fala connosco ✓ |
| `cacheex` com **CWC (CE)** | O cacheex dele leva o **cycletime** (criptoperíodo) nos pushes: `CWC (CE) push to %s cycletime: %i` |
| `csp` | O protocolo CSP (o cache do oscam — compatível com o nosso CACHE UDP "Csp-Cache") |
| `CAK7 mode` | A mesma transformação CAK7 Merlin que nós temos |
| `CWPK` | Control Word Protection Key (removemos na v1.40) |
| `cacheex_cw_check_for_push` | Valida as CWs **antes** de as empurrar |
| `cacheex_maxhop` / `_lg` | O limite de hops do cacheex |
| `cacheex_drop_csp` | Descarta entradas CSP más |
| Readers | `mouse` (cartões físicos!), cccam, newcamd, radegast, cs357x, constcw, emu, dvbapi |

## O que temos de implementar (por prioridade)

1. **O cycletime dos pushes cacheex → alimentar o nosso motor**
   - O oscam4 empurra o criptoperíodo real de cada CW (o `cycletime`)
   - Hoje o nosso motor aprende a cadência só pelas entregas locais
   - Implementar: ao receber um push cacheex com cycletime, alimentar a cadência do `dcwchan` do canal — o motor passa a saber o período REAL da fonte, não o esticado da malha
   - É a peça central do "cacheex afinado"

2. **Validação anti-lixo dos hits cacheex** (o equivalente ao `cacheex_cw_check_for_push` dele)
   - Hoje as CWs que chegam por cacheex não passam pela validação do motor (o caso do "bad dcw ch 17a1")
   - Implementar: as CWs recebidas por cacheex passam pelos mesmos filtros (estrutura, cadência, alternância) antes de serem aceites/entregues

3. **O csp no nosso CACHE** — confirmar o interop
   - O nosso CACHE UDP = o "Csp-Cache" (já fala CSP)
   - Testar o peer com o oscam4 (`csp=yes` nas linhas) — pode ser mais uma via das CWs limpas

4. **O CAK7** — garantir compatibilidade
   - Ambos têm o CAK7 Merlin — validar que as CWs transformadas batem certo nos dois lados

5. **O CWPK** (para discussão)
   - Removemos na v1.40; o oscam4 usa-o (as chaves NAGRA)
   - Ponderar re-adicionar ou ignorar — depende do papel que tem na malha dele

## Topologia actualizada (com o oscam4)

```
oscam4 (linhas quase-directas: mouse/cccam + emu + CAK7/CWPK)
   └─ cacheex (cs378x + CWC cycletime) ─→ multics (principal, 30W) ─→ nós
multics1 (satélites estrangeiros)
```

O fluxo das CWs mais limpas = oscam4 → cacheex → multics → nós. Por isso o
nosso cacheex tem de receber, validar e usar o cycletime desses pushes.
