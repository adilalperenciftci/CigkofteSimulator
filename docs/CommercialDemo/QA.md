# QA log

Only executed commands and observed results. Nothing here is predicted.

## 2026-07-26 — baseline

| Check | Command | Result |
|---|---|---|
| Static | `python Tools/check_sources.py` | clean: 13 CSVs, 695 keys, 2 languages |
| Build | `Build.bat CigkofteSimulatorEditor Win64 Development` | Succeeded |
| Tests | `Automation RunTests Cigkofte` | 50 passed, 0 failed, exit 0 |
| Data load | runtime log | 13/13 CSVs loaded, 0 warnings |

## 2026-07-26 — slice 0.2 + 0.3

| Check | Command | Result |
|---|---|---|
| Static | `python Tools/check_sources.py` | clean (Inspection.csv now 15 rows) |
| Build | `Build.bat CigkofteSimulatorEditor Win64 Development` | Succeeded |
| Tests | `Automation RunTests Cigkofte` | **52 passed, 0 failed**, exit 0 |

New tests observed passing:

- `Cigkofte.Staff.PackageKeepsDoughSpice`
- `Cigkofte.Inspection.ComplaintRaisesOddsWithoutGuaranteeing`

## 2026-07-26 — slice 0.5

| Check | Command | Result |
|---|---|---|
| Static | `python Tools/check_sources.py` | clean |
| Build | `Build.bat CigkofteSimulatorEditor Win64 Development` | Succeeded |
| Tests | `Automation RunTests Cigkofte` | **53 passed, 0 failed**, exit 0 |

New test observed passing: `Cigkofte.Events.DayLongEventsEndCleanly`.

Note: the first version of this test asserted only that `Active` emptied, which
the previous `Active.Empty()` implementation also satisfied. It was rewritten to
assert the retired count returned by `TumOlaylariBitir`, which the old path could
not produce.

## 2026-07-26 — slice 0.6

| Check | Command | Result |
|---|---|---|
| Static | `python Tools/check_sources.py` | clean |
| Build | `Build.bat CigkofteSimulatorEditor Win64 Development` | Succeeded |
| Tests | `Automation RunTests Cigkofte` | **55 passed, 0 failed**, exit 0 |

New tests observed passing: `Cigkofte.Reviews.IdSurvivesNewerReviews`,
`Cigkofte.Reviews.IdsAreUniqueAndTrimSafe`.

Save version raised to 12. A migrated v11 save has not yet been loaded end to end;
that is covered by slice 0.9, which adds the migration test group.

## 2026-07-27 — M1 first playable session

The first time anyone has run this game rather than reading it. Two ways: a PIE
session driven with real key events through the editor's Slate input path, and a
new automation test that walks the preparation chain through the live systems.

| Check | Command | Result |
|---|---|---|
| Static | `Scripts/ValidateAll.ps1` (static stage) | PASS — 21 asset folders, 19 cook rules |
| Build | `Scripts/BuildEditor.ps1` | PASS — `Result: Succeeded` |
| Tests | `Automation RunTests Cigkofte` | **71 passed, 0 failed** (was 69) |
| Data | runtime log | 14/14 balance files, 0 warnings |
| Package | not run | SKIPPED (`-IncludePackage` not passed) |

### PIE session

Map `/Engine/Maps/Entry` (the shop is built from code by `CigWorldBuilder`), the
existing `CigSave.sav` at day 3. Backed up before the session and restored after
it, because a played day writes to the same slot a player's game does.

Real input, not a simulated call path: a click into the PIE viewport and an Enter
key event through Slate's own input pipeline selected **Devam Et** on the title
screen, and the log answered with `Gün 3 başladı`. Observed over one full
180-second day:

- The HUD reads correctly in Turkish — day, money, remaining seconds, level,
  popularity, hygiene, and `Olay: Belediye Denetimi`. Screenshot:
  `docs/screenshots/M1-PIE-gun3.png`.
- The recipe panel (`KASE — Ekonomik TARİFİ`) and the next-customer panel
  (heat, portions, garnish, "İstemiyor: soğan", patience bar) both populate.
- Customers arrive, wait, and leave angry when not served; delivery orders are
  issued and time out; the inspection event runs and retires with
  `Denetim günü bitti`; the cat gets hungry.
- The day ended on schedule with `İflas: gün 3, kira 250` and the game-over
  screen `İFLAS ETTİN! Kira 250 TL'ydi; kasa yetmedi. 3 gün dayandın.`
  Screenshot: `docs/screenshots/M1-PIE-gunsonu.png`. The save was already
  1762 TL in debt and nothing was sold, so this is the correct outcome.

**What the scripted input could not do.** Injected key events are discrete presses
and releases; walking to a station needs a key held down over several frames. So
the ingredient → knead → wrap → serve chain could not be driven this way. `E`,
`W` and `1` were sent and produced no station interaction, which is consistent
with the player standing away from every station. That chain is covered by the
automation test below instead.

**Editor tooling defect, not a game defect.** Capturing the whole editor window
while PIE is running crashes the editor:
`Saved/Crashes/UECC-Windows-D92CDA0448752756ABAF288994E5CC84_0000`,
`EXCEPTION_ACCESS_VIOLATION`, call stack entirely D3D12RHI → SlateRHIRenderer →
RenderCore with no game module in it. Capturing the single Slate window instead
worked and the session completed without incident.

### End-to-end automation test

`Cigkofte.DayFlow.OneDayFromStockToSave` (Tests/CigDayFlowTests.cpp) opens a day,
takes a customer's order, mixes to the recipe, kneads **in rhythm** (0.30s between
presses, because a faster loop is "panic kneading" worth 2 progress and lands the
batch past MaxKnead), builds the wrap to the order including chopping garnish,
serves it and saves.

Observed in one run:

| | Before | After |
|---|---|---|
| Money | baseline | **+68 TL** |
| Reputation | 50.00 | raised (accuracy 100, quality 84.4 clears the 85/70 bar) |
| TotalServed | n | n+1 |
| Day tally | 0 served, 0 earned | 1 served, +68 |
| Service score | baseline | raised (RecordServe ran) |
| Reviews (end of day) | 0 | 0–3, each with a non-zero ID |
| Stock | bulgur, water, lavaş | −5, −3, −1 |
| Dough | 6 units | −1 per portion |

Kneading took 15 presses against the recipe's 12–24 window, quality 84.4, and the
customer tipped: `+68 TL | Doğruluk %100 | Kalite: İyi`. Save/load round trip:
money, served count, reputation, day and reviews all returned, save version 12.

The test's first version asserted that a served day produces a review, then read
`Reviews[0]` regardless. End of day writes `RandRange(0, 3)` of them, so zero is
a normal day — and on the run where the roll came up zero the read went out of
bounds and took the whole suite down at test 10 of 71. Corrected to assert what
is deterministic: the service category score, which `RecordServe` moves on every
serve. Confirmed by running `Cigkofte.DayFlow` three times in a row — 1, 0 and 1
reviews produced, all three green.

### Log

`Saved/Logs/CigkofteSimulator.log` after the run: **`LogCig: Error` = 0**, and no
`Fatal error`, `Assertion failed`, `Ensure condition failed` or
`Unhandled Exception`.

### Which plugins reach a packaged build

Worth recording as a method, because the answer is measurable and guessing at it
is how a development-only plugin ends up inside a shipped game. `Build.bat
<target> Win64 <config> -Mode=JsonExport` writes the fully resolved target — every
module that will be compiled into it — without building anything, so the module
list for `Game/Shipping` can be compared against `Editor/Development` directly.

A plugin reference in the `.uproject` can be constrained rather than removed:
`"TargetAllowList": ["Editor"]` keeps it out of the game targets while leaving it
enabled in the editor. The field is read at `PluginReferenceDescriptor.cpp:161`
and applied by `IsEnabledForTarget` at lines 64-79 of the same file; a plugin
whose own descriptor is `"EditorOnly": true` never reaches a game target at all
and needs nothing.

