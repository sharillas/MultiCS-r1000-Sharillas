# ops/ - scripts de operacao da VPS (copia de /opt/multics)

Sem credenciais: o que for secreto vive fora do repo.

| Script | Para que serve | Cron |
|---|---|---|
| monitor_cw.py | monitor de CWs | cada minuto |
| v130_stats.py | estatisticas v1.30 | de hora a hora |
| estudo_stats.py / nos_estudo_stats.py | estatisticas de estudo (MEO / NOS) | cada 30 min |
| v140_monitor.py | monitor v1.40 | cada 30 min |
| watchdog-circuito.sh | vigia so-leitura do multics do circuito; credenciais em /etc/watchdog-circuito.env (ver watchdog-circuito.env.example) | cada 2 min |
| update_ip2country.py | atualiza /var/etc/ip2country.csv (base diaria iplocate) | 05:30 diario |
| crash_report.sh | relatorio de crash | manual |

Instalar o cron do ip2country:
`30 5 * * * python3 /opt/multics/update_ip2country.py /var/etc/ip2country.csv >> /var/log/ip2country-update.log 2>&1`