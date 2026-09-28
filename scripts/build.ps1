param([string]$QtRoot, [int]$Jobs = 8, [string]$Target)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$QtRoot = Find-Qt $QtRoot
$vs = Find-VS
$root = Split-Path $PSScriptRoot -Parent
$cmake = Find-BuildTool 'cmake.exe' $QtRoot 'Tools/CMake_64/bin/cmake.exe'
$ninja = Find-BuildTool 'ninja.exe' $QtRoot 'Tools/Ninja/ninja.exe'
& (Join-Path $PSScriptRoot 'setup.ps1')
# Import only into this build process. Never modify the user's global environment.
$devcmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$processInfo = New-Object Diagnostics.ProcessStartInfo
$processInfo.FileName = $env:ComSpec
$processInfo.Arguments = '/d /s /c ""' + $devcmd + '" -arch=x64 -host_arch=x64 >nul && set"'
$processInfo.UseShellExecute = $false
$processInfo.CreateNoWindow = $true
$processInfo.RedirectStandardOutput = $true
$process = [Diagnostics.Process]::Start($processInfo)
$environmentLines = $process.StandardOutput.ReadToEnd() -split "`r?`n"
$process.WaitForExit()
if ($process.ExitCode -ne 0) { throw 'VsDevCmd failed' }
$process.Dispose()
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$build = Join-Path $root 'build/windows-release'
& $cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
if ($Target) { & $cmake --build $build --parallel $Jobs --target $Target }
else { & $cmake --build $build --parallel $Jobs }
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