### BuildEditor.ps1

The exit code is the sole success criterion and the log text is a second opinion.
`Test-BuildLog` fails a build only when the exit code is 0 *and* the log says
`Result: Failed` / `BUILD FAILED`; a missing `Result: Succeeded` warns and passes.
Verified three ways: a log with no stamp passes with a warning, a `Result: Failed`
log is caught, a normal log passes. Dry-run and a real editor build both exit 0.

## Not yet executed

Scenario tests required by the commercial-demo standard that do not exist yet:
power-cut refrigeration, closure blocking customers, placement round trip,
pooled seat release.

Localization placeholder parity has since been covered by the `Cigkofte.Localization`
group, which checks that both languages resolve every routed key and that a
template may reorder its arguments between them.

A played session still cannot exercise the preparation stations by hand. Doing
that needs either held-key input simulation or an in-game automation hook.

## 2026-08-01 — PR #6 release-hardening checkpoint

This is machine verification, not a human playtest. The current continuation was
validated through the editor and both packaged configurations:

| Check | Result |
|---|---|
| Static source rules | clean — **99** automation tests and **23** systems discovered |
| Self-test classifier fixture | **49/49** assertions passed |
| Path helper fixture | **7/7** assertions passed |
| PowerShell syntax | all modified release scripts parsed successfully |
| Editor build | succeeded |
| Focused label tests | **5 passed, 0 failed** |
| Focused output-argument parser test | **1 passed, 0 failed** |
| Full automation | **99 passed, 0 failed** |
| `ValidateAll.ps1` | static, harness, paths, build, tests and runtime data all passed |
| Development candidate | **2,339,690,706 bytes**; all eight smoke checks passed; mandatory self-test passed |
| Shipping candidate | **1,981,073,529 bytes**; process, cook coverage and mandatory eleven-check self-test passed |
| Player release archive | **1,589,032,416 bytes**, 46 entries, zero PDB files |
| Separate symbol archive | **64,347,504 bytes**, one Shipping PDB |
| Extracted player release | **1,750,423,673 bytes**, 46 files; `Verify-Release.ps1` PASS |

The packaged negative cases were run against staged copies and restored
immediately: an explicitly empty output argument, a forbidden `Config` path and
an unwritable parent all exited with the stable report-unwritable code **90**;
removing the balance directory produced a complete `RESULT FAIL 6`; and deleting
a previously valid report before classification produced `failed/missing-report`.
The PowerShell fixture separately covers timeout and exit/report disagreement.

Two defects were found during the Shipping runs. The first Shipping-only
controller branch initially lacked the complete `UCheatManager` type and failed
to compile. After that was corrected, the self-test incorrectly required a live
player controller during `InitGame` and returned `FAIL 11`; it now validates the
configured controller CDO and treats a runtime instance as an additional check
only when one exists. Both fixes were rebuilt, and the subsequent Shipping
package passed.

The player archive was created from `Build/PR6-Shipping`, not the Development
candidate. Its extracted launcher SHA-256 matches the raw Shipping launcher and
differs from the Development launcher; the checksum file also passed. Directory
structure is preserved, and debug symbols exist only in the separate symbol
archive.

Still manual: held-control kneading and chopping feel, wrap controls, gamepad
input, focus readability, signage while moving, ambience balance, customer
animation quality, lighting, price-policy queue balance, visible save/load and a
full played day. The packaged self-test does not substitute for those checks.

## 2026-08-01 — Stage 3.1 placement authority

This is deterministic geometry, integration and packaged machine verification;
it is not a human build-mode or navigation playtest.

| Check | Result |
|---|---|
| Static source rules | clean — **121** automation tests, **24** systems, 889 bilingual keys |
| Placement automation | **22 passed, 0 failed** |
| Full automation | **121 passed, 0 failed** |
| `ValidateAll.ps1` | static, harness, paths, build, tests and 14/14 runtime data all passed at `283b825`; the review fix then reran static checks, Editor build and all 121 tests |
| Cook policy fixture | **7/7** assertions passed without Unreal |
| Development package | **714,038,226 bytes**, 70 files, zero PDB; eight smoke checks and mandatory self-test passed; game EXE SHA-256 `A9B4EC34CDE4038CBEA45AE002851481E836D7CA041AA0E1756485C884C7CB77` |
| Shipping package | **504,254,016 bytes**, 49 files, zero PDB; process, cook coverage and mandatory self-test passed; game EXE SHA-256 `CC6DE1FC9A4D1CE2034538CED9D1934723A370E2649D4383E028E0540682F19C` |

The real-shop integration registered all 22 stations plus five authored seating
fixtures, selected the first valid declared delivery spot, spawned a physical
crate with a lifecycle-stable ID, and released its placement record after
unloading. Pure tests cover bounds, overlap, edge contact, quarter-turn geometry,
protected entrance/queue/service/station routes, duplicate and move identities,
deterministic failure precedence, all delivery alternatives occupied, rollback,
remove/reuse and missing-mesh fallback.

The final packages were built from detached commit `a96d012` in a clean D: validation
worktree containing only public repository assets. Both containers held 19
repository-owned Audio and 64 LowPoly assets. Thirty absent licensed/Fab cook
paths used the documented primitive fallback; no Marketplace files were copied
or committed. Both machine-readable reports used `CIGRELEASESELFTEST v1` and
ended in `RESULT PASS 0`; neither smoke run used skip or legacy compatibility.

Review found that the sofa visual was spawned at 90 degrees while its non-square
placement footprint used the default zero yaw. Passing the visual yaw to the
authority exposed a second fact that the old wrong rectangle had hidden: the
correctly rotated footprint extended 40 cm outside the shop. The first focused
rerun therefore failed 1 of 22 placement tests with `OutsideShopBounds`; it is
not counted as a pass. The visual and footprint now share one location and yaw,
50 cm farther inside the shop, and the real-shop integration test pins the
registered 90-degree transform. Static checks, Editor build, 22/22 placement
tests, 121/121 full automation and both packages then passed.

That public-checkout run found a real harness defect: the smoke test treated
every absent optional asset directory and every expected optional-mesh warning
as a package failure. It now keeps Audio and LowPoly mandatory, requires any
optional pack that actually exists in the source checkout, and explicitly skips
an absent licensed pack. The 7-case fixture covers mandatory roots, present,
empty and absent optional roots, sibling-prefix isolation and paths outside
`/Game`. The same already-built Development and Shipping packages then passed
the corrected smoke gate.

Two unsuccessful packaging attempts are not counted as validation. The first
`PackageDemo.ps1` run hit `LNK1140` while the large changed source set was
adaptive non-unity; after the source commit, C: fell to 0.26 GB and the next link
hit `LNK1180`. Moving generated validation work to D: fixed disk pressure. A
parallel UBA attempt was stopped after its low-memory guard repeatedly killed
PCH compilation; the successful Development and Shipping UAT runs used
`-MaxParallelActions=1`, `-nodebuginfo` and D: output, followed by the repository
smoke script. These are host resource failures, not game-test passes.

Still manual: shop-layout usability, full pawn navigation through future custom
layouts, crate readability while moving, held-control preparation feel,
gamepad, focus readability, signage in motion, ambience, customer animation,
lighting, price-policy queue balance, visible save/load and a full played day.
Rectangular protected zones do not prove Unreal AI navigation; that remains
Stage 3.4.

## 2026-08-01 — Stage 3.2 functional placement categories

This validates deterministic classification policy and its real-shop wiring. It
is not a human build-mode, layout-usability or navigation playtest.

