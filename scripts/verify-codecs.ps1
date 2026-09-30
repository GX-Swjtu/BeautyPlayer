param([int]$Seconds = 8, [string]$Output = 'artifacts/codecs', [switch]$RequireHardware, [string[]]$Only)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$player = Join-Path $root 'dist/windows-x64/BeautyPlayer.exe'
$report = [IO.Path]::GetFullPath((Join-Path $root $Output))
New-Item -ItemType Directory -Force $report | Out-Null
$cases = @(
    @{File='av1-opus.webm'; Video='AV1'; Width=1280},
    @{File='av1-opus.mkv'; Video='AV1'; Width=1280},
    @{File='av1-opus.mp4'; Video='AV1'; Width=1280},
    @{File='av1-10bit-opus.mp4'; Video='AV1'; Width=1920},
    @{File='hevc-opus.mkv'; Video='H265'; Width=1280},
    @{File='vp9-opus.webm'; Video='VP9'; Width=1280},
    @{File='audio.opus'; Video=''; Width=0}
)
if ($Only) {
    foreach ($name in $Only) { if ($name -notin $cases.File) { throw "Unknown fixture: $name" } }
    $cases = @($cases | Where-Object { $_.File -in $Only })
}
$oldRules = $env:QT_LOGGING_RULES
$results = @()
try {
    $env:QT_LOGGING_RULES = 'qt.multimedia.ffmpeg.hwaccel=true;qt.multimedia.playbackengine.codec=true'
    foreach ($case in $cases) {
        $source = Join-Path $root "test-media/codecs/$($case.File)"
        if (!(Test-Path -LiteralPath $source)) { throw 'Run scripts/prepare-codec-media.ps1 first.' }
        $prefix = Join-Path $report $case.File
        $arguments = '--probe "' + $prefix + '" --seconds ' + $Seconds + ' --profile default "' + $source + '"'
        $process = Start-Process -FilePath $player -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$prefix.log"
        if (!$process.WaitForExit(($Seconds + 60) * 1000)) { throw "Codec probe timed out (PID $($process.Id)): $($case.File)" }
        if (!(Test-Path -LiteralPath "$prefix.json")) { throw "Missing report: $prefix.json" }
        $probe = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
        $log = Get-Content "$prefix.log" -Raw
        $hardware = $log -match 'Selected format \d+ for hw [1-9]'
        $expectedVideo = $case.Video -ne ''
        $audioOk = $probe.final.hasAudio -and $probe.final.audioCodec -eq 'Opus' -and $probe.final.audioFramesDecoded -gt 1000 -and $probe.final.audioPeak -gt .001
        $videoOk = $probe.final.hasVideo -eq $expectedVideo
        if ($expectedVideo) { $videoOk = $videoOk -and $probe.final.width -eq $case.Width -and $probe.final.videoCodec -eq $case.Video }
        $passed = $process.ExitCode -eq 0 -and $probe.passed -and $audioOk -and $videoOk -and (!$RequireHardware -or !$expectedVideo -or $hardware)
        $results += [pscustomobject]@{file=$case.File; passed=$passed; videoCodec=$probe.final.videoCodec; audioCodec=$probe.final.audioCodec;
            hardwareVideo=$hardware; audioFrames=$probe.final.audioFramesDecoded; audioPeak=$probe.final.audioPeak;
            width=$probe.final.width; height=$probe.final.height; fps=$probe.final.fpsInterval; audioMinusVideoPtsMs=$probe.final.audioMinusVideoPtsMs}
        Write-Host "$($case.File): passed=$passed hardware=$hardware audioFrames=$($probe.final.audioFramesDecoded)"
    }
} finally { $env:QT_LOGGING_RULES = $oldRules }
$results | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $report 'summary.json') -Encoding utf8
if (@($results | Where-Object { !$_.passed }).Count) { throw 'Codec validation failed; inspect summary.json and the per-file logs.' }
