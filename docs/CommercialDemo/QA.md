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