| Check | Result |
|---|---|
| Static source rules | clean — **129** automation tests, **24** systems, 890 bilingual keys |
| Placement automation | **30 passed, 0 failed** |
| Full automation | **129 passed, 0 failed** |
| `ValidateAll.ps1` | static, 49/49 self-test-state assertions, 7/7 cook-policy assertions, 7/7 path-safety assertions, Editor build, 129 tests and 14/14 runtime balance files passed; package stage was separately executed below |
| Development package | **661,127,153 bytes**, 64 files, zero PDB; eight smoke checks and mandatory self-test passed; runtime EXE SHA-256 `3BFCDAABE7500F15AC27994D35FFA4C980742C4ACD134C121345CD5A09ACE9D1` |
| Shipping package | **451,315,085 bytes**, 43 files, zero PDB; process, cook coverage and mandatory self-test passed; runtime EXE SHA-256 `48A02409CB956E85F27DEF7BF2D7FAD0C8CAFADC3D07F11CBE958F476BBB10F8` |

Both packages were built from runtime commit `956073e` in the D: Stage 3.2
worktree with `-nodebuginfo`, `-NoUBA` and one build action. Their cook checks
found 19 repository-owned Audio and 64 LowPoly assets. All 31 optional licensed
asset paths were absent and explicitly used the public primitive fallback; no
Marketplace/Fab content was copied or committed. Neither smoke run used
`-SkipSelfTest` or legacy compatibility.

The pure policy tests reject unknown and undefined category, lifetime and
context values; invalid Installed/Transient context combinations; transient
non-storage deliveries; and ignore-ID misuse. Move tests prove that category
and lifetime are immutable and that rejection does not mutate the record.
Category/lifetime count queries are covered independently.

The real-shop integration pins the exact pre-delivery classification: 22
`Station`, four `Seating`, one `Decoration`, zero `Storage`, 27 `Installed` and
zero `Transient` records. The sofa remains a decoration rather than claiming a
seat. A physical delivery then adds exactly one `Storage` + `Transient` record,
and unloading removes it. No actor pointer became an identity and save version
12 did not change because no placement metadata is persisted in this slice.

Two initial Editor build attempts are not counted as passes. The first caught
that UE 5.8's `TArray` does not provide `CountByPredicate`; the count helpers now
use explicit loops. The next caught a missing forward declaration for the new
category enum in `CigWorldBuilder.h`. After those compile fixes, the clean Editor
build and every validation result listed above passed.

Still out of scope: player-facing category UI, category-specific layout effects,
AI/path validation, player-authored placement persistence and shop identity.
Stage 3.3 layout consequences is next; full navigation proof remains Stage 3.4.

## 2026-08-03 — navigation hardening

Closes the residual Stage 3.4 defects the stage itself documented rather than
fixed. Still not a human navigation playtest.

| Check | Result |
|---|---|
| Static source rules | clean — **153** automation tests, 25 systems |
| Release self-test state machine | **49/49** |
| Packaged cook policy | **7/7** |
| Cook plugin exclusion | **18/18** |
| Script path safety | **7/7** |
| Navigation automation | **14 passed, 0 failed** (11 before) |
| Full automation | **153 passed, 0 failed** (150 before) |
| Runtime data load | **14/14** balance files, **0** warnings |
| `ValidateAll.ps1` | all non-package stages PASS |
| Development package | BUILD SUCCESSFUL, 8/8 smoke, self-test passed |
| Shipping package | BUILD SUCCESSFUL, 3/3 smoke, self-test passed |
| `Verify-Release.ps1` (Shipping) | **PASS** |

### The direct fallback is gone

Stage 3.4 shipped a fallback: no route meant walk straight at the target. The
argument was that a frozen customer is worse than one clipping a table. That was
the wrong trade in the only direction that mattered — the fallback fires on
exactly the layouts the measured navigation exists to catch, so the one case
where the answer was needed was the case it was discarded.

A customer with no route now stops. `UCigCustomerSystem::RecoverStrandedCustomers`
takes ownership back — releasing the seat and queue slot first, because a chair
reserved by someone who cannot reach it is a chair the shop has lost — gives them
one attempt to walk out, and recycles them through the pool if the exit is
unreachable too. Bounded by `bStrandRecoveryAttempted`, counted by
`StrandedRecovered` and `StrandedRecycled`, and logged with the failure reason.

### A seat leak found while writing that

`RecycleFinished` dropped the `Seated` record without calling
`UCigWorldBuilder::ReleaseSeat`. `Seated` is only a view of a reservation that
lives in the world builder, so any customer recycled while seated left the chair
marked occupied for the rest of the day. Both paths now go through one
`ReleaseCustomerOwnership`.

### Stale routes

`UCigNavSystem::LayoutRevision()` increments with the dirty flag, and a customer
stores the revision its route was built at. Deliberately not the rebuild counter:
rebuilds are lazy, so a layout can change several times without one happening,
and a walker watching rebuilds would keep following a route through a table that
is already standing in it. One integer compare per customer per tick.

### Swept movement

`SetActorLocation` was unswept, and could not usefully be swept: the root is a
bare `USceneComponent` with no collision and the visible body is `QueryOnly` so
customers do not shove each other. Movement now sphere-sweeps
`ECC_WorldStatic` along the step, stops short of a blocker and repaths once. The
grid knows about placements and the shop shell; the sweep is what catches
anything else.

### The bug the tests found in the tests

All three new tests failed on first run because every customer had no navigation
at all. `ACigkofteCustomer` resolved it through
`GetWorld()->GetAuthGameMode()`, which is null under `FCigTestShop` — the harness
spawns a GameMode rather than installing one. So the customer behaved exactly as
if the shop were empty, and would have done in any future test too. The
dependency is now handed over explicitly by whoever spawns or reuses the
customer; an explicit dependency cannot be quietly absent.

### Packages

| | Development | Shipping |
|---|---|---|
| Files | 80 | 51 |
| Bytes | 2,349,271,780 | 1,799,632,683 |
| Runtime EXE SHA-256 | `C0A551633D5FF9A8EE93B502E607940CB9D039DF959C35F3999BAF19B340965F` | `E029928B8D785AF08CB8EA1F0F6504BA4AAFDCCDC983C5CA70C1A439FF7F9BFF` |
| PDB files | 1 | 0 |

### Not done on this branch

- Ambient street pedestrians still use direct movement and can cross authored
  static geometry. Constraining them needs authored pavement lanes or a modelled
  street region.
- The Dynamic Recast NavMesh comparison has not been run, so
  `docs/Architecture/NAVIGATION_AUTHORITY.md` does not exist and the grid is the
  authority by default rather than by measurement.

Both are recorded in `KNOWN_LIMITATIONS.md` as open.

## 2026-08-03 — packaging restored with the editor toolsets left enabled

Packaging had been impossible since PR #11. It works again, and the `.uproject`
still enables `ModelContextProtocol` and `AllToolsets` for editor work — that was
the constraint, not an afterthought.

| Check | Result |
|---|---|
| Static source rules | clean — 150 automation tests, 25 systems |
| Release self-test state machine | **49/49** assertions |
| Packaged cook policy | **7/7** assertions |
| Cook plugin exclusion | **18/18** assertions (new) |
| Script path safety | **7/7** assertions |
| Full automation | **150 passed, 0 failed** |
| Runtime data load | **14/14** balance files, **0** warnings |
| `ValidateAll.ps1` | static, harness, paths, Editor build, tests, data all PASS |
| Development package | **BUILD SUCCESSFUL**, 8/8 smoke checks, mandatory self-test passed |
| Shipping package | **BUILD SUCCESSFUL**, 3/3 smoke checks, mandatory self-test passed |
| `Verify-Release.ps1` (Shipping) | **PASS** |

### What was actually wrong

`TargetAllowList: ["Editor"]` keeps a plugin out of the shipped game. It does not
keep it out of the **cook**, because the cook commandlet is itself an editor.
`AllToolsets` depends on `GameFeaturesToolset`, which depends on `GameFeatures`,
which demands an asset-manager rule for `GameFeatureData`; the cook logs its
absence as an error and UAT fails with `Error_UnknownCookFailure`. Declaring the
rule is the wrong fix for reasons `DefaultGame.ini` gives at length and
`check_sources.py` now enforces, so both directions were closed.

### Three experiments, and what each one proved

