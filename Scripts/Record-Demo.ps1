<#
.SYNOPSIS
Records the CigTour route from the packaged build and encodes it to MP4 and GIF.

.DESCRIPTION
The demo clip for the README and the store page, produced the same way twice
rather than by whoever happened to be holding the mouse.

`-dumpmovie` writes one PNG per frame, and `-benchmark` fixes the timestep so the
dump is even: without it the game runs on wall clock and a slow frame becomes a
long frame in the finished video, which reads as a stutter that is not there.
FFmpeg then encodes the sequence.

Two outputs, because they are for different places. GitHub renders a GIF inline
in a README; an MP4 is smaller, sharper and what a store page wants. Neither is
committed by default - a few MB of video does not belong in a source repository
unless somebody decides it does.

FFmpeg is expected under Tools/FFmpeg (gitignored, see docs/ASSET_INTAKE_PIPELINE.md).
#>
[CmdletBinding()]
param(
    [string]$PackageDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Build\WindowsDemo'),
    [ValidateRange(320, 3840)]
    [int]$Width = 1280,
    [ValidateRange(240, 2160)]
    [int]$Height = 720,
    [ValidateRange(10, 60)]
    [int]$Fps = 30,
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'AssetWork\Renders'),
    [switch]$KeepFrames,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$ffmpeg = Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'Tools\FFmpeg') -Filter 'ffmpeg.exe' -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $ffmpeg -and -not $DryRun) {
    Write-Error "ffmpeg.exe bulunamadi: Tools\FFmpeg. Kurulum icin docs/ASSET_INTAKE_PIPELINE.md."
    exit 1
}

$package = Resolve-CigPath $PackageDirectory
$exe = Get-ChildItem -LiteralPath $package -Filter 'CigkofteSimulator.exe' -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $exe -and -not $DryRun) {
    Write-Error "Paketli EXE bulunamadi: $package"
    exit 1
}

$exeDir = if ($exe) { Split-Path -Parent $exe.FullName } else { $package }
# Frames land with the screenshots, not in VideoCaptures: dumping is implemented
# as a screenshot request per frame. The -dumpmovie switch does nothing in 5.8 -
# the flag lives behind the r.DumpingMovie cvar, and a negative value means "stay
# on" rather than "for n frames".
$frameDir = Join-Path $exeDir 'CigkofteSimulator\Saved\Screenshots\Windows'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$mp4 = Join-Path $OutputDirectory "CigkofteDemo-$stamp.mp4"
$gif = Join-Path $OutputDirectory "CigkofteDemo-$stamp.gif"

# The benchmark route rather than the screenshot tour: CigTour requests named
# screenshots of its own, which would land in the same folder and be encoded as
# part of the clip. CigBench walks the same five viewpoints and only moves the
# camera.
$dwell = 5

