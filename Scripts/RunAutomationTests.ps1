<#
.SYNOPSIS
Runs the Cigkofte automation tests headless and fails on any failure.

.PARAMETER Filter
Test prefix, e.g. Cigkofte.Pricing. Defaults to the whole suite.
#>
param(
    [string]$EngineRoot,
    [string]$Filter = 'Cigkofte'
)

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$engine = Get-CigEngineRoot -Override $EngineRoot
$editorCmd = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $RepoRoot 'Saved\Logs\CigkofteSimulator.log'

Write-CigStep "Running automation tests: $Filter"

# Note when this run began, so a log left behind by an earlier editor session
# cannot be mistaken for the results of this one.
$startedAt = Get-Date

& $editorCmd "$UProject" -ExecCmds="Automation RunTests $Filter;Quit" `
    -unattended -nullrhi -nopause -log | Out-Null

if (-not (Test-Path $log)) {
    Write-Error "No log at $log; the editor did not start."
    exit 1
}

if ((Get-Item $log).LastWriteTime -lt $startedAt) {
    Write-Error "The log at $log predates this run; the editor produced no output."
    exit 1
}

$lines = Get-Content $log
$passed = ($lines | Select-String 'Result=\{Success\}').Count
$failed = ($lines | Select-String 'Result=\{Fail').Count

$lines | Select-String 'Result=\{Fail' | ForEach-Object { Write-Host $_.Line -ForegroundColor Red }

$colour = if ($failed -eq 0) { 'Green' } else { 'Red' }
Write-Host "passed=$passed failed=$failed" -ForegroundColor $colour

# A run that produced no results at all is a failure, not a pass.
if ($failed -gt 0 -or $passed -eq 0) {
    Write-Error "Automation tests failed (passed=$passed failed=$failed)."
    exit 1
}