The engine parses `-DisablePlugins=` in
`FPluginManager::FindCommandLinePlugins`, and that runs *before*
`FindTargetPlugins` — which is precisely what lets a command-line disable beat a
plugin the `.uproject` enables. The mechanism was right from the start; both
earlier attempts failed for reasons that had nothing to do with it, and both
failed **silently**, producing the identical cook error.

1. **`-DisablePlugins=AllToolsets+ModelContextProtocol`** — failed. The engine
   splits the list on `,`. `"AllToolsets+ModelContextProtocol"` is not the name of
   any plugin, so nothing was disabled and nothing was reported.
2. **Comma-separated, added to `Package-Windows.ps1`** — failed. The run that
   matters goes through `PackageDemo.ps1`, which carries its own independent UAT
   argument list. The edit was never executed. Confirmed by searching the UAT log
   for `DisablePlugins` and finding no occurrence at all.
3. **Comma-separated, from one shared helper used by both entry points** —
   **BUILD SUCCESSFUL.**

Two argument lists is what turned a one-line fix into three attempts, so the
argument now has exactly one definition: `Get-CigCookPluginExclusionArg` in
`CigCommon.ps1`. `Test-CigCookPluginExclusion.ps1` pins both failure modes — the
separator, and either entry point drifting away from the helper — and also
asserts that the committed `.uproject` still enables both plugins for the editor,
so a future "fix" that quietly disables them there fails the tests instead of
passing them. Verified in both directions: changing the separator back to `+`
fails exactly 2 of 18 assertions.

No project descriptor is mutated at any point. `git diff CigkofteSimulator.uproject`
is empty across both package runs, so no transactional backup-and-restore wrapper
was needed.

### Packages

| | Development | Shipping |
|---|---|---|
| Command | `PackageDemo.ps1 -Configuration Development` | `Package-Windows.ps1 -Configuration Shipping` |
| Path | `Build/WindowsDemo` | `Build/Windows-Shipping` |
| Files | 77 | 51 |
| Bytes | 2,349,021,333 | 1,799,629,611 |
| Runtime EXE | `CigkofteSimulator/Binaries/Win64/CigkofteSimulator.exe` | `CigkofteSimulator/Binaries/Win64/CigkofteSimulator-Win64-Shipping.exe` |
| SHA-256 | `A742B9D08619079E89B1291E53C70446A24AA939F4F070156D18AB7C143A9579` | `E95ECEDBC6E9D503A0B8A7CAD3083EAB6781D130B4D9FC9F0965176DE41AFBEF` |
| PDB files | **1** | **0** |
| Editor tooling files | **0** | **0** |

Both built from runtime commit on `fix/cook-with-editor-toolsets`. The hashes are
of the runtime executable under the packaged `Binaries/Win64`, not of the 171 KB
bootstrap launcher in the archive root.

Development carries one PDB because `PackageDemo.ps1` does not pass
`-nodebuginfo`; `Package-Windows.ps1` does unless `-IncludeSymbols` is given,
which is why Shipping has none. That asymmetry is pre-existing and deliberate —
the demo build is the one people debug — but it means a Development archive is
not a release artefact.

Neither archive contains any file matching `Toolset`, `ModelContextProtocol`,
`GameFeature` or `MCP`. The exclusion worked at the level that matters.

### Found while verifying: the shipped game carries Sentry's crash handler

Not caused by this branch, and not fixed on it.

Both packages ship `Plugins/Sentry/Binaries/Win64/crashpad_handler.exe`
(1,094,656 bytes) and, in Development, `crashpad_wer.dll`. `Plugins/Sentry` is
gitignored and local-only precisely because `CRASH_PRIVACY.md` says an endpoint
is enabled only after explicit consent, a privacy policy, a stated retention
period and a configured DSN — none of which exist.

`WORKTREE_INVENTORY.md` already warned that a plugin dropped into `Plugins/` is
enabled by default whether or not the `.uproject` mentions it, and that this
machine and a fresh clone therefore do not build the same editor. That is now
also true of the shipped game: a clean checkout would produce a package without
these binaries, and this one does not.

No DSN is configured, so nothing is transmitted. It is recorded as a release
blocker rather than a privacy incident, and it needs its own branch.

## 2026-08-03 — Stage 3.4 navigation validation

This validates measured reachability across the shop floor and the customer
locomotion that now follows it. It is **not** a human navigation, build-mode or
layout-usability playtest. Nobody has walked the shop since this landed.

| Check | Result |
|---|---|
| Static source rules | clean — **150** automation tests, **25** systems, 893 bilingual keys |
| Release self-test state machine | **49/49** assertions |
| Packaged cook policy | **7/7** assertions |
| Script path safety | **7/7** assertions |
| Navigation automation | **11 passed, 0 failed** |
| Full automation | **150 passed, 0 failed** (139 before) |
| Runtime data load | **14/14** balance files, **0** warnings |
| `ValidateAll.ps1` | static, harness, paths, Editor build, tests and data all PASS |
| Development package | **blocked on master**, see below |
| Shipping package | not attempted — same blocker |

The eleven navigation tests split three ways. Seven exercise the pure grid with
no world at all: an open floor collapsing to two points, a sealed wall having no
route, endpoint failures being named separately rather than all reported as "no
route", diagonals refusing the squeeze between two objects that touch at a
corner, and the same query returning an identical path twice on a symmetric
layout. One is the reason the stage exists — `AGapNarrowerThanTheAgentIsNotARoute`
puts two obstacles 60cm apart, asserts through `RectsOverlap` that the placement
authority has no objection to either, and then shows a 70cm body cannot pass;
widening the gap to 120 opens it with the rectangles still not overlapping.

Two stand up the real shop through `BuildWorld` and audit its routes, and one
pins invalidation: five repeated queries rebuild the grid zero times, and a
single `RemovePlacement` rebuilds it exactly once and makes the floor the table
stood on walkable.

The last one is `Cigkofte.Navigation.Collision.TheEngineAgreesWithTheGrid`. It
overlaps the player's real capsule (38 x 88) against the actually spawned
geometry at four points the grid calls open and at every wall centre, so the grid
is checked against the engine rather than against itself.

### The packaging blocker, which is not this stage's

`PackageDemo.ps1 -Configuration Development` fails during cook:

```
LogGameFeatures: Error: Asset manager settings do not include a rule for assets
of type GameFeatureData, which is required for game feature plugins to function
AutomationTool exiting with ExitCode=25 (Error_UnknownCookFailure)
```

Confirmed pre-existing rather than assumed: `master` at `14d37db` was checked out
clean and packaged, and failed identically. This branch touches no config and no
plugin entry.

Root cause proven by experiment rather than reasoning. `AllToolsets` was set to
`"Enabled": false` in the `.uproject`, nothing else changed, and the same command
produced **BUILD SUCCESSFUL** with all eight smoke checks green including the
mandatory self-test. The experiment was then reverted.

So: `AllToolsets` is constrained to Editor targets, but **cook runs in the
editor**. It depends on `GameFeaturesToolset`, which depends on `GameFeatures`,
so the cook commandlet loads a module whose asset-manager rule the project
deliberately does not declare — and `DefaultGame.ini` explains at length why
declaring it is the wrong fix. Both directions currently fail. Packaging has been
impossible since PR #11 enabled the plugin; Stage 3.3's packages predate that
merge, which is why nobody noticed.

Evidence from the experimental run, recorded because it is the only measurement
of this branch's runtime behaviour in a packaged build, and flagged because it
was **not** built from the committed plugin list:

| | |
|---|---|
| Files | 74 |
| Bytes | 2,348,950,416 |
| Runtime EXE SHA-256 | `8CB8474E0BC05452F475CDF1DB19755EB8D3C00D5DA0B5F2CFC9DBCB4C7DC92A` |
| PDB files | **1** — this run did not use `-nodebuginfo`, unlike Stage 3.3's |
| Smoke checks | 8/8, mandatory self-test passed |

