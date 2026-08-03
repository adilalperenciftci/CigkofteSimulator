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

# Editor development plugins that must not be loaded by the cook.
#
# ModelContextProtocol and AllToolsets carry "TargetAllowList": ["Editor"], which
# is correct and keeps them out of the shipped game. It does not keep them out of
# the *cook*, because the cook commandlet is itself an editor. AllToolsets
# depends on GameFeaturesToolset, which depends on GameFeatures, which demands an
# asset-manager rule for GameFeatureData; the cook reports its absence as an
# error and UAT fails the run with Error_UnknownCookFailure.
#
# Declaring that rule is the wrong fix, and Config/DefaultGame.ini says why at
# length: the type's class does not load for the Game target, so the asset
# manager ensures on it rather than shrugging. Tools/check_sources.py rejects it
# statically. Both directions fail while the plugin is in the cook, so the plugin
# is taken out of the cook rather than out of the project. The editor keeps its
# tooling; the cook never loads it.
$script:CigCookDisabledPlugins = @('AllToolsets', 'ModelContextProtocol')

function Get-CigCookPluginExclusionArg {
    <#
    .SYNOPSIS
    The -additionalcookeroptions argument that keeps editor tooling out of the cook.

    .DESCRIPTION
    Two details here were each got wrong once, so both are pinned by
    Test-CigCookPluginExclusion.ps1 rather than left to memory.

    The separator is a comma. FPluginManager::FindCommandLinePlugins splits
    DisablePlugins= on "," (Engine/Source/Runtime/Projects/Private/PluginManager.cpp);
    an earlier attempt joined the names with "+" and silently did nothing, because
    "AllToolsets+ModelContextProtocol" is not the name of any plugin and the
    engine simply found nothing to disable.

    A command-line disable can override a plugin the .uproject enables only
    because FindCommandLinePlugins runs before FindTargetPlugins - it claims the
    name in ConfiguredPluginNames first, and the project-descriptor pass then
    skips it. Reverse that order and this argument would be a no-op.
    #>
    return "-additionalcookeroptions=-DisablePlugins=$($script:CigCookDisabledPlugins -join ',')"
}

function Get-CigCookDisabledPlugins {
    return $script:CigCookDisabledPlugins
}

# --------------------------------------------------------- local plugin policy
#
# A plugin dropped into Plugins/ needs no entry in the .uproject to be enabled.
# FPlugin::IsEnabledByDefault (Engine/Source/Runtime/Projects/Private/PluginManager.cpp)
# ends with "return GetLoadedFrom() == EPluginLoadedFrom::Project" for a descriptor
# that does not say EnabledByDefault - and a project plugin is loaded from the
# project. UnrealBuildTool reaches the same answer the same way and synthesises a
# reference for it (UEBuildTarget.cs, "synthesize references for plugins which are
# enabled by default").
#
# So an installed-and-forgotten plugin builds, cooks and stages, and a clean clone
# without it produces a different game. That is what this checks: not whether a
# local plugin exists, but whether it can still reach the packaged runtime.
#
# The fix for one is an explicit disabled reference in the .uproject. Both engine
# and UBT record project references before the enabled-by-default pass runs and
# skip a name that pass has already seen, and a disabled reference is dropped by
# FPluginReferenceDescriptor::IsEnabledForTarget before anything looks the plugin
# up - so it is also safe in a checkout where the plugin is absent.

# Module types that end up in a packaged game. Editor, Program and the various
# uncooked types do not. DeveloperTool is here deliberately: it is stripped from
# Shipping but not from Development, and a Development package is an artefact that
# gets handed to people.
$script:CigRuntimeModuleTypes = @(
    'Runtime', 'RuntimeNoCommandlet', 'RuntimeAndProgram',
    'ClientOnly', 'ClientOnlyNoCommandlet', 'ServerOnly',
    'CookedOnly', 'DeveloperTool'
)

# Target types whose output is shipped to a player.
$script:CigShippedTargetTypes = @('Game', 'Client', 'Server')

