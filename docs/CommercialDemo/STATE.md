# State

Resume point for the commercial-demo overhaul. Update before any interruption.

| | |
|---|---|
| Branch | `feat/stage3-layout-consequences` |
| Base | `e7372eb` (PR #8 merge on master) |
| Latest commit | see `git log -1` on the branch |
| Stage 3.1 PR | **#7**, merged as `c06650e` |
| Stage 3.2 PR | **#8**, merged as `e7372eb` |
| Current slice | **Stage 3.3 layout consequences** |
| Save version | **12** |

This table has gone stale twice, both times by naming a branch that had already
been merged. It is a resume point and nothing else: check it against
`git rev-parse --abbrev-ref HEAD` and `gh pr list` before trusting a word of it.
Everything below the table is a dated record of work already done, not a
statement about what is currently in flight.

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

## What this branch actually contains

The PR title says Stage 1.1, which was true when it was opened and is not any
more. What is on the branch:

- **1.1 Dough that looks like what it is.** `Cooking/CigDoughVisual.h` derives
  colour and size from fill, kneading, spice, quality and freshness; the station
  skips the material write when nothing visible changed. Corrected from its own
  brief: the material instances were already created once in `Setup`, not per
  frame. The real cost was the unconditional `SetVectorParameterValue`.
- **Shop asset pass.** Real market and bakery models on every station, with the
  primitive fallback kept for a checkout without the packs.
- **Wall materials.** Red tile and orange brick. The first attempt used white
  tile, which read as cool grey against cream walls.
- **Wrap assembly visuals.** Flatbread, filled, wrapped and packed states, with
  toppings as coloured spheres. Not the final look, but the player can see the
  order being built rather than a number changing.
- **Customer skeletal bodies and animation.** MC_Sample body and animation from
  the same pack, mannequin fallback, primitives below that.
- **Release, crash, performance, SteamPipe and asset-intake tooling.**

Bundling this much into one PR is not a defence of the practice; it is a record
of what a reviewer has to read.

## Current task

**Stage 3.3 layout consequences.** PR #8 is merged as `e7372eb` and this branch
starts from that exact merge. Placement now derives a *consequence* — a physical
rectangle, a separate category-specific use rectangle and a functional capacity —
by pure policy, and stores it inside the authoritative record. Embedding it is
the point: there is no second container that could keep a rectangle alive after
the record it belongs to moved or was removed. Register, move and remove are
atomic against that one structure, the stable-ID index is maintained with them,
and a move whose normalized value is unchanged reports no state change and
publishes no event.

Gameplay now reads that authority instead of its own copy of the layout. A
station is interactable only while its placement still owns a station
consequence, a seat can be reserved only within its table's authored capacity,
and a delivery crate's transient storage consequence is what makes it
unloadable — and is gone when the crate is unloaded or destroyed. This is
rectangular layout policy: it is not a navmesh claim and nothing about it is
persisted, so save version 12 did not change.

Local delivery gates are complete at runtime commit `8710588`: static checks,
49/49 self-test-state, 7/7 cook-policy and 7/7 path-safety assertions, the
Editor build, 40/40 placement automation, 139/139 full automation and 14/14
runtime balance files with zero warnings. Exact-head Development and Shipping
packages passed their mandatory release self-tests; `QA.md` records their sizes
and runtime executable hashes. Package output remains outside the repository.

Two defects were found by reviewing the slice rather than by running it, and are
recorded in `QA.md`: the station availability query had put `FindStation` back on
the per-frame path while making it allocate, and both packaging scripts could not
run at all while a UE editor was open on any project.

`UCigEventBus::PlacementChanged` is published but has no production subscriber
yet. It is groundwork for the slices that consume layout changes, and this
document should not be read as saying anything is listening.

## Stage 2.1 and 2.2, finished

- **2.1 A day with a shape.** `ECigPhase` splits the day into Intro, Opening,
  Playing, Closing, Summary and GameOver, and `CanWork()` is what the systems ask
  instead of `IsPlaying()`. Preparation and cleaning now have somewhere to happen
  without summoning customers into an empty shop.
- **2.1 Storage rules.** `CigStorage` gives the dry shelf a per-item limit and the
  fridge one shared pool, so the paid `BuyukBuzdolabi` upgrade has its first
  plannable effect and an order can be refused for want of room rather than money.
  Pinned against `Stock.csv` after the first capacity number - 40, chosen without
  reading the CSV - started the game two units over its own limit.
- **2.2 Physical stock.** A delivery arrives as an `ACigStockCrate` standing in the
  shop, labelled with what is in it, and becomes stock only when the player unloads
  it with E. Capacity is checked on unload as well as on order, so a part-load
  leaves the remainder on the crate rather than overflowing the fridge; perishables
  wilt visibly while they stand there, and whatever is still out at close is put
  away overnight at the quality it has by then. Stock no longer teleports.

Three positioning bugs, all found by looking rather than reasoning: the crate stood
at the world origin because the mesh was the root component and `Setup`'s relative
location *is* the actor location; the first row of spots was 44 degrees off the axis
a player at the counter looks down, so the delivery arrived outside the field of
view of the room it arrived in; and the label rendered inside the box, because a
child's relative location is multiplied by its parent's scale and the body is
scaled to 0.4 in Z - only the "9" of "Marul x9" poked out of the end.

## Stage 1, finished

- **1.1 Dough that looks like what it is.** See below.
- **1.2 Ingredient pouring.** A scoop in the ingredient's own colour lifts out of
  the tub, tips and drops back. No animation asset: one sine for the lift, a
  later curve for the tip, because a scoop that starts pouring on the way up is
  pouring back into the tub it came from.
- **1.3 Kneading progression.** Wet bulgur slumps and gathers into a ball as
  cohesion rises, holding its volume. The rhythm window - a stroke 0.25-0.85 s
  after the last is worth nearly twice a mistimed one - is finally visible: the
  dough answers the stroke it was given, and a wasted one still moves it about a
  third as far, because no movement at all reads as a dropped input.
- **1.4 Chopping states.** The board carries a head that shrinks and a fragment
  per stroke, swept when the garnish finishes. Pooled, and scattered from a fixed
  table so pieces accumulate rather than appearing to move.
- **1.5 Topping placement.** A table instead of index arithmetic: parsley is
  scattered flecks, tomato two slices, molasses a drizzle down the bread.
- **1.6 Readable recoverable failures.** The bowl now says what is wrong while it
  can still be fixed, and whether it still can. See below.
- **1.7 Deterministic tests.** Every derivation added here is pure and tested;
  74 → 81.

## The English that was not English

The game had a language setting that 46 strings ignored. They were written with
`LOCTEXT` while the rest of the project uses `CigText` and
`Config/Text/Strings.csv` - and no `.po` or `.locres` data has ever existed here,
so those strings were permanently Turkish in both languages while *looking*
localised, which is why nobody had noticed. Thirty-seven were in
`CigTabletData.cpp`. Another group never reached the text system at all: the
reputation title that sits under the money the whole time, the recipe and supplier
tables, the review pool, and every `SEVIYE N` sign in the world.

All of it now goes through the text table - 100 new keys - and
`Tools/check_sources.py` fails the build on a `LOCTEXT` anywhere in `Source`, so
it cannot come back. Recipes and suppliers use the same arrangement the balance
tables already had: a key per row, falling back to the table's Turkish literal, so
a forgotten translation degrades to Turkish rather than printing `recipe.gizli.name`
on the bowl.

Two real defects fell out of it, both invisible until the language was switched at
runtime:

- **A station's sign never changed.** `SetLocked` returned early when the lock had
  not moved, and the sign's wording is the one thing there built from a template -
  so switching to English left "SEVIYE 6" standing over a station in a shop whose
  HUD had already switched.
- **`ApplySettings` changed the language and nothing else.** The world's signage is
  written once, when a lock is applied, not rebuilt per frame. It now re-applies
  the current level, which rewrites the signs against the language just chosen.

What is deliberately still Turkish: customer names, rival shop names, the street
addresses, and the ingredient words on the station tubs. Isot and salça are what
the ingredients are called.

`CigShots` takes its pictures in English through the settings path a player would
use, rather than by setting the language directly - which is how the second defect
above was found rather than reasoned about.

## Shipping is verified now

The release configuration compiles out UE_LOG and disables the project cheat
manager: Shipping neither assigns nor creates it, and the self-test verifies that
the configured controller CDO (and a runtime controller when one exists) has no
cheat class or manager. `-CigReleaseSelfTest` runs eleven checks inside the game
after InitGame has built the world, writes a report to a path the harness
dictates, and exits with 0 or the index of the first failure. Shipping now
reports process liveness, cook coverage and a complete passing self-test without
depending on logs. The harness also fingerprints both normal save locations and
platform settings before and after the run, so the release test cannot silently
load, write or replace player state.

Two path defects were found by running it rather than by reading it. The report
first went to `FPaths::ProjectSavedDir()`, which redirects to `%LOCALAPPDATA%` in
Shipping, so a run that passed everything was reported as unsupported. And the
release scripts resolved a relative `-BuildDirectory` with
`[System.IO.Path]::GetFullPath`, which resolves against the .NET process directory
rather than the shell's - so they archived a different checkout's build. Both
fixed; `Resolve-CigPath` is the shared helper.

## Next exact task

**Publish Stage 3.2 for review.** Commit the recorded validation evidence, push
`feat/stage3-placement-categories` and open a focused pull request. Wait for CI,
inspect comments and review threads, and merge with a merge commit only when
every gate is green. After the merge, start Stage 3.3 layout consequences from
the new `origin/master`; do not add them to this branch.

Standing items that are not tasks in that chain:

- The preparation stations have never been driven by hand; see
  `KNOWN_LIMITATIONS.md`.
- Performance measurements describe a packaged **Development** build. The
  optimised build cannot be measured with a launcher-installed engine; see
  `PERFORMANCE_BUDGET.md`.

## What the last three slices found

Each of these was in the shipped build and none of them was found by playing.

- **The isot target was uninitialised memory.** `TargetCounts` fills the array by
  name and summed indices 0..3; the enum runs Bulgur, Isot, Salca, Su, Baharat,
  so it read the isot slot nothing had written and skipped baharat. The number
  the HUD showed came out different on every run - "İsot 0 / 6", "0 / 3", "0 / 2",
  "0 / 1" across four captures of the same recipe. Found by a diagnosis test
  failing on a mix that was on target by construction.
- **The counters were knee-high.** A four-metre counter mesh measured against a
  90cm cube fits at 0.22 scale. Height is now the binding measurement and the
  model may overhang its footprint.
- **The molasses drizzle ran off the bread.** Six pieces at 4.8 apart is a 24cm
  run and it started at -13. Caught by the placement test on the day it was
  written.

## Review fixes applied after the last push

Six defects, all found by reading the branch rather than by a test:

- **`AnimWalk` was overwritten unconditionally.** A leftover block after the
  `if (bMocap)` split reassigned the mannequin walk over the MC_Sample one, so a
  customer with an MC_Sample body would have tried to play a sequence from a
  different skeleton — the exact silent bind-pose failure the mesh/animation
  pairing was designed to avoid.
- **Toppings did not appear when added.** `ToggleTopping` changed the mask and
  said so in a message, but never pushed the visuals.
- **Dough did not visibly go stale.** Freshness decayed every frame; the station
  was told only when the batch spoiled outright, so it held its colour and then
  vanished. Now refreshed on a 0.25 s clock.
- **Finished dough was normalised by the wrong recipe.** `CurrentVisual` read
  `CurrentRecipe`, so changing the selection after kneading changed the colour of
  dough nobody had touched. It now reads `Dough.Recipe` when a batch exists.
- **`RayTracingMode=Full`.** Inherited from the template, never chosen, never
  measured. Now `Disabled`, with the reasoning in `DefaultEngine.ini`.
- **A machine-specific Android File Server token was committed.** The section is
  removed. It remains in git history, which is not being rewritten; treat the
  token as burned. It is regenerated by the editor and gates a USB-only debug
  service that was never shipped, so the exposure is small, but it should not
  have been there.

## What the packaging run proved

```
.\Scripts\PackageDemo.ps1 -Configuration Development
```

`BUILD SUCCESSFUL` in 3m 4s, 924.2 MB archived, and all four smoke checks green (2026-07-26; the check set and the package have both grown since — see the current figures at the end of this file):
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

> Superseded. As of the current branch the Development archive is **2282 MB** and
> Shipping is **1902 MB** (about **1673 MB** downloaded, symbols excluded), and
> the packaged check set is seven for Development and two for Shipping. The run
> recorded below is kept as history; `docs/PERFORMANCE_BUDGET.md` carries the
> current figures.

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

`Automation RunTests Cigkofte` — **93 passed, 0 failed**, exit code 0.

Test groups added on this branch: `Cigkofte.Sale`, `Cigkofte.Reviews`,
`Cigkofte.SaveMigration`, `Cigkofte.DoughVisual`, `Cigkofte.MixDiagnosis`,
`Cigkofte.ToppingVisual`, `Cigkofte.Storage`, `Cigkofte.Crate`,
`Cigkofte.Localization`. Still missing from the commercial-demo test standard:
`InventoryBatches` (Stage 2.3) and `DataValidation`.

`Cigkofte.Crate` covers the four cases a delivery has now that it is an object:
it fits, it half-fits and the crate keeps the rest, a second press against a full
shelf moves nothing, and nobody came to get it before closing. The fifth pins the
point of the feature — the timer running out must not move the numbers.

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
