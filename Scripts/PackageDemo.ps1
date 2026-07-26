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
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Shipping'
)

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$engine = Get-CigEngineRoot -Override $EngineRoot
$uat = Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'

Write-CigStep "Packaging Windows $Configuration to $OutputDir"

& $uat BuildCookRun `
    -project="$UProject" `
    -platform=Win64 -clientconfig=$Configuration `
    -build -cook -stage -pak -archive -archivedirectory="$OutputDir" `
    -unattended -nop4 -utf8output

if ($LASTEXITCODE -ne 0) {
    Write-Error "Packaging failed (exit $LASTEXITCODE)."
    exit 1
}

Write-Host "Package written to $OutputDir" -ForegroundColor Green
Write-Host 'Verify on a clean path: launch, switch language, load a save, navigate with a gamepad.'
