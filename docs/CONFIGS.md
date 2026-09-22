# ConfiguraÃ§Ã£o do Cardserver â€” guia exacto (v1.40)

> **A tabela completa de todas as opÃ§Ãµes (DCW/ECM/SID/CACHE/NAGRA/TIMING/HEALTH/FALLBACK/protecÃ§Ãµes) estÃ¡ no [TUTORIAL.md](../TUTORIAL.md)** â€” este guia resume a estrutura de ficheiros e o fluxo.

Os ficheiros ficam em `/var/etc/` (depois do `install.sh`). Qualquer alteraÃ§Ã£o pode ser feita pela **GUI (Configs â†’ Edit ou Upload)** â€” aplica na hora, sem restart.

## Estrutura de ficheiros (v1.40)

| Ficheiro | FunÃ§Ã£o |
|---|---|
| `multics.cfg` | mestre: portas, credenciais HTTP/TELNET, FILEs, INCLUDEs |
| `profiles.cfg` | perfis de saÃ­da (portas virtuais por CAID) + todas as opÃ§Ãµes |
| `servidores.cfg` | readers (N:/C:/L:/CACHE/CACHEEX) com `{ name=... }` |
| `clientes_cccam.cfg` | F-lines (clientes CCcam) |
| `clientes_mgcamd.cfg` / `clientes_cache.cfg` | clientes dos outros protocolos |
| `blocked_ips.cfg` | IPs bloqueados |
| `CCcam.channelinfo` | nomes dos canais (alimenta o feed e o last-used-share) |
| `CCcam.providers` | nomes dos providers |
| `CCcam.lite` | BUILD LITE: lista de canais activos |
| `multics.css` | tema externo (copiar de `../Configs/`) |

> Ordem de parsing: perfis antes dos clientes. A GUI resolve os caminhos a partir da config em execuÃ§Ã£o â€” funciona em qualquer layout.

## Fluxo de um pedido (v1.40)

cliente â†’ perfil (CAID/PROV/SID aceites + `SERVERS:` explÃ­cito se definido) â†’ cache â†’ readers (load-balance: `hop=` directa primeiro, health com cwbad, bad-cw cache por canal, priority) â†’ CW â†’ **validaÃ§Ã£o estrutural** (checksum/nagra) â†’ **CYCLE ENGINE** (stale hold + anomalias â†’ reputaÃ§Ã£o da fonte) â†’ entrega ao cliente; em falha: `LASTCWONNOK` + `SILENT_NOK`; feedback do cliente (hash repetido apÃ³s entrega) marca a fonte.

