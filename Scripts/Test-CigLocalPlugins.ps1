<#
.SYNOPSIS
Engine-free tests for the local-plugin release guard, plus the live check against
this checkout.

.DESCRIPTION
A plugin installed into Plugins/ is enabled without anyone writing it down. The
engine decides an unspecified EnabledByDefault by asking where the plugin was
loaded from, and for a project plugin the answer is "enabled"; UnrealBuildTool
synthesises the same reference. Sentry 1.17 arrived that way and put
crashpad_handler.exe, crashpad_wer.dll and a 65 MB Sentry.CrashReporter.exe into
packages built from a repository that does not contain the plugin at all.

Nothing failed. That is the problem: the package was simply not the package a
clean clone produces, and the only visible sign was a file list nobody diffed.

The fixture cases below carry the coverage, because the interesting states -
an unknown local plugin, a policy that forbids runtime inclusion, an editor-only
allow list - are states this machine does not have and must not need.
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

function New-TestPlugin {
    param(
        [string]$Name,
        [bool]$Tracked = $false,
        [string]$ModuleType = 'Runtime',
        [object]$EnabledByDefault = $null,
        [string[]]$Platforms = @('Win64')
    )

    $module = [ordered]@{ Name = $Name; Type = $ModuleType; LoadingPhase = 'Default' }
    if ($Platforms.Count -gt 0) { $module['PlatformAllowList'] = $Platforms }

    $descriptor = [ordered]@{ Modules = @([pscustomobject]$module) }
    if ($null -ne $EnabledByDefault) { $descriptor['EnabledByDefault'] = $EnabledByDefault }

    [pscustomobject]@{
        Name         = $Name
        RelativePath = "Plugins/$Name/$Name.uplugin"
        Tracked      = $Tracked
        Descriptor   = [pscustomobject]$descriptor
    }
}

function New-TestProject {
    param([object[]]$Plugins = @())
    [pscustomobject]@{ Plugins = @($Plugins) }
}

function New-TestPolicy {
    param([object[]]$Entries = @())
    [pscustomobject]@{ localPlugins = @($Entries) }
}

$forbidRuntime = [pscustomobject]@{
    name = 'Telemetry'; reason = 'test'; approvedTargets = @()
    cookMayLoad = $false; runtimeOutputMayInclude = $false
}

# ---------------------------------------------------------------- fixture cases

# The Sentry case exactly: untracked, no project reference, a Win64 Runtime module.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry') `
    -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy)
Assert-CigEqual 'kayitsiz yerel eklenti varsayilan olarak etkin sayilmali' $true $finding[0].Enabled
Assert-CigEqual 'kayitsiz yerel eklenti calisma zamanina ulasir' $true $finding[0].ReachesRuntime
Assert-CigEqual 'kayitsiz yerel eklenti sorun uretmeli' $true ($null -ne $finding[0].Problem)
Assert-CigEqual 'kayitsiz yerel eklenti local-unknown sayilmali' 'local-unknown' $finding[0].Classification

# The fix. An explicit disabled reference is what stops both the engine and UBT.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry') `
    -ProjectDescriptor (New-TestProject -Plugins @([pscustomobject]@{ Name = 'Telemetry'; Enabled = $false })) `
    -Policy (New-TestPolicy -Entries @($forbidRuntime))
Assert-CigEqual 'acikca kapatilan yerel eklenti etkin olmamali' $false $finding[0].Enabled
Assert-CigEqual 'acikca kapatilan yerel eklenti calisma zamanina ulasmaz' $false $finding[0].ReachesRuntime
Assert-CigEqual 'acikca kapatilan yerel eklenti sorun uretmemeli' $true ($null -eq $finding[0].Problem)

# Declaring it in the policy is not by itself permission to ship it.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry') `
    -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy -Entries @($forbidRuntime))
Assert-CigEqual 'politikada olmak etkin birakmayi mesrulastirmaz' $true ($null -ne $finding[0].Problem)
Assert-CigEqual 'politikada kayitli eklenti local-declared sayilmali' 'local-declared' $finding[0].Classification

