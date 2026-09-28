param([switch]$LongRun)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'dist/windows-x64/BeautyPlayer.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Build and package first.' }
if (-not (Test-Path -LiteralPath (Join-Path $root 'test-media/face-1080.mp4'))) { throw 'Run prepare-test-media.ps1 first.' }
Push-Location $root
try {
    New-Item -ItemType Directory -Force -Path artifacts/benchmark | Out-Null
    # Sequential: simultaneous players would invalidate GPU throughput comparisons.
    foreach ($profile in @('default', 'all', 'original')) {
        $prefix = 'artifacts/benchmark/4k-' + $profile
        & $exe --probe $prefix --seconds 60 --profile $profile --hidden test-media/pexels-7477538-4k.mp4 2> ($prefix + '.log') | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Playback failed: $prefix" }
    }
    & $exe --probe artifacts/benchmark/sync --seconds 60 --profile default --hidden test-media/sync-1080.mp4 2> artifacts/benchmark/sync.log | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Sync playback failed' }
    if ($LongRun) {
        & $exe --probe artifacts/benchmark/stability --seconds 600 --profile all --exercise --hidden test-media/face-1080.mp4 2> artifacts/benchmark/stability.log | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Stability playback failed' }
    }
    foreach ($file in Get-ChildItem artifacts/benchmark -Filter '*.json' | Where-Object { $_.Name -notlike '*-progress.json' }) {
        $report = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        $samples = @($report.samples | Where-Object { $_.second -ge 5 -and $_.playing })
        [pscustomobject]@{ Name=$file.BaseName; Passed=$report.passed; AverageFPS=[math]::Round(($samples | Measure-Object fpsInterval -Average).Average,2); ProcessingMs=[math]::Round($report.final.meanProcessingMs,2); PrivateMiB=[math]::Round($report.final.privateMB) }
    }
} finally { Pop-Location }
