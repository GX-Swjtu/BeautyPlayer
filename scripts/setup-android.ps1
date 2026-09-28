param([string]$ExistingSdk = 'D:\Android\Sdk')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$lock = Get-Content (Join-Path $root 'android-dependencies.lock.json') -Raw | ConvertFrom-Json
Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($item in $lock.files) {
    $archive = Join-Path $root $item.archive
    New-Item -ItemType Directory -Force (Split-Path $archive) | Out-Null
    if (!(Test-Path -LiteralPath $archive)) {
        & curl.exe --http1.1 -fL --retry 3 --retry-all-errors --silent --show-error $item.url -o "$archive.partial"
        if ($LASTEXITCODE) { throw "Download failed: $($item.url)" }
        if ((Get-FileHash -LiteralPath "$archive.partial").Hash -ne $item.sha256) { throw "Checksum mismatch: $archive.partial" }
        Move-Item -LiteralPath "$archive.partial" -Destination $archive
    }
    if ((Get-FileHash -LiteralPath $archive).Hash -ne $item.sha256) { throw "Checksum mismatch: $archive" }
    $destination = [IO.Path]::GetFullPath((Join-Path $root $item.destination))
    if (!(Test-Path -LiteralPath (Join-Path $destination $item.marker))) {
        New-Item -ItemType Directory -Force $destination | Out-Null
        $zip = [IO.Compression.ZipFile]::OpenRead($archive)
        try {
            foreach ($entry in $zip.Entries) {
                if (!$entry.FullName.StartsWith($item.prefix) -or !$entry.Name) { continue }
                $relative = $entry.FullName.Substring($item.prefix.Length)
                $target = [IO.Path]::GetFullPath((Join-Path $destination $relative))
                if (!$target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe archive path' }
                [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
            }
        } finally { $zip.Dispose() }
    }
    Write-Host "Verified: $($item.destination)"
}
# Reuse licenses already accepted for this computer's SDK installation.
$licenses = Join-Path $root 'third_party/android/toolchain/sdk/licenses'
if (!(Test-Path $licenses) -and (Test-Path (Join-Path $ExistingSdk 'licenses'))) {
    Copy-Item -LiteralPath (Join-Path $ExistingSdk 'licenses') -Destination $licenses -Recurse
}
if (!(Test-Path $licenses)) { Write-Warning 'SDK licenses are not configured. Accept them with Android sdkmanager --licenses, then pass -ExistingSdk to this script.' }
