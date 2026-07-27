# Baseline — commercial demo overhaul

Recorded before any change on `feat/commercial-demo-overhaul`.

| | |
|---|---|
| Base commit | `4f94f37` (master) |
| Branch created from | `master`, no history rewrite |
| Engine | Unreal Engine 5.8 — `C:\Program Files\Epic Games\UE_5.8` |
| Date | 2026-07-26 |

## Static checks

```
python Tools/check_sources.py
```

Result: clean.

- 57 headers scanned
- 13 balance CSVs validated
- 26 dialogue lines, 14 buckets
- 695 text keys, 2 languages
- text usage: keys and argument counts consistent
- decoupling: no publisher calls the quest system directly
- required repo files present

## Editor build

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  CigkofteSimulatorEditor Win64 Development `
  -project="<repo>\CigkofteSimulator.uproject" -WaitMutex
```

Result: **Succeeded**, no errors, no warnings other than the pre-existing engine
`APawn::GetMovementBase` deprecation.

## Automation tests

```
UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests Cigkofte;Quit" `
  -unattended -nullrhi -nopause -log
```

Result: **50 passed, 0 failed**, exit code 0.

Groups present at baseline:

`Cigkofte.Audio` `Cigkofte.Balance` `Cigkofte.Dialogue` `Cigkofte.EventBus`
`Cigkofte.Events` `Cigkofte.Input` `Cigkofte.Inspection` `Cigkofte.Orders`
`Cigkofte.Pricing` `Cigkofte.Random` `Cigkofte.Social` `Cigkofte.Staff`
`Cigkofte.Text` `Cigkofte.Tutorial`

Missing groups required by the commercial-demo test standard: `Sales`,
`BulkOrders`, `Reviews`, `FoodVisualState`, `InventoryBatches`, `Placement`,
`SaveMigration`, `Localization`, `DataValidation`.

## Runtime data load

All 13 balance CSVs load with the expected row counts and zero warnings
(no unknown keys, no invalid indices, no unreadable files).

## Known defects carried into this branch

Confirmed by inspection, not yet fixed:

1. Player and staff sales use different code paths and apply different rules.
2. Staff packaging reads dough spice after the last serving is consumed, so it
   falls back to medium.
3. `UCigInspectionSystem::DenetimRiskCarpani()` is never used by the inspector
   arrival roll.
4. Two overlapping bulk-order concepts (multi-day contract, `OzelTur == 5` event).
5. Day-long events (`Sure < 0`) are cleared at day end without running `EndEvent`,
   so their end message never shows.
6. The pending social reply is stored as an array index into a list that inserts
   at the front.
7. `PriceScore` still derives from the legacy global `PricePolicy`.
8. Dialogue context is filled from the requested order, not the delivered wrap.
9. Save version 11; no migration tests exist.
10. CI runs static checks only and does not build the engine target.
