[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputFile,
    [ValidateSet(44100, 48000)][int]$SampleRate = 48000,
    [ValidateSet(1, 2)][int]$Channels = 2,
    [string]$FfmpegPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CigCommon.ps1')
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
# The AssetWork\Audio guard below is only as good as this resolution.
$input = Resolve-CigPath $InputFile
$audioRoot = Join-Path $root 'AssetWork\Audio'
if (-not (Test-CigPathWithinDirectory -Path $input -Directory $audioRoot)) {
    throw 'Ses denetimi yalnız AssetWork\Audio altında çalışır.'
}
if (-not $FfmpegPath) {
    $projectFfmpeg = Get-ChildItem -LiteralPath (Join-Path $root 'Tools\FFmpeg') -Filter 'ffmpeg.exe' -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    $FfmpegPath = if ($projectFfmpeg) { $projectFfmpeg.FullName } else { (Get-Command ffmpeg -ErrorAction Stop).Source }
}
else {
    $FfmpegPath = Resolve-CigPath $FfmpegPath
}
$ffprobe = Join-Path (Split-Path -Parent $FfmpegPath) 'ffprobe.exe'
if (-not (Test-Path -LiteralPath $ffprobe -PathType Leaf)) { throw "ffprobe bulunamadı: $ffprobe" }
$probe = & $ffprobe -v error -show_entries stream=codec_name,sample_rate,channels -show_entries format=duration -of json $input
if ($LASTEXITCODE -ne 0) { throw 'ffprobe başarısız.' }
$data = $probe | ConvertFrom-Json
$stream = $data.streams[0]
$loudness = & $FfmpegPath -hide_banner -nostats -i $input -af ebur128=peak=true -f null NUL 2>&1
$peak = ($loudness | Select-String 'Peak:' | Select-Object -Last 1).Line
[ordered]@{
    file = $input
    codec = $stream.codec_name
    sampleRate = [int]$stream.sample_rate
    expectedSampleRate = $SampleRate
    channels = [int]$stream.channels
    expectedChannels = $Channels
    durationSeconds = [double]$data.format.duration
    peakReport = $peak
    pass = ([int]$stream.sample_rate -eq $SampleRate -and [int]$stream.channels -eq $Channels)
} | ConvertTo-Json
