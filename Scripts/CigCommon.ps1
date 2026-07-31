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
