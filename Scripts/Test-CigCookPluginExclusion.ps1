<#
.SYNOPSIS
Engine-free regression tests for the cook-time editor-plugin exclusion.

.DESCRIPTION
This argument has been got wrong twice, both times silently, and both failures
looked identical from outside: packaging still failed with the same cook error.

The first attempt joined the plugin names with "+". The engine splits
DisablePlugins= on "," and found nothing named by the "+"-joined string, so it
disabled nothing and said nothing.

The second attempt was written into Package-Windows.ps1 while the run that
mattered went through PackageDemo.ps1, which carries its own UAT argument list.
The edit was never executed.

Neither failure is visible without either reading engine source or diffing UAT
logs, so both are pinned here. These tests never start Unreal.
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

$arg = Get-CigCookPluginExclusionArg
$plugins = Get-CigCookDisabledPlugins

Assert-CigEqual 'UAT parametresi additionalcookeroptions olmali' $true `
    ($arg.StartsWith('-additionalcookeroptions='))

Assert-CigEqual 'cook argumani DisablePlugins tasimali' $true `
    ($arg.Contains('-DisablePlugins='))

# The whole point of the first failure. Checked as a property of the joined
# argument rather than against one hard-coded pair, so adding or removing a
# toolset does not require editing the assertion that guards the separator.
Assert-CigEqual 'eklenti adlari virgulle ayrilmali' $true `
    ($arg.Contains(($plugins -join ',')))
Assert-CigEqual 'arti isareti ayirici olarak kullanilmamali' $false `
    ($arg.Contains('+'))

foreach ($tool in @('EditorToolset', 'SlateInspectorToolset', 'ModelContextProtocol')) {
    Assert-CigEqual "$tool cook disi birakilmali" $true ($plugins -contains $tool)
}

# The umbrella must not come back. It enables all 21 engine toolsets, one of
# which (GameFeaturesToolset) depends on GameFeatures - the dependency that made
# the editor ask to add a GameFeatureData asset-manager rule, which breaks the
# cook. Naming the two toolsets this project uses keeps that out of the graph.
Assert-CigEqual 'AllToolsets semsiyesi .uproject icinde olmamali' $false `
    (((Get-Content -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) 'CigkofteSimulator.uproject') -Raw `
        | ConvertFrom-Json).Plugins | Where-Object { $_.Name -eq 'AllToolsets' }) -ne $null)
Assert-CigEqual 'GameFeaturesToolset acik olmamali' $false `
    (((Get-Content -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) 'CigkofteSimulator.uproject') -Raw `
        | ConvertFrom-Json).Plugins | Where-Object { $_.Name -eq 'GameFeaturesToolset' }) -ne $null)

# A gameplay plugin appearing here would silently ship a broken game rather than
# a failed build, which is the worse direction.
foreach ($gameplay in @('EnhancedInput', 'PythonScriptPlugin')) {
    Assert-CigEqual "oynanis eklentisi $gameplay disarida birakilmamali" $false `
        ($plugins -contains $gameplay)
}

# Every authoritative package entry point must carry the exclusion. The second
# failure was one script having it and the other not.
$root = Split-Path -Parent $PSScriptRoot
foreach ($entryPoint in @('PackageDemo.ps1', 'Package-Windows.ps1')) {
    $text = Get-Content -LiteralPath (Join-Path $root "Scripts\$entryPoint") -Raw
    Assert-CigEqual "$entryPoint cook dislama argumanini kullanmali" $true `
        ($text.Contains('Get-CigCookPluginExclusionArg'))
    # Duplicated literals are how the two lists drifted apart in the first place.
    Assert-CigEqual "$entryPoint argumani elle tekrar yazmamali" $false `
        ($text.Contains('-DisablePlugins='))
}

# The committed descriptor must keep the tooling enabled for editor work. This is
# the user's explicit decision, and a fix that quietly disabled it in the
# .uproject would pass every packaging test while removing the editor's MCP.
$descriptor = Get-Content -LiteralPath (Join-Path $root 'CigkofteSimulator.uproject') -Raw | ConvertFrom-Json
foreach ($required in @('ModelContextProtocol', 'EditorToolset', 'SlateInspectorToolset')) {
    $entry = $descriptor.Plugins | Where-Object { $_.Name -eq $required }
    Assert-CigEqual "$required .uproject icinde bulunmali" $true ($null -ne $entry)
    if ($entry) {
        Assert-CigEqual "$required editor icin acik kalmali" $true ([bool]$entry.Enabled)
        Assert-CigEqual "$required yalnizca Editor hedefine kisitli olmali" 'Editor' ($entry.TargetAllowList -join ',')
    }
}

if ($script:failures -gt 0) {
    Write-Host ("$($script:failures)/$($script:total) iddia basarisiz.") -ForegroundColor Red
    exit 1
}
Write-Host ("$($script:total) iddia gecti.") -ForegroundColor Green
exit 0
