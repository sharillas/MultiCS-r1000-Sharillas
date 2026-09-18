# Configuração do Cardserver — guia exacto (v1.29)

> **A tabela completa de todas as opções (DCW/ECM/SID/CACHE/NAGRA/TIMING/HEALTH/FALLBACK/protecções) está no [TUTORIAL.md](../TUTORIAL.md)** — este guia resume a estrutura de ficheiros e o fluxo.

Os ficheiros ficam em `/var/etc/` (depois do `install.sh`). Qualquer alteração pode ser feita pela **GUI (Configs → Edit ou Upload)** — aplica na hora, sem restart.

## Estrutura de ficheiros (v1.29)

| Ficheiro | Função |
|---|---|
| `multics.cfg` | mestre: portas, credenciais HTTP/TELNET, FILEs, INCLUDEs |
| `profiles.cfg` | perfis de saída (portas virtuais por CAID) + todas as opções |
| `servidores.cfg` | readers (N:/C:/L:/CACHE/CACHEEX) com `{ name=... }` |
| `clientes_cccam.cfg` | F-lines (clientes CCcam) |
| `clientes_mgcamd.cfg` / `clientes_camd35.cfg` / `clientes_cs378x.cfg` / `clientes_cache.cfg` | clientes dos outros protocolos |
| `Softcam.cfg` | chaves CONSTCW (BISS/Tandberg) |
| `blocked_ips.cfg` | IPs bloqueados |
| `CCcam.channelinfo` | nomes dos canais (alimenta o feed e o last-used-share) |
| `CCcam.providers` | nomes dos providers |
| `CCcam.lite` | BUILD LITE: lista de canais activos |
| `multics.css` | tema externo (copiar de `../Configs/`) |

> Ordem de parsing: perfis antes dos clientes. A GUI resolve os caminhos a partir da config em execução — funciona em qualquer layout.

## Fluxo de um pedido (v1.29)

cliente → perfil (CAID/PROV/SID aceites) → cache/static (keep CW) → readers (load-balance: priority, val, hops, FALLBACK ORDER, health) → CW → **checksum gate** (CW lixo não entregue) → entrega ao cliente; em falha: `LASTCWONNOK` + `SILENT_NOK`.
