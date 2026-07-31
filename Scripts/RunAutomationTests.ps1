<#
.SYNOPSIS
Runs the Cigkofte automation tests headless and fails on any failure.

.PARAMETER Filter
Test prefix, e.g. Cigkofte.Pricing. Defaults to the whole suite.
#>
param(
    [string]$EngineRoot,
    [string]$Filter = 'Cigkofte',
    [ValidateRange(10, 7200)]
    [int]$TimeoutSeconds = 900
)

# Compatibility entry point for older documentation and scripts. Keep one
# authoritative runner: it owns the timeout, fresh absolute log, process exit
# code, zero-result detection, and failure parsing.
& (Join-Path $PSScriptRoot 'RunUnrealTests.ps1') -EngineRoot $EngineRoot `
    -TestFilter $Filter -TimeoutSeconds $TimeoutSeconds
exit $LASTEXITCODE
