param([int]$Seconds = 60, [switch]$LongRun)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'dist/windows-x64/BeautyPlayer.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Build and package first.' }
Push-Location $root
try {
    New-Item -ItemType Directory -Force -Path artifacts/simd | Out-Null
    # Serial runs: shared CPU/GPU load would invalidate the A/B comparison.
    foreach ($profile in @('default', 'all')) {
        foreach ($mode in @('qt', 'auto', 'sse', 'scalar')) {
            $prefix = "artifacts/simd/4k-$profile-$mode"
            & $exe --probe $prefix --seconds $Seconds --profile $profile --converter $mode --hidden test-media/pexels-7477538-4k.mp4 2> "$prefix.log" | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "Playback failed: $prefix" }
        }
    }
    foreach ($media in @('sync-1080', 'sync-1080-bt709')) {
        if (-not (Test-Path -LiteralPath "test-media/$media.mp4")) { throw 'Run prepare-test-media.ps1 to generate both sync fixtures.' }
        & $exe --probe "artifacts/simd/$media" --seconds $Seconds --profile default --hidden "test-media/$media.mp4" 2> "artifacts/simd/$media.log" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Sync playback failed: $media" }
    }
    if ($LongRun) {
        & $exe --probe artifacts/simd/stability --seconds 600 --exercise --profile all --hidden test-media/face-1080.mp4 2> artifacts/simd/stability.log | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Stability playback failed' }
    }
    Get-ChildItem artifacts/simd -Filter '4k-*.json' | Where-Object { $_.Name -notlike '*-progress.json' } | ForEach-Object {
        $r = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        $samples = @($r.samples | Where-Object { $_.second -ge 5 -and $_.playing })
        [pscustomobject]@{ Name=$_.BaseName; Passed=$r.passed; Backend=$r.final.converter; FPS=[math]::Round(($samples | Measure-Object fpsInterval -Average).Average,2); ConversionMs=[math]::Round($r.final.meanConversionMs,2); GpuMs=[math]::Round($r.final.meanGpuStageMs,2); TotalMs=[math]::Round($r.final.meanProcessingMs,2); LibyuvFrames=$r.final.yuvConversions; QtFrames=$r.final.qtConversions }
    }
} finally { Pop-Location }
