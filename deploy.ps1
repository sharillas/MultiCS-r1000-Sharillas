# ============================================================
# MultiCS r1000 - Deploy para VPS (scp + ssh)
# Uso: .\deploy.ps1 -Host "IP" [-User "root"] [-Port 22] [-Path "/opt/multics"] [-Key "caminho/chave.pem"]
# ============================================================
param(
  [Parameter(Mandatory=$true)][string]$HostName,
  [string]$User = "root",
  [int]$Port = 22,
  [string]$Path = "/opt/multics",
  [string]$Key = ""
)

$ErrorActionPreference = "Stop"

$Dist = Join-Path $PSScriptRoot "dist/multics-r1000"
if (-not (Test-Path (Join-Path $Dist "install.sh"))) {
  Write-Host "package nao encontrado - a correr package.ps1 primeiro..."
  & (Join-Path $PSScriptRoot "package.ps1")
}

$PortArg = "-P $Port"
if ($Key) { $KeyArg = "-i `"$Key`"" } else { $KeyArg = "" }

Write-Host "== Upload para ${User}@${HostName}:$Path =="

# upload do package
$tarName = "multics-r1000.tar.gz"
tar -czf (Join-Path $PSScriptRoot "dist/$tarName") -C (Join-Path $PSScriptRoot "dist") multics-r1000
if ($LASTEXITCODE -ne 0) { throw "tar failed" }

Invoke-Expression "scp $PortArg $KeyArg `"$(Join-Path $PSScriptRoot "dist/$tarName")`" ${User}@${HostName}:/tmp/"

# extrai e instala na VPS
$remote = @"
cd /tmp
tar xzf $tarName
cd multics-r1000
bash install.sh $Path
echo '--- smoke test ---'
$Path/multics.x64 -b -C /var/etc/multics.cfg
sleep 2
curl -s -o /dev/null -w 'HTTP %{http_code}\n' http://127.0.0.1:5500/ || true
ps aux | grep multics | grep -v grep | head -3
"@

Invoke-Expression "ssh $PortArg $KeyArg -o StrictHostKeyChecking=accept-new ${User}@${HostName} `"$remote`""

Write-Host "== Deploy concluido =="
