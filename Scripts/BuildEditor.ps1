<#
.SYNOPSIS
Builds the editor target. Exits non-zero when the build fails.
#>
param(
    [string]$EngineRoot,
    [ValidateSet('Development', 'DebugGame', 'Shipping')]
    [string]$Configuration = 'Development'
)

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$engine = Get-CigEngineRoot -Override $EngineRoot
$build = Join-Path $engine 'Engine\Build\BatchFiles\Build.bat'

Write-CigStep "Building CigkofteSimulatorEditor Win64 $Configuration"

& $build CigkofteSimulatorEditor Win64 $Configuration -project="$UProject" -WaitMutex |
    Tee-Object -Variable output |
    Select-String -Pattern ': error|: warning C(?!4996)|Result:' |
    Out-Host

# Build.bat reports success in its output; check the exit code as well because a
# crashed toolchain can produce neither.
#
# -match against an array returns the matching elements rather than a boolean,
# so a plain -notmatch here is always truthy and reports every build as failed.
$succeeded = @(@($output) -match 'Result: Succeeded').Count -gt 0

if ($LASTEXITCODE -ne 0 -or -not $succeeded) {
    Write-Error "Build failed (exit $LASTEXITCODE, 'Result: Succeeded' found: $succeeded)."
    exit 1
}

Write-Host 'Build succeeded.' -ForegroundColor Green
