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
