<#
.SYNOPSIS
Engine-free regression tests for release-script path resolution and containment.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$script:failures = 0
$script:total = 0
function Assert-CigEqual {
    param([string]$What, $Expected, $Actual)
    $script:total++
    if ($Expected -eq $Actual) {
        Write-Host ("  PASS  {0}" -f $What) -ForegroundColor Green
    }
    else {
        Write-Host ("  FAIL  {0}: beklenen '{1}', gelen '{2}'" -f $What, $Expected, $Actual) -ForegroundColor Red
        $script:failures++
    }
}

$original = Get-Location
$base = Join-Path ([IO.Path]::GetTempPath()) 'Cig Path Fixture With Spaces'
try {
    Set-Location ([IO.Path]::GetTempPath())
    Assert-CigEqual 'goreli yol PowerShell konumuna gore cozulur' `
        ([IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) 'relative\file.txt'))) `
        (Resolve-CigPath 'relative\file.txt')

    $absolute = [IO.Path]::GetFullPath((Join-Path $base 'absolute.txt'))
    Assert-CigEqual 'mutlak yol degismez' $absolute (Resolve-CigPath $absolute)

    $inside = Join-Path $base 'Downloads\nested\asset.zip'
    $root = Join-Path $base 'Downloads'
    $sibling = Join-Path $base 'Downloads-Evil\asset.zip'
    Assert-CigEqual 'alt yol kabul edilir' $true `
        (Test-CigPathWithinDirectory -Path $inside -Directory $root)
    Assert-CigEqual 'kok varsayilan olarak dosya sayilmaz' $false `
        (Test-CigPathWithinDirectory -Path $root -Directory $root)
    Assert-CigEqual 'kok acik izinle kabul edilir' $true `
        (Test-CigPathWithinDirectory -Path $root -Directory $root -AllowDirectoryItself)
    Assert-CigEqual 'kardes-prefix yolu reddedilir' $false `
        (Test-CigPathWithinDirectory -Path $sibling -Directory $root)
    Assert-CigEqual 'nokta-nokta ile disari cikis reddedilir' $false `
        (Test-CigPathWithinDirectory -Path (Join-Path $root '..\outside.txt') -Directory $root)

    # The package output directory is deleted before each build, so the rule that
    # decides what may be deleted is worth more than a comment. Nothing here is
    # removed: only the predicate is exercised.
    $repo = Join-Path $base 'Repo'
    $null = New-Item -ItemType Directory -Path $repo -Force

    $missing = Join-Path $repo 'Build\NeverBuilt'
    Assert-CigEqual 'var olmayan cikti klasoru silinebilir sayilir' $true `
        (Test-CigPackageOutputDirectory -Path $missing -RepositoryRoot $repo)

    $empty = Join-Path $repo 'Build\Empty'
    $null = New-Item -ItemType Directory -Path $empty -Force
    Assert-CigEqual 'bos cikti klasoru silinebilir' $true `
        (Test-CigPackageOutputDirectory -Path $empty -RepositoryRoot $repo)

    $staged = Join-Path $repo 'Build\Staged'
    $null = New-Item -ItemType Directory -Path $staged -Force
    $null = New-Item -ItemType File -Path (Join-Path $staged 'Manifest_UFSFiles_Win64.txt') -Force
    Assert-CigEqual 'staging manifesti tasiyan klasor paket ciktisi sayilir' $true `
        (Test-CigPackageOutputDirectory -Path $staged -RepositoryRoot $repo)

    # Someone's documents folder passed as -OutputDirectory by mistake.
    $foreign = Join-Path $repo 'Build\Foreign'
    $null = New-Item -ItemType Directory -Path $foreign -Force
    $null = New-Item -ItemType File -Path (Join-Path $foreign 'notes.txt') -Force
    Assert-CigEqual 'paket gibi gorunmeyen dolu klasor silinmez' $false `
        (Test-CigPackageOutputDirectory -Path $foreign -RepositoryRoot $repo)

    foreach ($case in @(
        @{ What = 'depo kokunun kendisi reddedilir'; Path = $repo }
        @{ What = 'depoyu iceren ust klasor reddedilir'; Path = $base }
    )) {
        $threw = $false
        try { $null = Test-CigPackageOutputDirectory -Path $case.Path -RepositoryRoot $repo }
        catch { $threw = $true }
        Assert-CigEqual $case.What $true $threw
    }
}
finally {
    Set-Location $original
    if (Test-Path -LiteralPath $base) {
        Remove-Item -LiteralPath $base -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($script:failures -gt 0) {
    Write-Host ("$($script:failures)/$($script:total) iddia basarisiz.") -ForegroundColor Red
    exit 1
}
Write-Host ("$($script:total) iddia gecti.") -ForegroundColor Green
exit 0
