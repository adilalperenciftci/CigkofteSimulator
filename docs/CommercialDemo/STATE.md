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

- **0.8 Dialogue context describes the food, not the order.** Half the dialogue
  table was unreachable. See below.

- **0.9 Save migration tests.** The chain is walked from every version. See below.

**Stage 0 is complete.** All ten slices are done and every defect listed in
`BASELINE.md` is closed.

- **GameMode test harness.** `FCigTestShop` stands up a real shop in a throwaway
  world, and the two defects that had no runtime test now have one. See below.

- **0.11 Everything loaded by path is cooked.** The packaged demo was shipping a
  shop of grey boxes and a box cat. Nineteen cook directories, a static rule that
  the code and the cook list agree, and the first positive check in the packaging
  script. See below.

## Current task

**None in flight.** The packaged demo is verified end to end; see below.

## Next exact task

**Stage 1.2** — ingredient pouring with procedural utensil motion.

1.1 is done: the batch on the counter now derives its colour and size from what
it actually is (`Cooking/CigDoughVisual.h`), and the station skips the material
write when nothing visible changed. M1.1 and M1.2 are closed; M1.3 (the shop
renders as primitives) is what Stage 1 and Stage 8 exist to replace rather than
a bug to fix first. See `PLAN.md`.

One thing 1.1 corrected about its own brief: the material instances were already
created once in `Setup`, not per frame. The cost that was really there was the
unconditional `SetVectorParameterValue` on a station that updates every stroke.

## What the packaging run proved

```
.\Scripts\PackageDemo.ps1 -Configuration Development
```

`BUILD SUCCESSFUL` in 3m 4s, 924.2 MB archived, and all four smoke checks green:
`metin tablosu`, `denge verisi`, `ses varliklari`, `olumcul hata`. The staged log
has 14 of 14 `Denge dosyası uygulandı`, zero `Ses bulunamadı` and zero
`LogCig: Error`. Both halves of `4af66e0` do what they claim.

The previous entry said the proof run "died on a file lock, not on the fix". That
was wrong, and the archive sitting in `Build/WindowsDemo` was the evidence: it was
built at 00:41 and `4af66e0` was committed at 01:04, so it predated the half of
the fix that matters here. `DefaultGame.ini` has only two commits, and that one
added the `../Config/*` staging lines *and* the `DirectoriesToAlwaysCook` lines
together. The old archive had the first and not the second, which is exactly the
result it showed. Nothing had regressed; the artefact was simply older than the
fix, and re-running was always going to settle it.

Worth keeping: the container listing is the real evidence, not the smoke output.

```
UnrealPak.exe <...>\CigkofteSimulator-Windows.utoc -List
```

The old archive had **zero** `/Game` assets in it — 492 cooked uassets, all
Engine, and its only map was `/Engine/Maps/Entry`. The new one has all 19
`/Game/Audio` assets and 64 from `/Game/LowPoly`. That is a direct reading of
what the cooker did; the smoke check only reports that nothing complained.

## 0.11 Everything loaded by path is cooked, and a check that says so

The run above passed its four checks and produced 47 `Mesh bulunamadi` warnings
while doing it — the audio defect again, one layer down. `CigMeshLibrary` resolves
meshes by path exactly as `CigAudioSubsystem` resolves sounds, and
`DirectoriesToAlwaysCook` named `/Game/Audio` and `/Game/LowPoly` and stopped.
The library returns nullptr, the caller falls back to a primitive, and the shop
renders as grey boxes with no error logged anywhere.

Three things were wrong, and only the first was known when this started.

**The mesh packs.** Nineteen directories are now listed, named per requested
subfolder rather than per pack. That is not tidiness: `Scene_Bazaar_Vol1` is
6.1 GB on disk against the ten folders the game asks for, so cooking pack roots
would have added several gigabytes of props nothing spawns. `dukkan/Geometries`
is flat and goes in whole at 290 MB. The package went from 924 MB to 1836.7 MB.

**The cat.** `ACigCat::TrySetupSkeletalCat` loads a skeletal mesh and three
animations by path and returns early when they are absent, leaving the primitive
cat standing there. No warning, no error, nothing in the log at all — the quietest
version of this defect in the project, and nobody had noticed the packaged demo
shipped a box cat. The static check found it, not a person.

