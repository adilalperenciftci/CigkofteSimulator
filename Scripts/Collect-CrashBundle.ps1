[CmdletBinding()]
param(
    [string]$CrashRoot,
    [string]$OutputDirectory,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CigCommon.ps1')
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if (-not $CrashRoot) { $CrashRoot = Join-Path $root 'Saved\Crashes' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'Logs\Crashes' }
$crashRootFull = Resolve-CigPath $CrashRoot
$out = Resolve-CigPath $OutputDirectory
$latest = Get-ChildItem -LiteralPath $crashRootFull -Directory -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
$zip = Join-Path $out ("CrashBundle-{0:yyyyMMdd-HHmmss}.zip" -f (Get-Date))
if ($DryRun) {
    $candidate = if ($latest) { $latest.FullName } else { '<crash bulunamadı>' }
    Write-Output "Dry-run başarılı: Kaynak='$candidate'; redaction+zip='$zip'; gerçek telemetry yok."
    exit 0
}
if (-not $latest) { throw "Crash klasörü bulunamadı: $crashRootFull" }
$temp = Join-Path $out ('.bundle-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $temp -Force
try {
    Copy-Item -LiteralPath $latest.FullName -Destination (Join-Path $temp 'Crash') -Recurse
    $log = Get-ChildItem -LiteralPath (Join-Path $root 'Saved\Logs') -Filter '*.log' -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($log) { Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $temp 'Project.log') }
    $metadata = [ordered]@{
        BuildVersion = (Get-Content -LiteralPath (Join-Path $root 'CigkofteSimulator.uproject') -Raw | ConvertFrom-Json).EngineAssociation
        GitCommit = (git -C $root rev-parse HEAD).Trim()
        CollectedUtc = [DateTime]::UtcNow.ToString('o')
        SourceCrash = $latest.Name
    } | ConvertTo-Json
    [IO.File]::WriteAllText((Join-Path $temp 'bundle-metadata.json'), $metadata + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    $redactPatterns = @(
        '(?i)(api[_-]?key|token|password|secret|dsn)\s*[:=]\s*[^\s,;]+',
        '(?i)Bearer\s+[A-Za-z0-9._~+/-]+=*',
        '(?i)https://[^@\s]+@[^/\s]+/[^\s]+'
    )
    Get-ChildItem -LiteralPath $temp -File -Recurse | Where-Object { $_.Extension -in @('.log', '.txt', '.xml', '.json', '.ini') } |
        ForEach-Object {
            $text = Get-Content -LiteralPath $_.FullName -Raw
            foreach ($pattern in $redactPatterns) { $text = [regex]::Replace($text, $pattern, '[REDACTED]') }
            [IO.File]::WriteAllText($_.FullName, $text, [Text.UTF8Encoding]::new($false))
        }
    $null = New-Item -ItemType Directory -Path $out -Force
    Compress-Archive -Path (Join-Path $temp '*') -DestinationPath $zip -CompressionLevel Optimal
}
finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
Write-Output "Crash bundle hazır: $zip"
