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
- **Ambience beds.** `S_AmbStreet`, `S_AmbNight` and `S_AmbRain` exist, loop, and
  resolve at the paths `ResolveOrtam` uses. All three derive from a CC BY 4.0 pack;
  see `CREDITS.md` and the `ASSETS.md` section on why the other two library packs
  could not ship.
- **0.10 One validation command.** `Scripts/ValidateAll.ps1` runs static checks,
  the editor build, the automation suite and a runtime data load, reporting each
  stage separately with packaging opt-in and visibly SKIPPED. Three bugs in the
  scripts were found by running them; see the commit message.
- **0.1 One authoritative sale pipeline.** `UCigSaleSystem::SatisiIsle` now prices
  and books every counter sale. See below for what changed and what it fixed.

- **0.4 Contracts split from large deliveries.** Two unrelated mechanics were
  both called "Toplu Sipariş" and both driven by one balance row. See below.

- **0.7 One effective price, and perception reads it.** The price stars, the
  goodwill bonus, patience and footfall all answered the cheap/normal/expensive
  toggle instead of the bill. See below.

## Current task

None in progress.

## Next exact task

**0.8 — dialogue context uses the delivered wrap.** `RequestServeDialogue` is
filled from the order that was asked for rather than the wrap that was handed
over, so a customer can thank the player for something they did not get.

Then: 0.9 migration tests, which also wants the GameMode test harness 0.1 left
outstanding.

## What 0.1 changed

Both sale paths now build a `FCigSatisTalebi` and call
`UCigSaleSystem::SatisiIsle`, which owns pricing, the combo, the tip, the money,
the statistics, hygiene wear, reputation, XP, the `Served` event and the review.
Callers keep only presentation and whatever is specific to who they are serving.

The staff path had priced every wrap at a hardcoded 55 lira, ignoring the price
list, the recipe, the policy, events, hygiene and skills. It also never touched
`TotalServed`, `TotalPerfectOrders`, reputation, XP, the `Served` event or the
review log — so an apprentice could serve customers all day while a bulk order
sat at zero progress and then failed, and while quests and achievements counted
nothing. Both paths are now the same code.

Deliveries and contract payouts share `GeliriKaydet` (balance, day tally,
best-day record) but keep their own scoring: a delivery has its own accuracy
model, its own review and its own progression counters, and pushing it through a
counter-sale pipeline behind opt-out flags would have made "one pipeline" a
label rather than a fact. Contract payouts additionally now count towards the
day and the best-day record, which they previously did not.

`check_sources.py` gained a rule that `RegisterSale(` may only be called from
`CigSaleSystem.cpp`. That is the invariant a fourth caller would break, and it
was verified by re-injecting the call into the staff system and confirming the
check reports it at the exact line.

The pure rules — how quality and accuracy scale the price, and where the combo
starts and stops — are covered by `Cigkofte.Sale.QualityAndAccuracyBothMatter`
and `Cigkofte.Sale.ComboRewardsStreaksAndStops`. The staff/bulk-order regression
itself is not covered by a runtime test: `SatisiIsle` needs a live GameMode with
a dozen systems attached, and there is no harness for that yet. The static rule
plus the fact that both paths now call one function is what holds it. A test
that stands up a GameMode belongs with 0.9.

## What 0.4 changed

The player could be shown "Toplu Sipariş" twice on one day for two unrelated
things: a three-day contract for N wraps, and a one-off delivery of two packages
to a door. Both read `MinGun` and `Sans` from the same `Events.csv` row and each
rolled it separately, so the pair appeared together more often than either was
tuned for and neither could be adjusted without moving the other.

The contract keeps the name and now has its own `Contracts.csv` with all twelve
numbers that were hardcoded in `CigEventSystem.cpp` — notice period, size, fee
per wrap, the near-miss threshold and the three reputation outcomes. The
delivery event is now `BuyukTeslimat` / "Büyük Teslimat" with its own text keys,
and the contract's messages moved from `msg.event.bulk.*` to `msg.contract.*`.

No save change: `FCigTopluSiparis` has the same fields.

`check_sources.py` gained a check that every key-based balance CSV's keys match
the defaults in `CigBalance.cpp`. A key that matches nothing is not an error to
the loader — the row simply never applies, so the file looks like it is tuning
the game while the game keeps the built-in number. Renaming a key in one place
and not the other is how that happens, which is what this slice risked.
Verified in both directions by re-injecting each fault.

## What 0.7 changed

Two knobs set the price: the per-product markup and the shop-wide
cheap/normal/expensive policy. The sale path multiplied by both, but everything
that *judged* the price read only the markup. Switching the shop to "expensive"
raised every bill by a quarter while demand, the reviews, the reputation bonus
and the price shown on the tablet all carried on as if nothing had changed.

`UCigPricingSystem::EtkinCarpan` is now the single effective markup, policy
included, and `Fiyat()` returns it — so the tablet shows what is charged. The
sale pipeline stopped applying the policy separately; the net price is
unchanged. `SokakOrani` measures that against the rival average and everything
downstream reads it:

- `PriceScore` comes from `FiyatPuani(SokakOrani)` instead of a three-value
  lookup on the policy setting. It could previously sit at 4.5 stars while the
  same review text called the shop expensive.
- The half-point goodwill bonus per sale now needs the shop to actually be
  priced at or below the street, not merely set to "cheap".
- Patience responds to whether the price reads as expensive or as good value.
- Footfall sees the policy at all, which it did not before.
- `FDayServeData::PricePolicy` was written every serve and never read; removed.

`check_sources.py` now enforces two single-caller rules rather than one:
`RegisterSale(` outside the sale system, and `PolicyPriceMult(` outside pricing.
The second is what would silently reintroduce this defect — either by
double-charging the policy or by reopening the gap between what is charged and
what is judged. Verified by re-injecting the duplicate multiply.

## Last successful build

```
.\Scripts\ValidateAll.ps1
```

Result: static PASS, build PASS, tests PASS, data PASS, package SKIPPED.

## Last test result

`Automation RunTests Cigkofte` — **60 passed, 0 failed**, exit code 0.

## Blockers

- Replacement car/cat sounds and licensed art are blocked on assets that cannot be
  authored here. See `ASSETS.md`. Ambience is no longer blocked.
- Steam App ID and Steamworks credentials are required for Stage 10 and must never
  be committed. Not needed until then.
