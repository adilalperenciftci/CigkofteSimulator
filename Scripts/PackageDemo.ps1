<#
.SYNOPSIS
Packages a Windows demo build via UAT.

.DESCRIPTION
Not part of ValidateAll by default: packaging takes far longer than the other
checks and needs disk space, so it is opt-in.
#>
param(
    [string]$EngineRoot,
    [string]$OutputDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Build\WindowsDemo'),
    # Test is deliberately absent.
    #
    # It is the configuration this project's performance work wants - optimised
    # like Shipping, but with the log, the stats and the console kept, so CigBench
    # exists and Measure-Performance.ps1 can drive it. UBT refuses it outright
    # here: "Targets cannot be built in the Test configuration with this engine
    # distribution." A launcher-installed engine ships Development and Shipping
    # binaries and nothing else. Offering the option would only produce that error
    # a minute later; see docs/PERFORMANCE_BUDGET.md.
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Shipping',
    [switch]$SkipSmokeTest
)

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$OutputDir = Resolve-CigPath $OutputDir

$engine = Get-CigEngineRoot -Override $EngineRoot
$uat = Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'

Write-CigStep "Packaging Windows $Configuration to $OutputDir"

# Built as an array of explicitly quoted strings and splatted.
#
# Backtick-continued bare arguments look tidier and are a trap: a token like
# -clientconfig=$Configuration reached UAT with the variable name still in it,
# and UAT answered "Invalid configuration '$Configuration'" - a failure that
# takes a dig through the AutomationTool log to explain. Quoting each argument
# makes the expansion unambiguous.
$uatArgs = @(
    'BuildCookRun'
    "-project=$UProject"
    '-platform=Win64'
    "-clientconfig=$Configuration"
    '-build'
    '-cook'
    '-stage'
    '-pak'
    '-archive'
    "-archivedirectory=$OutputDir"
    '-unattended'
    '-nop4'
    '-utf8output'
)

& $uat @uatArgs | Tee-Object -Variable output | Out-Host

if ($LASTEXITCODE -ne 0) {
    # UAT writes its real reason into its own log and prints a one-line summary,
    # so a bare "packaging failed" sends the reader looking in the wrong place.
    Write-Host ''
    Write-Host 'UAT hata satirlari:' -ForegroundColor Red
    @($output) -match 'ERROR|AutomationException|BUILD FAILED|Invalid ' |
        Select-Object -Last 15 |
        ForEach-Object { Write-Host "  $_" -ForegroundColor Red }

    Write-Error "Packaging failed (exit $LASTEXITCODE). Tam kayit: %APPDATA%\Unreal Engine\AutomationTool\Logs"
    exit 1
}

$exe = Get-ChildItem $OutputDir -Recurse -Filter 'CigkofteSimulator.exe' -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $exe) {
    # UAT can report success having staged nothing useful; an archive with no
    # executable in it is not a build the player can start.
    Write-Error "Packaging reported success but no CigkofteSimulator.exe was archived under $OutputDir."
    exit 1
}

$sizeMB = [math]::Round((Get-ChildItem $OutputDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
Write-Host "Package written to $OutputDir ($sizeMB MB)" -ForegroundColor Green
Write-Host "Executable: $($exe.FullName)"

if ($SkipSmokeTest) {
    Write-Host 'Smoke test skipped (-SkipSmokeTest).' -ForegroundColor Yellow
    return
}

# One smoke test, shared with Package-Windows.ps1. It used to live inline here,
# which is how the other packaging path ended up with a weaker version of it.
& (Join-Path $PSScriptRoot 'SmokeTest-PackagedBuild.ps1') -PackageDirectory $OutputDir -EngineRoot $engine -Configuration $Configuration
exit $LASTEXITCODE
