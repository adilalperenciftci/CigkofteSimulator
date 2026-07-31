<#
.SYNOPSIS
Regression test for the release self-test state machine in CigCommon.ps1.

.DESCRIPTION
The rule this covers is that a package built from this source tree has to prove
itself, and that only two things on the command line may excuse it. That rule
lived inline in SmokeTest-PackagedBuild.ps1 and was wrong in the one direction
that mattered: a missing report answered "unsupported", printed as a skip, stayed
out of the check list and let the smoke test exit 0. A build whose self-test never
ran read exactly like a build that passed.

Running the real thing costs a full package and a game launch, so the state
machine takes a report path and an exit code and nothing else, and this drives it
with fixture files. No engine, no build, no network - it runs on the CI runner
alongside check_sources.py.

.EXAMPLE
pwsh -File Scripts/Test-SelfTestState.ps1
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$script:failures = 0
$script:total = 0

function Assert-CigEqual {
    param([string]$What, $Expected, $Actual)
    $script:total++
    if ($Expected -eq $Actual) {
        Write-Host ("  PASS  {0}" -f $What) -ForegroundColor Green
    }
    else {
        Write-Host ("  FAIL  {0}: beklenen '{1}', gelen '{2}'" -f $What, $Expected, $Actual) -ForegroundColor Red
        $script:failures++
    }
}

