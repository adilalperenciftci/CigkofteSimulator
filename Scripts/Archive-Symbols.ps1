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
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'Releases\Symbols' }
$zip = Join-Path (Resolve-CigPath $OutputDirectory) "CigkofteSimulator-$Version-symbols.zip"
$symbols = @(Get-ChildItem -LiteralPath $build -File -Recurse -Include '*.pdb','*.debug','*.sym' -ErrorAction SilentlyContinue)
if ($DryRun) {
    Write-Output "Dry-run başarılı: Build='$build'; SymbolCount=$($symbols.Count); Archive='$zip'."
    exit 0
}
if ($symbols.Count -eq 0) { throw "Arşivlenecek symbol bulunamadı: $build" }
$null = New-Item -ItemType Directory -Path (Split-Path -Parent $zip) -Force
Compress-Archive -LiteralPath $symbols.FullName -DestinationPath $zip -CompressionLevel Optimal
Write-Output "Symbol arşivi hazır: $zip"
