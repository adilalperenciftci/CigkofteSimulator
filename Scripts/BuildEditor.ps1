<#
.SYNOPSIS
UE 5.8 Editor target'ını Win64 Development olarak derler.
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [ValidateSet('Development', 'DebugGame', 'Shipping')]
    [string]$Configuration = 'Development',
    [switch]$Clean,
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
        $buildBat = Join-Path $candidate 'Engine\Build\BatchFiles\Build.bat'
        if (Test-Path -LiteralPath $buildBat -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw 'UE 5.8 EngineRoot veya resmi Engine\Build\BatchFiles\Build.bat bulunamadı.'
}

function Find-EditorTarget {
    param([Parameter(Mandatory)][string]$SourceRoot)

    $targets = @()
    foreach ($file in @(Get-ChildItem -LiteralPath $SourceRoot -Filter '*.Target.cs' -File -Recurse)) {
        $content = Get-Content -Raw -LiteralPath $file.FullName
        if ($content -notmatch 'Type\s*=\s*TargetType\.Editor\s*;') {
            continue
        }
        $match = [regex]::Match($content, 'public\s+class\s+(?<Name>[A-Za-z_][A-Za-z0-9_]*)\s*:\s*TargetRules')
        if (-not $match.Success) {
            throw "Editor target sınıfı okunamadı: $($file.FullName)"
        }
        $className = $match.Groups['Name'].Value
        $targets += if ($className.EndsWith('Target', [StringComparison]::Ordinal)) {
            $className.Substring(0, $className.Length - 6)
        }
        else {
            $className
        }
    }

    $targets = @($targets | Select-Object -Unique)
    if ($targets.Count -ne 1) {
        throw "Tam olarak bir Editor target bekleniyordu; bulunan: $($targets -join ', ')."
    }
    return $targets[0]
}

function Write-FailureSummary {
    param([Parameter(Mandatory)][string]$LogPath)

    $firstError = Select-String -LiteralPath $LogPath -Pattern `
        'fatal error|error C[0-9]+|error LNK[0-9]+|UnrealBuildTool.*Error|BUILD FAILED|Exception:' |
        Select-Object -First 1
    if ($null -ne $firstError) {
        [Console]::Error.WriteLine("İlk gerçek hata: $($firstError.Path):$($firstError.LineNumber): $($firstError.Line.Trim())")
    }
    [Console]::Error.WriteLine('Logun son 30 satırı:')
    Get-Content -LiteralPath $LogPath -Tail 30 | ForEach-Object {
        [Console]::Error.WriteLine($_)
    }
}

function Invoke-BuildBat {
    param(
        [Parameter(Mandatory)][string]$BuildBat,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$LogPath
    )

    & $BuildBat @Arguments 2>&1 | Tee-Object -FilePath $LogPath -Append | Out-Null
    if ($null -eq $LASTEXITCODE) {
        return 1
    }
    return [int]$LASTEXITCODE
}

# Ek doğrulama; başarı kriteri değil. Başarının tek kaynağı Build.bat'ın gerçek
# çıkış kodudur: UBT çıktısını değiştirebilir, "Target is up to date" yolunda
# farklı satırlar basar ve -clean çalışması "Result: Succeeded" hiç yazmayabilir.
# Bu betiğin daha önceki bir sürümü metin aramasını başarı koşulu yapmış ve her
# derlemeyi başarısız raporlamıştı; o hatayı tekrar etmemek için burada metin
# yalnız iki iş yapar:
#   * çıkış kodu 0 iken logda açık bir başarısızlık damgası varsa hata verir
#     (çöken bir araç zinciri sıfırla dönebilir),
#   * başarı damgası yoksa yalnız uyarır, çıkış kodunu asla ezmez.
function Test-BuildLog {
    param([Parameter(Mandatory)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        Write-Warning "Build logu bulunamadı: $LogPath. Sonuç yalnız çıkış koduna dayanıyor."
        return $true
    }

    $failureStamp = @(Select-String -LiteralPath $LogPath -Pattern 'Result:\s*Failed|BUILD FAILED' |
        Select-Object -First 1)
    if ($failureStamp.Count -gt 0) {
        [Console]::Error.WriteLine(
            "Çıkış kodu 0 ama log başarısızlık bildiriyor: $($failureStamp[0].Line.Trim())")
        return $false
    }

    $successStamp = @(Select-String -LiteralPath $LogPath -Pattern 'Result:\s*Succeeded' |
        Select-Object -First 1)
    if ($successStamp.Count -eq 0) {
        Write-Warning ("Logda 'Result: Succeeded' yok; çıkış kodu 0 olduğu için " +
            "derleme başarılı sayıldı. Log: $LogPath")
    }
    return $true
}

try {
    $projectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
    $projects = @(Get-ChildItem -LiteralPath $projectRoot -Filter '*.uproject' -File)
    if ($projects.Count -ne 1) {
        throw "Proje kökünde tam olarak bir .uproject bekleniyordu; bulunan: $($projects.Count)."
    }

    $engine = Find-EngineRoot -Override $EngineRoot
    $buildBat = Join-Path $engine 'Engine\Build\BatchFiles\Build.bat'
    $target = Find-EditorTarget -SourceRoot (Join-Path $projectRoot 'Source')
    $baseArguments = @(
        $target,
        'Win64',
        $Configuration,
        "-Project=$($projects[0].FullName)",
        '-WaitMutex',
        '-NoHotReloadFromIDE'
    )

    if ($DryRun) {
        $cleanText = if ($Clean) { ' Önce aynı komut -clean ile çalıştırılacak.' } else { '' }
        Write-Output "Dry-run başarılı: `"$buildBat`" $($baseArguments -join ' ').$cleanText"
        exit 0
    }

    $logDirectory = Join-Path $projectRoot 'Logs'
    $null = New-Item -ItemType Directory -Path $logDirectory -Force
    $logPath = Join-Path $logDirectory 'BuildEditor-latest.log'
    "Build başlangıcı: $(Get-Date -Format o)" | Set-Content -LiteralPath $logPath -Encoding utf8

    if ($Clean) {
        $cleanExitCode = Invoke-BuildBat -BuildBat $buildBat -Arguments ($baseArguments + '-clean') -LogPath $logPath
        if ($cleanExitCode -ne 0) {
            Write-FailureSummary -LogPath $logPath
            [Console]::Error.WriteLine("Editor clean başarısız (exit code $cleanExitCode). Log: $logPath")
            exit $cleanExitCode
        }
    }

    $buildExitCode = Invoke-BuildBat -BuildBat $buildBat -Arguments $baseArguments -LogPath $logPath
    if ($buildExitCode -ne 0) {
        Write-FailureSummary -LogPath $logPath
        [Console]::Error.WriteLine("Editor derlemesi başarısız (exit code $buildExitCode). Log: $logPath")
        exit $buildExitCode
    }

    if (-not (Test-BuildLog -LogPath $logPath)) {
        Write-FailureSummary -LogPath $logPath
        [Console]::Error.WriteLine("Editor derlemesi başarısız (log damgası). Log: $logPath")
        exit 1
    }

    Write-Output "Editor derlemesi başarılı. Target=$target; Log=$logPath"
    exit 0
}
catch {
    Write-Error "BuildEditor başarısız: $($_.Exception.Message)"
    exit 1
}
