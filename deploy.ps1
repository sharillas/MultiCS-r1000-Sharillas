# ============================================================
# deploy.ps1 - Build + Deploy para a VPS de producao
# Uso: .\deploy.ps1
# Passos: build (zig musl) -> pscp -> backup -> restart -> verifica
# Requisitos: plink/pscp em C:\TMP\opencode, VPS acessivel por SSH
# SEGREDOS: as credenciais vivem em deploy.secrets.ps1 (NUNCA no
#           git - esta no .gitignore). Exemplo do ficheiro:
#             $VPSHost = "1.2.3.4"
#             $VPSUser = "root"
#             $VPSPass = "a-tua-password"
#             $GuiUser = "admin"
#             $GuiPass = "a-tua-password-gui"
# ============================================================
$ErrorActionPreference = "Stop"

# --- CONFIG ---
$VPSHost = ""
$VPSUser = ""
$VPSPass = ""
$GuiUser = ""
$GuiPass = ""
$ToolDir = "C:\TMP\opencode"
$Root    = Split-Path -Parent $MyInvocation.MyCommand.Path
# credenciais: $env:MULTICS_SECRETS, ou ..\..\_secrets\ (fora do repo), ou junto do script
$SecretsFile = @(
	$(if ($env:MULTICS_SECRETS) { $env:MULTICS_SECRETS }),
	(Join-Path $Root "..\..\_secrets\deploy.secrets.ps1"),
	(Join-Path $Root "deploy.secrets.ps1")
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if ($SecretsFile) {
	. $SecretsFile
}
if (!$VPSHost -or !$VPSUser -or !$VPSPass -or !$GuiUser -or !$GuiPass) {
	Write-Host "ERRO: cria o ficheiro deploy.secrets.ps1 (gitignored) com `$VPSHost/`$VPSUser/`$VPSPass/`$GuiUser/`$GuiPass" -ForegroundColor Red
	exit 1
}
$VPSBin  = "/opt/multics/multics.x64"
$VPSBin32 = "/opt/multics/multics.x32"

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
# os scripts remotos vao para o bash da VPS: com core.autocrlf (checkout CRLF no Windows) o \r ficaria colado
# aos nomes de ficheiro/comandos, por isso e sempre removido antes de enviar
$remote = $remote -replace "`r",""
& "$ToolDir\plink.exe" -ssh "$VPSUser@$VPSHost" -pw $VPSPass -batch $remote
if ($LASTEXITCODE -ne 0) { Write-Host "deploy remoto falhou"; exit 1 }

Write-Host "== 4. Verifica (GUI + stats) =="
Start-Sleep -Seconds 10
# password via ficheiro (base64): caracteres especiais na password partiam o curl -d inline
$loginB64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes("user=$GuiUser&pass=$GuiPass"))
$chk = @'
echo @@LOGIN@@ | base64 -d > /tmp/lg.txt; chmod 600 /tmp/lg.txt
rm -f /tmp/ck
c=$(curl -s -m 8 -c /tmp/ck -b /tmp/ck --data @/tmp/lg.txt http://127.0.0.1:5500/login -o /dev/null -w '%{http_code}')
rm -f /tmp/lg.txt
echo "GUI login: HTTP $c (302 = OK)"
fail=0; [ "$c" = "302" ] || fail=1
for p in /dashboard /profiles /iptables /favicon.ico; do
  c=$(curl -s -m 8 -b /tmp/ck http://127.0.0.1:5500$p -o /dev/null -w '%{http_code}')
  echo "GET $p: HTTP $c"
  [ "$c" = "200" ] || fail=1
done
rm -f /tmp/ck
top -bn1 | grep -m1 multics
[ $fail = 0 ] && echo "VERIFICA: OK" || echo "VERIFICA: FALHOU"
exit $fail
'@
$chk = $chk.Replace('@@LOGIN@@', $loginB64) -replace "`r",""
# o script vai por ficheiro: aspas duplas na linha de comandos do plink perdem-se no Windows
$chkFile = Join-Path $env:TEMP "multics_verify.sh"
[IO.File]::WriteAllText($chkFile, $chk, (New-Object Text.UTF8Encoding($false)))
& "$ToolDir\pscp.exe" -pw $VPSPass -q $chkFile "$VPSUser@${VPSHost}:/tmp/multics_verify.sh"
& "$ToolDir\plink.exe" -ssh "$VPSUser@$VPSHost" -pw $VPSPass -batch "bash /tmp/multics_verify.sh; r=`$?; rm -f /tmp/multics_verify.sh; exit `$r"
Remove-Item $chkFile -ErrorAction SilentlyContinue
if ($LASTEXITCODE -ne 0) { Write-Host "Verificacao falhou - ver acima (o binario antigo esta em /opt/multics/*.bak-*)" -ForegroundColor Red; exit 1 }
Write-Host "== DEPLOY COMPLETO ==" -ForegroundColor Green
