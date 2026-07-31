[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuildDirectory,
    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9A-Za-z][0-9A-Za-z._-]{0,63}$')]
    [string]$Version,
    [string]$OutputDirectory,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CigCommon.ps1')
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$build = Resolve-CigPath $BuildDirectory
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'Releases' }
$out = Resolve-CigPath $OutputDirectory
$commit = (git -C $root rev-parse HEAD).Trim()
$utc = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$base = "CigkofteSimulator-$Version-$utc"
$zip = Join-Path $out "$base.zip"
$checksum = Join-Path $out "$base.sha256"
$notes = Join-Path $out "$base-RELEASE_NOTES.md"
if ($DryRun) {
    Write-Output "Dry-run başarılı: Build='$build'; Archive='$zip'; Checksum='$checksum'; Commit=$commit."
    exit 0
}
if (-not (Test-Path -LiteralPath $build -PathType Container)) { throw "Build klasörü bulunamadı: $build" }
$null = New-Item -ItemType Directory -Path $out -Force

# Debug symbols do not go to the player.
#
# This zipped the build directory whole, and a Shipping build directory is
# 1902 MB of which 229 MB is .pdb - twelve per cent of a download that exists
# only to symbolicate a crash on a developer's machine. They are not deleted:
# Archive-Symbols.ps1 collects the same files into their own archive, which is
# what a crash report is resolved against, and that has to be run for the same
# build before this.
$symbolExt = @('.pdb', '.debug', '.sym')
$all = Get-ChildItem -LiteralPath $build -Recurse -File
$payload = @($all | Where-Object { $symbolExt -notcontains $_.Extension.ToLowerInvariant() })
$skipped = @($all | Where-Object { $symbolExt -contains $_.Extension.ToLowerInvariant() })
if ($skipped.Count -gt 0) {
    $mb = [math]::Round((($skipped | Measure-Object Length -Sum).Sum / 1MB), 1)
    Write-Output "Symbol dosyalari arsive alinmadi: $($skipped.Count) dosya, $mb MB (Archive-Symbols.ps1 ile ayrica paketlenir)."
}
if ($payload.Count -eq 0) { throw "Arsivlenecek dosya yok: $build" }

# Entries are written one at a time to keep the tree.
#
# Compress-Archive takes a list of files and puts every one of them at the root
# of the archive - the build's directory structure, which is the thing the game
# needs to start, is silently flattened. Passing a wildcard keeps the tree but
# has no way to exclude anything. So the archive is built by hand, with each
# entry named by its path relative to the build directory.
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
$zipStream = [System.IO.Compression.ZipFile]::Open($zip, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    $prefix = $build.TrimEnd('\') + '\'
    foreach ($f in $payload) {
        $rel = $f.FullName.Substring($prefix.Length).Replace('\', '/')
        $null = [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zipStream, $f.FullName, $rel, [System.IO.Compression.CompressionLevel]::Optimal)
    }
}
finally {
    $zipStream.Dispose()
}
$hash = Get-FileHash -LiteralPath $zip -Algorithm SHA256
[IO.File]::WriteAllText($checksum, "$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($zip))`n", [Text.UTF8Encoding]::new($false))
$notesText = "# CigkofteSimulator $Version`n`n- Git commit: $commit`n- UTC build: $utc`n- Doğrulama: `Scripts/Verify-Release.ps1` ile yapılmalı.`n"
[IO.File]::WriteAllText($notes, $notesText, [Text.UTF8Encoding]::new($false))
Write-Output "Release arşivi hazır: $zip"
