[CmdletBinding()]
param(
    [switch]$Cpu,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$comfy = Join-Path $root 'Tools\ComfyUI'
$python = Join-Path $comfy '.venv\Scripts\python.exe'
$main = Join-Path $comfy 'main.py'
$output = Join-Path $root 'AssetWork\Generated'
if (-not (Test-Path -LiteralPath $python -PathType Leaf) -or -not (Test-Path -LiteralPath $main -PathType Leaf)) {
    throw 'ComfyUI runtime kurulumu bulunamadı.'
}
$args = @($main, '--listen', '127.0.0.1', '--port', '8188', '--output-directory', $output, '--disable-auto-launch')
if ($Cpu) { $args += '--cpu' } else { $args += '--lowvram' }
if ($DryRun) {
    Write-Output "Dry-run başarılı: '$python' $($args -join ' ')"
    exit 0
}
Start-Process -FilePath $python -ArgumentList $args -WorkingDirectory $comfy -WindowStyle Hidden
Write-Output 'ComfyUI yalnız 127.0.0.1:8188 üzerinde başlatıldı.'
