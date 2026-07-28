<#
.SYNOPSIS
Projeye ait bir süreç için sınırlı süreli Unreal trace kaydı başlatır.
#>
[CmdletBinding()]
param(
    [string]$ExecutablePath,
    [ValidateRange(5, 3600)]
    [int]$DurationSeconds = 60,
    [ValidatePattern('^[a-zA-Z0-9_,.-]+$')]
    [string]$TraceChannels = 'cpu,frame,gpu,bookmark,log',
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$engine = 'C:\Program Files\Epic Games\UE_5.8'
$insights = Join-Path $engine 'Engine\Binaries\Win64\UnrealInsights.exe'
$traceServer = Join-Path $engine 'Engine\Binaries\Win64\UnrealTraceServer.exe'
foreach ($path in @($insights, $traceServer)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "UE 5.8 aracı bulunamadı: $path" }
}

$traceDir = Join-Path $root 'Logs\Insights'
$tracePath = Join-Path $traceDir ("Cigkofte-{0:yyyyMMdd-HHmmss}.utrace" -f (Get-Date))
if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe'
    $project = Get-ChildItem -LiteralPath $root -Filter '*.uproject' -File | Select-Object -First 1
    $launchArgs = @("`"$($project.FullName)`"")
}
else {
    $ExecutablePath = [System.IO.Path]::GetFullPath($ExecutablePath)
    $launchArgs = @()
}
if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw "Executable bulunamadı: $ExecutablePath" }
$launchArgs += @("-trace=$TraceChannels", "-tracefile=`"$tracePath`"", '-log')

if ($DryRun) {
    Write-Output "Dry-run başarılı: Insights='$insights'; TraceServer='$traceServer'; Target='$ExecutablePath'; Args=$($launchArgs -join ' '); Duration=$DurationSeconds; Output='$tracePath'."
    exit 0
}

$null = New-Item -ItemType Directory -Path $traceDir -Force
$process = Start-Process -FilePath $ExecutablePath -ArgumentList $launchArgs -WorkingDirectory $root -PassThru -WindowStyle Hidden
try {
    if ($process.WaitForExit($DurationSeconds * 1000)) {
        if ($process.ExitCode -ne 0) { throw "Trace hedefi erken kapandı (exit $($process.ExitCode))." }
    }
}
finally {
    if (-not $process.HasExited) {
        $null = $process.CloseMainWindow()
        if (-not $process.WaitForExit(10000)) { Stop-Process -Id $process.Id -Force }
    }
}
if (-not (Test-Path -LiteralPath $tracePath -PathType Leaf)) { throw "Trace dosyası oluşmadı: $tracePath" }
Write-Output "Trace kaydı tamamlandı: $tracePath"
