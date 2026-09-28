# Dot-source to discover the local toolchain; explicit parameters/environment win.
function Find-Qt([string]$Requested) {
    if ($Requested) { $candidate = $Requested }
    elseif ($env:QT_ROOT) { $candidate = $env:QT_ROOT }
    elseif ($env:Qt6_DIR) { $candidate = [IO.Path]::GetFullPath((Join-Path $env:Qt6_DIR '../../..')) }
    else {
        $candidate = @('D:/Qt/6.11.2/msvc2022_64', 'C:/Qt/6.11.2/msvc2022_64') | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'bin/qmake.exe') } | Select-Object -First 1
        if (-not $candidate) {
            $qmakeCommand = Get-Command qmake.exe -ErrorAction SilentlyContinue
            if ($qmakeCommand) { $candidate = Split-Path (Split-Path $qmakeCommand.Source) }
        }
    }
    if (-not $candidate -or -not (Test-Path -LiteralPath (Join-Path $candidate 'bin/qmake.exe'))) { throw 'Set QT_ROOT to a Qt 6.11 MSVC x64 kit (including Multimedia and Quick).' }
    return [IO.Path]::GetFullPath($candidate)
}
function Find-VS {
    if ($env:BEAUTY_VS_ROOT) { return $env:BEAUTY_VS_ROOT }
    $locator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $locator) {
        $vs = & $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) { return $vs }
    }
    $vs = 'D:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools'
    if (Test-Path -LiteralPath $vs) { return $vs }
    throw 'Install Visual Studio 2022 Desktop development with C++, or set BEAUTY_VS_ROOT.'
}
function Find-BuildTool([string]$Name, [string]$QtRoot, [string]$Bundled) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $base = Split-Path (Split-Path $QtRoot)
    $tool = Join-Path $base $Bundled
    if (Test-Path -LiteralPath $tool) { return $tool }
    throw "Cannot find $Name. Install it or add it to PATH."
}
function Find-ClangCl([string]$VsRoot) {
    if ($env:BEAUTY_CLANG_CL -and (Test-Path -LiteralPath $env:BEAUTY_CLANG_CL)) { return $env:BEAUTY_CLANG_CL }
    $command = Get-Command clang-cl.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $candidates = @((Join-Path $VsRoot 'VC/Tools/Llvm/x64/bin/clang-cl.exe'), 'C:/Program Files/LLVM/bin/clang-cl.exe', 'D:/Program Files/LLVM/bin/clang-cl.exe')
    foreach ($sdk in @($env:ANDROID_SDK_ROOT, $env:ANDROID_HOME, 'D:/Android/Sdk')) {
        if ($sdk -and (Test-Path -LiteralPath (Join-Path $sdk 'ndk'))) {
            $candidates += Get-ChildItem -LiteralPath (Join-Path $sdk 'ndk') -Directory | Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'toolchains/llvm/prebuilt/windows-x86_64/bin/clang-cl.exe' }
        }
    }
    foreach ($candidate in $candidates) { if (Test-Path -LiteralPath $candidate) { return $candidate } }
    throw 'Install LLVM (winget install LLVM.LLVM), VS C++ Clang tools, or set BEAUTY_CLANG_CL to clang-cl.exe. Only libyuv uses Clang.'
}