# An editor-only allow list keeps a plugin out of the game without disabling it,
# which is how ModelContextProtocol and the editor toolsets are handled.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry') `
    -ProjectDescriptor (New-TestProject -Plugins @(
        [pscustomobject]@{ Name = 'Telemetry'; Enabled = $true; TargetAllowList = @('Editor') })) `
    -Policy (New-TestPolicy -Entries @([pscustomobject]@{
        name = 'Telemetry'; reason = 'test'; approvedTargets = @('Editor')
        cookMayLoad = $true; runtimeOutputMayInclude = $false }))
Assert-CigEqual 'Editor kisitli eklenti calisma zamanina ulasmaz' $false $finding[0].ReachesRuntime
Assert-CigEqual 'Editor kisitli eklenti sorun uretmemeli' $true ($null -eq $finding[0].Problem)

# Same allow list, but the policy approved no target at all: still a problem,
# because the plugin is loaded by the editor a clean clone does not have.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry') `
    -ProjectDescriptor (New-TestProject -Plugins @(
        [pscustomobject]@{ Name = 'Telemetry'; Enabled = $true; TargetAllowList = @('Editor') })) `
    -Policy (New-TestPolicy -Entries @($forbidRuntime))
Assert-CigEqual 'hicbir hedefe onaylanmayan eklenti etkin kalmamali' $true ($null -ne $finding[0].Problem)

# Game in the allow list is not a restriction away from the game.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry') `
    -ProjectDescriptor (New-TestProject -Plugins @(
        [pscustomobject]@{ Name = 'Telemetry'; Enabled = $true; TargetAllowList = @('Game') })) `
    -Policy (New-TestPolicy -Entries @($forbidRuntime))
Assert-CigEqual 'Game hedefine izinli eklenti calisma zamanina ulasir' $true $finding[0].ReachesRuntime

# EnabledByDefault: false in the descriptor is the plugin author's own opt-out.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry' -EnabledByDefault $false) `
    -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy -Entries @($forbidRuntime))
Assert-CigEqual 'EnabledByDefault false eklenti etkin olmamali' $false $finding[0].Enabled

# An editor-only module ships nothing, whatever the plugin's enabled state.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry' -ModuleType 'Editor') `
    -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy)
Assert-CigEqual 'yalniz Editor modullu eklenti calisma zamanina ulasmaz' $false $finding[0].ReachesRuntime

# DeveloperTool is stripped from Shipping but not from Development, and a
# Development package is something people are handed.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry' -ModuleType 'DeveloperTool') `
    -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy)
Assert-CigEqual 'DeveloperTool modulu calisma zamani ciktisi sayilmali' $true $finding[0].ReachesRuntime

# A module for other platforms only cannot reach a Win64 package.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry' -Platforms @('Android', 'IOS')) `
    -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy)
Assert-CigEqual 'Win64 disi platform modulu calisma zamanina ulasmaz' $false $finding[0].ReachesRuntime

# Sentry's descriptor uses the old spelling, so it has to be understood too.
$legacy = New-TestPlugin -Name 'Telemetry' -Platforms @()
$legacy.Descriptor.Modules[0] | Add-Member -NotePropertyName 'WhitelistPlatforms' -NotePropertyValue @('Win64')
$finding = Get-CigLocalPluginFindings -Inventory @($legacy) `
    -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy)
Assert-CigEqual 'eski WhitelistPlatforms yazimi da okunmali' $true $finding[0].ReachesRuntime

# The clean-clone case, which is also CI's. Discovery finds nothing, PowerShell
# returns an empty array from a function as $null, and a Mandatory [object[]]
# refuses to bind it - so the one checkout this guard exists to describe was the
# one it could not run on. Both spellings of "nothing" are pinned here, and so is
# discovery itself, because the null arrives from there rather than from a caller.
$emptyRoot = Join-Path ([IO.Path]::GetTempPath()) 'CigCleanCloneFixture'
if (Test-Path -LiteralPath $emptyRoot) { Remove-Item -LiteralPath $emptyRoot -Recurse -Force }
try {
    $null = New-Item -ItemType Directory -Path $emptyRoot -Force
    Assert-CigEqual 'Plugins klasoru olmayan checkout bos envanter verir' 0 `
        (@(Get-CigProjectPluginInventory -RepositoryRoot $emptyRoot)).Count

    $null = New-Item -ItemType Directory -Path (Join-Path $emptyRoot 'Plugins') -Force
    $null = New-Item -ItemType File -Path (Join-Path $emptyRoot 'Plugins/README.md') -Force
    Assert-CigEqual 'uplugin icermeyen Plugins klasoru bos envanter verir' 0 `
        (@(Get-CigProjectPluginInventory -RepositoryRoot $emptyRoot)).Count
}
finally {
    if (Test-Path -LiteralPath $emptyRoot) {
        Remove-Item -LiteralPath $emptyRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

foreach ($case in @(
    @{ What = 'bos dizi envanteri'; Inventory = @() }
    @{ What = 'null envanter'; Inventory = $null }
)) {
    $finding = @(Get-CigLocalPluginFindings -Inventory $case.Inventory `
        -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy))
    Assert-CigEqual "$($case.What) bulgu uretmemeli" 0 $finding.Count
}

