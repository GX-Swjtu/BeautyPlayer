param([string]$Serial, [string]$Video, [string]$MediaUri, [int]$Seconds = 35, [switch]$SelfTest, [switch]$Exercise,
    [switch]$Library, [string]$Search = 'BeautyPlayer-', [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$ThumbnailCacheName = 'library-thumbnails-v04',
    [Alias('Profile')][ValidateSet('default','all','original')][string]$BeautyProfile = 'default', [switch]$SoftwareDecode)
$ErrorActionPreference = 'Stop'
if ($Video -and $MediaUri) { throw 'Choose -Video or -MediaUri, not both.' }
if ($MediaUri -and !$MediaUri.StartsWith('content://')) { throw '-MediaUri requires an Android content:// URI.' }
$root = Split-Path $PSScriptRoot -Parent
$adb = Join-Path $root 'third_party/android/toolchain/sdk/platform-tools/adb.exe'
$apk = Join-Path $root 'dist/android-arm64/BeautyPlayer-arm64-v8a.apk'
if (!(Test-Path $apk)) { throw 'Run scripts/build-android.ps1 first.' }
$devices = @(& $adb devices | Select-String '^([^\s]+)\s+device$' | ForEach-Object { $_.Matches[0].Groups[1].Value })
if (!$Serial) {
    if ($devices.Count -ne 1) { throw 'Connect one authorized Android device, or specify -Serial.' }
    $Serial = $devices[0]
}
function Invoke-Adb { & $adb -s $Serial @args; if ($LASTEXITCODE) { throw "adb failed: $($args[0])" } }
$abi = (Invoke-Adb shell getprop ro.product.cpu.abilist) -join ''
if ($abi -notmatch 'arm64-v8a') { throw "Device does not support arm64-v8a: $abi" }
Invoke-Adb install -r $apk
$package = 'org.beautyplayer.app'
$appUid = ((Invoke-Adb shell dumpsys package $package | Select-String '(?:appId|userId)=(\d+)' | Select-Object -First 1).Matches[0].Groups[1].Value)
$component = "$package/org.beautyplayer.PlayerActivity"
if (!$SelfTest -and !$Library -and !$Video -and !$MediaUri) { Invoke-Adb shell am start -n $component; return }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$report = Join-Path $root "artifacts/android/device-$stamp"
New-Item -ItemType Directory -Force $report | Out-Null
@("model=$(Invoke-Adb shell getprop ro.product.model)", "android=$(Invoke-Adb shell getprop ro.build.version.release)", "abi=$abi", "pageSize=$(Invoke-Adb shell getconf PAGE_SIZE)") | Set-Content "$report/device.txt" -Encoding utf8
$remote = "/data/user/0/$package/files/validation"
Invoke-Adb shell run-as $package mkdir -p $remote
$prefix = "$remote/$stamp"
if ($SelfTest) { $arguments = "--self-test $prefix" }
elseif ($Library) { $arguments = "--fresh --library-probe $prefix --library-search `"$Search`" --thumbnail-cache $remote/$ThumbnailCacheName" }
else {
    $source = $MediaUri
    if (!$source) {
        $videoFile = (Resolve-Path -LiteralPath $Video).Path
        Invoke-Adb push $videoFile '/data/local/tmp/beautyplayer-validation.mp4'
        $extension = [IO.Path]::GetExtension($videoFile).ToLowerInvariant()
        if ($extension -notmatch '^\.[a-z0-9]{1,8}$') { $extension = '.bin' }
        Invoke-Adb shell run-as $package cp '/data/local/tmp/beautyplayer-validation.mp4' "$remote/input$extension"
        Invoke-Adb shell rm '/data/local/tmp/beautyplayer-validation.mp4'
        $source = "$remote/input$extension"
    }
    $arguments = "--fresh --probe $prefix --seconds $Seconds --profile $BeautyProfile $source"
    if ($Exercise) { $arguments += ' --exercise' }
}
$extra = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($arguments))
$environment = "QT_LOGGING_RULES=qt.multimedia.ffmpeg.hwaccel=true;qt.multimedia.playbackengine.codec=true"
if ($SoftwareDecode) { $environment += "`tQT_FFMPEG_DECODING_HW_DEVICE_TYPES=," }
$envEncoded = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($environment))
Invoke-Adb shell am force-stop $package
$since = ((Invoke-Adb shell date '+%m-%d_%H:%M:%S.000') -join '').Replace('_', ' ')
Invoke-Adb shell am start -W -n $component --es extraappparams $extra --es extraenvvars $envEncoded
$deadline = (Get-Date).AddSeconds($(if ($SelfTest) { 90 } else { $Seconds + 60 }))
$ready = $false
while ((Get-Date) -lt $deadline) {
    & $adb -s $Serial shell run-as $package test -f "$prefix.json"
    if (!$LASTEXITCODE) { $ready = $true; break }
    if (!((& $adb -s $Serial shell pidof $package) -join '')) { break }
    Start-Sleep -Seconds 2
}
# Include Activity teardown after the report is flushed, not only playback.
if ($ready -and !$SelfTest) { Start-Sleep -Seconds 3 }
if ($appUid) { & $adb -s $Serial logcat -d "--uid=$appUid" -T $since -v threadtime | Set-Content "$report/logcat.txt" -Encoding utf8 }
if (!$ready) { throw "No device report was produced. Inspect $report/logcat.txt" }
function Read-AppFile([string]$source, [string]$destination) {
    python -c 'import subprocess,sys,pathlib; pathlib.Path(sys.argv[5]).write_bytes(subprocess.check_output([sys.argv[1],"-s",sys.argv[2],"exec-out","run-as",sys.argv[3],"cat",sys.argv[4]]))' $adb $Serial $package $source $destination
    if ($LASTEXITCODE) { throw "Cannot read app report: $source" }
}
Read-AppFile "$prefix.json" "$report/report.json"
$names = @(& $adb -s $Serial shell run-as $package ls $remote)
foreach ($name in $names) {
    if ($name -like "$stamp*.png") { Read-AppFile "$remote/$($name.Trim())" (Join-Path $report $name.Trim()) }
}
$result = Get-Content "$report/report.json" -Raw | ConvertFrom-Json
Write-Host "Device report: $report/report.json"
if (Select-String -Path "$report/logcat.txt" -Pattern 'Fatal signal|FATAL EXCEPTION|ANR in' -Quiet) {
    throw 'Android process crashed during playback or Activity teardown; inspect logcat.txt.'
}
if (!$result.passed) { throw 'Device checks failed; inspect report.json and logcat.txt.' }
