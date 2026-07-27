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

Real input, not a simulated call path: `SlateInspectorToolset.Click` on the PIE
viewport then `PressKey` "Enter" selected **Devam Et** on the title screen, and
the log answered with `Gün 3 başladı`. Observed over one full 180-second day:

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

**What PIE could not do.** `SlateInspectorToolset.PressKey` presses and releases;
there is no hold. Walking to a station needs a held key, so the ingredient →
knead → wrap → serve chain could not be driven by hand. `E`, `W` and `1` were
sent and produced no station interaction, which is consistent with the player
standing away from every station. That chain is covered by the automation test
below instead.

**Tool defect, not a game defect.** `EditorToolset.EditorAppToolset.CaptureEditorImage`
crashed the editor during the first PIE attempt:
`Saved/Crashes/UECC-Windows-D92CDA0448752756ABAF288994E5CC84_0000`,
`EXCEPTION_ACCESS_VIOLATION`, call stack entirely D3D12RHI → SlateRHIRenderer →
RenderCore with no game module in it. The second session used
`SlateInspectorToolset.Screenshot` and completed without incident.

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

### MCP plugins and the packaged build

Measured, not assumed, with `Build.bat <target> -Mode=JsonExport`:

| Target | Before | After |
|---|---|---|
| Game/Development | 509 modules, incl. `ModelContextProtocol`, `ModelContextProtocolEngine` | 506, neither present |
| Game/Shipping | 467 modules, both present | 464, neither present |
| Editor/Development | 6 MCP modules + 26 toolset modules | unchanged |

`ModelContextProtocol.uplugin` declares those two modules `Runtime`/`Default`, so
they linked into the monolithic game executable, and the runtime module registers
`ModelContextProtocol.StartServer` as an `FAutoConsoleCommand` — an HTTP server
reachable from a shipped build's console. Auto-start is Editor-only
(`ModelContextProtocolEditor.cpp:64`), so nothing was listening by default.
`AllToolsets.uplugin` is `"EditorOnly": true` and never reached the game target;
it was left alone.

The fix is the engine's own field, not a removal:
`"TargetAllowList": ["Editor"]` on the plugin reference. Verified against the
local 5.8 schema — `PluginReferenceDescriptor.h:62`, applied by `IsEnabledForTarget`
in `PluginReferenceDescriptor.cpp:64-79`, read from JSON at line 161. The editor
MCP connection still works; this session ran through it afterwards.

### BuildEditor.ps1

The exit code is the sole success criterion and the log text is a second opinion.
`Test-BuildLog` fails a build only when the exit code is 0 *and* the log says
`Result: Failed` / `BUILD FAILED`; a missing `Result: Succeeded` warns and passes.
Verified three ways: a log with no stamp passes with a warning, a `Result: Failed`
log is caught, a normal log passes. Dry-run and a real editor build both exit 0.

## Not yet executed

Scenario tests required by the commercial-demo standard that do not exist yet:
power-cut refrigeration, closure blocking customers, placement round trip,
pooled seat release, localization placeholder parity.

A played session still cannot exercise the preparation stations by hand. Doing
that needs either held-key input simulation or an in-game automation hook.
