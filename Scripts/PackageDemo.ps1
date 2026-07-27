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
    [string]$Configuration = 'Shipping',
    [switch]$SkipSmokeTest
)

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

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

# A package that builds is not a package that runs.
#
# The first successful package of this project had no text, no audio and none of
# its balance data: the CSVs are read from disk and the sounds are resolved by
# path, so the cooker saw no reference to any of them and staged none of them.
# UAT reported success, the archive had an executable in it, and the game showed
# raw keys like "msg.customer.served" in silence. Nothing short of starting it
# would have found that.
Write-CigStep 'Smoke test: starting the packaged build'

$stagedLog = Join-Path (Split-Path -Parent $exe.FullName) 'CigkofteSimulator\Saved\Logs\CigkofteSimulator.log'
if (Test-Path $stagedLog) { Remove-Item $stagedLog -Force }

$proc = Start-Process $exe.FullName -PassThru `
    -ArgumentList '-nullrhi', '-unattended', '-nosound', '-log'
Start-Sleep -Seconds 25

# Kill the tree and wait for it, rather than the launcher handle alone.
#
# A bare .Kill() on the Start-Process handle left the game running here: the
# next packaging run then failed because the surviving process still held
# DirectML.dll open, and UAT reported a file-access error that has nothing to
# do with the build. A smoke test that leaves a process behind breaks the
# thing it was added to protect.
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
    Write-Error 'Smoke-test process would not exit; it will lock the next package. Kill it manually.'
    exit 1
}

if (-not (Test-Path $stagedLog)) {
    Write-Error "Packaged build produced no log at $stagedLog; it did not start."
    exit 1
}

$lines = Get-Content $stagedLog
$csvCount = (Get-ChildItem (Join-Path $RepoRoot 'Config\Balance\*.csv')).Count

# Read the cooked container and count what each declared directory produced.
#
# Every log check below is the absence of a complaint, and two of them are the
# absence of a complaint from a lazy loader: ResolveSound and CigMesh both load
# on first use, so a headless run that never triggers a sound cannot fail the
# audio check. It passed for exactly that reason against a build that had not
# cooked a single /Game asset. This check is positive: it asks the container
# what is in it, and a directory that produced nothing is a directory whose
# assets the player will never see.
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
        $n = ([regex]::Matches($listing, [regex]::Escape($rel) + '/[^\s"]+\.uasset')).Count
        if ($n -eq 0) {
            $emptyDirs += $d
            Write-Host ("  {0,6}  {1}" -f 0, $d) -ForegroundColor Red
        }
        else {
            Write-Host ("  {0,6}  {1}" -f $n, $d)
        }
    }
}

# Each check names the symptom the player would see, not the internal fault.
$checks = @(
    @{ Name = 'metin tablosu';   Ok = -not (@($lines) -match 'Strings\.csv okunama')
       Fail = 'Strings.csv yuklenmedi - arayuzde ham anahtarlar gorunur' }
    @{ Name = 'denge verisi';    Ok = (@($lines) -match 'Denge dosyası uygulandı').Count -ge $csvCount
       Fail = "denge CSV'leri yuklenmedi - oyun C++ varsayilanlariyla calisir" }
    @{ Name = 'ses varliklari';  Ok = -not (@($lines) -match 'Ses bulunamadı: /Game/Audio/')
       Fail = 'ses varliklari cook edilmemis - oyun tamamen sessiz' }
    @{ Name = 'mesh varliklari'; Ok = -not (@($lines) -match 'Mesh bulunamadi')
       Fail = 'mesh varliklari cook edilmemis - dukkan ilkel kutulardan gorunur' }
    @{ Name = 'cook kapsami';    Ok = ($emptyDirs.Count -eq 0)
       Fail = "hicbir sey uretmeyen cook girdisi: $($emptyDirs -join ', ')" }
    @{ Name = 'olumcul hata';    Ok = -not (@($lines) -match 'LogCig: Error|Fatal error|Assertion failed')
       Fail = 'baslangicta hata var' }
)

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
    Write-Error "Packaged build starts but is not shippable. Log: $stagedLog"
    exit 1
}

Write-Host 'Smoke test passed.' -ForegroundColor Green
Write-Host 'Still needs a human: launch it, switch language, load a save, navigate with a gamepad.'
