<#
.SYNOPSIS
Runs the CigBench route against a packaged build and reports the frame numbers.

.DESCRIPTION
Fills in PERFORMANCE_BUDGET.md with measurements rather than estimates.

Deliberately not Unreal Insights. A `.utrace` is a binary that needs the Insights
GUI to read, so a number taken from one cannot be pasted into a document, diffed
against last week's, or checked by anyone who was not sitting at the machine. The
CSV profiler writes the same frame timings as text, which can be.

The route, the seed and the dwell time come from `CigBench` in the game, so what
varies between two runs is the build - not where the camera happened to be
pointing.

Requires a Development build: the CSV profiler is compiled out of Shipping.
#>
[CmdletBinding()]
param(
    [string]$PackageDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Build\WindowsDemo'),
    [ValidateRange(2, 120)]
    [int]$SecondsPerStop = 6,
    [ValidateRange(640, 7680)]
    [int]$Width = 1920,
    [ValidateRange(480, 4320)]
    [int]$Height = 1080,
    [string]$Label = 'run',
    # Re-reads a capture instead of taking a new one. A run costs a minute of
    # wall clock and a fresh set of frames; re-reading one that already exists
    # is how a reporting change gets checked without pretending to be a new
    # measurement.
    [string]$CsvPath,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$package = [System.IO.Path]::GetFullPath($PackageDirectory)
$reuseCsv = [bool]$CsvPath
$exe = Get-ChildItem -LiteralPath $package -Filter 'CigkofteSimulator.exe' -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $exe -and -not $DryRun -and -not $reuseCsv) {
    Write-Error "Paketli EXE bulunamadi: $package"
    exit 1
}

# Five stops plus the setup step, plus one period of lead-in before the first
# stop and one to notice the route has ended.
$stops = 5
$expectedSeconds = ($stops + 3) * $SecondsPerStop

$exeDir = if ($exe) { Split-Path -Parent $exe.FullName } else { $package }
$csvDir = Join-Path $exeDir 'CigkofteSimulator\Saved\Profiling\CSV'

