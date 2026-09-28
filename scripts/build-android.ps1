param(
    [string]$QtRoot = 'D:\Qt\6.11.2\android_arm64_v8a',
    [string]$QtHost = 'D:\Qt\6.11.2\msvc2022_64',
    [int]$Jobs = 8,
    [switch]$NativeOnly,
    [switch]$SkipSetup
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$SkipSetup) { & (Join-Path $PSScriptRoot 'setup-android.ps1') }
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$cmake = Find-BuildTool 'cmake.exe' $QtHost 'Tools/CMake_64/bin/cmake.exe'
$ninja = Find-BuildTool 'ninja.exe' $QtHost 'Tools/Ninja/ninja.exe'
$env:JAVA_HOME = Join-Path $root 'third_party/android/java/jdk-21.0.12.1+1'
$env:ANDROID_SDK_ROOT = Join-Path $root 'third_party/android/toolchain/sdk'
$env:ANDROID_HOME = $env:ANDROID_SDK_ROOT
$env:ANDROID_NDK_ROOT = Join-Path $env:ANDROID_SDK_ROOT 'ndk/27.2.12479018'
$env:GRADLE_USER_HOME = Join-Path $root 'third_party/android/gradle'
$env:ANDROID_USER_HOME = Join-Path $root 'artifacts/android/user'
$env:TEMP = Join-Path $root 'artifacts/android/temp'; $env:TMP = $env:TEMP
$env:PATH = (Join-Path $env:JAVA_HOME 'bin') + ';' + $env:PATH
foreach ($path in @($env:GRADLE_USER_HOME, $env:ANDROID_USER_HOME, $env:TEMP)) { New-Item -ItemType Directory -Force $path | Out-Null }
# Gradle/Java do not use HTTPS_PROXY themselves. Keep proxy configuration local
# to this repository. No credentials are stored and TLS verification stays on.
$gradleProperties = @('org.gradle.daemon=false', 'org.gradle.jvmargs=-Xmx3072m -Dfile.encoding=UTF-8', 'org.gradle.workers.max=8')
if ($env:HTTPS_PROXY) {
    $proxy = [Uri]$env:HTTPS_PROXY
    if ($proxy.UserInfo) { throw 'Authenticated proxies require a user-managed Gradle configuration.' }
    foreach ($protocol in @('http', 'https')) {
        $gradleProperties += "systemProp.$protocol.proxyHost=$($proxy.Host)"
        $gradleProperties += "systemProp.$protocol.proxyPort=$($proxy.Port)"
    }
}
$gradleProperties | Set-Content (Join-Path $env:GRADLE_USER_HOME 'gradle.properties') -Encoding utf8
$build = Join-Path $root 'build/android-arm64'
& $cmake -S $root -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$QtRoot/lib/cmake/Qt6/qt.toolchain.cmake" "-DQT_HOST_PATH=$QtHost" "-DANDROID_SDK_ROOT=$env:ANDROID_SDK_ROOT" "-DANDROID_NDK_ROOT=$env:ANDROID_NDK_ROOT" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DCMAKE_BUILD_TYPE=Release -DQT_ANDROID_BUILD_ALL_ABIS=OFF
if ($LASTEXITCODE) { throw 'Android configure failed' }
& $cmake --build $build --target BeautyPlayer --parallel $Jobs
if ($LASTEXITCODE) { throw 'Android native build failed' }
if ($NativeOnly) { return }
# A clean deployment prevents removed Qt plugins from surviving incremental
# androiddeployqt runs. Preserve the previous directory, never delete user data.
$packageDirectory = [IO.Path]::GetFullPath((Join-Path $build 'android-build'))
if (!$packageDirectory.StartsWith([IO.Path]::GetFullPath($build) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe package path' }
if (Test-Path -LiteralPath $packageDirectory) {
    $backup = Join-Path $root ('artifacts/android/package-backups/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Force (Split-Path $backup) | Out-Null
    Move-Item -LiteralPath $packageDirectory -Destination $backup
}
& $cmake --build $build --target BeautyPlayer_prepare_apk_dir --parallel $Jobs
if ($LASTEXITCODE) { throw 'Android deployment directory preparation failed' }
# Release native code with a local debug signature: installable for testing.
# This is not a Play Store release and does not embed a production signing key.
# Qt 6.11 always appends SOURCE_DIR to the scanner roots. Use a derived settings
# file scoped to our QML, otherwise downloaded Qt examples pull in QtQuick3D.
$deployment = Get-Content "$build/android-BeautyPlayer-deployment-settings.json" -Raw | ConvertFrom-Json
$deployment.'qml-root-path' = @((Join-Path $root 'qml').Replace('\', '/'))
$deploymentPath = Join-Path $build 'android-BeautyPlayer-scoped-deployment-settings.json'
$deployment | ConvertTo-Json -Depth 20 | Set-Content $deploymentPath -Encoding utf8
& "$QtHost/bin/androiddeployqt.exe" --input $deploymentPath --output "$build/android-build" --android-platform android-36 --jdk $env:JAVA_HOME --gradle
if ($LASTEXITCODE) { throw 'Android APK packaging failed' }
$apk = Join-Path $build 'android-build/build/outputs/apk/debug/android-build-debug.apk'
if (!(Test-Path $apk)) { throw "APK not found: $apk" }
$python = (Get-Command python.exe -ErrorAction Stop).Source
& $python (Join-Path $PSScriptRoot 'inspect-apk.py') $apk (Join-Path $root 'artifacts/android/apk-inspection.json')
if ($LASTEXITCODE) { throw 'APK dependency/ELF validation failed; package not delivered.' }
& (Join-Path $env:ANDROID_SDK_ROOT 'build-tools/36.0.0/apksigner.bat') verify --verbose $apk
if ($LASTEXITCODE) { throw 'APK signature verification failed' }
& (Join-Path $env:ANDROID_SDK_ROOT 'build-tools/36.0.0/zipalign.exe') -c -P 16 4 $apk
if ($LASTEXITCODE) { throw 'APK alignment verification failed' }
$dist = Join-Path $root 'dist/android-arm64'; New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item -LiteralPath $apk -Destination (Join-Path $dist 'BeautyPlayer-arm64-v8a.apk') -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/ANDROID.md') -Destination (Join-Path $dist 'README.md') -Force
if (Test-Path (Join-Path $root 'docs/ANDROID-VALIDATION.md')) {
    Copy-Item -LiteralPath (Join-Path $root 'docs/ANDROID-VALIDATION.md') -Destination $dist -Force
}
foreach ($name in @('LICENSE', 'THIRD_PARTY_NOTICES.md', 'android-dependencies.lock.json')) {
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $dist -Force
}
Copy-Item -LiteralPath (Join-Path $root 'licenses') -Destination $dist -Recurse -Force
Get-FileHash -LiteralPath (Join-Path $dist 'BeautyPlayer-arm64-v8a.apk') | Format-List
