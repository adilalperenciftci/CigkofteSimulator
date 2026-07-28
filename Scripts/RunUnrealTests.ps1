<#
.SYNOPSIS
UE 5.8 Automation testlerini hedefli bir filtreyle commandlet modunda çalıştırır.
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [ValidateNotNullOrEmpty()]
    [string]$TestFilter = 'Cigkofte',
    [ValidateRange(10, 7200)]
    [int]$TimeoutSeconds = 900,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Find-EngineRoot {
    param([string]$Override)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($Override)) {
        $candidates += $Override
    }
    if (-not [string]::IsNullOrWhiteSpace($env:CIG_UE_ROOT)) {
        $candidates += $env:CIG_UE_ROOT
    }
    $candidates += 'C:\Program Files\Epic Games\UE_5.8'
    foreach ($root in @('C:\Program Files\Epic Games', 'D:\Epic Games', 'D:\Program Files\Epic Games')) {
        if (Test-Path -LiteralPath $root -PathType Container) {
            $candidates += @(Get-ChildItem -LiteralPath $root -Directory -Filter 'UE_5.8*' -ErrorAction SilentlyContinue |
                ForEach-Object FullName)
        }
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        $editorCmd = Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
        if (Test-Path -LiteralPath $editorCmd -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw 'UE 5.8 UnrealEditor-Cmd.exe bulunamadı.'
}

try {
    $projectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
    $projects = @(Get-ChildItem -LiteralPath $projectRoot -Filter '*.uproject' -File)
    if ($projects.Count -ne 1) {
        throw "Proje kökünde tam olarak bir .uproject bekleniyordu; bulunan: $($projects.Count)."
    }

    $engine = Find-EngineRoot -Override $EngineRoot
    $editorCmd = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $logDirectory = Join-Path $projectRoot 'Logs'
    $logPath = Join-Path $logDirectory 'RunUnrealTests-latest.log'
    $stdoutPath = Join-Path $logDirectory 'RunUnrealTests-stdout.log'
    $stderrPath = Join-Path $logDirectory 'RunUnrealTests-stderr.log'
    $arguments = @(
        "`"$($projects[0].FullName)`"",
        "-ExecCmds=`"Automation RunTests $TestFilter;Quit`"",
        '-unattended',
        '-nullrhi',
        '-nop4',
        '-nosplash',
        '-NoSound',
        '-stdout',
        '-FullStdOutLogOutput',
        "-AbsLog=`"$logPath`""
    )

    if ($DryRun) {
        Write-Output "Dry-run başarılı: `"$editorCmd`" $($arguments -join ' ') (timeout=$TimeoutSeconds saniye)."
        exit 0
    }

    $null = New-Item -ItemType Directory -Path $logDirectory -Force
    $process = Start-Process -FilePath $editorCmd -ArgumentList $arguments `
        -WorkingDirectory $projectRoot -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath -PassThru

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        [Console]::Error.WriteLine("Automation testi $TimeoutSeconds saniyede tamamlanmadı; yalnız bu scriptin başlattığı PID $($process.Id) durduruldu.")
        exit 124
    }

    $process.Refresh()
    $exitCode = [int]$process.ExitCode
    if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        throw "Unreal test logu oluşmadı: $logPath"
    }

    $failed = @(Select-String -LiteralPath $logPath -Pattern 'Result=\{Fail|Automation Test Failed|Test Completed\. Result=\{Fail')
    $passed = @(Select-String -LiteralPath $logPath -Pattern 'Result=\{Success\}|Test Completed\. Result=\{Success\}')
    if ($failed.Count -gt 0) {
        $failed | Select-Object -First 10 | ForEach-Object {
            [Console]::Error.WriteLine("$($_.Path):$($_.LineNumber): $($_.Line.Trim())")
        }
        exit 2
    }
    if ($exitCode -ne 0) {
        [Console]::Error.WriteLine("UnrealEditor-Cmd exit code $exitCode döndürdü. Log: $logPath")
        exit $exitCode
    }
    if ($passed.Count -eq 0) {
        [Console]::Error.WriteLine("Filtre için tamamlanmış başarılı test bulunamadı: '$TestFilter'. Log: $logPath")
        exit 3
    }

    Write-Output "Automation testleri başarılı: filtre='$TestFilter'; başarılı kayıt=$($passed.Count); Log=$logPath"
    exit 0
}
catch {
    Write-Error "RunUnrealTests başarısız: $($_.Exception.Message)"
    exit 1
}
