param([string]$QtRoot)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$QtRoot = Find-Qt $QtRoot
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build/windows-release'
$dest = Join-Path $root 'dist/windows-x64'
if (-not (Test-Path -LiteralPath (Join-Path $build 'BeautyPlayer.exe'))) { throw 'Run scripts/build.cmd first.' }
# Start with a fresh package so previously auto-deployed optional plugins cannot
# survive an update. Preserve the old directory; no user files are deleted.
if (Test-Path -LiteralPath $dest) {
    $workspacePrefix = [IO.Path]::GetFullPath($root).TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
    $resolvedDest = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $dest).Path)
    $backup = [IO.Path]::GetFullPath((Join-Path $root ('artifacts/package-backups/windows-x64-' + [Guid]::NewGuid().ToString('N'))))
    if (-not $resolvedDest.StartsWith($workspacePrefix, [StringComparison]::OrdinalIgnoreCase) -or -not $backup.StartsWith($workspacePrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Package paths must stay inside the project.' }
    if ((Get-Item -LiteralPath $dest).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Package target cannot be a directory junction.' }
    if (Get-Process BeautyPlayer -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq (Join-Path $dest 'BeautyPlayer.exe') }) { throw 'Close the packaged BeautyPlayer before packaging again.' }
    New-Item -ItemType Directory -Force -Path (Split-Path $backup) | Out-Null
    Move-Item -LiteralPath $resolvedDest -Destination $backup
}
New-Item -ItemType Directory -Force -Path $dest | Out-Null
foreach ($name in @('BeautyPlayer.exe', 'gpupixel.dll', 'mars-face-kit.dll')) { Copy-Item -LiteralPath (Join-Path $build $name) -Destination $dest -Force }
foreach ($name in @('models', 'res')) { Copy-Item -LiteralPath (Join-Path $build $name) -Destination $dest -Recurse -Force }
& (Join-Path $QtRoot 'bin/windeployqt.exe') --release --verbose 0 --no-compiler-runtime --skip-plugin-types qmltooling,platforminputcontexts,vectorimageformats --qmldir (Join-Path $root 'qml') --dir $dest (Join-Path $dest 'BeautyPlayer.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed: $LASTEXITCODE" }
foreach ($pattern in @('Qt6VirtualKeyboard*.dll', 'Qt6Quick3D*.dll', 'Qt6Lottie*.dll')) {
    if (Get-ChildItem -LiteralPath $dest -Filter $pattern) { throw "Unexpected unused GPL-only module in package: $pattern" }
}
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
