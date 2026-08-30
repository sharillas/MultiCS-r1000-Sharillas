# ============================================================
# deploy.ps1 - Build + Deploy para a VPS de producao
# Uso: .\deploy.ps1
# Passos: build (zig musl) -> pscp -> backup -> restart -> verifica
# Requisitos: plink/pscp em C:\TMP\opencode, VPS acessivel por SSH
# ============================================================
$ErrorActionPreference = "Stop"

# --- CONFIG (ajusta se necessario) ---
$VPSHost = "187.124.172.185"
$VPSUser = "root"
$VPSPass = "114494"
$VPSBin  = "/opt/multics/multics.x64"
$VPSBin32 = "/opt/multics/multics.x32"
$ToolDir = "C:\TMP\opencode"
$Root    = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "== 1. Build =="
cmd /c "powershell -ExecutionPolicy Bypass -File `"$Root\build.ps1`" > $ToolDir\bl.log 2>&1"
if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FALHOU:" -ForegroundColor Red
    Get-Content "$ToolDir\bl.log" -Tail 15
    exit 1
}
Write-Host "   build OK" -ForegroundColor Green

Write-Host "== 2. Upload =="
& "$ToolDir\pscp.exe" -pw $VPSPass -q "$Root\build\multics.x64" "$VPSUser@${VPSHost}:/tmp/multics.x64.new"
if ($LASTEXITCODE -ne 0) { Write-Host "pscp x64 falhou"; exit 1 }
& "$ToolDir\pscp.exe" -pw $VPSPass -q "$Root\build\multics.x32" "$VPSUser@${VPSHost}:/tmp/multics.x32.new"
Write-Host "   upload OK" -ForegroundColor Green

Write-Host "== 3. Deploy + restart =="
$remote = @"
systemctl stop multics
cp -a $VPSBin ${VPSBin}.bak-deploy-`$(date +%s)
mv /tmp/multics.x64.new $VPSBin
chmod 755 $VPSBin
if [ -f /tmp/multics.x32.new ]; then cp -a $VPSBin32 ${VPSBin32}.bak-deploy-`$(date +%s); mv /tmp/multics.x32.new $VPSBin32; chmod 755 $VPSBin32; fi
systemctl reset-failed multics 2>/dev/null
systemctl start multics
sleep 5
systemctl is-active multics
"@
& "$ToolDir\plink.exe" -ssh "$VPSUser@$VPSHost" -pw $VPSPass -batch $remote
if ($LASTEXITCODE -ne 0) { Write-Host "deploy remoto falhou"; exit 1 }

Write-Host "== 4. Verifica (GUI + stats) =="
Start-Sleep -Seconds 10
$chk = @"
curl -s -m 8 -c /tmp/ck -b /tmp/ck -d 'user=admin&pass=admin' http://127.0.0.1:5500/login -o /dev/null
curl -s -m 8 -c /tmp/ck -b /tmp/ck http://127.0.0.1:5500/packages -o /tmp/deploy_check.html -w '%{http_code} %{size_download}\n'
top -bn1 | grep -m1 multics
"@
& "$ToolDir\plink.exe" -ssh "$VPSUser@$VPSHost" -pw $VPSPass -batch $chk
Write-Host "== DEPLOY COMPLETO ==" -ForegroundColor Green