# One fixture directory per run, removed at the end whatever happens.
$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("cig-selftest-" + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $fixtureRoot -Force

function New-CigReportFixture {
    param([Parameter(Mandatory)][string]$Name, [string[]]$Lines)
    $path = Join-Path $fixtureRoot "$Name.txt"
    # -Encoding utf8 rather than the default, to match what the game writes and to
    # behave the same on both platforms this script runs on.
    Set-Content -LiteralPath $path -Value $Lines -Encoding utf8
    return $path
}

$passReport = @(
    'CIGRELEASESELFTEST v1'
    'PASS  sistemler  (23 sistem)'
    'PASS  dunya'
    'PASS  metin-tablosu'
    'PASS  metin-tr'
    'PASS  metin-en'
    'PASS  denge-verisi  (14/14 csv)'
    'PASS  ses-varliklari'
    'PASS  mesh-varliklari'
    'PASS  kayit-surumu  (1 -> 12, hedef 12)'
    'PASS  kayit-turu'
    'PASS  shipping-hileleri  (Shipping disi yapida uygulanmaz)'
    'RESULT PASS 0'
)
$failReport = @(
    'CIGRELEASESELFTEST v1'
    'PASS  sistemler  (23 sistem)'
    'PASS  dunya'
    'PASS  metin-tablosu'
    'PASS  metin-tr'
    'PASS  metin-en'
    'FAIL  denge-verisi  (0/14 csv)'
    'PASS  ses-varliklari'
    'PASS  mesh-varliklari'
    'PASS  kayit-surumu  (1 -> 12, hedef 12)'
    'PASS  kayit-turu'
    'PASS  shipping-hileleri  (Shipping disi yapida uygulanmaz)'
    'RESULT FAIL 6'
)

Write-Host ''
Write-Host 'Surum oz-testi durum makinesi' -ForegroundColor Cyan
Write-Host ''

try {
    # --- The one shape that is allowed to pass -------------------------------
    $p = New-CigReportFixture -Name 'pass' -Lines $passReport
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'gecerli PASS raporu + cikis 0 -> passed' 'passed' $s.State

    $p = New-CigReportFixture -Name 'incomplete-pass' -Lines @(
        'CIGRELEASESELFTEST v1', 'PASS  sistemler', 'RESULT PASS 0')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'eksik PASS raporu -> failed' 'failed' $s.State
    Assert-CigEqual 'eksik PASS raporu -> incomplete-checks' 'incomplete-checks' $s.Reason

    $p = New-CigReportFixture -Name 'result-not-final' -Lines ($passReport + 'PASS  fazladan')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'RESULT son satir degil -> failed' 'failed' $s.State
    Assert-CigEqual 'RESULT son satir degil -> result-not-final' 'result-not-final' $s.Reason

    $duplicateCheck = @($passReport)
    $duplicateCheck[2] = 'PASS  sistemler'
    $p = New-CigReportFixture -Name 'duplicate-check' -Lines $duplicateCheck
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'yinelenen kontrol -> failed' 'failed' $s.State
    Assert-CigEqual 'yinelenen kontrol -> incomplete-checks' 'incomplete-checks' $s.Reason

    # --- Everything else is a failure ---------------------------------------
    $missing = Join-Path $fixtureRoot 'never-written.txt'
    $s = Get-CigSelfTestState -ReportPath $missing -ProcessExitCode 0
    Assert-CigEqual 'rapor yok -> failed' 'failed' $s.State
    Assert-CigEqual 'rapor yok -> missing-report' 'missing-report' $s.Reason

    $p = New-CigReportFixture -Name 'bad-header' -Lines @('CIGRELEASESELFTEST v2', 'RESULT PASS 0')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'baslik gecersiz -> failed' 'failed' $s.State
    Assert-CigEqual 'baslik gecersiz -> bad-header' 'bad-header' $s.Reason

    $p = New-CigReportFixture -Name 'empty' -Lines @()
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'bos rapor -> failed' 'failed' $s.State

    # Header written, process died before the verdict. The game writes to a
    # temporary file and renames, so this should not occur - which is exactly why
    # it must not be mistaken for anything but a failure if it does.
    $p = New-CigReportFixture -Name 'no-result' -Lines @('CIGRELEASESELFTEST v1', 'PASS  sistemler')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'RESULT satiri yok -> failed' 'failed' $s.State
    Assert-CigEqual 'RESULT satiri yok -> no-result' 'no-result' $s.Reason

    $p = New-CigReportFixture -Name 'fail-6' -Lines $failReport
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 6
    Assert-CigEqual 'RESULT FAIL 6 + cikis 6 -> failed' 'failed' $s.State
    Assert-CigEqual 'RESULT FAIL 6 + cikis 6 -> check-failed' 'check-failed' $s.Reason

    # The defect that was measured for real: report says FAIL, process says 0.
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'RESULT FAIL 6 + cikis 0 -> failed' 'failed' $s.State
    Assert-CigEqual 'RESULT FAIL 6 + cikis 0 -> exit-mismatch' 'exit-mismatch' $s.Reason

    # And its mirror image, which no version of this has ever produced but which
    # a partially written report or a wrong exit path would.
    $p = New-CigReportFixture -Name 'pass-exit-6' -Lines $passReport
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 6
    Assert-CigEqual 'RESULT PASS 0 + cikis 6 -> failed' 'failed' $s.State
    Assert-CigEqual 'RESULT PASS 0 + cikis 6 -> exit-mismatch' 'exit-mismatch' $s.Reason

    $p = New-CigReportFixture -Name 'pass-with-fail-line' -Lines @(
        'CIGRELEASESELFTEST v1', 'FAIL  denge-verisi', 'RESULT PASS 0')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'PASS ama FAIL satiri var -> failed' 'failed' $s.State

    $p = New-CigReportFixture -Name 'inconsistent' -Lines @('CIGRELEASESELFTEST v1', 'RESULT PASS 3')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 3
    Assert-CigEqual 'RESULT PASS 3 -> failed' 'failed' $s.State

    $p = New-CigReportFixture -Name 'garbage-result' -Lines @('CIGRELEASESELFTEST v1', 'RESULT MAYBE')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'RESULT cozulemiyor -> failed' 'failed' $s.State
    Assert-CigEqual 'RESULT cozulemiyor -> bad-result' 'bad-result' $s.Reason

    $p = New-CigReportFixture -Name 'duplicate-same-result' -Lines @(
        'CIGRELEASESELFTEST v1', 'RESULT PASS 0', 'RESULT PASS 0')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'ayni RESULT tekrari -> failed' 'failed' $s.State
    Assert-CigEqual 'ayni RESULT tekrari -> duplicate-result' 'duplicate-result' $s.Reason

    $p = New-CigReportFixture -Name 'duplicate-conflicting-result' -Lines @(
        'CIGRELEASESELFTEST v1', 'FAIL  denge-verisi', 'RESULT PASS 0', 'RESULT FAIL 6')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'celisen RESULT tekrari -> failed' 'failed' $s.State
    Assert-CigEqual 'celisen RESULT tekrari -> duplicate-result' 'duplicate-result' $s.Reason

    $p = New-CigReportFixture -Name 'fail-zero' -Lines @(
        'CIGRELEASESELFTEST v1', 'FAIL  sistemler', 'RESULT FAIL 0')
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
    Assert-CigEqual 'RESULT FAIL 0 -> failed' 'failed' $s.State
    Assert-CigEqual 'RESULT FAIL 0 -> inconsistent-result' 'inconsistent-result' $s.Reason

    # Timeout beats everything, including a report left behind by an earlier run.
    $p = New-CigReportFixture -Name 'timeout' -Lines $passReport
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0 -TimedOut
    Assert-CigEqual 'zaman asimi -> failed' 'failed' $s.State
    Assert-CigEqual 'zaman asimi -> timeout' 'timeout' $s.Reason

    $s = Get-CigSelfTestState -ReportPath '' -ProcessExitCode 0
    Assert-CigEqual 'bos yol -> failed' 'failed' $s.State

    $p = New-CigReportFixture -Name 'no-exit' -Lines $passReport
    $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode $null
    Assert-CigEqual 'cikis kodu yok -> failed' 'failed' $s.State
    Assert-CigEqual 'cikis kodu yok -> no-exit-code' 'no-exit-code' $s.Reason

    # Windows mandatory locks let the unreadable branch be exercised without
    # changing ACLs. Unix locks are advisory, so that platform skips this one.
    if ($IsWindows) {
        $p = New-CigReportFixture -Name 'locked' -Lines $passReport
        $lock = [IO.File]::Open($p, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        try {
            $s = Get-CigSelfTestState -ReportPath $p -ProcessExitCode 0
            Assert-CigEqual 'okunamayan rapor -> failed' 'failed' $s.State
            Assert-CigEqual 'okunamayan rapor -> unreadable-report' 'unreadable-report' $s.Reason
        }
        finally { $lock.Dispose() }
    }

    # --- The two outcomes that are neither -----------------------------------
    $o = Resolve-CigSelfTestOutcome -SkipSelfTest
    Assert-CigEqual '-SkipSelfTest -> skipped' 'skipped' $o.State
    Assert-CigEqual '-SkipSelfTest kontrol listesine girmez' $false $o.CountsAsCheck
    Assert-CigEqual '-SkipSelfTest gecti sayilmaz' $false $o.IsPass

    $absent = Get-CigSelfTestState -ReportPath (Join-Path $fixtureRoot 'absent.txt') -ProcessExitCode 0
    $o = Resolve-CigSelfTestOutcome -AllowLegacyPackageWithoutSelfTest -Observed $absent
    Assert-CigEqual 'eski paket izni -> unsupported' 'unsupported' $o.State
    Assert-CigEqual 'unsupported kontrol listesine girmez' $false $o.CountsAsCheck
    Assert-CigEqual 'unsupported gecti sayilmaz' $false $o.IsPass

    # The allowance covers an absent report and nothing else. A self-test that ran
    # and failed still fails, which is what stops it becoming a way past a real
    # defect.
    $reallyFailed = Get-CigSelfTestState -ReportPath (New-CigReportFixture -Name 'legacy-fail' -Lines $failReport) -ProcessExitCode 6
    $o = Resolve-CigSelfTestOutcome -AllowLegacyPackageWithoutSelfTest -Observed $reallyFailed
    Assert-CigEqual 'eski paket izni gercek basarisizligi kurtarmaz' 'failed' $o.State
    Assert-CigEqual 'gercek basarisizlik kontrol listesine girer' $true $o.CountsAsCheck

    # --- Default path: no switches, nothing excuses it -----------------------
    $o = Resolve-CigSelfTestOutcome -Observed $absent
    Assert-CigEqual 'varsayilan yolda rapor yoksa -> failed' 'failed' $o.State
    Assert-CigEqual 'varsayilan yolda rapor yoksa kontrol listesine girer' $true $o.CountsAsCheck

    $o = Resolve-CigSelfTestOutcome -Observed (Get-CigSelfTestState -ReportPath (New-CigReportFixture -Name 'ok2' -Lines $passReport) -ProcessExitCode 0)
    Assert-CigEqual 'varsayilan yolda gecerli rapor -> passed' 'passed' $o.State
    Assert-CigEqual 'passed gecti sayilir' $true $o.IsPass
}
finally {
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ''
if ($script:failures -gt 0) {
    Write-Host ("$($script:failures)/$($script:total) iddia basarisiz.") -ForegroundColor Red
    exit 1
}
Write-Host ("$($script:total) iddia gecti.") -ForegroundColor Green
exit 0