function Get-CigJsonField {
    <#
    .SYNOPSIS
    Reads an optional field off a parsed JSON object.

    .DESCRIPTION
    ConvertFrom-Json omits absent fields entirely rather than leaving them null,
    and Set-StrictMode turns reading one into a terminating error. Every field
    this policy cares about is optional in at least one real descriptor, so they
    all go through here.
    #>
    param(
        [Parameter(Mandatory)][AllowNull()][object]$Object,
        [Parameter(Mandatory)][string]$Name,
        [object]$Default = $null
    )

    if ($null -eq $Object) { return $Default }
    $property = $Object.PSObject.Properties[$Name]
    if (-not $property) { return $Default }
    if ($null -eq $property.Value) { return $Default }
    return $property.Value
}

function Test-CigPluginHasRuntimeModule {
    <#
    .SYNOPSIS
    Whether a .uplugin descriptor carries a module that would be staged into a
    packaged Win64 game.
    #>
    param(
        [Parameter(Mandatory)][AllowNull()][object]$Descriptor,
        [string]$Platform = 'Win64'
    )

    if ($null -eq $Descriptor) { return $false }
    foreach ($module in @(Get-CigJsonField -Object $Descriptor -Name 'Modules' -Default @())) {
        if ($null -eq $module) { continue }
        if ($script:CigRuntimeModuleTypes -notcontains (Get-CigJsonField -Object $module -Name 'Type')) { continue }

        # PlatformAllowList is the current spelling; WhitelistPlatforms is the old
        # one and Sentry 1.17 still ships it. Absent means every platform.
        $platforms = @()
        $platforms += @(Get-CigJsonField -Object $module -Name 'PlatformAllowList' -Default @())
        $platforms += @(Get-CigJsonField -Object $module -Name 'WhitelistPlatforms' -Default @())
        $platforms = @($platforms | Where-Object { $_ })
        if ($platforms.Count -gt 0 -and $platforms -notcontains $Platform) { continue }

        return $true
    }
    return $false
}

function Get-CigLocalPluginFindings {
    <#
    .SYNOPSIS
    Classifies every discovered project plugin and reports the ones that break
    build reproducibility.

    .DESCRIPTION
    Pure: it takes an inventory rather than reading the disk, so
    Test-CigLocalPlugins.ps1 can exercise the classification against cases this
    machine does not have.

    .PARAMETER Inventory
    Objects with Name, RelativePath, Tracked and Descriptor (the parsed .uplugin).

    .PARAMETER ProjectDescriptor
    The parsed .uproject.

    .PARAMETER Policy
    The parsed Tools/LocalPluginPolicy.json.
    #>
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Inventory,
        [Parameter(Mandatory)][object]$ProjectDescriptor,
        [Parameter(Mandatory)][object]$Policy
    )

    $approved = @{}
    foreach ($entry in @(Get-CigJsonField -Object $Policy -Name 'localPlugins' -Default @())) {
        $name = Get-CigJsonField -Object $entry -Name 'name'
        if ($name) { $approved[[string]$name] = $entry }
    }

    $projectReferences = @(Get-CigJsonField -Object $ProjectDescriptor -Name 'Plugins' -Default @())

    $findings = @()
    foreach ($plugin in $Inventory) {
        $reference = $projectReferences |
            Where-Object { $_ -and (Get-CigJsonField -Object $_ -Name 'Name') -eq $plugin.Name } |
            Select-Object -First 1

        if ($reference) {
            $enabled = [bool](Get-CigJsonField -Object $reference -Name 'Enabled' -Default $false)
            $allowList = @(Get-CigJsonField -Object $reference -Name 'TargetAllowList' -Default @() |
                Where-Object { $_ })
            $referenceState = if ($enabled) { 'enabled' } else { 'disabled' }
        }
        else {
            # No reference at all: the descriptor decides, and for a project plugin
            # an unspecified EnabledByDefault means enabled.
            $enabled = [bool](Get-CigJsonField -Object $plugin.Descriptor -Name 'EnabledByDefault' -Default $true)
            $allowList = @()
            $referenceState = 'absent'
        }

        $restrictedAwayFromGame = ($allowList.Count -gt 0) -and
            (-not (@($allowList | Where-Object { $script:CigShippedTargetTypes -contains $_ }).Count -gt 0))
        $hasRuntimeModule = Test-CigPluginHasRuntimeModule -Descriptor $plugin.Descriptor
        $reachesRuntime = $enabled -and (-not $restrictedAwayFromGame) -and $hasRuntimeModule

        $policyEntry = $approved[$plugin.Name]

        if ($plugin.Tracked) {
            $classification = if ($enabled) { 'tracked' } else { 'tracked-disabled' }
        }
        elseif ($policyEntry) {
            $classification = 'local-declared'
        }
        else {
            $classification = 'local-unknown'
        }

        $problem = $null
        if (-not $plugin.Tracked) {
            if (-not $policyEntry) {
                if ($reachesRuntime) {
                    $problem = "$($plugin.RelativePath): bu eklenti depoda yok ama paketlenen oyuna girebiliyor. Temiz bir klon baska bir oyun uretir. Tools/LocalPluginPolicy.json'a kaydet ya da .uproject icinde acikca kapat."
                }
                elseif ($enabled) {
                    $problem = "$($plugin.RelativePath): bu eklenti depoda yok ve yine de etkin. Tools/LocalPluginPolicy.json'da kaydi olmali."
                }
            }
            elseif ((-not (Get-CigJsonField -Object $policyEntry -Name 'runtimeOutputMayInclude' -Default $false)) -and $reachesRuntime) {
                $problem = "$($plugin.RelativePath): politika calisma zamani ciktisina girmesini yasakliyor, ama $referenceState referansiyla paketlenen oyuna giriyor."
            }
            elseif (@(Get-CigJsonField -Object $policyEntry -Name 'approvedTargets' -Default @()).Count -eq 0 -and $enabled) {
                $problem = "$($plugin.RelativePath): politika hicbir hedef icin onaylamiyor, ama eklenti etkin. .uproject icinde 'Enabled: false' olmali."
            }
        }

        $findings += [pscustomobject]@{
            Name             = $plugin.Name
            RelativePath     = $plugin.RelativePath
            Tracked          = [bool]$plugin.Tracked
            Reference        = $referenceState
            Enabled          = $enabled
            HasRuntimeModule = $hasRuntimeModule
            ReachesRuntime   = $reachesRuntime
            Classification   = $classification
            Problem          = $problem
        }
    }

    return $findings
}

