param([string]$FFmpeg)
$ErrorActionPreference = 'Stop'
if (-not $FFmpeg) {
    $command = Get-Command ffmpeg.exe -ErrorAction SilentlyContinue
    if ($command) { $FFmpeg = $command.Source }
    elseif (Test-Path -LiteralPath 'D:/Program Files/ffmpeg/bin/ffmpeg.exe') { $FFmpeg = 'D:/Program Files/ffmpeg/bin/ffmpeg.exe' }
    else { throw 'Install FFmpeg and add it to PATH, or pass -FFmpeg with its path.' }
}
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force -Path test-media | Out-Null
    if (-not (Test-Path test-media/pexels-7477538-4k.mp4)) {
        Invoke-WebRequest 'https://videos.pexels.com/video-files/7477538/7477538-uhd_2160_3840_24fps.mp4' -OutFile test-media/pexels-7477538-4k.mp4
    }
    & $FFmpeg -hide_banner -loglevel error -y -i test-media/pexels-7477538-4k.mp4 -vf 'scale=1080:1920,fps=30' -c:v libx264 -preset fast -crf 19 -an test-media/face-1080.mp4
    if ($LASTEXITCODE -ne 0) { throw 'Face clip conversion failed' }
    & $FFmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=1920x1080:rate=30:duration=20' -f lavfi -i 'aevalsrc=if(lt(mod(t\,1)\,0.04)\,0.4*sin(2*PI*1000*t)\,0):s=48000:d=20' -vf "drawbox=x=0:y=0:w=iw:h=ih:color=white:t=fill:enable='lt(mod(t,1),0.04)'" -c:v libx264 -preset fast -crf 20 -c:a aac -shortest test-media/sync-1080.mp4
    if ($LASTEXITCODE -ne 0) { throw 'Sync clip generation failed' }
    & $FFmpeg -hide_banner -loglevel error -y -f lavfi -i 'color=red:s=320x240:r=30:d=6' -vf 'drawbox=x=160:y=0:w=160:h=120:color=lime:t=fill,drawbox=x=0:y=120:w=160:h=120:color=blue:t=fill,drawbox=x=160:y=120:w=160:h=120:color=yellow:t=fill' -c:v libx264 -crf 12 test-media/colors.mp4
    if ($LASTEXITCODE -ne 0) { throw 'Color clip generation failed' }
    & $FFmpeg -hide_banner -loglevel error -y -display_rotation:v:0 90 -i test-media/colors.mp4 -c copy test-media/rotated.mp4
    if ($LASTEXITCODE -ne 0) { throw 'Rotated clip generation failed' }
    & $FFmpeg -hide_banner -loglevel error -y -ss 1 -i test-media/face-1080.mp4 -frames:v 1 test-media/face-frame.png
    if ($LASTEXITCODE -ne 0) { throw 'Reference frame extraction failed' }
} finally { Pop-Location }