$gameArgs = @(
    "-ResX=$Width", "-ResY=$Height", '-windowed', '-unattended', '-nosplash',
    # Fixed timestep. The dump is one file per frame either way; this is what
    # makes those frames equal lengths of game time, so a slow frame does not
    # become a long frame in the finished video and read as a stutter.
    '-benchmark', "-fps=$Fps",
    # English, because this clip sits at the top of an English README. CigLang
    # goes through the settings path, which also rebuilds the world's signage -
    # setting the language alone leaves the station signs in the other language.
    "-ExecCmds=`"CigLang 1, r.DumpingMovie -1, CigBench $dwell`""
)

if ($DryRun) {
    Write-Output "Dry-run: '$exeDir' $($gameArgs -join ' '); kareler=$frameDir; cikti=$mp4"
    exit 0
}

Write-CigStep "Demo kaydi: ${Width}x${Height} @ $Fps fps"

# Old frames are cleared rather than encoded by accident. The dump names files by
# frame number and restarts at 1, so a shorter run leaves the tail of a longer
# one behind and FFmpeg happily encodes both as one clip.
if (Test-Path $frameDir) { Remove-Item $frameDir -Recurse -Force }
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force

$proc = Start-Process $exe.FullName -PassThru -WorkingDirectory $exeDir -ArgumentList $gameArgs
if (-not $proc.WaitForExit(600 * 1000)) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Get-Process -Name 'CigkofteSimulator*' -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Error 'Kayit zaman asimina ugradi.'
    exit 1
}

$all = @(Get-ChildItem -LiteralPath $frameDir -Filter '*.png' -File -ErrorAction SilentlyContinue | Sort-Object Name)
if ($all.Count -lt $Fps) {
    Write-Error "Yeterli kare uretilmedi ($($all.Count)). r.DumpingMovie calisti mi?"
    exit 1
}

# Drop the lead-in: two dwell periods, not one.
#
# Dumping starts the moment the game does, so the first frames are the title
# screen and the world being built. CigBench then spends its first period setting
# the shop up - unlocking, restocking - and only reaches viewpoint one at the
# second. Skipping a single period looked right and was not: the clip opened on
# the setup step's own "[DEBUG] Stoklar dolduruldu", which is cleared when the
# camera arrives at the first stop.
$skip = [math]::Min($Fps * $dwell * 2, $all.Count - $Fps)
$frames = @($all | Select-Object -Skip $skip)
Write-Host "  $($all.Count) kare, ilk $skip atlandi -> $([math]::Round($frames.Count / $Fps, 1)) sn"

# A concat list rather than a numbered pattern. The dump names files with a
# timestamp suffix, not a clean counter, so %05d has nothing to match; the
# demuxer takes the files in the order given.
$listFile = Join-Path $OutputDirectory "frames-$stamp.txt"
$sb = [System.Text.StringBuilder]::new()
foreach ($f in $frames) {
    [void]$sb.AppendLine("file '$($f.FullName -replace "'", "'\''")'")
    [void]$sb.AppendLine("duration $([math]::Round(1.0 / $Fps, 5))")
}
[System.IO.File]::WriteAllText($listFile, $sb.ToString())

& $ffmpeg.FullName -y -loglevel error -f concat -safe 0 -i $listFile -r $Fps `
    -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart $mp4
$encodeOk = ($LASTEXITCODE -eq 0)
Remove-Item $listFile -Force -ErrorAction SilentlyContinue
if (-not $encodeOk) { Write-Error 'MP4 kodlamasi basarisiz.'; exit 1 }

# The GIF is a preview, not the video.
#
# The MP4 is the artefact worth watching; the GIF exists because that is what
# GitHub renders inline in a README. Cut to the first 12 seconds at 480 wide and
# 12 fps: the full 30 seconds at 720/15 came out at 38 MB, which is not something
# to put at the top of a page somebody opens on a phone.
#
# It gets its own palette. The default 216-colour web palette bands the shop's
# cream walls into visible steps, which is the one thing a still frame of this
# game should not do.
$gifFilters = 'fps=12,scale=480:-1:flags=lanczos'
$palette = Join-Path $OutputDirectory "palette-$stamp.png"
& $ffmpeg.FullName -y -loglevel error -t 12 -i $mp4 -vf "$gifFilters,palettegen=max_colors=192" $palette
& $ffmpeg.FullName -y -loglevel error -t 12 -i $mp4 -i $palette `
    -lavfi "$gifFilters [x]; [x][1:v] paletteuse=dither=bayer:bayer_scale=3" $gif
Remove-Item $palette -Force -ErrorAction SilentlyContinue

if (-not $KeepFrames) { Remove-Item $frameDir -Recurse -Force -ErrorAction SilentlyContinue }

foreach ($f in $mp4, $gif) {
    if (Test-Path $f) {
        Write-Host ("  {0}  {1:N1} MB" -f (Split-Path -Leaf $f), ((Get-Item $f).Length / 1MB)) -ForegroundColor Green
    }
}
exit 0
