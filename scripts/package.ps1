param([string]$QtRoot)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$QtRoot = Find-Qt $QtRoot
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build/windows-release'
$dest = Join-Path $root 'dist/windows-x64'
if (-not (Test-Path -LiteralPath (Join-Path $build 'BeautyPlayer.exe'))) { throw 'Run scripts/build.cmd first.' }
New-Item -ItemType Directory -Force -Path $dest | Out-Null
foreach ($name in @('BeautyPlayer.exe', 'gpupixel.dll', 'mars-face-kit.dll')) { Copy-Item -LiteralPath (Join-Path $build $name) -Destination $dest -Force }
foreach ($name in @('models', 'res')) { Copy-Item -LiteralPath (Join-Path $build $name) -Destination $dest -Recurse -Force }
& (Join-Path $QtRoot 'bin/windeployqt.exe') --release --compiler-runtime --qmldir (Join-Path $root 'qml') --dir $dest (Join-Path $dest 'BeautyPlayer.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed: $LASTEXITCODE" }
$redist = Join-Path (Find-VS) 'VC/Redist/MSVC'
$latest = Get-ChildItem -LiteralPath $redist -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$crt = Join-Path $latest.FullName 'x64/Microsoft.VC143.CRT'
if (-not (Test-Path -LiteralPath $crt)) { throw "MSVC runtime not found: $crt" }
Get-ChildItem -LiteralPath $crt -Filter '*.dll' | Copy-Item -Destination $dest -Force
Copy-Item -LiteralPath (Join-Path $root 'licenses') -Destination $dest -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $dest -Force
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $dest -Force
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $dest -Force
Copy-Item -LiteralPath (Join-Path $root 'docs') -Destination $dest -Recurse -Force
Write-Host "Ready: $dest\BeautyPlayer.exe"
