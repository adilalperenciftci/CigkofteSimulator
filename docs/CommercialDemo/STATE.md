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
- **0.5 One event lifecycle.** `OnDayEnd` and `OnDayStart` both cleared `Active`
  directly, so a day-long event (`Sure < 0`, never retired by `UpdateSystem`)
  vanished without running `EndEvent` or showing its end message. Both now route
  through `TumOlaylariBitir`, which returns the retired count so the teardown is
  observable rather than indistinguishable from emptying the array.
  Test: `Cigkofte.Events.DayLongEventsEndCleanly`.

## Current task

None in progress.

## Next exact task

**0.6 — stable review IDs.** `UCigSocialSystem::YanitlanacakYorum` stores an index
into `UCigReviewSystem::Reviews`, and `PushReview` inserts at index 0. A second
review generated the same day therefore silently redirects the pending reply to a
different review. Add a monotonic `Id` to `FCigReview`, assign it in `PushReview`,
store the ID rather than the index, look the review up by ID, and cover it with a
test that generates several reviews in one day. Save version must rise because
`FCigSaveReview` gains the field.

Then in order: 0.1 unified sale pipeline, 0.4 contracts, 0.7 price score,
0.8 dialogue context, 0.9 migration tests, 0.10 build scripts.

## Last successful build

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  CigkofteSimulatorEditor Win64 Development `
  -project="<repo>\CigkofteSimulator.uproject" -WaitMutex
```

Result: Succeeded.

## Last test result

`Automation RunTests Cigkofte` — **53 passed, 0 failed**, exit code 0.

## Blockers

- Ambience audio, replacement car/cat sounds and any licensed art are blocked on
  assets that cannot be authored here. See `ASSETS.md`.
- Steam App ID and Steamworks credentials are required for Stage 10 and must never
  be committed. Not needed until then.