# A tracked plugin is in every clone and is not this check's business.
$finding = Get-CigLocalPluginFindings `
    -Inventory @(New-TestPlugin -Name 'Telemetry' -Tracked $true) `
    -ProjectDescriptor (New-TestProject) -Policy (New-TestPolicy)
Assert-CigEqual 'depoda kayitli eklenti sorun uretmemeli' $true ($null -eq $finding[0].Problem)
Assert-CigEqual 'depoda kayitli eklenti tracked sayilmali' 'tracked' $finding[0].Classification

# ------------------------------------------------------------------- live check

$root = Split-Path -Parent $PSScriptRoot
$policyPath = Join-Path $root 'Tools\LocalPluginPolicy.json'
Assert-CigEqual 'yerel eklenti politikasi dosyasi bulunmali' $true (Test-Path -LiteralPath $policyPath)

$policy = Get-Content -LiteralPath $policyPath -Raw -Encoding UTF8 | ConvertFrom-Json

# No absolute path, user name or drive letter may appear: this file is read on
# machines that have none of them.
$policyText = Get-Content -LiteralPath $policyPath -Raw
Assert-CigEqual 'politika mutlak yol icermemeli' $false ($policyText -match '[A-Za-z]:[\\/]')
Assert-CigEqual 'politika kullanici adi icermemeli' $false ($policyText -match '(?i)users[\\/]')

$project = Get-Content -LiteralPath (Join-Path $root 'CigkofteSimulator.uproject') -Raw | ConvertFrom-Json
$inventory = @(Get-CigProjectPluginInventory -RepositoryRoot $root)
$findings = @(Get-CigLocalPluginFindings -Inventory $inventory -ProjectDescriptor $project -Policy $policy)

foreach ($f in $findings) {
    $state = if ($f.ReachesRuntime) { 'calisma zamanina girer' } else { 'calisma zamani disi' }
    Write-Host ("  {0,-24} {1,-16} referans={2,-8} {3}" -f $f.Name, $f.Classification, $f.Reference, $state)
}

$problems = @($findings | Where-Object { $_.Problem })
foreach ($p in $problems) { Write-Host ("  SORUN  {0}" -f $p.Problem) -ForegroundColor Red }
Assert-CigEqual 'bu checkout onaysiz yerel eklenti tasimamali' 0 $problems.Count

# Whatever else changes, the one plugin known to have contaminated a package must
# stay disabled, and it must stay disabled in the .uproject rather than by luck.
$sentry = $project.Plugins | Where-Object { $_.Name -eq 'Sentry' }
Assert-CigEqual 'Sentry .uproject icinde acikca kayitli olmali' $true ($null -ne $sentry)
if ($sentry) { Assert-CigEqual 'Sentry kapali olmali' $false ([bool]$sentry.Enabled) }

# On a machine that has the plugin, check the real descriptor rather than only the
# fixtures: if Sentry's own modules stopped being runtime modules this guard would
# start passing for the wrong reason, and the next such plugin would walk through.
$sentryFinding = $findings | Where-Object { $_.Name -eq 'Sentry' }
if ($sentryFinding) {
    Assert-CigEqual 'Sentry tanimi gercekten calisma zamani modulu tasiyor' $true $sentryFinding.HasRuntimeModule
    Assert-CigEqual 'Sentry calisma zamanina ulasmiyor' $false $sentryFinding.ReachesRuntime
}
else {
    Write-Host '  ATLA  Sentry bu checkout''ta kurulu degil (temiz klon); fixture testleri gecerli.' -ForegroundColor Yellow
}

if ($script:failures -gt 0) {
    Write-Host ("$($script:failures)/$($script:total) iddia basarisiz.") -ForegroundColor Red
    exit 1
}
Write-Host ("$($script:total) iddia gecti.") -ForegroundColor Green
exit 0