That package is not release evidence and must not be quoted as such. Fixing the
blocker belongs on its own branch: it is a packaging and plugin-scope defect, not
a gameplay one, and the choice between disabling `AllToolsets` and excluding it
from the cook is a decision about the editor workflow rather than about the game.

## 2026-08-02 — Stage 3.3 layout consequences

This validates deterministic layout-consequence policy and the gameplay that now
reads it. It is not a human build-mode, layout-usability or navigation playtest.

| Check | Result |
|---|---|
| Static source rules | clean — **139** automation tests, **24** systems, 893 bilingual keys |
| Release self-test state machine | **49/49** assertions |
| Packaged cook policy | **7/7** assertions |
| Script path safety | **7/7** assertions |
| Placement automation | **40 passed, 0 failed** |
| Full automation | **139 passed, 0 failed** |
| Runtime data load | **14/14** balance files, **0** warnings |
| `ValidateAll.ps1` | static, harness, paths, Editor build, tests and data all PASS; package stage separately executed below |
| Development package | **714,140,798 bytes**, 70 files, zero PDB; eight smoke checks and mandatory self-test passed; runtime EXE SHA-256 `20BE74A4A0F31A837648F9902655B8E135D2DDAF77135146BF369E0480DB4E46` |
| Shipping package | **504,257,191 bytes**, 49 files, zero PDB; process, cook coverage and mandatory self-test passed; runtime EXE SHA-256 `D87ED8E1103044C5CF83CBA0B04564C97999B8661AE52708FA87BCAC6CB847E9` |

Both packages were built from runtime commit `8710588` in the D: Stage 3.3
worktree with `-nodebuginfo`. Their cook checks found 19 repository-owned Audio
and 64 LowPoly assets; all 31 optional licensed asset paths were absent and used
the documented public fallback. No Marketplace or Fab content was copied or
committed. Neither smoke run used `-SkipSelfTest` or legacy compatibility, and
`Verify-Release.ps1` returned PASS with no forbidden debug or secret artefact in
either archive.

The hashes above are of the runtime executable under
`Windows/CigkofteSimulator/Binaries/Win64`, not of the 171 KB bootstrap launcher
in the archive root. That launcher is byte-identical between Development and
Shipping, so quoting it would have produced two "different" builds with the same
hash. `Verify-Release.ps1` resolves the first `CigkofteSimulator.exe` it finds
and therefore reports the bootstrap; that is adequate for the artefact scan it
performs and is not adequate as build identity.

Ten of the 40 placement tests are new and cover the consequence itself: pure
derivation, category-specific geometry, determinism across repeated derivation,
invalid use-area rejection, protected-route interaction, functional clearance,
the query surface, and what a move, a remove and an event each do to it. They
need no `UWorld`.

The real-shop integration pins the exact authored outcome: 22 `Station`, four
`Seating`, one `Decoration`, zero `Storage`, 27 `Installed`, zero `Transient`,
27 consequences, 22 usable station units, 8 usable seats from four tables, and
zero capacity from the sofa. A ninth seat reservation fails because the table's
authored capacity, not the chair count, is what answers. A delivery then adds
exactly one transient storage consequence, and unloading or destroying the crate
removes it.

`UCigEventBus::PlacementChanged` is published on real state changes only, but
nothing in production subscribes to it yet. It is groundwork for the slices that
will consume layout changes and should not be read as a wired-up feature.

Two defects were found by reviewing the slice rather than by running it, and
both are fixed on this branch.

The first: tying station gameplay to its placement consequence put `FindStation`
on the per-frame focus trace, and `FindStation` had simultaneously become an
allocating call, building its stable ID with `FString::Printf` on every
invocation. The dough visual is throttled to four times a second with a comment
saying it exists to keep `FindStation` off the per-frame path; the focus trace
then walked straight onto it. The identity is now interned once per station type
and shared by registration and every availability query, so the two also cannot
drift apart.

The second: neither packaging script could run at all. Both died with
`Result: Failed (OtherCompilationError)` and AutomationTool exit 6 — which reads
as a broken compile — before compiling anything. The real cause is further up the
UBT log: the Live Coding mutex is named after the engine's editor executable
rather than after a project, so an editor open on *any* project on this install
refuses every packaging build. `BuildEditor.ps1` has passed
`-NoHotReloadFromIDE` since it was written, which is exactly why the editor build
in the same validation run succeeded while packaging did not. Both packaging
paths now pass it too. The first failed run is not counted as validation.

Still manual: build-mode and layout usability, whether fixtures are comfortably
reachable, crate readability while moving, gamepad navigation, focus
readability, and a full played day. Rectangular use rectangles are floor policy
and prove nothing about Unreal AI navigation; that remains Stage 3.4. Placement
persistence is Stage 3.5 and shop identity is Stage 3.6, and neither is
implemented here.

## 2026-08-02 — the validation run that reported a green suite it had not built

The first `ValidateAll.ps1` run in the canonical checkout, after consolidation,
produced this summary:

```
static   PASS
harness  PASS
paths    PASS
build    FAIL - build exit 6
tests    PASS
data     PASS
```

`tests PASS` was true and meaningless. The build had failed on `LNK1104`,
unable to write `UnrealEditor-CigkofteSimulator.dll` because the editor open on
that same project had it loaded. The automation suite then ran against the
module already on disk — dated five days earlier, from before Stages 3.2 and 3.3
were merged — and 93 of 93 tests in that binary passed. The count is the only
visible symptom: this branch has 139.

`data PASS` followed the same stale binary. Its staleness guard only checks that
the log postdates the run, which it did.

The script now skips the test stage when the build stage did not pass, the same
way the data stage already skipped when the tests did not run. The overall exit
code was already 1, so nothing was silently shipped; what was wrong is that a
reader of the summary would have recorded a passing suite.

This is the third time this project has recorded a check that answered from
stale output — the packaged archive that predated its own fix by 23 minutes, the
runtime data log that could be answered by any earlier editor session, and now
this. The pattern is the same each time: the artefact exists, so the check finds
one, and nothing in it says which run produced it.

Two things this run also settled:

- **Building the canonical project needs its editor closed.** `-NoHotReloadFromIDE`
  gets past the Live Coding mutex, which is what blocked packaging from an
  unrelated checkout, but it cannot make Windows release a DLL a running editor
  has loaded. Different problem, different fix.
- **`Plugins/Sentry` is not inert.** A plugin in `Plugins/` is enabled by default
  whether or not the `.uproject` names it, and this one has no
  `"EnabledByDefault": false`. It compiles into the editor. Nothing ships — no
  Game target links it — but the local editor is not the editor a fresh clone
  builds. Recorded in `docs/Integration/WORKTREE_INVENTORY.md`.

### The same run, with the editor closed

| Check | Result |
|---|---|
| Static source rules | clean — 139 automation tests, 24 systems, 893 bilingual keys |
| Harness assertions | 49/49, 7/7, 7/7 |
| Editor build | **PASS** |
| Full automation | **139 passed, 0 failed** |
| Runtime data load | 14/14 balance files, 0 warnings |

This is the first time the suite has run against the canonical checkout with the
editor plugins enabled, and the first time it has run there at all since the
consolidation. The cook audit differs from the D: runs in the way it should:
canonical holds the licensed packs, so all 35 asset directories answered 33
rules with nothing skipped, where the public D: checkout had 8 packs absent and
fell back.

The count is the point. The same command against the same source reported 93 an
hour earlier because the build had failed and left the previous module in place.
139 is what the suite says when the binary is the one the run produced.

## 2026-08-04 — the package stops being a record of every earlier package

Branch `fix/exclude-unapproved-local-sentry`. Two faults, found in that order,
both of which made the packaged game something a clean clone does not build.

### Found first: the GameFeatureData dialog had been accepted

`Config/DefaultGame.ini` was modified in the working tree, uncommitted, with a
`PrimaryAssetTypesToScan` entry for `GameFeatureData` and the whole
`[/Script/Engine.AssetManagerSettings]` section the editor writes with it. This
is exactly the change `check_sources.py` was taught to reject in PR #16, and it
did:

