[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceFile,
    [Parameter(Mandatory)][string]$Prompt,
    [string]$NegativePrompt = '',
    [Parameter(Mandatory)][string]$Model,
    [Parameter(Mandatory)][string]$ModelLicense,
    [Parameter(Mandatory)][long]$Seed,
    [Parameter(Mandatory)][ValidatePattern('^\d+x\d+$')][string]$Size,
    [Parameter(Mandatory)][string]$UsagePlan
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$source = [System.IO.Path]::GetFullPath($SourceFile)
if (-not $source.StartsWith((Join-Path $root 'AssetWork\Generated'), [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Metadata yalnız AssetWork\Generated altındaki dosyalar için oluşturulur.'
}
$metadata = [ordered]@{
    prompt = $Prompt
    negativePrompt = $NegativePrompt
    model = $Model
    modelLicense = $ModelLicense
    seed = $Seed
    size = $Size
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    sourceFile = $source
    usagePlan = $UsagePlan
}
$path = "$source.metadata.json"
[IO.File]::WriteAllText($path, ($metadata | ConvertTo-Json -Depth 5) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
Write-Output $path
