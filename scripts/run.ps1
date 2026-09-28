param([string]$Video)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'dist/windows-x64/BeautyPlayer.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Run scripts/build.cmd and scripts/package.ps1 first.' }
if ($Video) { & $exe $Video } else { & $exe }
