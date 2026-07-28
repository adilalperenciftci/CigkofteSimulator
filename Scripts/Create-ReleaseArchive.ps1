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
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$build = [System.IO.Path]::GetFullPath($BuildDirectory)
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'Releases' }
$out = [System.IO.Path]::GetFullPath($OutputDirectory)
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
Compress-Archive -Path (Join-Path $build '*') -DestinationPath $zip -CompressionLevel Optimal
$hash = Get-FileHash -LiteralPath $zip -Algorithm SHA256
[IO.File]::WriteAllText($checksum, "$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($zip))`n", [Text.UTF8Encoding]::new($false))
$notesText = "# CigkofteSimulator $Version`n`n- Git commit: $commit`n- UTC build: $utc`n- Doğrulama: `Scripts/Verify-Release.ps1` ile yapılmalı.`n"
[IO.File]::WriteAllText($notes, $notesText, [Text.UTF8Encoding]::new($false))
Write-Output "Release arşivi hazır: $zip"
