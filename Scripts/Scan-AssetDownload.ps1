[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Path,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CigCommon.ps1')
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
# The Downloads/Quarantine guard below is only as good as this resolution.
$target = Resolve-CigPath $Path
$downloads = Join-Path $root 'AssetWork\Downloads'
$quarantine = Join-Path $root 'AssetWork\Quarantine'
if (-not ((Test-CigPathWithinDirectory -Path $target -Directory $downloads -AllowDirectoryItself) -or
          (Test-CigPathWithinDirectory -Path $target -Directory $quarantine -AllowDirectoryItself))) {
    throw 'Defender taraması yalnız AssetWork Downloads/Quarantine altında çalışır.'
}
$defender = Join-Path $env:ProgramFiles 'Windows Defender\MpCmdRun.exe'
if (-not (Test-Path -LiteralPath $defender -PathType Leaf)) {
    $defender = Get-ChildItem -LiteralPath 'C:\ProgramData\Microsoft\Windows Defender\Platform' -Filter 'MpCmdRun.exe' -File -Recurse |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $defender) { throw 'Windows Defender MpCmdRun.exe bulunamadı.' }
$dangerous = @()
if (Test-Path -LiteralPath $target -PathType Container) {
    $dangerous = @(Get-ChildItem -LiteralPath $target -File -Recurse | Where-Object { $_.Extension -in @('.exe','.dll','.bat','.cmd','.ps1','.vbs','.js','.msi','.scr') })
}
elseif ([IO.Path]::GetExtension($target) -in @('.exe','.dll','.bat','.cmd','.ps1','.vbs','.js','.msi','.scr')) {
    $dangerous = @(Get-Item -LiteralPath $target)
}
if ($DryRun) {
    Write-Output "Dry-run başarılı: Defender='$defender'; Target='$target'; şüpheli yürütülebilir sayısı=$($dangerous.Count)."
    exit 0
}
if ($dangerous.Count -gt 0) { throw "Paket yürütülebilir/script içeriyor; import engellendi: $($dangerous.Name -join ', ')" }
& $defender -Scan -ScanType 3 -File $target -DisableRemediation
if ($LASTEXITCODE -ne 0) { throw "Defender taraması temiz dönmedi (exit $LASTEXITCODE)." }
Write-Output "Defender taraması temiz: $target"