**The market stalls.** Found by the first version of the static check being
wrong. It parsed call sites, which is precise and sees only arguments written at
the call; `BuildBazaar` keeps its produce in a static `FBazaarGood[]` table and
passes `G.Folder`, so six stalls' worth of goods stayed uncooked while the check
reported all clear. It now also searches the source for any string that names a
real subfolder of a pack the loaders use, which does not care how the literal
reaches the loader. Its failure direction is a folder cooked for nothing rather
than a prop missing from the shipped game.

### What now holds it

`check_sources.py` gained `check_cooked_assets`. Every `/Game` folder the code can
reach must be listed in `DirectoriesToAlwaysCook`. Verified in both directions:
deleting the `dukkan/Geometries` line reports `CigMeshLibrary.h:24`, and a cook
entry pointing at a folder that is not there is reported as cooking nothing.

The existence half of that is asserted per pack, only where the pack root is
checked out. Twelve packs are deliberately not in the repository — they carry
uassets over GitHub's 100 MB limit — so a check that requires them on disk is
true on the machine that has them and false everywhere else. The first version
required them unconditionally: clean locally, fourteen failures on CI, which is
the wrong direction for a check whose whole purpose is catching what a local run
cannot see. What travels is the code-against-ini comparison, and that still
fails on a missing cook line for a pack this checkout does not have. The summary
line names the skipped packs rather than counting them, so a machine that is
supposed to have one finds out that it does not.

`PackageDemo.ps1` gained two checks. One reads the log for `Mesh bulunamadi`. The
other is the important one, because it is the first positive assertion in that
script: it runs `UnrealPak -List` against the cooked container and counts what
each declared directory actually produced. Every other check there is the absence
of a complaint, and two of them are the absence of a complaint from a lazy
loader — `ResolveSound` and `CigMesh` both load on first use, so a 25-second
headless run that never triggers a sound cannot fail the audio check. That is not
hypothetical: it is why `ses varliklari` passed against a build with no `/Game`
asset in it whatsoever.

Still not covered: a mesh *name* that does not exist inside a folder that is
correctly cooked. The log check catches it only for props the smoke run happens
to spawn. Worth doing if a third instance of this family shows up.

## The test harness, and what it closed

`ACigkofteGameMode::CreateSystems` was split out of `InitGame`. The two halves
were always different jobs: one builds the rules, the other builds the world
those rules are played in — meshes, the car, the save file, the widgets. Only
the second needs a map, so a test can now have the whole rule set without one.
`FCigTestShop` (Tests/CigTestShop.h) does that in about forty lines, and
deliberately does not call `InitGame`, which would load the player's real save.

Four tests moved off "verified by reading":

- `StaffSaleCountsLikeAPlayerSale` — the 0.1 defect directly. An apprentice's
  sale must move `TotalServed`, the till and the day's tally.
- `BothSourcesPriceTheSameWrap` — the same wrap is worth the same money whoever
  hands it over, with the combo as the one documented difference.
- `StaffSalesAdvanceABulkOrder` — the end-to-end version, and the one that cost
  the player money: accept a contract, serve it entirely with staff, and it must
  settle paid in full.
- `SaveRoundTripKeepsTheShop` — `CaptureSave` then `ApplySave` over a wrecked
  live state. Migration tests prove an old file reaches the current schema; they
  say nothing about whether that schema can carry the shop.

Verified the way the others were: the staff-sale defect was re-injected into
`SatisiIsle`, and exactly the two tests aimed at it failed, then passed again on
restore.

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

## What 0.8 changed

`RequestServeDialogue` filled the context from the order the customer asked for
and copied the delivered side across from it — the comment even said so:
`bGotAyran = C->Spec.bWantsAyran`. So every service looked, to the dialogue
system, like a service that went right.

That is not just a wrong prompt. The bucket key's a-flag is
`bWantedAyran == bGotAyran`, so it was pinned to 1, and the 1200 buckets written
for a wrong order could never be addressed. The evidence is in the seed table:
all fourteen of its buckets were `a1`. Nobody had written a single line for a
wrong order, because no runtime state could reach one.

The context now carries `RequestedSpice` alongside `ServedSpice` and takes the
delivered wrap as a parameter, so `OrderMatched()` is a real comparison.
`MistakeSummary()` names what went wrong and the AI prompt carries it: an
accuracy percentage says the order was wrong without saying what was wrong, so
the line came back generically disappointed. Fourteen `a0` seed lines were
written so the newly reachable half is not empty.

`check_sources.py` now fails if the dialogue table covers only one side of the
a-flag. A table written entirely against one value of a flag is the symptom of a
runtime that can only produce that value, which is exactly how this went
unnoticed. Verified by deleting the `a0` rows.