function Get-CigForbiddenArtefactPatterns {
    <#
    .SYNOPSIS
    Path fragments that must not appear anywhere in a packaged build.

    .DESCRIPTION
    Derived rather than listed, so the answer cannot drift from the two places
    that decide it: the cook exclusion list and the local plugin policy. Both
    mechanisms are meant to keep these files out; this is the check that they did,
    performed on the artefact instead of on the argument that was supposed to
    produce it.
    #>
    param([Parameter(Mandatory)][string]$RepositoryRoot)

    # "Toolset" on its own covers GameFeaturesToolset and every other toolset
    # module the editor plugins pull in, which are named individually nowhere.
    $patterns = @('Toolset', 'unreal-mcp')
    $patterns += Get-CigCookDisabledPlugins

    $policyPath = Join-Path $RepositoryRoot 'Tools\LocalPluginPolicy.json'
    if (Test-Path -LiteralPath $policyPath -PathType Leaf) {
        $policy = Get-Content -LiteralPath $policyPath -Raw -Encoding UTF8 | ConvertFrom-Json
        foreach ($entry in @(Get-CigJsonField -Object $policy -Name 'localPlugins' -Default @())) {
            if (Get-CigJsonField -Object $entry -Name 'runtimeOutputMayInclude' -Default $false) { continue }
            $name = Get-CigJsonField -Object $entry -Name 'name'
            if ($name) { $patterns += [string]$name }
            $patterns += @(Get-CigJsonField -Object $entry -Name 'artefactPatterns' -Default @())
        }
    }

    return @($patterns | Where-Object { $_ } | Sort-Object -Unique)
}

