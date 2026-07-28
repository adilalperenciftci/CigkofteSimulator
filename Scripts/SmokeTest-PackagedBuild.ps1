[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PackageDirectory,
    [ValidateRange(5, 600)]
    [int]$TimeoutSeconds = 30,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$package = [System.IO.Path]::GetFullPath($PackageDirectory)
$exe = Get-ChildItem -LiteralPath $package -Filter 'CigkofteSimulator.exe' -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $exe -and -not $DryRun) { throw "Paketli EXE bulunamadı: $package" }
$exePath = if ($exe) { $exe.FullName } else { Join-Path $package 'CigkofteSimulator.exe' }
$log = Join-Path $root 'Logs\Packaging\SmokeTest-latest.log'
$args = @('-unattended', '-nosplash', '-nullrhi', '-nosound', "-AbsLog=`"$log`"")
if ($DryRun) {
    Write-Output "Dry-run başarılı: '$exePath' $($args -join ' '); timeout=$TimeoutSeconds."
    exit 0
}
$null = New-Item -ItemType Directory -Path (Split-Path -Parent $log) -Force
$process = Start-Process -FilePath $exePath -ArgumentList $args -WorkingDirectory (Split-Path -Parent $exePath) -PassThru -WindowStyle Hidden
try {
    Start-Sleep -Seconds ([Math]::Min(5, $TimeoutSeconds))
    if ($process.HasExited) { throw "Paketli oyun erken kapandı (exit $($process.ExitCode))." }
    $remaining = [Math]::Max(0, $TimeoutSeconds - 5)
    if ($remaining -gt 0) { Start-Sleep -Seconds $remaining }
}
finally {
    if (-not $process.HasExited) {
        $null = $process.CloseMainWindow()
        if (-not $process.WaitForExit(10000)) { Stop-Process -Id $process.Id -Force }
    }
}
if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { throw "Smoke test logu oluşmadı: $log" }
$fatal = Select-String -LiteralPath $log -Pattern 'Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception' -CaseSensitive:$false
if ($fatal) { throw "Smoke test kritik hata buldu: $($fatal[0].Line.Trim())" }
Write-Output "Smoke test başarılı: $TimeoutSeconds saniye; Log=$log"