Not fixed here: the bucket key has one bit for "order correct", so the offline
table cannot distinguish a missing ayran from wrong spice. Widening the key
would multiply the 2400 buckets and invalidate the generated table; the AI
prompt does distinguish them. Recorded in `KNOWN_LIMITATIONS.md`.

## What 0.9 changed

`MigrateSave` is a row of independent `if`s and then an unconditional
`SaveVersion = CurrentVersion`. A missing link therefore does not fail to
compile, does not throw, and does not even leave the version wrong — the save
simply arrives half-converted and is stamped current. This project has already
shipped one version-stamp bug of that family.

Four tests now walk it. The load-bearing one starts a save at every version from
1 to 12 and checks two things: that the money, day, level, reputation and serve
count a player already had survive untouched, and that `RuhsatBitisGunu` equals
`Day + 14` for every pre-v10 start. That second assertion is the one that
actually detects a broken chain, because the licence date is the only field any
conversion writes to a value no default produces — checking the version stamp
alone would pass against a completely gutted `MigrateSave`.

The others cover: migrating a current save changes nothing (the version-stamp
bug seen from the other side), v11 reviews come out with unique newest-first IDs
and a counter parked past them with the stale pending reply dropped, and the
guards hold — a pre-licence shop does not open already fined, a corrupt UI scale
comes back inside the legible range, and a pre-pricing save returns to list
price rather than being silently repriced.

`MigrateSave` was made public. The justification is the same as `PushReview` in
0.6: it is the entire schema contract, it takes a save and nothing else, and the
thing that decides whether a year-old file still opens should be checkable
directly rather than only through a subsystem that needs a game around it.

Verified by deleting the `MigrateV9ToV10` call from the chain: 2 of the 4 tests
failed, and restoring it returned 65 of 65.

## Corrected from the previous entry

The packaged demo was recorded as unproven because the verifying run "died on a
file lock". It had not: the archive on disk predated the fix by 23 minutes. See
"What the packaging run proved". The lesson is narrow and worth keeping — an
archive is dated, the commit that was supposed to change it is dated, and
comparing the two costs one command and would have saved re-deriving the whole
diagnosis.

## Last successful build

```
.\Scripts\ValidateAll.ps1
```

Result: static PASS, build PASS, tests PASS, data PASS, package SKIPPED.

```
.\Scripts\PackageDemo.ps1 -Configuration Development
```

Result: BUILD SUCCESSFUL, 1836.7 MB, all six smoke checks green, 19 of 19 cook
directories producing assets, and a staged log with no `LogCig` warning of any
kind in it — down from 47.

Run against the committed plugin list. A locally enabled editor plugin whose
modules are `Runtime`/`Default` gets compiled into the packaged game, so if a
packaging run ever comes out unexpectedly large or slow, diff the plugin list in
`CigkofteSimulator.uproject` against the committed one first — and constrain
anything editor-only with `"TargetAllowList": ["Editor"]` rather than toggling it
by hand before each build.

## Last test result

`Automation RunTests Cigkofte` — **69 passed, 0 failed**, exit code 0.

Test groups added on this branch: `Cigkofte.Sale`, `Cigkofte.Reviews`,
`Cigkofte.SaveMigration`. Still missing from the commercial-demo test standard:
`FoodVisualState`, `InventoryBatches`, `Placement`, `Localization`,
`DataValidation` — all belong to stages not yet started.

## Blockers

- Replacement car/cat sounds and licensed art are blocked on assets that cannot be
  authored here. See `ASSETS.md`. Ambience is no longer blocked.
- Steam App ID and Steamworks credentials are required for Stage 10 and must never
  be committed. Not needed until then.

## Build script: what counts as success

`Scripts/BuildEditor.ps1` treats the build's exit code as the sole success
criterion and the log text as a second opinion. That split is deliberate and has
been got wrong twice here. An early version made the text the criterion with a
bare `-notmatch` against an array that is always truthy, so every build was
reported as failed; a later rewrite went the other way and dropped the text check
altogether, which cannot tell a clean build from a toolchain that crashed before
it could report anything.

`Test-BuildLog` now fails a build only when the exit code is 0 *and* the log says
`Result: Failed` or `BUILD FAILED`, and warns without failing when the success
stamp is simply absent — UBT does not always print one, particularly on a
`-clean` pass or an up-to-date target.
