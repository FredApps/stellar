# build.ps1 - set Open Watcom env and build SHOOTER.EXE (run from project root)
$ErrorActionPreference = 'Stop'

$root = Split-Path $PSScriptRoot -Parent
$localWatcom = Join-Path $root 'tools\watcom'
$watcom = if (Test-Path $localWatcom) { $localWatcom } else { 'C:\tools\watcom' }
$hostBin = if (Test-Path (Join-Path $watcom 'binnt64\wmake.exe')) {
    Join-Path $watcom 'binnt64'
} else {
    Join-Path $watcom 'binnt'
}

$env:WATCOM  = $watcom
$env:PATH    = "$hostBin;$(Join-Path $watcom 'binnt');$(Join-Path $watcom 'binw');$env:PATH"
$env:INCLUDE = Join-Path $watcom 'h'
$env:EDPATH  = Join-Path $watcom 'eddat'
$env:LIB     = "$(Join-Path $watcom 'lib286\dos');$(Join-Path $watcom 'lib286')"

Push-Location $root
try {
    Get-ChildItem -Filter *.obj -ErrorAction SilentlyContinue | Remove-Item -Force
    & wmake -f build\wmakefile
    if ($LASTEXITCODE -ne 0) { throw "wmake failed ($LASTEXITCODE)" }
    $exe = Get-Item SHOOTER.EXE
    Write-Host ("BUILD OK: SHOOTER.EXE = {0} bytes" -f $exe.Length)
} finally {
    Pop-Location
}
