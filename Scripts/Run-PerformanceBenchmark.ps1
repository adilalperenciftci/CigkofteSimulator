[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ExecutablePath,
    [ValidateRange(10, 3600)]
    [int]$DurationSeconds = 120,
    [ValidateSet('Low', 'Medium', 'High', 'Epic')]
    [string]$Quality = 'High',
    [ValidateRange(640, 7680)]
    [int]$Width = 1920,
    [ValidateRange(480, 4320)]
    [int]$Height = 1080,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$exe = [System.IO.Path]::GetFullPath($ExecutablePath)
if (-not (Test-Path -LiteralPath $exe -PathType Leaf) -and -not $DryRun) { throw "Paketli EXE bulunamadı: $exe" }
$outDir = Join-Path $root 'Logs\Insights'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log = Join-Path $outDir "Benchmark-$stamp.log"
$trace = Join-Path $outDir "Benchmark-$stamp.utrace"
$qualityMap = @{ Low = 0; Medium = 1; High = 2; Epic = 3 }
$args = @(
    '-benchmark',
    "-BenchmarkSeconds=$DurationSeconds",
    "-ResX=$Width",
    "-ResY=$Height",
    '-windowed',
    "-sg.ViewDistanceQuality=$($qualityMap[$Quality])",
    "-sg.ShadowQuality=$($qualityMap[$Quality])",
    "-sg.TextureQuality=$($qualityMap[$Quality])",
    '-trace=cpu,frame,gpu,bookmark,log',
    "-tracefile=`"$trace`"",
    "-AbsLog=`"$log`""
)
if ($DryRun) {
    Write-Output "Dry-run başarılı: '$exe' $($args -join ' ')"
    exit 0
}
$null = New-Item -ItemType Directory -Path $outDir -Force
$process = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory (Split-Path -Parent $exe) -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(($DurationSeconds + 60) * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "Benchmark timeout: $($DurationSeconds + 60) saniye."
}
if ($process.ExitCode -ne 0) { throw "Benchmark exit code $($process.ExitCode). Log: $log" }
Write-Output "Benchmark tamamlandı. Log=$log; Trace=$trace"