$gameArgs = @(
    "-ResX=$Width", "-ResY=$Height", '-windowed', '-unattended', '-nosplash',
    # GPU timings are off by default and are half the point of the exercise.
    '-csvGpuStats',
    # The inner quotes are load-bearing. Without them the space splits the token,
    # UE reads -ExecCmds=CigBench and drops the number on the floor as a stray
    # argument - so the run silently used the default dwell time and the -SecondsPerStop
    # switch appeared to do nothing.
    "-ExecCmds=`"CigBench $SecondsPerStop`""
)

if ($DryRun) {
    Write-Output "Dry-run: '$exeDir' $($gameArgs -join ' '); ~$expectedSeconds sn; CSV=$csvDir"
    exit 0
}

if ($reuseCsv) {
    $csv = Get-Item -LiteralPath $CsvPath -ErrorAction SilentlyContinue
    if (-not $csv) {
        Write-Error "CSV bulunamadi: $CsvPath"
        exit 1
    }
    Write-CigStep "Mevcut capture yeniden okunuyor (yeni kosu yok): $($csv.Name)"
}
else {
    Write-CigStep "Performans olcumu: $stops durak x $SecondsPerStop sn, ${Width}x${Height}"

    # The capture this run writes is identified by not having been there before
    # it. Taking "newest in the folder" instead reports the previous run's
    # numbers whenever a run produces no capture at all - which is the one case
    # where a wrong answer would go unnoticed.
    $before = @(Get-ChildItem -LiteralPath $csvDir -Filter '*.csv' -File -ErrorAction SilentlyContinue |
        ForEach-Object { $_.FullName })

    $proc = Start-Process $exe.FullName -PassThru -WorkingDirectory $exeDir -ArgumentList $gameArgs
    if (-not $proc.WaitForExit(($expectedSeconds + 120) * 1000)) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        Get-Process -Name 'CigkofteSimulator*' -ErrorAction SilentlyContinue |
            Stop-Process -Force -ErrorAction SilentlyContinue
        Write-Error "Benchmark zaman asimina ugradi (~$expectedSeconds sn bekleniyordu)."
        exit 1
    }

    $csv = Get-ChildItem -LiteralPath $csvDir -Filter '*.csv' -File -ErrorAction SilentlyContinue |
        Where-Object { $before -notcontains $_.FullName } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    if (-not $csv) {
        Write-Error "Bu kosu CSV uretmedi: $csvDir. Development yapisi mi? Shipping'de CSV profiler derlenmez."
        exit 1
    }
}

# Parsed by hand rather than with Import-Csv, for two reasons.
#
# The capture repeats column names - TextureStreaming/RenderAssetStreamingUpdate
# appears twice - and Import-Csv treats a duplicate header as a fatal error
# because it cannot build an object with two identical properties. Reading by
# column index does not care.
#
# And every number in the file uses a decimal point. On a Turkish-locale machine
# a culture-sensitive parse reads "16.7" as 167, so the run would report an
# impossibly fast build. The parse is pinned to the invariant culture.
$fileLines = [System.IO.File]::ReadAllLines($csv.FullName)
if ($fileLines.Count -lt 31) {
    Write-Error "CSV'de yalnizca $($fileLines.Count - 1) kare var; olcum icin yetersiz."
    exit 1
}
$header = $fileLines[0] -split ','
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
$floatStyle = [System.Globalization.NumberStyles]::Float

function Get-Series {
    param([string[]]$Lines, [string[]]$Header, [string]$Column)
    $idx = [Array]::IndexOf($Header, $Column)
    if ($idx -lt 0) { return @() }

    $out = [System.Collections.Generic.List[double]]::new()
    for ($i = 1; $i -lt $Lines.Count; $i++) {
        $parts = $Lines[$i] -split ','
        if ($parts.Count -le $idx) { continue }
        $d = 0.0
        # The file ends with metadata rows that are not frames. Anything that is
        # not a number is dropped rather than counted as zero, which would pull
        # every average down and make the build look faster than it is.
        if ([double]::TryParse($parts[$idx], $floatStyle, $invariant, [ref]$d)) { $out.Add($d) }
    }
    return $out.ToArray()
}

function Get-Stat {
    param([double[]]$Values)
    if (-not $Values -or $Values.Count -eq 0) { return $null }
    $sorted = @($Values | Sort-Object)
    $n = $sorted.Count
    [pscustomobject]@{
        Count = $n
        Avg   = [math]::Round((($Values | Measure-Object -Average).Average), 2)
        # 99th percentile of frame time is the slow-frame figure; the "1% low" in
        # the budget is its reciprocal, which is why both are reported.
        P99   = [math]::Round($sorted[[math]::Min($n - 1, [int][math]::Floor($n * 0.99))], 2)
        Max   = [math]::Round($sorted[$n - 1], 2)
    }
}

# Timings first, then the scene cost, then what the machine is holding. A column
# that the capture does not carry is skipped rather than reported as zero: the
# set differs with the command line (-csvGpuStats) and with the build
# configuration, and a silent zero reads as "free" instead of "not measured".
$timings = 'FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime'
$sceneCost = 'DrawCall/ShadowDepths', 'DrawCall/Basepass', 'DrawCall/BeginOcclusionTests',
             'RHI/DrawCalls', 'RHI/PrimitivesDrawn', 'ActorCount/StaticMeshActor'
$memory = 'PhysicalUsedMB', 'GPUMem/LocalUsedMB', 'GPUMem/LocalBudgetMB',
          'TextureStreaming/StreamingPool', 'TextureStreaming/RequiredPool'

$measurements = [ordered]@{}
foreach ($col in ($timings + $sceneCost + $memory)) {
    $stat = Get-Stat -Values (Get-Series -Lines $fileLines -Header $header -Column $col)
    if ($stat) { $measurements[$col] = $stat }
}

if (-not $measurements.Contains('FrameTime')) {
    Write-Error "CSV'de FrameTime sutunu yok: $($csv.Name)"
    exit 1
}

$frame = $measurements['FrameTime']
$avgFps = [math]::Round(1000.0 / [math]::Max($frame.Avg, 0.001), 1)
$lowFps = [math]::Round(1000.0 / [math]::Max($frame.P99, 0.001), 1)

$groups = [ordered]@{ 'sure (ms)' = $timings; 'sahne' = $sceneCost; 'bellek (MB)' = $memory }
foreach ($groupName in $groups.Keys) {
    $present = @($groups[$groupName] | Where-Object { $measurements.Contains($_) })
    if (-not $present) { continue }
    Write-Host ''
    Write-Host ("  {0,-30} {1,9} {2,9} {3,9}" -f $groupName, 'ort', 'p99', 'en kotu')
    foreach ($name in $present) {
        $m = $measurements[$name]
        Write-Host ("  {0,-30} {1,9} {2,9} {3,9}" -f $name, $m.Avg, $m.P99, $m.Max)
    }
}

Write-Host ''
Write-Host ("  kare: {0} | ortalama {1} FPS | %1 dusuk {2} FPS" -f $frame.Count, $avgFps, $lowFps) -ForegroundColor Cyan

# The texture budget is a ratio, and a ratio is the thing worth reading. Reported
# only when both halves are present, rather than assuming a default budget.
if ($measurements.Contains('GPUMem/LocalUsedMB') -and $measurements.Contains('GPUMem/LocalBudgetMB')) {
    $budget = $measurements['GPUMem/LocalBudgetMB'].Avg
    if ($budget -gt 0) {
        $pct = [math]::Round(100.0 * $measurements['GPUMem/LocalUsedMB'].Max / $budget, 1)
        Write-Host ("  GPU bellek: en yuksek {0} MB / butce {1} MB = %{2}" -f
            $measurements['GPUMem/LocalUsedMB'].Max, $budget, $pct) -ForegroundColor Cyan
    }
}
# Per stop, which is the reason CigBench emits an event at each one.
#
# A single average over the whole route hides the expensive viewpoint inside four
# cheap ones, and "the peak is somewhere" is not a finding anybody can act on.
# The EVENTS column carries "CigBench/<stop>##<timestamp>" on the frame the
# camera moved; a stop owns every frame from its marker up to the next one.
$eventsIdx = [Array]::IndexOf($header, 'EVENTS')
$frameIdx = [Array]::IndexOf($header, 'FrameTime')
if ($eventsIdx -ge 0 -and $frameIdx -ge 0) {
    $marks = [System.Collections.Generic.List[object]]::new()
    for ($i = 1; $i -lt $fileLines.Count; $i++) {
        $parts = $fileLines[$i] -split ','
        if ($parts.Count -le $eventsIdx) { continue }
        $m = [regex]::Match($parts[$eventsIdx], 'CigBench/([A-Za-z0-9_]+)')
        if ($m.Success) { $marks.Add([pscustomobject]@{ Row = $i; Name = $m.Groups[1].Value }) }
    }

    if ($marks.Count -gt 0) {
        Write-Host ''
        Write-Host ("  {0,-30} {1,9} {2,9} {3,9} {4,7}" -f 'durak (ms)', 'ort', 'p99', 'en kotu', 'kare')
        for ($k = 0; $k -lt $marks.Count; $k++) {
            $from = $marks[$k].Row
            $to = if ($k + 1 -lt $marks.Count) { $marks[$k + 1].Row - 1 } else { $fileLines.Count - 1 }

            $vals = [System.Collections.Generic.List[double]]::new()
            for ($i = $from; $i -le $to; $i++) {
                $parts = $fileLines[$i] -split ','
                if ($parts.Count -le $frameIdx) { continue }
                $d = 0.0
                if ([double]::TryParse($parts[$frameIdx], $floatStyle, $invariant, [ref]$d)) { $vals.Add($d) }
            }

            $s = Get-Stat -Values $vals.ToArray()
            if ($s) {
                Write-Host ("  {0,-30} {1,9} {2,9} {3,9} {4,7}" -f $marks[$k].Name, $s.Avg, $s.P99, $s.Max, $s.Count)
            }
        }
    }
}

Write-Host ''
Write-Host "  CSV: $($csv.FullName)"
Write-Host "  etiket: $Label; cozunurluk ${Width}x${Height}; commit $(git rev-parse --short HEAD)"
exit 0
