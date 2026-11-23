# ============================================================
# deploy.ps1 - Build + Deploy para a VPS de producao
# Uso: .\deploy.ps1
# Passos: build (zig musl) -> scp -> backup -> restart -> verifica
# Requisitos: OpenSSH (ssh/scp) nativo do Windows + alias "stockroom-vps"
#             em ~/.ssh/config, apontando para a chave
#             ~/.ssh/id_ed25519_stockroom (sem password, so' a chave).
#             Isto substitui o plink/pscp -pw usados antes do v1.70.9:
#             a autenticacao e' feita so' por chave publica, nunca por
#             password escrita em disco ou em memoria neste script.
# SEGREDOS: so' resta a password da GUI HTTP (nao tem equivalente a
#           chave SSH), que vive em deploy.secrets.ps1 (NUNCA no git -
#           esta no .gitignore). Exemplo do ficheiro:
#             $GuiUser = "admin"
#             $GuiPass = "a-tua-password-gui"
# ============================================================
$ErrorActionPreference = "Stop"

# --- CONFIG ---
$VPSAlias = "stockroom-vps"   # Host alias definido em ~/.ssh/config (chave, sem password)
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
if (!$GuiUser -or !$GuiPass) {
	Write-Host "ERRO: cria o ficheiro deploy.secrets.ps1 (gitignored) com `$GuiUser/`$GuiPass" -ForegroundColor Red
	exit 1
}
$VPSBin  = "/opt/multics/multics.x64"
$VPSBin32 = "/opt/multics/multics.x32"

# teste rapido de que a chave SSH funciona sem pedir password (BatchMode=yes falha em vez de prompt)
$probe = & ssh -o BatchMode=yes -o ConnectTimeout=8 $VPSAlias "echo ssh-ok" 2>$null
if ($LASTEXITCODE -ne 0 -or $probe -ne "ssh-ok") {
	Write-Host "ERRO: nao foi possivel autenticar em '$VPSAlias' por chave SSH (sem password). Confirma ~/.ssh/config e a chave." -ForegroundColor Red
	exit 1
}

Write-Host "== 1. Build =="
cmd /c "powershell -ExecutionPolicy Bypass -File `"$Root\build.ps1`" > $ToolDir\bl.log 2>&1"
if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FALHOU:" -ForegroundColor Red
    Get-Content "$ToolDir\bl.log" -Tail 15
    exit 1
}
Write-Host "   build OK" -ForegroundColor Green

Write-Host "== 2. Upload =="
& scp -q "$Root\build\multics.x64" "${VPSAlias}:/tmp/multics.x64.new"
if ($LASTEXITCODE -ne 0) { Write-Host "scp x64 falhou"; exit 1 }
& scp -q "$Root\build\multics.x32" "${VPSAlias}:/tmp/multics.x32.new"
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
$deployFile = Join-Path $env:TEMP "multics_deploy.sh"
[IO.File]::WriteAllText($deployFile, $remote, (New-Object Text.UTF8Encoding($false)))
& scp -q $deployFile "${VPSAlias}:/tmp/multics_deploy.sh"
& ssh $VPSAlias "bash /tmp/multics_deploy.sh; r=`$?; rm -f /tmp/multics_deploy.sh; exit `$r"
$deployExit = $LASTEXITCODE
Remove-Item $deployFile -ErrorAction SilentlyContinue
if ($deployExit -ne 0) { Write-Host "deploy remoto falhou"; exit 1 }

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
# o script vai por ficheiro: aspas duplas na linha de comandos do ssh perdem-se no Windows
$chkFile = Join-Path $env:TEMP "multics_verify.sh"
[IO.File]::WriteAllText($chkFile, $chk, (New-Object Text.UTF8Encoding($false)))
& scp -q $chkFile "${VPSAlias}:/tmp/multics_verify.sh"
& ssh $VPSAlias "bash /tmp/multics_verify.sh; r=`$?; rm -f /tmp/multics_verify.sh; exit `$r"
$verifyExit = $LASTEXITCODE
Remove-Item $chkFile -ErrorAction SilentlyContinue
if ($verifyExit -ne 0) { Write-Host "Verificacao falhou - ver acima (o binario antigo esta em /opt/multics/*.bak-*)" -ForegroundColor Red; exit 1 }
Write-Host "== DEPLOY COMPLETO ==" -ForegroundColor Green
