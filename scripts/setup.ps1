param()
$ErrorActionPreference = 'Stop'
function File-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '') }
    finally { $hasher.Dispose(); $stream.Dispose() }
}
$root = Split-Path $PSScriptRoot -Parent
$manifest = Get-Content -LiteralPath (Join-Path $root 'dependencies.lock.json') -Raw | ConvertFrom-Json
foreach ($item in $manifest.files) {
    $path = Join-Path $root $item.path
    $valid = (Test-Path -LiteralPath $path) -and ((File-Sha256 $path) -eq $item.sha256)
    if (-not $valid) {
        New-Item -ItemType Directory -Force -Path (Split-Path $path) | Out-Null
        Write-Host "Downloading $($item.path)"
        Invoke-WebRequest -Uri $item.url -OutFile $path -UseBasicParsing
    }
    if ((File-Sha256 $path) -ne $item.sha256) { throw "SHA256 mismatch: $($item.path)" }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sdk = [IO.Path]::GetFullPath((Join-Path $root 'third_party/gpupixel')) + [IO.Path]::DirectorySeparatorChar
$zip = [IO.Compression.ZipFile]::OpenRead((Join-Path $root 'third_party/gpupixel_windows_x86_64.zip'))
try {
    foreach ($entry in $zip.Entries) {
        $dest = [IO.Path]::GetFullPath((Join-Path $sdk $entry.FullName))
        if (-not $dest.StartsWith($sdk, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid archive path' }
        if ($entry.Name) {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dest, $true)
        }
    }
} finally { $zip.Dispose() }
$yuv = [IO.Path]::GetFullPath((Join-Path $root 'third_party/libyuv')) + [IO.Path]::DirectorySeparatorChar
$zip = [IO.Compression.ZipFile]::OpenRead((Join-Path $root 'third_party/libyuv-source.zip'))
try {
    $prefix = 'libyuv-' + $manifest.libyuv.commit + '/'
    foreach ($entry in $zip.Entries) {
        if (-not $entry.FullName.StartsWith($prefix, [StringComparison]::Ordinal)) { throw 'Unexpected libyuv archive root' }
        if (-not $entry.Name) { continue }
        $dest = [IO.Path]::GetFullPath((Join-Path $yuv $entry.FullName.Substring($prefix.Length)))
        if (-not $dest.StartsWith($yuv, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid libyuv archive path' }
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest)) | Out-Null
        # Preserve timestamps of unchanged sources so incremental builds stay fast.
        if ((Test-Path -LiteralPath $dest) -and (Get-Item -LiteralPath $dest).Length -eq $entry.Length) {
            $existing = [IO.File]::ReadAllBytes($dest)
            $stream = $entry.Open(); $buffer = New-Object IO.MemoryStream
            try { $stream.CopyTo($buffer); $same = [Convert]::ToBase64String($existing) -eq [Convert]::ToBase64String($buffer.ToArray()) }
            finally { $stream.Dispose(); $buffer.Dispose() }
            if ($same) { continue }
        }
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dest, $true)
    }
} finally { $zip.Dispose() }
Write-Host "Verified GPUPixel $($manifest.version), commit $($manifest.commit)"
Write-Host "Verified libyuv $($manifest.libyuv.commit)"
