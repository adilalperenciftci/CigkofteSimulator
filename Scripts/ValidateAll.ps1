<#
.SYNOPSIS
Everything that must pass before a commit is considered good.

.DESCRIPTION
Static checks alone are not a build. This runs the source validator, the editor
build and the automation suite, and reports each separately so a green summary
cannot hide a step that was skipped.

.PARAMETER IncludePackage
Also run PackageDemo.ps1. Off by default because it is slow.
#>
param(
    [string]$EngineRoot,
    [switch]$IncludePackage
)

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$results = [ordered]@{}
$failed = $false
$startedAt = Get-Date

function Invoke-CigStage {
    param([string]$Name, [scriptblock]$Body)

    try {
        & $Body
        $script:results[$Name] = 'PASS'
    }
    catch {
        $script:results[$Name] = "FAIL - $($_.Exception.Message)"
        $script:failed = $true
    }
}

Invoke-CigStage 'static' {
    Write-CigStep 'Static source and data checks'
    # Out-Host keeps the validator's output in step with the stage headings;
    # native stdout is otherwise flushed after everything else and the summary
    # ends up printed above the checks it summarises.
    python (Join-Path $RepoRoot 'Tools\check_sources.py') | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "check_sources.py exit $LASTEXITCODE" }
}

# The release self-test decides whether a package ships, and it is PowerShell, so
# the compiler and the automation suite both say nothing about it. Fixture-driven
# and a second or two; there is no reason for it not to run with the static checks.
Invoke-CigStage 'harness' {
    Write-CigStep 'Surum oz-testi durum makinesi'
    & (Join-Path $PSScriptRoot 'Test-SelfTestState.ps1') | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Test-SelfTestState.ps1 exit $LASTEXITCODE" }

    Write-CigStep 'Paket cook kapsami politikasi'
    & (Join-Path $PSScriptRoot 'Test-SmokeCookPolicy.ps1') | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Test-SmokeCookPolicy.ps1 exit $LASTEXITCODE" }

    Write-CigStep 'Cook eklenti dislama'
    & (Join-Path $PSScriptRoot 'Test-CigCookPluginExclusion.ps1') | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Test-CigCookPluginExclusion.ps1 exit $LASTEXITCODE" }
}

Invoke-CigStage 'paths' {
    Write-CigStep 'Script path safety'
    & (Join-Path $PSScriptRoot 'Test-CigPathHelpers.ps1') | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Test-CigPathHelpers.ps1 exit $LASTEXITCODE" }
}

Invoke-CigStage 'build' {
    & (Join-Path $PSScriptRoot 'BuildEditor.ps1') -EngineRoot $EngineRoot
    if ($LASTEXITCODE -ne 0) { throw "build exit $LASTEXITCODE" }
}

# The automation suite loads whatever editor module is on disk. After a failed
# build that is the previous one, so the run reports a green suite for code that
# is not the code under test - and reports it with the wrong test count, which is
# the only visible sign anything is wrong. Seen for real: a build that failed on
# a linker lock was followed by 93 passing tests against a five-day-old DLL, from
# before two merged stages existed.
if ($results['build'] -eq 'PASS') {
    Invoke-CigStage 'tests' {
        & (Join-Path $PSScriptRoot 'RunAutomationTests.ps1') -EngineRoot $EngineRoot
        if ($LASTEXITCODE -ne 0) { throw "tests exit $LASTEXITCODE" }
    }
}
else {
    $results['tests'] = 'SKIPPED (build did not produce a binary)'
}

if ($results['tests'] -eq 'PASS') {
    Invoke-CigStage 'data' {
    Write-CigStep 'Runtime data load'
    $log = Join-Path $RepoRoot 'Logs\RunUnrealTests-latest.log'
    if (-not (Test-Path $log)) { throw 'no log from the test run' }

    # This stage reads the log the test run wrote. If the tests never produced
    # one, an old log from any earlier editor session would answer instead and
    # report a clean data load that nothing in this run actually performed.
    if ((Get-Item $log).LastWriteTime -lt $startedAt) {
        throw 'the log predates this run; nothing loaded the data'
    }

    $lines = Get-Content $log
    $loaded = ($lines | Select-String 'Denge dosyası uygulandı').Count
    $csvCount = (Get-ChildItem (Join-Path $RepoRoot 'Config\Balance\*.csv')).Count
    $warnings = ($lines | Select-String 'bilinmeyen anahtar|geçersiz Index|okunamadı').Count

    Write-Host "balance files loaded: $loaded/$csvCount, warnings: $warnings"
    if ($loaded -lt $csvCount) { throw "only $loaded of $csvCount balance files loaded" }
    if ($warnings -gt 0) { throw "$warnings data warnings in the log" }
    }
}
else {
    $results['data'] = 'SKIPPED (tests did not run)'
}

if ($IncludePackage) {
    Invoke-CigStage 'package' {
        & (Join-Path $PSScriptRoot 'PackageDemo.ps1') -EngineRoot $EngineRoot
        if ($LASTEXITCODE -ne 0) { throw "package exit $LASTEXITCODE" }
    }
}
else {
    $results['package'] = 'SKIPPED (use -IncludePackage)'
}

Write-CigStep 'Summary'
foreach ($k in $results.Keys) {
    $v = $results[$k]
    $colour = if ($v -eq 'PASS') { 'Green' } elseif ($v -like 'SKIPPED*') { 'Yellow' } else { 'Red' }
    Write-Host ("{0,-8} {1}" -f $k, $v) -ForegroundColor $colour
}

if ($failed) { exit 1 }
