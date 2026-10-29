# Handoff de Sessão — MultiCS r1000 by Sharillas

> Ponto de partida para uma nova sessão continuar sem repetir trabalho.
> Ver também: `README.md`, `CHANGELOG.md`, `docs/` e `PROXIMOS_PASSOS.md`.

## Estado atual

- **Versão em produção**: v1.63.4 (VPS Debian 12, serviço systemd `multics`)
- **Repo GitHub**: sincronizada com produção — tags `v1.63.2` e `v1.63.4`
  publicadas; `main` = código da v1.63.4 + CHANGELOG atualizado
- **Repos relacionados**: `cak7-lab` (ferramentas fase-1, estudo CAK7)
- **Sessão anterior**: terminou com overflow de contexto; este ficheiro
  resume o estado para retomar

## Layout do projeto

| Local | Conteúdo |
|---|---|
| `src/` | Código C do MultiCS r1000 (66 .c + headers) |
| `Configs/` | Configs de referência (multics.cfg, profiles.cfg, CSS, ip2country...) |
| `configs_exemplos/` | Todas as configs comentadas em PT |
| `docs/` | Estudos, docs de config, implementação |
| `tools/` | Scripts auxiliares (update channelinfo/SoftCam/ip2country) |
| `gui_embedded_*` | CSS/JS embutidos da GUI |
| `build.sh` / `build.ps1` | Build musl estático (Linux) / build Windows |
| `.github/workflows/build-musl.yml` | CI de build musl |

## Máquina de produção (VPS)

- Binário: `/opt/multics/multics.x64` (musl estático)
- Configs: `/var/etc/`
- Serviço: `systemctl start|stop|status|restart multics`
- **Source tree não-commitada**: `/opt/multics-src` (baseada no upstream
  `multi-cs/multics` com as alterações Sharillas aplicadas por cima) —
  **não é a fonte de verdade**; a fonte de verdade é a repo GitHub
- Backups de binários históricos: `/opt/multics/*.bak-*`

> ⚠️ Não colocar IPs, credenciais ou tokens na repo pública — usar
> placeholders nos exemplos (regra já aplicada em commits anteriores).

## Pendências confirmadas (ordem sugerida)

1. **Login guard (brute-force da GUI)** — 5 falhas/30s por IP, tabela de 64
   entradas + throttle global, configurável no multics.cfg
2. **Fuzz hardening do parser HTTP** — harness contra /login, query strings,
   headers e páginas da GUI (zero crashes/hangs)
3. **Responsividade mobile** — tabelas com scroll horizontal < 900px
4. Sessões GUI: "Terminar todas as sessões" + invalidação ao mudar password
5. Rotação de segredos no fecho: token GitHub novo (permissões mínimas,
   expiração), password VPS nova, segredos fora de scripts/repo

## Como retomar

```bash
git clone https://github.com/sharillas/MultiCS-r1000-Sharillas.git
cd MultiCS-r1000-Sharillas
./build.sh            # build musl estático (binários para build/)
```

- Deploy para a VPS: `deploy.ps1` (preencher as variáveis localmente)
- CI: `build-musl.yml` corre em cada push (build + smoke test)
- Versão: `src/common.h` → `VERSION_STR`

## Histórico de versões relevantes

- v1.60 — editor linha-a-linha, filtros anti-veneno 1802, árbitro
- v1.61 — dashboard Fair-Share/Proteções
- v1.62 — SHEEP DETECTOR, coluna Truth, VERDICT end-to-end
- v1.63.2 — fix crítico do consenso invertido (NOS negra)
- v1.63.4 — filtro de paridade (desativado), estado final de produção
