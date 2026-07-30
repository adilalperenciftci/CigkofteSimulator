[CmdletBinding()]
param(
    [Parameter(Mandatory)][uri]$Url,
    [Parameter(Mandatory)][string]$Author,
    [Parameter(Mandatory)][string]$License,
    [Parameter(Mandatory)][ValidateSet('Yes','No','Unknown')][string]$CommercialUse,
    [Parameter(Mandatory)][ValidateSet('Required','NotRequired','Unknown')][string]$Attribution,
    [Parameter(Mandatory)][string]$DownloadedFile,
    [Parameter(Mandatory)][string]$UnrealUsage
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$file = [System.IO.Path]::GetFullPath($DownloadedFile)
$downloads = Join-Path $root 'AssetWork\Downloads'
if (-not $file.StartsWith($downloads, [StringComparison]::OrdinalIgnoreCase)) { throw 'Dosya AssetWork\Downloads altında olmalı.' }
if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Dosya bulunamadı: $file" }
$record = [ordered]@{
    url = $Url.AbsoluteUri
    author = $Author
    license = $License
    commercialUse = $CommercialUse
    attribution = $Attribution
    downloadedUtc = [DateTime]::UtcNow.ToString('o')
    file = $file
    sha256 = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    archiveContents = @()
    defenderResult = 'NotScanned'
    polygonCount = 'Unknown'
    textureResolution = 'Unknown'
    format = [IO.Path]::GetExtension($file)
    unrealUsage = $UnrealUsage
    reviewStatus = 'Quarantine'
}
$safeName = ([IO.Path]::GetFileNameWithoutExtension($file) -replace '[^A-Za-z0-9._-]', '_')
$path = Join-Path $root "AssetWork\Licenses\$safeName.asset-license.json"
[IO.File]::WriteAllText($path, ($record | ConvertTo-Json -Depth 8) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
Write-Output $path