function Get-CigForbiddenPackageArtefacts {
    <#
    .SYNOPSIS
    Files in a packaged build that came from tooling the game must not ship.

    .DESCRIPTION
    Matches on the path relative to the package root, so a directory named after
    the plugin is caught even when the files inside it are named something else.
    #>
    param(
        [Parameter(Mandatory)][string]$PackageDirectory,
        [Parameter(Mandatory)][string]$RepositoryRoot
    )

    if (-not (Test-Path -LiteralPath $PackageDirectory -PathType Container)) {
        throw "Paket klasoru bulunamadi: $PackageDirectory"
    }

    $patterns = Get-CigForbiddenArtefactPatterns -RepositoryRoot $RepositoryRoot
    $regex = ($patterns | ForEach-Object { [regex]::Escape($_) }) -join '|'
    $root = (Resolve-CigPath $PackageDirectory).TrimEnd([char]'\', [char]'/')

    return @(Get-ChildItem -LiteralPath $root -File -Recurse -ErrorAction Stop | Where-Object {
        $_.FullName.Substring($root.Length) -match "(?i)$regex"
    })
}

function Get-CigProjectPluginInventory {
    <#
    .SYNOPSIS
    Every Plugins/**/*.uplugin on disk, with whether git tracks it.
    #>
    param([Parameter(Mandatory)][string]$RepositoryRoot)

    $pluginsDir = Join-Path $RepositoryRoot 'Plugins'
    if (-not (Test-Path -LiteralPath $pluginsDir -PathType Container)) { return @() }

    $inventory = @()
    foreach ($file in Get-ChildItem -LiteralPath $pluginsDir -Recurse -Filter '*.uplugin' -File -ErrorAction Stop) {
        $relative = $file.FullName.Substring($RepositoryRoot.Length).TrimStart([char]'\', [char]'/').Replace('\', '/')

        $descriptor = $null
        try { $descriptor = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8 | ConvertFrom-Json }
        catch { throw "Okunamayan eklenti tanimi: $relative - $($_.Exception.Message)" }

        # --error-unmatch exits non-zero for a path git does not track. Nothing is
        # written either way.
        git -C $RepositoryRoot ls-files --error-unmatch -- $relative *> $null
        $tracked = ($LASTEXITCODE -eq 0)

        $inventory += [pscustomobject]@{
            Name         = [IO.Path]::GetFileNameWithoutExtension($file.Name)
            RelativePath = $relative
            Tracked      = $tracked
            Descriptor   = $descriptor
        }
    }
    return $inventory
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

function Test-CigPathWithinDirectory {
    <#
    .SYNOPSIS
    Tests path containment with an actual directory boundary.

    .DESCRIPTION
    A raw StartsWith check lets a sibling such as Downloads-Evil through a guard
    intended for Downloads. Both sides are resolved the same way as other script
    inputs, then the directory separator is made part of the comparison.
    #>
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Directory,
        [switch]$AllowDirectoryItself
    )

    $fullPath = Resolve-CigPath $Path
    $fullDirectory = Resolve-CigPath $Directory
    $separators = [char[]]@([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $fullDirectory = $fullDirectory.TrimEnd($separators)

    if ($AllowDirectoryItself -and
        [string]::Equals($fullPath, $fullDirectory, [StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }

    $prefix = $fullDirectory + [IO.Path]::DirectorySeparatorChar
    return $fullPath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)
}

function Test-CigCookDirectoryMustProduce {
    <#
    .SYNOPSIS
    Decides whether an always-cook entry must produce packaged assets.

    .DESCRIPTION
    Audio and LowPoly are repository-owned and always mandatory. Other entries
    name optional licensed packs: they become mandatory when the current source
    checkout actually contains uassets, but an absent pack must remain a valid
    primitive-fallback build. This keeps the smoke test strict without making a
    public checkout depend on Marketplace files that cannot be committed.
    #>
    param(
        [Parameter(Mandatory)][string]$GamePath,
        [Parameter(Mandatory)][string]$RepositoryRoot,
        [string[]]$MandatoryPaths = @('/Game/Audio', '/Game/LowPoly')
    )

    if (-not $GamePath.StartsWith('/Game/', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Always-cook path is outside /Game: $GamePath"
    }
    $segments = @($GamePath.Substring('/Game/'.Length).Split('/') |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($segments.Count -eq 0 -or $segments -contains '.' -or $segments -contains '..') {
        throw "Always-cook path is not a concrete game directory: $GamePath"
    }
    if ($MandatoryPaths -icontains $GamePath) {
        return $true
    }

    $source = Join-Path (Join-Path $RepositoryRoot 'Content') ($segments -join [IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        return $false
    }
    return $null -ne (Get-ChildItem -LiteralPath $source -Filter '*.uasset' -File -Recurse -ErrorAction Stop |
        Select-Object -First 1)
}

# ------------------------------------------------------------ release self-test

$script:CigSelfTestHeader = 'CIGRELEASESELFTEST v1'
$script:CigSelfTestChecks = @(
    'sistemler',
    'dunya',
    'metin-tablosu',
    'metin-tr',
    'metin-en',
    'denge-verisi',
    'ses-varliklari',
    'mesh-varliklari',
    'kayit-surumu',
    'kayit-turu',
    'shipping-hileleri'
)

function Get-CigSelfTestState {
    <#
    .SYNOPSIS
    Classifies one release self-test run as passed or failed. Never anything else.

    .DESCRIPTION
    Split out of SmokeTest-PackagedBuild.ps1 so it can be exercised against
    fixture files by Test-SelfTestState.ps1 without packaging a game, and so the
    rule lives in one place.

    Everything that is not a complete, internally consistent PASS is a failure.
    The earlier version of this logic answered "unsupported" to a missing or
    unreadable report and the smoke test then exited 0, so a package whose
    self-test never ran - or ran and died halfway - read as a verified build. A
    package built from this source tree has the mode; if the report is not there,
    something went wrong, and that is the whole point of the check.

    "Skipped" and "unsupported" are decided by the caller from its switches, not
    here, so neither can ever be produced by reading a report.
    #>
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$ReportPath,
        # $null means the exit code could not be observed, which is itself a fault.
        [AllowNull()][object]$ProcessExitCode = $null,
        [switch]$TimedOut
    )

    function New-State([string]$Reason, [string]$Detail) {
        [pscustomobject]@{ State = 'failed'; Reason = $Reason; Detail = $Detail }
    }

    if ($TimedOut) {
        return New-State 'timeout' 'oz-test sureci zaman asimina ugradi ve sonlandirildi'
    }
    if ([string]::IsNullOrWhiteSpace($ReportPath)) {
        return New-State 'no-path' 'oz-test rapor yolu bos'
    }
    if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
        return New-State 'missing-report' "oz-test raporu yazilmadi: $ReportPath"
    }

    $lines = @()
    try { $lines = @(Get-Content -LiteralPath $ReportPath -ErrorAction Stop) }
    catch { return New-State 'unreadable-report' "oz-test raporu okunamadi: $($_.Exception.Message)" }

    if ($lines.Count -eq 0 -or $lines[0] -ne $script:CigSelfTestHeader) {
        $first = if ($lines.Count -gt 0) { $lines[0] } else { '<bos dosya>' }
        return New-State 'bad-header' "rapor basligi gecersiz: '$first' (beklenen '$($script:CigSelfTestHeader)')"
    }

    $resultLines = @($lines | Where-Object { $_ -match '^RESULT(?:\s|$)' })
    if ($resultLines.Count -eq 0) {
        # The report is written to a temporary file and renamed into place, so a
        # header with no verdict means the rename produced a truncated file or
        # something wrote the path underneath us. Either way it is not a result.
        return New-State 'no-result' 'raporda RESULT satiri yok - oz-test tamamlanmadan bitti'
    }
    if ($resultLines.Count -ne 1) {
        return New-State 'duplicate-result' "raporda tam bir RESULT satiri olmali, $($resultLines.Count) bulundu"
    }
    $result = $resultLines[0]
    if ($result -notmatch '^RESULT (PASS|FAIL) (\d+)$') {
        return New-State 'bad-result' "RESULT satiri cozulemedi: '$result'"
    }

    $verdict = $Matches[1]
    $index = [int]$Matches[2]

    # The verdict and its index have to agree with each other before either is
    # compared with the process.
    if (($verdict -eq 'PASS') -ne ($index -eq 0)) {
        return New-State 'inconsistent-result' "RESULT $verdict $index kendi icinde tutarsiz"
    }
    $nonEmptyLines = @($lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($nonEmptyLines[-1] -ne $result) {
        return New-State 'result-not-final' 'RESULT raporun son bos olmayan satiri degil'
    }

    # A header and verdict alone do not prove that the packaged executable ran
    # its release checks. Every v1 check must appear exactly once, in the stable
    # order emitted by CigReleaseSelfTest::FormatReport.
    $checkLines = @($nonEmptyLines | Select-Object -Skip 1 | Select-Object -SkipLast 1)
    if ($checkLines.Count -ne $script:CigSelfTestChecks.Count) {
        return New-State 'incomplete-checks' "raporda $($script:CigSelfTestChecks.Count) kontrol bekleniyordu, $($checkLines.Count) bulundu"
    }

    $checkVerdicts = @()
    for ($i = 0; $i -lt $script:CigSelfTestChecks.Count; $i++) {
        $line = $checkLines[$i]
        if ($line -notmatch '^(PASS|FAIL)  ([^ ]+)(?:  \(.*\))?$') {
            return New-State 'bad-check' "kontrol satiri cozulemedi: '$line'"
        }
        $actualName = $Matches[2]
        $expectedName = $script:CigSelfTestChecks[$i]
        if ($actualName -ne $expectedName) {
            return New-State 'incomplete-checks' "kontrol $($i + 1) '$expectedName' olmaliydi, '$actualName' bulundu"
        }
        $checkVerdicts += $Matches[1]
    }

    $failedIndexes = @()
    for ($i = 0; $i -lt $checkVerdicts.Count; $i++) {
        if ($checkVerdicts[$i] -eq 'FAIL') { $failedIndexes += ($i + 1) }
    }
    $firstFail = if ($failedIndexes.Count -gt 0) { $checkLines[$failedIndexes[0] - 1] } else { $null }
    if ($verdict -eq 'PASS' -and $failedIndexes.Count -gt 0) {
        return New-State 'inconsistent-result' "RESULT PASS ama raporda basarisiz kontrol var: $firstFail"
    }
    if ($verdict -eq 'FAIL' -and ($failedIndexes.Count -eq 0 -or $failedIndexes[0] -ne $index)) {
        $found = if ($failedIndexes.Count -gt 0) { $failedIndexes[0] } else { 'yok' }
        return New-State 'inconsistent-result' "RESULT FAIL $index ama ilk basarisiz kontrol $found"
    }

    # The exit code carries the same verdict through a second channel. They
    # disagreed once for real: RequestExitWithStatus(Force=false) left a run whose
    # report said FAIL 6 exiting 0, which would have made every failure read as a
    # pass. A mismatch is a failure whichever side is right.
    if ($null -eq $ProcessExitCode) {
        return New-State 'no-exit-code' 'oz-test surecinin cikis kodu okunamadi'
    }
    $expected = $index
    if ([int]$ProcessExitCode -ne $expected) {
        return New-State 'exit-mismatch' "rapor RESULT $verdict $index diyor, surec $ProcessExitCode ile cikti"
    }

    if ($verdict -eq 'FAIL') {
        return New-State 'check-failed' "$firstFail; cikis kodu $ProcessExitCode"
    }
    return [pscustomobject]@{ State = 'passed'; Reason = 'ok'; Detail = "$($lines.Count) satirlik rapor" }
}

function Resolve-CigSelfTestOutcome {
    <#
    .SYNOPSIS
    Turns the caller's switches plus an observed state into the four distinct
    outcomes the smoke test reports.

    .DESCRIPTION
    passed      - ran, complete, consistent. The only outcome that counts as a pass.
    failed      - ran and did not produce that. Fails packaging.
    skipped     - -SkipSelfTest was given explicitly. Diagnostic use only.
    unsupported - a report is absent AND the caller explicitly allowed a package
                  built before the mode existed. Never a pass, never a default.

    CountsAsCheck is what keeps the last two honest: they do not enter the
    pass/fail set at all, so nothing can add them up as verified.
    #>
    param(
        [switch]$SkipSelfTest,
        [switch]$AllowLegacyPackageWithoutSelfTest,
        [AllowNull()][object]$Observed = $null
    )

    if ($SkipSelfTest) {
        return [pscustomobject]@{
            State = 'skipped'; Detail = '-SkipSelfTest verildi'
            CountsAsCheck = $false; IsPass = $false
        }
    }
    if ($null -eq $Observed) {
        return [pscustomobject]@{
            State = 'failed'; Detail = 'oz-test calistirilmadi'
            CountsAsCheck = $true; IsPass = $false
        }
    }
    if ($Observed.State -eq 'passed') {
        return [pscustomobject]@{
            State = 'passed'; Detail = $Observed.Detail
            CountsAsCheck = $true; IsPass = $true
        }
    }
    if ($AllowLegacyPackageWithoutSelfTest -and $Observed.Reason -eq 'missing-report') {
        return [pscustomobject]@{
            State = 'unsupported'
            Detail = 'bu paket -CigReleaseSelfTest modunu tanimiyor (-AllowLegacyPackageWithoutSelfTest)'
            CountsAsCheck = $false; IsPass = $false
        }
    }
    return [pscustomobject]@{
        State = 'failed'; Detail = $Observed.Detail
        CountsAsCheck = $true; IsPass = $false
    }
}
