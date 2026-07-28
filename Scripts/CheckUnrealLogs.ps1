<#
.SYNOPSIS
En yeni Unreal logundaki kritik hata sinyallerini kısa biçimde özetler.
#>
[CmdletBinding()]
param(
    [ValidateRange(1, 100)]
    [int]$MaxMatches = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

try {
    $projectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
    $savedLogs = Join-Path $projectRoot 'Saved\Logs'
    if (-not (Test-Path -LiteralPath $savedLogs -PathType Container)) {
        throw "Saved log klasörü bulunamadı: $savedLogs"
    }

    $latest = Get-ChildItem -LiteralPath $savedLogs -Filter '*.log' -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $latest) {
        throw "Saved\\Logs altında .log dosyası bulunamadı."
    }

    $pattern = '(?i)(?:^|\])(?:Log\w+|LoadErrors):\s+(?:Error|Fatal):|=== Critical error ===|Fatal error:|Ensure condition failed|Assertion failed|Unhandled Exception'
    $matches = @(Select-String -LiteralPath $latest.FullName -Pattern $pattern)
    Write-Output "Log: $($latest.FullName)"
    Write-Output "Tarih: $($latest.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')); kritik eşleşme: $($matches.Count)"

    if ($matches.Count -eq 0) {
        Write-Output 'Kritik Error/Fatal/Ensure/Assertion/Crash sinyali bulunmadı.'
        exit 0
    }

    $matches | Select-Object -First $MaxMatches | ForEach-Object {
        Write-Output "$($_.Path):$($_.LineNumber): $($_.Line.Trim())"
    }
    if ($matches.Count -gt $MaxMatches) {
        Write-Output "... $($matches.Count - $MaxMatches) ek eşleşme gösterilmedi."
    }
    exit 1
}
catch {
    Write-Error "CheckUnrealLogs başarısız: $($_.Exception.Message)"
    exit 2
}
