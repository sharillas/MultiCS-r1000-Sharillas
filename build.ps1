# ============================================================
# MultiCS r1000 v1.0 - Build Script (Windows + Zig cross-compile)
# Gera build/multics.x64 e build/multics.x32 (Linux, musl static)
# ============================================================
$ErrorActionPreference = "Stop"

Set-Location -LiteralPath (Join-Path $PSScriptRoot "src")

$Zig = "C:\TMP\opencode\zig-x86_64-windows-0.15.2\zig.exe"

$Opts = @(
  "-DCHECK_NEXTDCW", "-DSID_FILTER", "-DNEWCACHE",
  "-DCCCAM_CLI", "-DRADEGAST_CLI", "-DCAMD35_CLI", "-DCS378X_CLI",
  "-DHTTP_SRV", "-DTELNET", "-DMGCAMD_SRV", "-DCCCAM_SRV",
  "-DCAMD35_SRV", "-DCS378X_SRV", "-DSRV_CSCACHE",
  "-DEXPIREDATE", "-DDCWSWAP", "-DCACHEEX", "-DIPLIST", "-DTESTCHANNEL",
  "-DTHREAD_DCW", "-DEPOLL_NEWCAMD", "-DEPOLL_CCCAM", "-DEPOLL_MGCAMD",
  "-DEPOLL_ECM", "-DPEERLIST", "-DECMLIST", "-DEPOLL_FREECCCAM", "-DSIG_HANDLER",
  "-DCLI_CSCACHE", "-DSRV_CSCACHE"
)

$Srcs = @(
  "sha1.c", "des.c", "md5.c", "aes.c", "dcw.c", "convert.c", "tools.c",
  "debug.c", "parser.c", "ipdata.c", "threads.c", "sockets.c",
  "msg-newcamd.c", "msg-cccam.c", "msg-radegast.c", "config.c",
  "ecmdata.c", "httpserver.c", "telnet.c", "main.c"
)

$BuildDir = Join-Path $PSScriptRoot "build"
$ObjDir = Join-Path $BuildDir "obj"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

function Build-Target {
  param([string]$Target, [string]$Out, [string[]]$Extra)
  Write-Host "== Building $Out =="
  $objs = @()
  foreach ($s in $Srcs) {
    $o = Join-Path $ObjDir ($Target + "/" + [System.IO.Path]::GetFileNameWithoutExtension($s) + ".o")
    New-Item -ItemType Directory -Force -Path (Split-Path $o) | Out-Null
    & $Zig cc -target $Target -O3 -fpack-struct -I. @Extra @Opts -c $s -o $o
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $s" }
    $objs += $o
  }
  & $Zig cc -target $Target @objs -o $Out -pthread -s
  if ($LASTEXITCODE -ne 0) { throw "link failed: $Out" }
}

Build-Target -Target "x86_64-linux-musl" -Out (Join-Path $BuildDir "multics.x64") -Extra @("-std=gnu90")
Build-Target -Target "x86-linux-musl" -Out (Join-Path $BuildDir "multics.x32") -Extra @("-std=gnu90")

Write-Host "== Done =="
Get-Item (Join-Path $BuildDir "multics.x64"), (Join-Path $BuildDir "multics.x32") | Select-Object Name, Length
