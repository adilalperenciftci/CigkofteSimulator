# State

Resume point for the commercial-demo overhaul. Update before any interruption.

| | |
|---|---|
| Branch | `feat/commercial-demo-overhaul` |
| Base | `4f94f37` (master, not rewritten) |
| Latest commit | see `git log -1` on the branch |
| Save version | **12** (was 11 at baseline) |

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
- **0.5 One event lifecycle.** `OnDayEnd` and `OnDayStart` both cleared `Active`
  directly, so a day-long event (`Sure < 0`, never retired by `UpdateSystem`)
  vanished without running `EndEvent` or showing its end message. Both now route
  through `TumOlaylariBitir`, which returns the retired count so the teardown is
  observable rather than indistinguishable from emptying the array.
  Test: `Cigkofte.Events.DayLongEventsEndCleanly`.
- **0.6 Stable review IDs.** The pending social reply was an index into a list
  that inserts at the front, so a second poor review on the same day silently
  redirected it. `FCigReview` gained a monotonic `Id`, `PushReview` assigns it and
  became the public entry point, and the social system holds the ID and resolves
  it through `YorumBul`. Save version 12 with `MigrateV11ToV12`; apply also parks
  the counter past the highest surviving ID so an edited save cannot duplicate one.
  Tests: `Cigkofte.Reviews.IdSurvivesNewerReviews`,
  `Cigkofte.Reviews.IdsAreUniqueAndTrimSafe`.

## Current task

None in progress.

## Next exact task

**0.1 — one authoritative sale pipeline.** The player path
(`UCigCustomerSystem::ServeFront`) computes price, quality, combo, tip,
reputation, loyalty, review recording, quest and achievement progress inline. The
staff path (`UCigStaffSystem::DoWork`, `ECigStaffTask::Kasa`) awards money and a
message without most of it. Extract one `ProcessSale(const FCigSaleRequest&)`
returning `FCigSaleResult`, with an `ECigSaleSource` of Player / Staff / Delivery
/ BulkOrder / DebugTest, and route both callers plus delivery through it. Prove
with a test that an identical wrap sold by player and staff produces identical
global effects apart from documented source modifiers.

Then in order: 0.4 contracts, 0.7 price score, 0.8 dialogue context,
0.9 migration tests, 0.10 build scripts.

## Last successful build

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  CigkofteSimulatorEditor Win64 Development `
  -project="<repo>\CigkofteSimulator.uproject" -WaitMutex
```

Result: Succeeded.

## Last test result

`Automation RunTests Cigkofte` — **55 passed, 0 failed**, exit code 0.

## Blockers

- Ambience audio, replacement car/cat sounds and any licensed art are blocked on
  assets that cannot be authored here. See `ASSETS.md`.
- Steam App ID and Steamworks credentials are required for Stage 10 and must never
  be committed. Not needed until then.