```
Config/DefaultGame.ini:154: /Script/GameFeatures paketlenen oyunda yok, ama bir
Primary Asset Type onu taban sınıf olarak bildiriyor.
```

Reverted, not committed. The guard works; the dialog will keep appearing, and
the answer stays no.

### Root cause: nothing enabled Sentry, and that is the mechanism

`Sentry.uplugin` has no `EnabledByDefault`. In UE 5.8,
`FPlugin::IsEnabledByDefault` (`Engine/Source/Runtime/Projects/Private/PluginManager.cpp`)
ends:

```cpp
else
{
    return GetLoadedFrom() == EPluginLoadedFrom::Project;
}
```

Being under the project's `Plugins/` **is** the decision. UnrealBuildTool agrees
and synthesises a reference for it (`UEBuildTarget.cs`, "synthesize references
for plugins which are enabled by default"). Both process project references
first and skip a name already claimed, which is why `"Enabled": false` in the
`.uproject` is the fix — and
`FPluginReferenceDescriptor::IsEnabledForTarget` returns false for a disabled
reference before the plugin is ever looked up, so the entry is also safe in a
checkout that does not have the plugin.

### Found second: the archive directory is never cleared

With the fix in, the new stray-artefact check still failed:

```
yabanci dosya  FAIL - crashpad_handler.exe, crashpad_wer.dll
```

The staging manifests that run wrote (`Manifest_UFSFiles_Win64.txt`,
`Manifest_NonUFSFiles_Win64.txt`, `Manifest_DebugFiles_Win64.txt`) contain no
match for `Sentry` or `crashpad`, and the string does not appear anywhere in the
build, cook or stage output. UAT copies into `-archivedirectory` and removes
nothing, so files an earlier build staged survive every later package. Copied
files keep their source timestamps, so the leftovers did not look old either.

Both entry points now clear the output directory first, behind a guard that
refuses the repository root, anything containing it, a drive root, and any
non-empty directory that does not look like UAT output.

### Gates

| Check | Command | Result |
|---|---|---|
| Static source rules | `python Tools/check_sources.py` | clean — 153 automation tests, 25 systems, 893 bilingual keys |
| Release self-test state machine | `Test-SelfTestState.ps1` | **49/49** |
| Packaged cook policy | `Test-SmokeCookPolicy.ps1` | **7/7** |
| Cook plugin exclusion | `Test-CigCookPluginExclusion.ps1` | **18/18** |
| Local plugin policy | `Test-CigLocalPlugins.ps1` | **28/28** (new) |
| Script path safety | `Test-CigPathHelpers.ps1` | **13/13** (was 7/7) |
| Editor build | `ValidateAll.ps1` | **PASS** |
| Full automation | `ValidateAll.ps1` | **153 passed, 0 failed** |
| Runtime data load | `ValidateAll.ps1` | 14/14 balance files, 0 warnings |

`ValidateAll` ran at `01ff825`. Nothing in C++ or content changed after it; the
later commit is PowerShell, covered by the focused tests above.

### Packages

Both built from the tree now at `1d776d9`, with the editor closed.

| | Development | Shipping |
|---|---|---|
| Command | `PackageDemo.ps1 -Configuration Development` | `Package-Windows.ps1 -Configuration Shipping` |
| Path | `Build/WindowsDemo` | `Build/Windows-Shipping` |
| Files | 74 (was 77) | 49 (was 51) |
| Bytes | 2,339,920,566 (was 2,349,021,333) | 1,797,793,166 (was 1,799,629,611) |
| Runtime EXE | `CigkofteSimulator/Binaries/Win64/CigkofteSimulator.exe` | `CigkofteSimulator-Win64-Shipping.exe` |
| SHA-256 | `4C87837D93B19CDE882764D106D063E18E86DCFA9B80DDA6F023FD1EBFEE535F` | `257E6716512B3F3217B1383AFAD5D336E99000A154EA4421D917942E22EDE4D2` |
| PDB files | 1 | **0** |
| Sentry / crashpad files | **0** (was 2) | **0** (was 2) |
| Editor tooling files | **0** | **0** |
| Smoke test | **9/9** | **4/4** (Shipping has no log) |
| Mandatory self-test | **passed** | **passed** |
| `Verify-Release` | not run (Development is not a release artefact) | **PASS** |

Both runtime hashes differ from the PR #18 evidence, which is the expected
consequence: the Sentry module is no longer linked into the monolithic
executable. The Development PDB is the pre-existing asymmetry —
`PackageDemo.ps1` does not pass `-nodebuginfo`.

### Still needs a human

Nobody has launched either package by hand this session. The smoke test starts
the game with `-nullrhi -nosound` and reads its log; it says nothing about
rendering, input, language switching or loading a save.

## 2026-08-04 — pedestrians stop walking in the traffic

Branch `feat/ambient-pedestrian-containment`, runtime commit `e5b7821`.

### What the numbers said

`BuildCity` spawns the street from literals, and ambient pedestrians were handed
their wander box from different literals in the same function. Put side by side:

| | X range |
|---|---|
| Carriageway | **-2150 … -1450** |
| Car lanes | -1950, -1650 |
| West pavement | -2350 … -2150 |
| East pavement | -1450 … -1250 |
| **Old wander box** | **-2250 … -1450** |

The wander box was the road. Eight pedestrians walked in both lanes of moving
traffic for the whole game.

They also walked through the trees and lamp posts. The sweep in
`ACigkofteCustomer::StepTowards` is not at fault: `UCigWorldBuilder::SpawnProp`
declares `bCollision = false` and both furniture call sites take the default, so
there is nothing there to sweep against. Buildings do have collision — and a
pedestrian that hit one pressed into it for the rest of the session, because the
repath on a blocked step is guarded by `!bAmbient`.

### What was done

`FCigPedRegion`. A lane per pavement, derived from the same `CigStreet` constants
the geometry is spawned from. A lane graph rather than a grid or splines because
the street is straight, the furniture stands on known lines, and there are eight
pedestrians; anything more general would model a city nobody has surveyed.

The clamp is applied to the **position** after the step, not to the target. A
pedestrian aimed inside a 60 cm lane can still finish a long frame outside it, so
containment has to survive the step rather than only constrain the intent.

Districts keep one rectangle each. They get containment and blocked-step
recovery; they do not get a route, and `KNOWN_LIMITATIONS.md` says so.

### Gates

| Check | Command | Result |
|---|---|---|
| Static source rules | `python Tools/check_sources.py` | clean — 165 automation tests, 25 systems |
| Pedestrian automation | `RunUnrealTests.ps1 -TestFilter Cigkofte.Pedestrian` | **12 passed, 0 failed** |
| Full automation | `ValidateAll.ps1` | **165 passed, 0 failed** (153 before) |
| Editor build | `ValidateAll.ps1` | **PASS** |
| Runtime data load | `ValidateAll.ps1` | 14/14 balance files, 0 warnings |
| Harness assertions | `ValidateAll.ps1` | 49/49, 7/7, 18/18, 32/32, 13/13 |

The first build attempt failed `LNK1104` on
`UnrealEditor-CigkofteSimulator.dll`. Compilation had already succeeded; the
editor was open and holding the DLL. Closed it and rebuilt. Recorded because the
error reads like a code fault and is not one.

### Packages

| | Development | Shipping |
|---|---|---|
| Command | `PackageDemo.ps1 -Configuration Development` | `Package-Windows.ps1 -Configuration Shipping` |
| Files | 71 | 49 |
| Bytes | 2,340,039,155 | 1,797,797,774 |
| Runtime EXE | `CigkofteSimulator.exe` | `CigkofteSimulator-Win64-Shipping.exe` |
| SHA-256 | `17F1C3CF233AB2A5A75C9F8CD33BED35CBD7AF06A888EF961BECEDD6ECBDECCC` | `65389E82B0AB02B1AD697B610219A2A7A433E8C7D2BEB23194012D3A977FE8E1` |
| PDB files | 1 | **0** |
| Sentry / crashpad / editor tooling | **0** | **0** |
| Smoke test | **9/9** | **4/4** |
| Mandatory self-test | **passed** | **passed** |
| `Verify-Release` | not run | **PASS** |

The Development file count moved from 74 to 71 without any content changing, and
the reason is worth writing down rather than reporting as a saving: 7 of the
files in that tree are written by the smoke test itself — the self-test report,
two logs, `GameUserSettings.ini`, and a `CrashReportClient.ini` under a directory
named after the run. The number of those directories depends on how many times
the harness launched the game, so a file count measured after a smoke test is
run-dependent by a few files and should not be read as a content difference.

### Still needs a human

Nobody has watched a pedestrian walk. The tests prove the region's geometry and
that a position cannot leave its lane; whether the result *looks* like people
using a pavement — spacing, turning at the ends, walking through each other —
is a thing to see, not to assert.

## 2026-08-04 — the Recast experiment, and what it could not reach

Branch `spike/runtime-recast-navmesh`. The decision and its evidence are in
`docs/Architecture/NAVIGATION_AUTHORITY.md`; this records the runs.

`ACigRecastProbe` was built behind `-CigNavExperiment`, spawned from one guarded
line in `InitGame`, and removed again before this branch's PR. Production
navigation was not touched at any point: `UCigNavSystem`, `FCigNavGrid` and
customer path following are identical before and after.

### Runs

| | Editor `-game` | Packaged Development | Packaged Shipping |
|---|---|---|---|
| Navigation system present | yes | yes | yes |
| Invoker registered | yes | yes | yes |
| **Nav data instances** | **0** | **0** | **0** |
| **`ARecastNavMesh` created** | **no** | **no** | **no** |
| Path queries succeeding | 0 of 5 | 0 of 5 | 0 of 5 |
| Probe exit status | 13 | 13 | 13 |

Three configurations were tried before stopping, which is the limit this project
sets for one hypothesis family: `RuntimeGeneration=Dynamic` on the derived class,
the same on the declaring base class, and `bWholeWorldNavigable=True`. None
changed the result, and the third one explains the other two —
`bWholeWorldNavigable`'s `UPROPERTY(config)` is commented out in
`NavigationSystem.h:370` with the note *"currently broken"*.

The probe reports `build_*_ms` figures of 0.86–7.77 ms per mutation. **These are
not rebuild times.** With no navigation data registered the first poll returns
immediately; quoting them as measurements of Recast would be inventing evidence
for a system that was never created.

### Gates after the experiment was removed

| Check | Result |
|---|---|
| `python Tools/check_sources.py` | clean — 165 automation tests, 25 systems |
| `Test-SelfTestState.ps1` | 49/49 |
| `Test-SmokeCookPolicy.ps1` | 7/7 |
| `Test-CigCookPluginExclusion.ps1` | 18/18 |
| `Test-CigLocalPlugins.ps1` | 32/32 |
| `Test-CigPathHelpers.ps1` | 13/13 |
| Editor build | PASS |
| Full automation | **165 passed, 0 failed** |
| Runtime data load | 14/14, 0 warnings |

### Packages, rebuilt from the cleaned tree

| | Development | Shipping |
|---|---|---|
| Files | 71 | 49 |
| Bytes | 2,340,039,155 | 1,797,797,774 |
| SHA-256 | `1FAC8FAB2D95AB4C53771AA0D3EFE1D55F810B3B4B960E709C7DEDF15711D8F4` | `65389E82B0AB02B1AD697B610219A2A7A433E8C7D2BEB23194012D3A977FE8E1` |
| PDB | 1 | 0 |
| Sentry / crashpad / editor tooling | 0 | 0 |
| Smoke test | 9/9 | 4/4 |
| Mandatory self-test | passed | passed |
| `Verify-Release` | — | PASS |

The Shipping hash is byte-identical to the one recorded before the experiment,
which is the cleanest evidence available that the branch leaves the shipped game
unchanged. The Development hash differs from its own earlier value with identical
source; Development is not built reproducibly here and never has been, so that
difference says nothing either way.

### Still needs a human

Nobody played either package. The experiment queried paths; it moved nobody.

## 2026-08-04 — Stage 3.5: the shop layout persists

Branch `feat/stage3-placement-persistence`, from master `44bd304`. Design and the
audit that shaped it: `docs/Architecture/PLACEMENT_PERSISTENCE.md`.

### What the audit found before any code

Three things, and two of them changed the plan:

1. **There is no player-facing build mode.** `MoveExisting` and `BuildMode` appear
   in the placement layer and its tests and nowhere else. So this stage is the
   machinery; its visible effect arrives with build mode. Said before writing a
   schema justified by a feature that does not exist.
2. **Records and meshes were not connected.** Nothing mapped a `StableId` to an
   actor, so restoring a moved table would have moved the record — and navigation,
   validation and seat capacity with it — while the mesh stayed put. That became
   step 1 rather than a follow-up.
3. **A record is self-describing**, which is what chose a full snapshot over a
   delta: no definition table is needed to rebuild one, so there is exactly one
   failure mode per record instead of tombstones and merge rules.

### Defects found by the tests written for them

- `fixture.seating.sofa` had been a registered placement since Stage 3.3 with no
  actor attached to it. Found on the first run of the "every installed record has
  something to look at" invariant: 26 of 27.
- Re-registering the saved records into the live authority refused an **unchanged**
  layout with `BlocksEntrance`. The authored service counter stands in the entrance
  by design, which world registration allows and a move does not. The swap became
  an assignment.
- That assignment then dropped delivery crates, because it replaces every record
  including the transient ones the file does not carry. A defect this branch
  introduced and its own transient tests caught.
- Two unity-build name collisions, one of them pre-existing and exposed by the new
  files shifting the grouping.

### Gates

| Check | Result |
|---|---|
| `python Tools/check_sources.py` | clean — 203 automation tests, 25 systems |
| `Test-SelfTestState.ps1` | 49/49 |
| `Test-SmokeCookPolicy.ps1` | 7/7 |
| `Test-CigCookPluginExclusion.ps1` | 18/18 |
| `Test-CigLocalPlugins.ps1` | 32/32 |
| `Test-CigPathHelpers.ps1` | 13/13 |
| Editor build | PASS |
| Full automation | **203 passed, 0 failed** (170 at the start of this stage) |
| Runtime data load | 14/14, 0 warnings |

New automation: 14 `Cigkofte.LayoutApply`, 9 `Cigkofte.LayoutLoad`, 6
`Cigkofte.SavePlacement`, 5 `Cigkofte.PlacementVisual`, 4 `Cigkofte.SaveMigration`.

### Packages

| | Development | Shipping |
|---|---|---|
| Files | 71 | 49 |
| Bytes | 2,341,076,195 | 1,797,823,934 |
| Runtime EXE SHA-256 | `88D8D091515C2E365E4F42F31A668FFEFB57DF2CAA15C3D1D617BB4BF898600B` | `ECE10158C29777FA25F4CB7A5895A3AE27BEBA057B4BAE1089ED05FD9BB46628` |
| PDB files | 1 | **0** |
| Sentry / crashpad / editor tooling | **0** | **0** |
| Smoke test | 9/9 | 4/4 |
| Mandatory self-test | **passed** | **passed** |
| `Verify-Release` | — | **PASS** |

**The packaged round trip is real rather than inferred.** The existing
`kayit-turu` self-test check — capture, apply, capture, compare — was extended to
compare the layout as well: both flags set, the same number of placements, and the
same stable IDs in the same order at the same positions. It reports `PASS` in both
packages, which is the only evidence available for a Shipping build with no log
and no console.

Extended rather than added as a twelfth check, deliberately: the packaged harness
contract in `CigCommon.ps1` and its fixtures in `Test-SelfTestState.ps1` pin the
check list, and moving it would have meant changing the thing that verifies
packages in the same commit as the thing being verified.

### Still needs a human

Nobody has moved a table. There is no build mode to move one with, so the layout
this stage persists is the authored default in every run so far — the move cases
are exercised by editing the captured layout in a test rather than by playing.
That is the honest limit of what has been shown.

## 2026-08-05 — Stage 3.6: the shop has a name

Branch `feat/stage3-shop-identity`, from master `1638031`.

### What was there before

A literal in the middle of the world builder:
`SpawnWorldText(..., TEXT("CIGKOFTECI"), ...)` on the shopfront board — not routed
through `CigText`, owned by nothing, saved by nothing. `YeniTabela` turned out to
be an upgrade that changes the customer arrival rate, and "signage" elsewhere in
the source means station labels. There was no shop name to find.

### Decisions worth stating

- **A name is not translated.** If a Turkish player names their shop and switches
  the interface to English, the board must still say what they called it. So the
  default is a fixed string rather than a text key, and the language setting has
  no opinion about it. `CigText` is deliberately not involved.
- **Inner spacing is left alone.** `Cig  Kofte` is a name somebody chose;
  collapsing it is rewriting it. Only the ends are trimmed, because a leading
  space is a slip.
- **The character fault is reported ahead of the length fault.** A name that is
  both too long and contains a pasted newline would otherwise send the player to
  shorten something whose real problem is the paste.
- **Version 14 needs no companion flag, unlike 13.** An empty layout array is a
  state a player can reach, so it needed one. An empty name is not — the rules
  refuse it — so empty means "never named" and nothing else.
- **The migration clears rather than fills.** Writing the default in would turn
  "never named" into "named it that", and a later change to the default would
  leave old shops carrying a name nobody chose. The name is stored raw on both
  sides for the same reason.

### Gates

| Check | Result |
|---|---|
| `python Tools/check_sources.py` | clean — 213 automation tests, 25 systems |
| Harness assertions | 49/49, 7/7, 18/18, 32/32, 13/13 |
| Editor build | PASS |
| Full automation | **213 passed, 0 failed** (203 before) |
| Runtime data load | 14/14, 0 warnings |

New: 10 `Cigkofte.ShopIdentity` — 6 pure name rules, 4 through a real shop and a
real save. Turkish names are in the accepted list explicitly: if
`ÇİĞKÖFTECİ ŞÜKRÜ` were refused the shop could not be named in its own language.

### Packages

| | Development | Shipping |
|---|---|---|
| Files | 71 | 49 |
| Bytes | 2,341,132,940 | 1,797,829,477 |
| Runtime EXE SHA-256 | `95CC1F6E00221CA19E0112AC23C31DE657D7B94C1C7EF4306FB837F67FF5C675` | `1DC3F8CEF56EC55C2263A361D2B8D2D246B9A6F27ADA8553F25577D9ACA6D021` |
| PDB files | 1 | **0** |
| Sentry / crashpad / editor tooling | **0** | **0** |
| Smoke test | 9/9 | 4/4 |
| `kayit-surumu` | **PASS** (1 → 14, target 14) | **PASS** (1 → 14, target 14) |
| `kayit-turu` | **PASS** | **PASS** |
| `Verify-Release` | — | **PASS** |

The migration chain is exercised end to end inside both packages: a version 1 save
reaches 14 through every step. `kayit-turu` still carries the layout round trip
added in Stage 3.5.

### The gap this stage does not close

**The player cannot rename the shop.** Nothing in this project calls
`SetInputMode`, so the game is always in game input — including while the tablet
is open — and an editable field would take no keystrokes. It would look usable and
do nothing, which is worse than not having one. The rename path exists and is
tested; the missing piece is an input mode to type into, which changes how the
tablet takes input, affects movement and look, and cannot be verified without
playing.

This is the same shape as the Stage 3.5 gap: the machinery is there and the way
for a player to reach it is not. Both close with the same piece of work — a build
and edit mode the player can actually drive.

### Still needs a human

Nobody has seen the board. The tests prove the name is validated, stored, restored
and put on a `UTextRenderComponent`; whether it reads well at 90 units on a
shopfront across a street is a thing to look at.

## 2026-08-05 — the keyboard changes hands

Branch `feat/build-mode`, from master `2c65deb`. Plan:
`docs/Architecture/BUILD_MODE.md`.

### Why this came before build mode itself

Two finished stages have the same gap: Stage 3.5 persists a layout nobody can
rearrange, Stage 3.6 validates a name nobody can type. Both are machinery with no
way for a player to reach it, and both need the same thing first.

### The finding that shaped it

Input here is polled, not bound. `CigInput` asks the controller for raw key state
every tick, and `PC->WasInputKeyJustPressed` keeps returning true whether or not a
widget has focus. A text field added without a gate gives a rename where typing
"W" also walks the player forward and every digit fires a tablet shortcut. That is
why nothing in this project had ever called `SetInputMode` — and why it could not
simply be called now.

The gate already existed in the right shape: `bTabletOpen` swallows world
interaction the same way. `ECigInputScope` names the idea and adds the layer above
it, with text entry outranking the tablet because the field is hosted by it.

### Gates

| Check | Result |
|---|---|
| `python Tools/check_sources.py` | clean — 215 automation tests, 25 systems |
| Harness assertions | 49/49, 7/7, 18/18, 32/32, 13/13 |
| Editor build | PASS |
| Full automation | **215 passed, 0 failed** (213 before) |
| Runtime data load | 14/14, 0 warnings |

New: 2 `Cigkofte.Input.Scope` — the precedence, and that every combination of the
two flags resolves to exactly one layer. The second guards against a future third
modal state being added as another boolean with two true at once.

### Packages

| | Development | Shipping |
|---|---|---|
| Files | 71 | 49 |
| Bytes | 2,341,214,554 | 1,797,828,150 |
| Runtime EXE SHA-256 | `0D7FB30EE583D4C23C8CCD530E2D1D2B4CDCBF813CF707332466FDF6CA4B8640` | `42386BFC58895F4EBE375981B83A0153CC4DE4FADCEC6FDC7E0962782580A1CD` |
| PDB files | 1 | **0** |
| Sentry / crashpad / editor tooling | **0** | **0** |
| Smoke test | 9/9 | 4/4 |
| Mandatory self-test | **passed** | **passed** |
| `Verify-Release` | — | **PASS** |

### What the automation does not cover, in plain terms

This is the first change in the project to call `SetInputMode`, and none of the
things that make it right or wrong are checked by anything above:

- that F2 on the shop tab actually moves focus to the field;
- that typing does not walk the player or fire tablet shortcuts;
- that Enter commits, returns input to the game and hides the cursor again;
- that the mouse ends up where a player expects.

The packages start, self-test and pass their smoke checks, which says the change
does not break startup. It says nothing about whether the feature works. Somebody
has to press the keys, and until they do this is unverified rather than done.

Three steps: open the tablet, go to the shop tab, press F2; type a name and watch
whether the character moves; press Enter and check the board, the cursor and WASD.

### Correction: the gate was in the wrong place

The first version of this branch put the gate in the middle of `PollInput`, below
the mouse delta and below the WASD block. The decision it made was right and the
place it was applied was wrong, so typing a shop name still turned the camera and
walked the player — the exact thing it exists to stop.

The entry above originally listed that as unverified. It was not unverified, it
was wrong, and reading the function was enough to see it. "Not verified" implied
somebody had looked and could not tell; nobody had looked.

`84af1c3` moves the gate to the top of `PollInput`, above look, movement and the
pause handling, and gives Escape to the field while the field has the keyboard —
otherwise leaving a half-typed name opens the pause menu behind it.

`CigInput::Scope`'s tests could not have caught this. They check what the gate
decides; nothing checked where the decision was applied. So `check_sources.py` now
reads `PollInput` and fails when the gate falls after `AddControllerYawInput` or
`AddMovementInput`. Both failure directions were exercised against the real file
before committing — removing the gate, and moving it back to where the bug was —
and each is reported separately.

The package table above was rebuilt from `84af1c3`. The hashes it carried before
were of a binary containing the defect, which would have made the evidence a
record of the wrong thing.
