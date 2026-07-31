# Shared helpers. Dot-source this; it defines paths and fails loudly rather than
# guessing when the engine is not where it is expected.

$ErrorActionPreference = 'Stop'

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:UProject = Join-Path $RepoRoot 'CigkofteSimulator.uproject'

function Get-CigEngineRoot {
    param([string]$Override)

    if ($Override) { $candidates = @($Override) }
    elseif ($env:CIG_UE_ROOT) { $candidates = @($env:CIG_UE_ROOT) }
    else {
        # Newest first, so a machine with several engines picks the one this
        # project targets rather than an old install.
        $candidates = @(
            'C:\Program Files\Epic Games\UE_5.8',
            'C:\Program Files\Epic Games\UE_5.7',
            'D:\Program Files\Epic Games\UE_5.8'
        )
    }

    foreach ($c in $candidates) {
        if (Test-Path (Join-Path $c 'Engine\Build\BatchFiles\Build.bat')) { return $c }
    }

    throw "Unreal Engine not found. Tried: $($candidates -join ', '). Set CIG_UE_ROOT or pass -EngineRoot."
}

function Write-CigStep {
    param([string]$Text)
    Write-Host ''
    Write-Host "==> $Text" -ForegroundColor Cyan
}

function Resolve-CigPath {
    <#
    .SYNOPSIS
    Turns a possibly relative path into an absolute one, against the shell's
    location.

    .DESCRIPTION
    [System.IO.Path]::GetFullPath resolves against the .NET process working
    directory, which is not PowerShell's location and does not follow Set-Location.
    Passing -BuildDirectory 'Build\WindowsDemo' from one checkout therefore
    resolved against whichever directory the host process happened to start in -
    and Create-ReleaseArchive.ps1 built a release zip out of a different
    checkout's packaged build, quietly and with a plausible-looking result.
    #>
    param([Parameter(Mandatory)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location).ProviderPath $Path))
}

# ------------------------------------------------------------ release self-test

$script:CigSelfTestHeader = 'CIGRELEASESELFTEST v1'

