[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^v?[0-9A-Za-z][0-9A-Za-z._-]{0,63}$')][string]$Tag,
    [Parameter(Mandatory)][string]$ArchivePath,
    [Parameter(Mandatory)][string]$ChecksumPath,
    [Parameter(Mandatory)][string]$NotesPath,
    [switch]$ConfirmCreate
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
foreach ($path in @($ArchivePath, $ChecksumPath, $NotesPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Release girdisi bulunamadı: $path" }
}
$status = git -C $root status --short
$command = "gh release create $Tag `"$ArchivePath`" `"$ChecksumPath`" --draft --notes-file `"$NotesPath`""
if (-not $ConfirmCreate) {
    Write-Output "Dry-run başarılı: $command"
    if ($status) { Write-Output 'Uyarı: çalışma ağacı kirli; release commit kapsamını doğrula.' }
    exit 0
}
throw 'Bu script bilinçli olarak release oluşturmaz. Açık kullanıcı talebinde komutu insan denetimiyle, draft olarak çalıştırın.'
