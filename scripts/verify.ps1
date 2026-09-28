param([switch]$LongRun, [string]$QtRoot)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$QtRoot = Find-Qt $QtRoot
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
    & .\build\windows-release\FrameConversionTests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Frame conversion tests failed' }
    & .\build\windows-release\LibraryHistoryTests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Library/history tests failed' }
    & .\build\windows-release\PipelineTests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Pipeline tests failed' }
    if (-not (Test-Path test-media/face-1080.mp4)) { throw 'Run scripts/prepare-test-media.ps1 first' }
    & .\dist\windows-x64\BeautyPlayer.exe --probe artifacts/verify --seconds 32 --exercise --library-folder "$root/test-media" test-media/face-1080.mp4 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Playback checks failed; inspect artifacts/verify.json' }
    if ($LongRun) {
        & .\dist\windows-x64\BeautyPlayer.exe --probe artifacts/verify-long --seconds 600 --exercise --hidden test-media/face-1080.mp4 | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Long playback checks failed' }
    }
} finally { Pop-Location }
