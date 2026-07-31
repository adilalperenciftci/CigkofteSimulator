[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuildDirectory,
    [string]$ChecksumFile,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CigCommon.ps1')
$build = Resolve-CigPath $BuildDirectory
if ($DryRun) {
    Write-Output "Dry-run başarılı: Build='$build'; hash/default-map/EXE/CrashReporter/prereq/secret/debug denetimleri çalıştırılacak."
    exit 0
}
if (-not (Test-Path -LiteralPath $build -PathType Container)) { throw "Build klasörü bulunamadı: $build" }
$exe = Get-ChildItem -LiteralPath $build -Filter 'CigkofteSimulator.exe' -File -Recurse | Select-Object -First 1
if (-not $exe) { throw 'CigkofteSimulator.exe bulunamadı.' }
$crashReporter = Get-ChildItem -LiteralPath $build -Filter 'CrashReportClient.exe' -File -Recurse | Select-Object -First 1
$prereq = Get-ChildItem -LiteralPath $build -Filter 'UEPrereqSetup_x64.exe' -File -Recurse | Select-Object -First 1
# Point this at an extracted release archive, not at the raw build directory.
#
# A packaged build directory always contains .pdb files, and the rule below
# treats one as a forbidden artefact - correctly, because it must not reach a
# player, but it means this script and a raw build directory can never agree.
# Create-ReleaseArchive.ps1 leaves symbols out of the zip and Archive-Symbols.ps1
# packs them separately; what this verifies is the thing that ships.
$forbidden = Get-ChildItem -LiteralPath $build -File -Recurse | Where-Object {
    $_.Name -match '(?i)(\.env$|credentials|secret|token|\.pem$|id_rsa|\.pdb$)'
}
if ($forbidden) { throw "Yasak debug/secret adayı bulundu: $($forbidden.FullName -join ', ')" }
if ($ChecksumFile) {
    $line = (Get-Content -LiteralPath $ChecksumFile -Raw).Trim()
    if ($line -notmatch '^(?<hash>[0-9a-fA-F]{64})\s+\*?(?<file>.+)$') { throw 'Checksum formatı geçersiz.' }
    $target = Join-Path (Split-Path -Parent $ChecksumFile) $Matches.file
    $actual = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($actual -ne $Matches.hash) { throw 'SHA-256 checksum uyuşmuyor.' }
}
$size = (Get-ChildItem -LiteralPath $build -File -Recurse | Measure-Object Length -Sum).Sum
[ordered]@{
    Executable = $exe.FullName
    CrashReporter = [bool]$crashReporter
    Prerequisites = [bool]$prereq
    SizeBytes = [int64]$size
    Result = 'PASS'
} | ConvertTo-Json
