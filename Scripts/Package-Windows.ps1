<#
.SYNOPSIS
UE 5.8 RunUAT BuildCookRun ile Windows Development/Shipping paketi üretir.
#>
[CmdletBinding()]
param(
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Development',
    [string]$OutputDirectory,
    [switch]$Clean,
    [switch]$IncludePrereqs,
    [switch]$IncludeSymbols,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$engine = if ($env:CIG_UE_ROOT) { $env:CIG_UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
$uat = Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path -LiteralPath $uat -PathType Leaf)) { throw "RunUAT.bat bulunamadı: $uat" }
$projects = @(Get-ChildItem -LiteralPath $root -Filter '*.uproject' -File)
if ($projects.Count -ne 1) { throw "Tam bir .uproject bekleniyordu; bulunan: $($projects.Count)." }
$gameTargets = @()
foreach ($targetFile in @(Get-ChildItem -LiteralPath (Join-Path $root 'Source') -Filter '*.Target.cs' -File -Recurse)) {
    $content = Get-Content -LiteralPath $targetFile.FullName -Raw
    if ($content -match 'TargetType\.Game') {
        $class = [regex]::Match($content, 'class\s+(?<n>[A-Za-z_][A-Za-z0-9_]*)\s*:\s*TargetRules').Groups['n'].Value
        $gameTargets += if ($class.EndsWith('Target')) { $class.Substring(0, $class.Length - 6) } else { $class }
    }
}
$gameTargets = @($gameTargets | Select-Object -Unique)
if ($gameTargets.Count -ne 1) { throw "Tam bir Game target bekleniyordu; bulunan: $($gameTargets -join ', ')." }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root "Build\Windows-$Configuration" }
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
$logDir = Join-Path $root 'Logs\Packaging'
$log = Join-Path $logDir ("Package-{0}-{1:yyyyMMdd-HHmmss}.log" -f $Configuration, (Get-Date))
$configText = (Get-Content -LiteralPath (Join-Path $root 'Config\DefaultGame.ini') -Raw) +
    (Get-Content -LiteralPath (Join-Path $root 'Config\DefaultEngine.ini') -Raw)
$useIoStore = $configText -match '(?im)^\s*bUseIoStore\s*=\s*True\s*$'

$args = @(
    'BuildCookRun',
    "-project=$($projects[0].FullName)",
    "-target=$($gameTargets[0])",
    '-platform=Win64',
    "-clientconfig=$Configuration",
    '-build', '-cook', '-stage', '-pak', '-archive',
    "-archivedirectory=$output",
    '-CrashReporter',
    '-unattended', '-nop4', '-utf8output'
)
if ($useIoStore) { $args += '-iostore' } else { $args += '-noiostore' }
if ($Clean) { $args += '-clean' }
if ($IncludePrereqs) { $args += '-prereqs' }
if (-not $IncludeSymbols) { $args += '-nodebuginfo' }

if ($DryRun) {
    Write-Output "Dry-run başarılı: `"$uat`" $($args -join ' '); Log='$log'."
    exit 0
}
$null = New-Item -ItemType Directory -Path $logDir -Force
& $uat @args 2>&1 | Tee-Object -FilePath $log
$exitCode = if ($null -eq $LASTEXITCODE) { 1 } else { [int]$LASTEXITCODE }
if ($exitCode -ne 0) { throw "Paketleme başarısız (exit $exitCode). Log: $log" }
$exe = Get-ChildItem -LiteralPath $output -Filter "$($gameTargets[0]).exe" -File -Recurse | Select-Object -First 1
if (-not $exe) { throw "UAT başarılı döndü ancak EXE bulunamadı: $output" }
Write-Output "Paketleme başarılı: $($exe.FullName)"
