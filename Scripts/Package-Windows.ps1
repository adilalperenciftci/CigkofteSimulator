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
    [switch]$SkipSmokeTest,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CigCommon.ps1')
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
$output = Resolve-CigPath $OutputDirectory
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
# The Live Coding mutex UBT checks is keyed on the engine's editor executable,
# not on a project, so any editor open anywhere on this install refuses the
# build - and it refuses it after cook planning, as OtherCompilationError, which
# reads like a compile failure rather than "close the editor". BuildEditor.ps1
# has opted out of the IDE hot-reload path since it was written; packaging is the
# same kind of batch build and had no equivalent.
$args += '-ubtargs=-NoHotReloadFromIDE'
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

# A green UAT exit and an EXE on disk is the bar this project has already failed
# against: the archive was complete and the game came up with raw text keys, no
# sound and no balance data, because none of it was referenced by an asset and so
# none of it cooked. Content is verified by the same smoke test PackageDemo.ps1
# runs, never by a weaker local copy.
if (-not $SkipSmokeTest) {
    & (Join-Path $PSScriptRoot 'SmokeTest-PackagedBuild.ps1') -PackageDirectory $output -EngineRoot $engine -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Paket smoke testi başarısız (exit $LASTEXITCODE)." }
}
