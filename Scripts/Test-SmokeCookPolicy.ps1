<#
.SYNOPSIS
Fixture coverage for mandatory repository assets and absent optional packs.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CigCommon.ps1')

$passed = 0
$failed = 0

function Assert-CigCookPolicy {
    param([string]$Name, [bool]$Actual, [bool]$Expected)
    if ($Actual -eq $Expected) {
        Write-Host "  PASS  $Name" -ForegroundColor Green
        $script:passed++
    }
    else {
        Write-Host "  FAIL  $Name - expected=$Expected actual=$Actual" -ForegroundColor Red
        $script:failed++
    }
}

$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ("CigSmokeCookPolicy-{0}-{1}" -f $PID, [guid]::NewGuid())
try {
    $null = New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'Content\OptionalPresent\Nested') -Force
    $null = New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'Content\OptionalEmpty') -Force
    $null = New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'Content\OptionalAbsent-Evil') -Force
    $null = New-Item -ItemType File -Path (Join-Path $fixtureRoot 'Content\OptionalPresent\Nested\Mesh.uasset') -Force
    $null = New-Item -ItemType File -Path (Join-Path $fixtureRoot 'Content\OptionalAbsent-Evil\Mesh.uasset') -Force

    Assert-CigCookPolicy 'Audio kaynak olmasa da zorunlu' `
        (Test-CigCookDirectoryMustProduce '/Game/Audio' $fixtureRoot) $true
    Assert-CigCookPolicy 'LowPoly kaynak olmasa da zorunlu' `
        (Test-CigCookDirectoryMustProduce '/Game/LowPoly' $fixtureRoot) $true
    Assert-CigCookPolicy 'kaynakta olmayan opsiyonel paket atlanir' `
        (Test-CigCookDirectoryMustProduce '/Game/OptionalAbsent' $fixtureRoot) $false
    Assert-CigCookPolicy 'uasset bulunan opsiyonel paket zorunlu olur' `
        (Test-CigCookDirectoryMustProduce '/Game/OptionalPresent' $fixtureRoot) $true
    Assert-CigCookPolicy 'bos opsiyonel klasor atlanir' `
        (Test-CigCookDirectoryMustProduce '/Game/OptionalEmpty' $fixtureRoot) $false
    Assert-CigCookPolicy 'kardes-prefix klasor varligi sayilmaz' `
        (Test-CigCookDirectoryMustProduce '/Game/OptionalAbsent' $fixtureRoot) $false

    $threw = $false
    try { $null = Test-CigCookDirectoryMustProduce '/Engine/NotGame' $fixtureRoot }
    catch { $threw = $true }
    Assert-CigCookPolicy '/Game disindaki girdi reddedilir' $threw $true
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}

Write-Host ''
Write-Host "$passed iddia gecti."
if ($failed -gt 0) {
    Write-Error "$failed cook politikasi iddiasi basarisiz."
    exit 1
}
exit 0
