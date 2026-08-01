<#
.SYNOPSIS
Starts a packaged build and checks that it is shippable, not merely that it runs.

.DESCRIPTION
The single smoke test for every packaging path. Both PackageDemo.ps1 and
Package-Windows.ps1 call it, because there used to be two: one that verified
content and one that was satisfied by a green UAT exit and an EXE on disk. That
second bar is the one this project has already failed against - see below.

A package that builds is not a package that runs. The first successful package
of this project had no text, no audio and none of its balance data: the CSVs are
read from disk and the sounds are resolved by path, so the cooker saw no
reference to any of them and staged none of them. UAT reported success, the
archive had an executable in it, and the game showed raw keys like
"msg.customer.served" in silence. Nothing short of starting it would have found
that, which is why every packaging path has to end here.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PackageDirectory,
    [string]$EngineRoot,
    [ValidateRange(5, 600)]
    [int]$TimeoutSeconds = 25,
    # Shipping has no log, and that is a property of the configuration rather
    # than a fault in the build. Passed in by the caller rather than sniffed off
    # the staged files: the caller already knows, and guessing it wrong turns a
    # good build into a failed smoke test or the reverse.
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Development',
    # The self-test is a second launch of the packaged game. Skipping it is for
    # diagnosing the harness itself; a skip is reported, never counted as a pass.
    # No packaging path passes this - PackageDemo.ps1 and Package-Windows.ps1 both
    # call this script without it, deliberately.
    [switch]$SkipSelfTest,
    # For pointing the harness at a package built before -CigReleaseSelfTest
    # existed. Reports "unsupported", which is neither a pass nor a skip, and is
    # only ever reachable by asking for it here.
    [switch]$AllowLegacyPackageWithoutSelfTest,
    [ValidateRange(10, 900)]
    [int]$SelfTestTimeoutSeconds = 120,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$package = Resolve-CigPath $PackageDirectory
$exe = Get-ChildItem -LiteralPath $package -Filter 'CigkofteSimulator.exe' -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1

if ($DryRun) {
    Write-Output "Dry-run: '$package' icindeki paket $TimeoutSeconds saniye calistirilip icerik denetlenecek."
    exit 0
}
if (-not $exe) {
    # UAT can report success having staged nothing useful; an archive with no
    # executable in it is not a build the player can start.
    Write-Error "Paketli EXE bulunamadi: $package"
    exit 1
}

$engine = Get-CigEngineRoot -Override $EngineRoot

Write-CigStep 'Smoke test: paketli yapi baslatiliyor'

$stagedLog = Join-Path (Split-Path -Parent $exe.FullName) 'CigkofteSimulator\Saved\Logs\CigkofteSimulator.log'
if (Test-Path $stagedLog) { Remove-Item $stagedLog -Force }