function Get-CigSelfTestState {
    <#
    .SYNOPSIS
    Classifies one release self-test run as passed or failed. Never anything else.

    .DESCRIPTION
    Split out of SmokeTest-PackagedBuild.ps1 so it can be exercised against
    fixture files by Test-SelfTestState.ps1 without packaging a game, and so the
    rule lives in one place.

    Everything that is not a complete, internally consistent PASS is a failure.
    The earlier version of this logic answered "unsupported" to a missing or
    unreadable report and the smoke test then exited 0, so a package whose
    self-test never ran - or ran and died halfway - read as a verified build. A
    package built from this source tree has the mode; if the report is not there,
    something went wrong, and that is the whole point of the check.

    "Skipped" and "unsupported" are decided by the caller from its switches, not
    here, so neither can ever be produced by reading a report.
    #>
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$ReportPath,
        # $null means the exit code could not be observed, which is itself a fault.
        [AllowNull()][object]$ProcessExitCode = $null,
        [switch]$TimedOut
    )

    function New-State([string]$Reason, [string]$Detail) {
        [pscustomobject]@{ State = 'failed'; Reason = $Reason; Detail = $Detail }
    }

    if ($TimedOut) {
        return New-State 'timeout' 'oz-test sureci zaman asimina ugradi ve sonlandirildi'
    }
    if ([string]::IsNullOrWhiteSpace($ReportPath)) {
        return New-State 'no-path' 'oz-test rapor yolu bos'
    }
    if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
        return New-State 'missing-report' "oz-test raporu yazilmadi: $ReportPath"
    }

    $lines = @()
    try { $lines = @(Get-Content -LiteralPath $ReportPath -ErrorAction Stop) }
    catch { return New-State 'unreadable-report' "oz-test raporu okunamadi: $($_.Exception.Message)" }

    if ($lines.Count -eq 0 -or $lines[0] -ne $script:CigSelfTestHeader) {
        $first = if ($lines.Count -gt 0) { $lines[0] } else { '<bos dosya>' }
        return New-State 'bad-header' "rapor basligi gecersiz: '$first' (beklenen '$($script:CigSelfTestHeader)')"
    }

    $result = @($lines | Where-Object { $_ -like 'RESULT *' }) | Select-Object -First 1
    if (-not $result) {
        # The report is written to a temporary file and renamed into place, so a
        # header with no verdict means the rename produced a truncated file or
        # something wrote the path underneath us. Either way it is not a result.
        return New-State 'no-result' 'raporda RESULT satiri yok - oz-test tamamlanmadan bitti'
    }
    if ($result -notmatch '^RESULT (PASS|FAIL) (\d+)$') {
        return New-State 'bad-result' "RESULT satiri cozulemedi: '$result'"
    }

    $verdict = $Matches[1]
    $index = [int]$Matches[2]
    $firstFail = @($lines | Where-Object { $_ -like 'FAIL *' }) | Select-Object -First 1

    # The verdict and its index have to agree with each other before either is
    # compared with the process.
    if (($verdict -eq 'PASS') -ne ($index -eq 0)) {
        return New-State 'inconsistent-result' "RESULT $verdict $index kendi icinde tutarsiz"
    }
    if ($verdict -eq 'PASS' -and $firstFail) {
        return New-State 'inconsistent-result' "RESULT PASS ama raporda basarisiz kontrol var: $firstFail"
    }

    # The exit code carries the same verdict through a second channel. They
    # disagreed once for real: RequestExitWithStatus(Force=false) left a run whose
    # report said FAIL 6 exiting 0, which would have made every failure read as a
    # pass. A mismatch is a failure whichever side is right.
    if ($null -eq $ProcessExitCode) {
        return New-State 'no-exit-code' 'oz-test surecinin cikis kodu okunamadi'
    }
    $expected = $index
    if ([int]$ProcessExitCode -ne $expected) {
        return New-State 'exit-mismatch' "rapor RESULT $verdict $index diyor, surec $ProcessExitCode ile cikti"
    }

    if ($verdict -eq 'FAIL') {
        return New-State 'check-failed' "$firstFail; cikis kodu $ProcessExitCode"
    }
    return [pscustomobject]@{ State = 'passed'; Reason = 'ok'; Detail = "$($lines.Count) satirlik rapor" }
}

function Resolve-CigSelfTestOutcome {
    <#
    .SYNOPSIS
    Turns the caller's switches plus an observed state into the four distinct
    outcomes the smoke test reports.

    .DESCRIPTION
    passed      - ran, complete, consistent. The only outcome that counts as a pass.
    failed      - ran and did not produce that. Fails packaging.
    skipped     - -SkipSelfTest was given explicitly. Diagnostic use only.
    unsupported - a report is absent AND the caller explicitly allowed a package
                  built before the mode existed. Never a pass, never a default.

    CountsAsCheck is what keeps the last two honest: they do not enter the
    pass/fail set at all, so nothing can add them up as verified.
    #>
    param(
        [switch]$SkipSelfTest,
        [switch]$AllowLegacyPackageWithoutSelfTest,
        [AllowNull()][object]$Observed = $null
    )

    if ($SkipSelfTest) {
        return [pscustomobject]@{
            State = 'skipped'; Detail = '-SkipSelfTest verildi'
            CountsAsCheck = $false; IsPass = $false
        }
    }
    if ($null -eq $Observed) {
        return [pscustomobject]@{
            State = 'failed'; Detail = 'oz-test calistirilmadi'
            CountsAsCheck = $true; IsPass = $false
        }
    }
    if ($Observed.State -eq 'passed') {
        return [pscustomobject]@{
            State = 'passed'; Detail = $Observed.Detail
            CountsAsCheck = $true; IsPass = $true
        }
    }
    if ($AllowLegacyPackageWithoutSelfTest -and $Observed.Reason -eq 'missing-report') {
        return [pscustomobject]@{
            State = 'unsupported'
            Detail = 'bu paket -CigReleaseSelfTest modunu tanimiyor (-AllowLegacyPackageWithoutSelfTest)'
            CountsAsCheck = $false; IsPass = $false
        }
    }
    return [pscustomobject]@{
        State = 'failed'; Detail = $Observed.Detail
        CountsAsCheck = $true; IsPass = $false
    }
}
