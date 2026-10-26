# Estudo ECM→CW da NOS (CAID 1802) — RTP 1 HD (SID 0097)

Documento de referência do estudo de reverse engineering da troca ECM→CW,
baseado no arquivo de pares limpos da linha CCcam_Clean (ip:porta),
10-13 Out 2026.

## 1. O circuito (feed → box → card → descrambler)

```
Feed SAT  ──> Box (enigma/oscam) ──ECM (146 bytes)──> Cartão CAK7 ──CW (16 bytes)──> Descrambler CSA
```

- O feed emite ECMs a cada ~7.5s, alternando nano (0x80/0x81).
- A box pede 1 ECM por nano; o cartão devolve **1 metade** da CW por pedido.
- A box junta a metade par + a metade ímpar = chave CSA completa.

## 2. Estrutura do ECM (146 bytes = 0x92, NOS 1802)

```
Offset 0:    0x80 | 0x81  -> nano (paridade): qual metade da CW este ECM calcula
Offset 1-4:  30 8F 07 8D   -> cabeçalho fixo (constante nos 1785 ECMs)
Offset 5-145: payload      -> cifra 3DES-CBC com as chaves CAK7 do cartão
```

Exemplo real (10/10 01:05:55, abriu a RTP1):

```
81 30 8F 07 8D 7B 62 23 2A 5F 51 8E 88 60 A6 12 57 85 44 53 ...
```

## 3. Estrutura da CW (16 bytes)

```
metade0 (par)    metade1 (ímpar)
[ 8 bytes ][ 8 bytes ]
cada metade = 2 grupos CAK7 de 4 bytes: 3 data + 1 soma
   byte[3] = (byte[0] + byte[1] + byte[2]) & 0xFF   (idem 7, 11, 15)
```

Exemplo: CW `16 87 8D 2A DC 62 9C DA | 2A 85 3C EB 1B B4 33 02`
`16+87+8D = 2A` ✓  `DC+62+9C = DA` ✓  `2A+85+3C = EB` ✓  `1B+B4+33 = 02` ✓

## 4. Regra da paridade (a descoberta-chave)

Extraída dos 1785 pares limpos, com **zero excepções**:

| ECM byte0 | regra |
|---|---|
| 0x80 | metade0 MUDA, metade1 persiste (670/670 transições) |
| 0x81 | metade0 persiste, metade1 MUDA (671/671 transições) |

Ambas as metades mudam apenas após gaps > ~40s (12.5% dos casos — reinício
de ciclo). Sequência real do arquivo:

```
01:05:55 ecm=81 -> CW [16878D2A...][2A853CEB...]   (metade1 fresca)
01:06:04 ecm=80 -> CW [16878D2A...][AD389479...]   (metade0 persiste)
01:06:19 ecm=81 -> CW [2C30EB47...][AD389479...]   (metade1 persiste)
01:06:34 ecm=80 -> CW [2C30EB47...][E7A448D3...]   (metade0 persiste)
```

## 5. O cálculo criptográfico (o que se sabe e o que não)

- O cartão desencripta o payload (3DES-CBC) com as chaves CAK7; lê o seed
  do texto claro; deriva a metade da CW para o nano pedido; soma o checksum
  dos grupos CAK7.
- **A CW não é o payload decifrado** — é um produto dele (relação de mão única).
- O mapeamento ECM→CW é **determinístico** (1785 ECMs → 1785 CWs, 0 colisões),
  com difusão total: ~94% dos bytes do ECM mudam a cada transição de CW.

### Testes feitos ao payload (todos infrutíferos, como esperado)

| teste | resultado |
|---|---|
| Blocos de 8 bytes repetidos (ECB leak) | 0 em 30.345 blocos → modo CBC, nada vaza |
| Checksum do ECM (CRC32/Adler/soma/XOR no tail) | nenhum candidato bate |
| Known-plaintext sobre 3DES com pares ECM→CW | impraticável (espaço de chaves 2^112) |

**Conclusão**: sem as chaves CAK7 do cartão, o par (ECM, CW) é inexplorável —
é exactamente essa a função do cartão. A CW e o payload são distinguíveis
estatisticamente como aleatórios; o único campo legível do ECM é o nano +
cabeçalho fixo.

## 6. Impacto prático no R1000

- **Consenso (v1.63.2)**: o bug da comparação invertida (`!dcwcmp16`) rejeitava
  CWs correctas — corrigido; era a causa da NOS permanentemente negra.
- **Filtro de paridade (v1.63.4, `#if 0` em setdcw.c)**: a regra da secção 4
  implementada mas desactivada — as fontes actuais servem a metade errada com
  frequência (lag de 1 período) e rejeitar esfomeia a box (34% entregas).
  Reactivar quando existir uma fonte limpa como referência.
- A regra da paridade é a arma para quando uma fonte limpa voltar: permite
  detectar em tempo real quem serve a metade certa para o nano pedido.

## 7. Dados

- Arquivo clean: `/opt/estudo_clean_0097.json` (VPS) — 1785 pares `{date, time, ecm, cw}`.
- Fonte: `DEBUG - <= cw from CCcam server (IP-do-colega:10300)- 1802:000000:0097/ECM => CW`
  no `/var/log/multics.log` (sem rotação — guarda a era clean desde 3 Out).
- Scripts do estudo: `/tmp/dissect1.py`, `/tmp/payload1.py`, `/tmp/parity_map.py` (VPS).
