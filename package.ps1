# ============================================================
# MultiCS r1000 - Package Script
# Cria dist/multics-r1000/ com binarios + configs (layout VPS)
# ============================================================
$ErrorActionPreference = "Stop"

$Dist = Join-Path $PSScriptRoot "dist/multics-r1000"
$BinDir = Join-Path $Dist "bin"
$CfgDir = Join-Path $Dist "configs"

if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory -Force -Path $BinDir, $CfgDir | Out-Null

Copy-Item (Join-Path $PSScriptRoot "build/multics.x64") $BinDir
Copy-Item (Join-Path $PSScriptRoot "build/multics.x32") $BinDir
Copy-Item (Join-Path $PSScriptRoot "configs_exemplos/*") $CfgDir
Copy-Item (Join-Path $PSScriptRoot "Configs/ip2country.csv") $CfgDir

# ferramentas de atualizacao (Update Channel Info + Update SoftCam.Key)
foreach ($t in @("C:\TMP\opencode\tools_update_channelinfo.py","C:\TMP\opencode\tools_update_softcam.py")) {
    if (Test-Path $t) { Copy-Item $t $BinDir }
}

# install script para a VPS
@'
#!/bin/bash
# MultiCS r1000 v1.10.9 - instalador VPS (Debian/Ubuntu)
# Uso: sudo bash install.sh [/caminho/instalacao]
DIR=${1:-/opt/multics}
if [ "$(id -u)" -ne 0 ]; then echo "executa como root"; exit 1; fi
mkdir -p $DIR/configs /var/etc
cp bin/multics.x64 bin/multics.x32 $DIR/
cp bin/tools_update_*.py $DIR/ 2>/dev/null
cp configs/* $DIR/configs/
cp configs/* /var/etc/
chmod 755 $DIR/multics.x64 $DIR/multics.x32 $DIR/tools_update_*.py
chmod 666 /var/etc/*.cfg /var/etc/CCcam.* /var/etc/ip2country.csv /var/etc/multics.css 2>/dev/null
# servico systemd a correr como ROOT (a GUI salva/uploads/restart sem problemas
# de permissoes mesmo que a comunidade mude os ficheiros para 755)
cat > /etc/systemd/system/multics.service <<EOF
[Unit]
Description=MultiCS r1000 by Sharillas (cardserver proxy)
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=$DIR
ExecStart=$DIR/multics.x64 -C /var/etc/multics.cfg
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
systemctl daemon-reload
systemctl enable multics 2>/dev/null
echo "Instalado em $DIR (configs em /var/etc, tools junto do binario)"
echo "Arranque: systemctl start multics"
echo "Web UI:   http://IP:5500  (user/pass definidos em multics.cfg)"
'@ | Set-Content -Encoding ASCII (Join-Path $Dist "install.sh")

Write-Host "== Package criado em dist/multics-r1000 =="
Get-ChildItem $Dist -Recurse | Select-Object FullName, Length
