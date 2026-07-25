# State

Resume point for the commercial-demo overhaul. Update before any interruption.

| | |
|---|---|
| Branch | `feat/commercial-demo-overhaul` |
| Base | `4f94f37` (master, not rewritten) |
| Latest commit | see `git log -1` on the branch |
| Save version | 11 (unchanged so far) |

## Completed vertical slices

- **Baseline** — build, static checks and tests recorded in `BASELINE.md`.
- **0.2 Staff packaging keeps real dough spice.** The caller read the spice after
  `UseServings` had emptied the batch, so the last package of every batch came out
  medium. Spice is now captured first and passed to `UCigStaffSystem::PaketHazirla`.
  Test: `Cigkofte.Staff.PackageKeepsDoughSpice`.
- **0.3 Complaint risk feeds the inspector roll.** `DenetimRiskCarpani()` existed
  but nothing used it, and the old roll guaranteed an inspector on even days.
  Replaced with one clamped probability (`DenetimSansi`) capped below certainty.
  Test: `Cigkofte.Inspection.ComplaintRaisesOddsWithoutGuaranteeing`.

## Current task

None in progress.

## Next exact task

**0.5 — one event lifecycle.** Events with `Sure < 0` are cleared by
`Active.Empty()` in `UCigEventSystem::OnDayEnd` without running `EndEvent`, so
their end message never shows and nothing confirms their modifiers stopped.
Route day-long events through the same `EndEvent` path, guarantee the end message
fires exactly once, and add a test that a day-long event ends cleanly.

Then in order: 0.6 stable review IDs, 0.1 unified sale pipeline, 0.4 contracts.

## Last successful build

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  CigkofteSimulatorEditor Win64 Development `
  -project="<repo>\CigkofteSimulator.uproject" -WaitMutex
```

Result: Succeeded.

## Last test result

`Automation RunTests Cigkofte` — **52 passed, 0 failed**, exit code 0.

## Blockers

- Ambience audio, replacement car/cat sounds and any licensed art are blocked on
  assets that cannot be authored here. See `ASSETS.md`.
- Steam App ID and Steamworks credentials are required for Stage 10 and must never
  be committed. Not needed until then.
