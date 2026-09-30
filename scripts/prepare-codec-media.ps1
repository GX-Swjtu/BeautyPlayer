param([string]$FFmpeg)
$ErrorActionPreference = 'Stop'
if (!$FFmpeg) {
    $command = Get-Command ffmpeg.exe -ErrorAction SilentlyContinue
    if ($command) { $FFmpeg = $command.Source }
    elseif (Test-Path 'D:/Program Files/ffmpeg/bin/ffmpeg.exe') { $FFmpeg = 'D:/Program Files/ffmpeg/bin/ffmpeg.exe' }
    else { throw 'Pass -FFmpeg with an FFmpeg build supporting libsvtav1, libopus, libx265 and libvpx-vp9.' }
}
$root = Split-Path $PSScriptRoot -Parent
$media = Join-Path $root 'test-media/codecs'
New-Item -ItemType Directory -Force $media | Out-Null
function Encode([string]$name, [string[]]$ffargs) {
    $destination = Join-Path $media $name
    if (Test-Path -LiteralPath $destination) { return }
    & $FFmpeg -hide_banner -loglevel error -y @ffargs $destination
    if ($LASTEXITCODE) { throw "Fixture failed: $name" }
}
$input720 = @('-f','lavfi','-i','testsrc2=size=1280x720:rate=30:duration=12',
    '-f','lavfi','-i','sine=frequency=880:sample_rate=48000:duration=12')
$opus = @('-c:a','libopus','-b:a','96k','-shortest')
Encode 'av1-opus.webm' ($input720 + @('-c:v','libsvtav1','-preset','12','-svtav1-params','lp=4','-crf','38','-pix_fmt','yuv420p','-g','60') + $opus)
foreach ($container in @('mp4','mkv')) {
    Encode "av1-opus.$container" @('-i',"$media/av1-opus.webm",'-map','0','-c','copy')
}
Encode 'av1-10bit-opus.mp4' (@('-f','lavfi','-i','testsrc2=size=1920x1080:rate=30:duration=12',
    '-f','lavfi','-i','sine=frequency=660:sample_rate=48000:duration=12',
    '-c:v','libsvtav1','-preset','12','-svtav1-params','lp=4','-crf','38','-pix_fmt','yuv420p10le','-g','60',
    '-color_primaries','bt709','-color_trc','bt709','-colorspace','bt709') + $opus)
Encode 'hevc-opus.mkv' ($input720 + @('-c:v','libx265','-preset','ultrafast','-crf','30','-x265-params','log-level=error:pools=4','-g','60') + $opus)
Encode 'vp9-opus.webm' ($input720 + @('-c:v','libvpx-vp9','-deadline','realtime','-cpu-used','8','-crf','38','-b:v','0','-g','60') + $opus)
Encode 'audio.opus' @('-i',"$media/av1-opus.webm",'-vn','-c:a','copy')
Write-Host "Codec fixtures: $media"
