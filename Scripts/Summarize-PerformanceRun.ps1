[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$InputPath,
    [string]$OutputPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$path = [System.IO.Path]::GetFullPath($InputPath)
if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Girdi bulunamadı: $path" }
if ([IO.Path]::GetExtension($path) -eq '.utrace') { throw 'Binary .utrace doğrudan özetlenmez; Insights içinden CSV/stat çıktısı dışa aktarın.' }
$lines = Get-Content -LiteralPath $path
function Find-Value([string[]]$Patterns) {
    foreach ($pattern in $Patterns) {
        $match = $lines | Select-String -Pattern $pattern | Select-Object -Last 1
        if ($match) {
            $number = [regex]::Match($match.Line, '(-?\d+(?:[.,]\d+)?)')
            if ($number.Success) { return $number.Value.Replace(',', '.') }
        }
    }
    return 'N/A'
}
$summary = [ordered]@{
    Source = $path
    AverageFPS = Find-Value @('Average FPS', 'AvgFPS')
    OnePercentLowFPS = Find-Value @('1% low', 'OnePercentLow')
    GameThreadMs = Find-Value @('Game Thread', 'GameThread')
    RenderThreadMs = Find-Value @('Render Thread', 'RenderThread')
    GPUFrameMs = Find-Value @('GPU Frame', 'GPUFrame')
    MemoryMB = Find-Value @('Memory.*MB', 'Physical Memory')
    HitchCount = Find-Value @('Hitch Count', 'Hitches')
}
$json = $summary | ConvertTo-Json
if ($OutputPath) {
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($OutputPath), $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}
$json
