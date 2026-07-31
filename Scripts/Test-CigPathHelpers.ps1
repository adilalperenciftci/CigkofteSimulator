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
}
finally {
    Set-Location $original
}

if ($script:failures -gt 0) {
    Write-Host ("$($script:failures)/$($script:total) iddia basarisiz.") -ForegroundColor Red
    exit 1
}
Write-Host ("$($script:total) iddia gecti.") -ForegroundColor Green
exit 0