$proc = Start-Process $exe.FullName -PassThru -WindowStyle Hidden `
    -ArgumentList '-nullrhi', '-unattended', '-nosplash', '-nosound', '-log'
Start-Sleep -Seconds $TimeoutSeconds

# Did it survive the run, and if not, how did it go?
#
# This is the only check available to a build that cannot write a log, and it is
# worth having in either case: a game that crashes three seconds after start has
# exited by now, with a code that says so.
$exitedEarly = $proc.HasExited
$earlyExitCode = if ($exitedEarly) { $proc.ExitCode } else { 0 }

# Kill the tree and wait for it, rather than the launcher handle alone.
#
# A bare .Kill() on the Start-Process handle left the game running: the next
# packaging run then failed because the surviving process still held
# DirectML.dll open, and UAT reported a file-access error that has nothing to do
# with the build. A smoke test that leaves a process behind breaks the thing it
# was added to protect.
if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
Get-Process -Name 'CigkofteSimulator*' -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue

$deadline = (Get-Date).AddSeconds(30)
while ((Get-Process -Name 'CigkofteSimulator*' -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
}
if (Get-Process -Name 'CigkofteSimulator*' -ErrorAction SilentlyContinue) {
    Write-Error 'Smoke-test sureci kapanmadi; sonraki paketlemeyi kilitler. Elle sonlandirin.'
    exit 1
}

$haveLog = Test-Path $stagedLog
if (-not $haveLog -and $Configuration -ne 'Shipping') {
    Write-Error "Paketli yapi log uretmedi: $stagedLog - hic baslamadi."
    exit 1
}
if (-not $haveLog) {
    # Shipping compiles logging out, so the six checks below - every one of which
    # is "the log did not complain" - have nothing to read. This is not a build
    # that failed to start; it is a build that cannot say anything, and the first
    # Shipping package of this project was reported as "hic baslamadi" for exactly
    # that reason while running perfectly well.
    #
    # What survives: the container check, which asks the cooked data what is in it
    # and never needed the game to run, and the fact that the process was still
    # alive when the timeout came round.
    Write-Host '  log yok (Shipping): log tabanli kontroller atlaniyor' -ForegroundColor Yellow
}

$lines = if ($haveLog) { Get-Content $stagedLog } else { @() }
$csvCount = (Get-ChildItem (Join-Path $RepoRoot 'Config\Balance\*.csv')).Count

# Read the cooked container and count what each declared directory produced.
#
# Every log check below is the absence of a complaint, and two of them are the
# absence of a complaint from a lazy loader: ResolveSound and CigMesh both load
# on first use, so a headless run that never triggers a sound cannot fail the
# audio check. It passed for exactly that reason against a build that had not
# cooked a single /Game asset. This check is positive: it asks the container what
# is in it. Repository-owned directories must always produce assets. Optional
# licensed packs only become mandatory when their uassets exist in this source
# checkout; otherwise the public repository's primitive fallback is the intended
# result, not a failed package.
Write-CigStep 'Cook denetimi: her DirectoriesToAlwaysCook girdisi varlik uretti mi'

$unrealPak = Join-Path $engine 'Engine\Binaries\Win64\UnrealPak.exe'
$toc = Join-Path (Split-Path -Parent $exe.FullName) 'CigkofteSimulator\Content\Paks\CigkofteSimulator-Windows.utoc'
$emptyDirs = @()

if (-not (Test-Path $unrealPak)) { $emptyDirs += "<UnrealPak yok: $unrealPak>" }
elseif (-not (Test-Path $toc))   { $emptyDirs += "<container yok: $toc>" }
else {
    $listing = & $unrealPak $toc -List 2>&1 | Out-String
    $cookDirs = [regex]::Matches(
            (Get-Content (Join-Path $RepoRoot 'Config\DefaultGame.ini') -Raw),
            '\+DirectoriesToAlwaysCook=\(Path="([^"]+)"\)') |
        ForEach-Object { $_.Groups[1].Value }

    if (-not $cookDirs) { $emptyDirs += '<DefaultGame.ini icinde hic cook girdisi yok>' }

    foreach ($d in $cookDirs) {
        # /Game/Audio maps to CigkofteSimulator/Content/Audio in the container.
        $rel = 'Content/' + $d.Substring('/Game/'.Length)
        # Case-insensitive on purpose. Package paths are case-preserving but not
        # case-sensitive, and the container writes its own casing: a folder that
        # is "materials" on disk and in the ini is listed as "Materials". Matching
        # exactly reported an empty directory for assets that had in fact cooked.
        $n = ([regex]::Matches($listing, [regex]::Escape($rel) + '/[^\s"]+\.uasset', 'IgnoreCase')).Count
        $mustProduce = Test-CigCookDirectoryMustProduce -GamePath $d -RepositoryRoot $RepoRoot
        if ($n -eq 0 -and $mustProduce) {
            $emptyDirs += $d
            Write-Host ("  {0,6}  {1}" -f 0, $d) -ForegroundColor Red
        }
        elseif ($n -eq 0) {
            Write-Host ("  {0,6}  {1}  (opsiyonel paket kaynakta yok)" -f 0, $d) -ForegroundColor Yellow
        }
        else {
            Write-Host ("  {0,6}  {1}" -f $n, $d)
        }
    }
}

# The release self-test: a second, separate run.
#
# Deliberately not folded into the run above. That one's whole value is that it
# survived $TimeoutSeconds of ordinary startup; a self-test run exits on purpose
# within seconds and would fail 'surec ayakta' every time. They answer different
# questions.
#
# This is the only positive evidence available about a Shipping build. Its checks
# run inside the game, so they need neither the log nor the cheat manager.
# Snapshot both save roots before that launch. Development writes beside its
# executable while Shipping normally redirects to LocalAppData; checking both
# makes the assertion independent of which path policy this engine build chose.
$packagedSaved = Join-Path (Split-Path -Parent $exe.FullName) 'CigkofteSimulator\Saved'
$localSaved = Join-Path $env:LOCALAPPDATA 'CigkofteSimulator\Saved'
$persistencePaths = @(
    (Join-Path $packagedSaved 'SaveGames\CigSave.sav'),
    (Join-Path $packagedSaved 'Config\Windows\GameUserSettings.ini'),
    (Join-Path $localSaved 'SaveGames\CigSave.sav'),
    (Join-Path $localSaved 'Config\Windows\GameUserSettings.ini')
) | Select-Object -Unique

function Get-CigPersistenceSnapshot {
    param([string[]]$Paths)
    $snapshot = @{}
    foreach ($path in $Paths) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $item = Get-Item -LiteralPath $path
            $snapshot[$path] = [pscustomobject]@{
                Exists = $true
                Length = $item.Length
                LastWriteTicks = $item.LastWriteTimeUtc.Ticks
                Hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            }
        }
        else {
            $snapshot[$path] = [pscustomobject]@{
                Exists = $false; Length = 0; LastWriteTicks = 0; Hash = ''
            }
        }
    }
    return $snapshot
}

$persistenceBefore = Get-CigPersistenceSnapshot -Paths $persistencePaths

# The report path is dictated, not discovered.
#
# FPaths::ProjectSavedDir() sits beside the executable in Development and
# redirects to %LOCALAPPDATA%\<Project>\Saved in Shipping, so a harness that
# guesses reads a passing Shipping run as a build with no self-test at all. The
# game takes the path as an argument and this is the only caller.
#
# Mandatory. The first version of this reported a missing or unreadable report as
# "unsupported", printed it beside a skip, kept it out of $checks and exited 0 -
# so the one outcome that most needed to fail packaging was the one that could not.
# Get-CigSelfTestState now answers only passed or failed, and the two states that
# are neither are reachable only by asking for them on the command line:
# -SkipSelfTest for diagnosing this harness, -AllowLegacyPackageWithoutSelfTest for
# a package built before the mode existed. Both print, neither counts.
$selfTestFile = Join-Path $package 'CigReleaseSelfTest.txt'
$observed = $null
if (-not $SkipSelfTest) {
    Write-CigStep 'Surum oz-testi calistiriliyor'

    # Clear the way, and prove the path is usable before blaming the game for not
    # writing to it. A stale report from an earlier run is the one file that could
    # make a self-test that never started read as a pass.
    try {
        $selfTestDir = Split-Path -Parent $selfTestFile
        if (-not (Test-Path -LiteralPath $selfTestDir)) {
            $null = New-Item -ItemType Directory -Path $selfTestDir -Force
        }
        if (Test-Path -LiteralPath $selfTestFile) { Remove-Item -LiteralPath $selfTestFile -Force }
        Set-Content -LiteralPath $selfTestFile -Value 'probe' -ErrorAction Stop
        Remove-Item -LiteralPath $selfTestFile -Force
    }
    catch {
        Write-Error "Oz-test rapor yolu yazilabilir degil: $selfTestFile - $($_.Exception.Message)"
        exit 1
    }

    $st = Start-Process $exe.FullName -PassThru -WindowStyle Hidden `
        -ArgumentList '-CigReleaseSelfTest', "-CigReleaseSelfTestOut=`"$selfTestFile`"", `
                      '-nullrhi', '-unattended', '-nosplash', '-nosound'
    $timedOut = -not $st.WaitForExit($SelfTestTimeoutSeconds * 1000)
    if ($timedOut) {
        # Same reasoning as the kill above: a survivor locks the next package.
        Stop-Process -Id $st.Id -Force -ErrorAction SilentlyContinue
        Get-Process -Name 'CigkofteSimulator*' -ErrorAction SilentlyContinue |
            Stop-Process -Force -ErrorAction SilentlyContinue
    }

    $exitCode = if ($timedOut) { $null } else { $st.ExitCode }
    $observed = Get-CigSelfTestState -ReportPath $selfTestFile -ProcessExitCode $exitCode -TimedOut:$timedOut

    $persistenceAfter = Get-CigPersistenceSnapshot -Paths $persistencePaths
    $changedPersistence = @()
    foreach ($path in $persistencePaths) {
        $before = $persistenceBefore[$path]
        $after = $persistenceAfter[$path]
        if ($before.Exists -ne $after.Exists -or $before.Length -ne $after.Length `
            -or $before.LastWriteTicks -ne $after.LastWriteTicks -or $before.Hash -ne $after.Hash) {
            $changedPersistence += $path
        }
    }
    if ($changedPersistence.Count -gt 0) {
        $observed = [pscustomobject]@{
            State = 'failed'
            Reason = 'persistence-changed'
            Detail = "surum oz-testi kalici oyuncu verisini degistirdi: $($changedPersistence -join ', ')"
        }
    }
}

$selfTest = Resolve-CigSelfTestOutcome -SkipSelfTest:$SkipSelfTest `
    -AllowLegacyPackageWithoutSelfTest:$AllowLegacyPackageWithoutSelfTest -Observed $observed
$selfTestState = $selfTest.State
$selfTestDetail = $selfTest.Detail

# Each check names the symptom the player would see, not the internal fault.
$requiredMeshWarnings = @($lines | Where-Object { $_ -match 'Mesh bulunamadi: /Game/LowPoly/' })

$checks = @(
    @{ Name = 'surec ayakta';    Ok = (-not $exitedEarly)
       Fail = "oyun $TimeoutSeconds sn dolmadan kapandi (cikis kodu $earlyExitCode) - baslangicta cokuyor" }
)
if ($haveLog) { $checks += @(
    @{ Name = 'metin tablosu';   Ok = -not (@($lines) -match 'Strings\.csv okunama')
       Fail = 'Strings.csv yuklenmedi - arayuzde ham anahtarlar gorunur' }
    @{ Name = 'denge verisi';    Ok = (@($lines) -match 'Denge dosyası uygulandı').Count -ge $csvCount
       Fail = "denge CSV'leri yuklenmedi - oyun C++ varsayilanlariyla calisir" }
    @{ Name = 'ses varliklari';  Ok = -not (@($lines) -match 'Ses bulunamadı: /Game/Audio/')
       Fail = 'ses varliklari cook edilmemis - oyun tamamen sessiz' }
    @{ Name = 'mesh varliklari'; Ok = ($requiredMeshWarnings.Count -eq 0)
       Fail = 'repository-owned LowPoly mesh varliklari cook edilmemis' }
    @{ Name = 'olumcul hata';    Ok = -not (@($lines) -match 'LogCig: Error|Fatal error|Assertion failed|Unhandled Exception')
       Fail = 'baslangicta hata var' }
) }
$checks += @(
    @{ Name = 'cook kapsami';    Ok = ($emptyDirs.Count -eq 0)
       Fail = "hicbir sey uretmeyen cook girdisi: $($emptyDirs -join ', ')" }
)
if ($selfTest.CountsAsCheck) {
    $checks += @(
        @{ Name = 'oz-test';     Ok = $selfTest.IsPass
           Fail = "surum oz-testi basarisiz - $selfTestDetail" }
    )
}
else {
    # Printed with the state's own word rather than a shared "ATLANDI", because
    # the two mean different things and only one of them was asked for. Neither
    # is in $checks, so neither can be summed into the pass count.
    $label = if ($selfTestState -eq 'skipped') { 'ATLANDI' } else { 'DESTEKLENMIYOR' }
    Write-Host ("  {0,-16} {1} - {2}" -f 'oz-test', $label, $selfTestDetail) -ForegroundColor Yellow
    Write-Host '  (oz-test dogrulanmadi; bu paket surum icin dogrulanmis sayilmaz)' -ForegroundColor Yellow
}

$smokeFailed = $false
foreach ($c in $checks) {
    if ($c.Ok) {
        Write-Host ("  {0,-16} OK" -f $c.Name) -ForegroundColor Green
    }
    else {
        Write-Host ("  {0,-16} FAIL - {1}" -f $c.Name, $c.Fail) -ForegroundColor Red
        $smokeFailed = $true
    }
}

if ($smokeFailed) {
    Write-Error "Paketli yapi basliyor ama sevk edilebilir degil. Log: $stagedLog"
    exit 1
}

if ($haveLog) {
    Write-Host "Smoke test passed (oz-test: $selfTestState)." -ForegroundColor Green
}
else {
    Write-Host "Smoke test passed (Shipping: surec + cook kapsami + oz-test: $selfTestState; log tabanli kontroller yapilamadi)." -ForegroundColor Green
}
Write-Host 'Still needs a human: launch it, switch language, load a save, navigate with a gamepad.'
# Explicit, because callers read $LASTEXITCODE. Falling off the end would leave
# it holding whatever UnrealPak.exe last returned.
exit 0
