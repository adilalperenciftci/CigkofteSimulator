[CmdletBinding()]
param(
    [string]$AppBuildVdf = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Templates\SteamPipe\app_build_TEMPLATE.vdf'),
    [string]$SteamCmdPath,
    [switch]$ConfirmUpload
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$vdf = [System.IO.Path]::GetFullPath($AppBuildVdf)
if (-not (Test-Path -LiteralPath $vdf -PathType Leaf)) { throw "VDF bulunamadı: $vdf" }
$text = Get-Content -LiteralPath $vdf -Raw
if ($text -match 'APP_ID_HERE|DEPOT_ID_HERE|VERSION_HERE') {
    Write-Output "Dry-run: placeholder değerler mevcut; upload engellendi. VDF=$vdf"
    exit 0
}
if ($text -notmatch '"preview"\s+"1"') { throw 'VDF preview=1 içermiyor; dry-run güvenliği yok.' }
if (-not $ConfirmUpload) {
    Write-Output "Dry-run başarılı: VDF doğrulandı; gerçek upload için açık görev + -ConfirmUpload gerekir. setlive yine uygulanmaz."
    exit 0
}
if (-not $SteamCmdPath -or -not (Test-Path -LiteralPath $SteamCmdPath -PathType Leaf)) {
    throw 'Gerçek upload için kullanıcıya ait resmi SteamCMD yolu ve etkileşimli Steam Guard girişi gerekir.'
}
throw 'Bu hazırlık scripti credential kabul etmez ve otomatik upload yapmaz. Açık yayın görevi için ayrı, etkileşimli operasyon gerekir.'
