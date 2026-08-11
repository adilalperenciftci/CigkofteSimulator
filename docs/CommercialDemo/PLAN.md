# Commercial demo — dependency-ordered plan

Ordered by dependency, not by calendar. A slice is only started once everything
it relies on is correct, because a feature built on a broken pipeline has to be
rebuilt rather than fixed.

## Stage 0 — correctness and shared pipelines

Nothing in later stages is worth building until sales, events, reviews and saves
are internally consistent. Stage 0 adds no player-facing feature by design.

| # | Slice | Depends on | State |
|---|---|---|---|
| 0.2 | Staff packaging keeps real dough spice | — | done |
| 0.3 | Complaint risk feeds the inspector roll | — | done |
| 0.5 | Every event ends through one lifecycle | — | done |
| 0.6 | Stable review IDs for the reply queue | — | done |
| 0.10 | Build/test/package scripts, honest CI | — | done |
| 0.1 | One authoritative sale pipeline | 0.2 | done |
| 0.4 | Unified bulk-order contracts | 0.1 | done |
| 0.7 | PriceScore from real per-product prices | 0.1 | done |
| 0.8 | Dialogue context uses the delivered wrap | 0.1 | done |
| 0.9 | Save migration tests | 0.1, 0.4 | done |

0.1 is sequenced after 0.2 deliberately: the spice defect lives in the staff sale
path, and fixing it first means the extraction has one correct behaviour to
preserve rather than two divergent ones to reconcile.

**Stage 0 is complete.** One item it surfaced is carried forward rather than
closed: there is still no way to stand up a `ACigkofteGameMode` with its systems
in a test, so anything whose behaviour only exists once the systems are wired
together — the sale parity test from 0.1, a save round-trip through
`CaptureSave`/`ApplySave` — is covered by static rules and reading rather than by
a test. That harness is the first item of Stage 1.

## M1 — first playable session, and what it found

Stage 0 was closed without anyone having played the game. M1 did: a PIE session
driven with real key events, and an end-to-end automation test that walks the
preparation chain through the live systems. See `QA.md` for the evidence.

Everything below was observed, not predicted. Nothing here is speculative, and
these come before 1.1.

| # | Severity | Defect | State |
|---|---|---|---|
| M1.1 | **Blocker** | Automation tests overwrote the player's save file | **fixed** |
| M1.2 | High | Dialogue language ignored the game's own language setting | **fixed** |
| M1.3 | Low | Sold-out stations and the recipe board are unreadable primitives | open |

**M1.1 — the suite ate a save.** `FCigTestShop` never loads the player's save and
that was mistaken for safety. It does not need to load one: a test shop is a real
GameMode on a real game instance, so `RequestSave` reaches the same slot the
player's game does, and `BroadcastDayStart` calls it. A headless run of the suite
replaced a real day-3 file (8691 bytes) with the test world's day 1 (5772),
confirmed by hashing the file before and after. Fixed with
`ACigkofteGameMode::bSaveDisabled`, set by `FCigTestShop` before anything can
broadcast, and held by `Cigkofte.DayFlow.TestShopNeverWritesThePlayersSave` —
verified by clearing the flag and watching that test fail, then restoring it.

**M1.2 — two different answers to "what language is this game in".** The UI read
`FCigRuntimeSettings::Language` through `CigText` while the dialogue provider
asked UE's culture (`FInternationalization::GetCurrentCulture`). On a machine
whose culture is English the shop spoke Turkish and the customers answered in
English — observed in one run as `Müşteri: 'Keep the change, that was lovely.'`
between Turkish HUD messages.

Fixing the source of truth turned out to be half of it. The canned mood lines and
the three trait-specific retorts were written straight into `TEXT()` in Turkish
only, against this project's own rule that player-facing text lives in
`Config/Text/Strings.csv`. So an English game would still have answered in
Turkish wherever the generated table has no bucket — which is 2379 of 2400 of
them. Nineteen keys moved to the table with both columns filled.

Held by `Cigkofte.Dialogue.CustomersSpeakTheGamesLanguage`, which covers both
paths out of `PickLine`: a bucket the seed table has, and a VIP bucket it does
not, which falls through to the pool. Verified by restoring the culture lookup
and watching the test fail. `check_sources.py` also gained a rule for keys held
in a table rather than written at the call site — the mood pools are arrays of
keys indexed at random, which the existing `CigText::Get(TEXT("..."))` scan could
not see. Verified by deleting one key from the CSV and getting the exact line
reported.

**M1.3 — the shop reads as grey boxes in the editor.** Stations, counters and
props render as primitives with floating labels. This is what Stage 1 and Stage 8
exist to replace and is not a regression — `0.11` proved the packaged build cooks
the mesh packs — so it is recorded as the baseline the tactile work starts from
rather than as a bug to fix first.

Not a game defect, recorded so it is not rediscovered: capturing the whole editor
window while PIE is running crashes the editor (D3D12/Slate, no game code in the
call stack) — capture the single window instead. And injected key events are
discrete presses, so walking to a station, which needs a key held across frames,
cannot be scripted from outside the game. Cover the hands-on chain with
automation until there is an in-game input hook.

## Stage 1 — tactile food preparation

The single most important stage for the sales pitch. Depends on Stage 0.1 only
in that the finished product must already be authoritative.

- 1.1 Mixture visual state driven from food state, cached material instances —
  **done.** The station was handed two numbers (fill, knead) and lerped between
  two hardcoded colours, so isot, mix quality and freshness were invisible on the
  counter while feeding the price, the customer's reaction and the review.
  `FCigDoughVisual` (Cooking/CigDoughVisual.h) is pure and testable: it takes the
  batch and returns a colour and a scale. The material instances were already
  created once in `Setup` rather than per frame, so what was actually missing was
  the write itself being skipped when nothing changed — `NearlyEqual` on the
  state and an equality check on the colour, which matters because the kneading
  station pushes its state on every stroke. Tests: `Cigkofte.DoughVisual`.
- 1.2 Ingredient pouring with procedural utensil motion — **done.** A scoop in
  the ingredient's colour, lifted from the tub on one sine with the tip on a
  later curve.
- 1.3 Kneading progression: pitch, cohesion, deformation, completion — **done.**
  Pitch already tracked progress; what was missing was the batch gathering as it
  is worked, and any sign at all of the rhythm window the gain depends on.
- 1.4 Chopping states and pooled fragments — **done.** A shrinking head and a
  fragment per stroke, from a pool, scattered from a fixed table.
- 1.5 Wrap assembly surface, data-driven topping placement — **done.**
  `Orders/CigToppingVisual.h`. Tests: `Cigkofte.ToppingVisual`.
- 1.6 Readable recoverable failures — **done.** `Cooking/CigMixDiagnosis.h` names
  the worst fault and whether adding can still fix it. Tests:
  `Cigkofte.MixDiagnosis`.
- 1.7 Deterministic tests for visual state derivation — **done.** Every
  derivation added in this stage is pure and tested; 74 → 81.

**Stage 1 is complete.** Three defects it turned up in existing code are recorded
in `STATE.md`; the loudest is that the isot target the HUD showed the player was
uninitialised stack memory.

## Stage 2 — opening, closing, physical inventory

Depends on Stage 1 for the preparation loop it wraps around.

- 2.1 Opening routine — **done.** A day starts in `ECigPhase::Opening`: the
  stations work, the clock does not run and the queue stays out. The player opens
  the shop at the service counter, which is the door during preparation and the
  hand-off during service. What limits preparation is energy, not a timer:
  kneading and chopping cost the same as they do at noon, so a morning spent
  preparing is a shift spent tired. Dough ages from the moment it is made, so
  preparing everything as early as possible is not free either.
- 2.4 Batch spoilage — **already there.** Freshness decays per recipe, the fridge
  slows it, and Stage 1.3 made it visible on the counter.
- 2.2 Physical stock — **done.** A delivery arrives as an `ACigStockCrate` that
  stands in the shop until the player unloads it; capacity is checked on unload as
  well as on order, perishables wilt while they wait, and anything still out at
  close is put away at the quality it has by then.
- 2.3 Storage rules — **done.** `CigStorage` gives the dry shelf a per-item limit
  and the fridge one shared pool, which is what makes the paid fridge upgrade a
  decision rather than a slower spoilage curve.
- 2.5 Closing routine — **done.** `ECigPhase::Closing` gives the day a tail: the
  queue stops, the clock stops, and cleaning and tidying have somewhere to happen
  before the books close.

**Stage 2 is complete.** 2.1 through 2.5 are done and covered by
`Cigkofte.Crate`, `Cigkofte.Storage` and `Cigkofte.DayFlow`.

## Stage 3 — shop customization and build mode

Depends on Stage 2 storage actors existing as placeable objects.

- 3.1 single placement authority — **done** on
  `feat/stage3-placement-authority` and merged through PR #7.
- 3.2 functional placement categories — **done** on
  `feat/stage3-placement-categories`. Records now separate semantic function
  (`Station`, `Seating`, `Storage`, `Decoration`) from runtime lifetime
  (`Installed`, `Transient`), validate context compatibility and keep both
  dimensions immutable while moving an existing stable ID. No persisted layout
  data was added, so save version 12 remains current.
- 3.3 layout consequences — **done.** The placement authority now derives and
  atomically stores category-specific physical/use geometry and functional
  capacity by stable ID. Register, move and remove update the embedded
  consequence without Tick/world scans; station, seating and transient crate
  gameplay query that authority. This is rectangular layout policy only, not a
  navmesh or persistence claim; save version 12 is unchanged.
- 3.4 real path/navigation validation — **done.** The stage started from a wrong
  premise: there was no navigation to validate. `Source` contained no
  `NavigationSystem`, no `AIController` and no navmesh, and customers were plain
  actors interpolating in a straight line at their target — through counters,
  tables and the shopfront wall. So 3.4 built the missing half rather than
  auditing it. `FCigNavGrid` rasterises the placement records and the shop shell
  into an occupancy grid inflated by the agent's own radius and runs A* over it;
  `UCigNavSystem` owns that grid, rebuilds it only when a placement actually
  changes, and audits the routes the shop must keep open. Customers now walk the
  routes it returns. The grid is checked against real engine collision with the
  player's own capsule, so it is measured rather than asserted. It is not a
  navmesh and does not claim to be — see `KNOWN_LIMITATIONS.md`.
- 3.5 persistence by stable ID — **done**, in three steps: what a placement looks
  like in the save file, loading a layout as one transaction that can be refused
  whole, and applying it to a shop that is already standing. Only authored inputs
  are written; the derived consequence is recomputed on load, so a save cannot
  smuggle in a geometry the authority would not have produced. Transient
  placements (crates) are deliberately not written, and a crate standing in the
  shop survives a load anyway. A refused layout leaves the authored default
  standing rather than a half-applied shop, and every refusal names itself — a
  malformed record, a duplicate stable ID, an overlap, an out-of-bounds record and
  a seat walled in by individually legal placements are five different answers
  rather than one. Covered by `Cigkofte.SavePlacement`, `Cigkofte.LayoutLoad`,
  `Cigkofte.LayoutApply` and `Cigkofte.BuildMode`, including repeated save/load
  not drifting the shop.
- 3.6 shop identity — **done.** The sign was a literal in the middle of the world
  builder, owned by nothing and saved by nothing. The shop now has a name it
  keeps. The one decision worth stating: a name is not translated, so a Turkish
  player who names their shop and switches to English still sees what they called
  it. Validation names its faults rather than lumping them, an unnamed shop is
  saved as unnamed rather than as its default, and a v13 save arrives unnamed
  rather than silently named. Covered by `Cigkofte.ShopIdentity`.

**Stage 3 is complete.** 3.1 through 3.6 are done. Save version is 15.

## Stage 4 — customer life and readability

Depends on Stage 3 for seating and pathing.

- 4.1 body language, 4.2 order ticket, 4.3 groups — done.
- 4.4 seating — **done.** The dine-in flow itself already existed; what did not
  was any test that ran a customer through it. The gap mattered because a chair
  is claimed in the world builder and only *viewed* by the customer system, so
  every way of losing one is silent: no error, no message, just a shop that
  seats fewer people as the day goes on. `Cigkofte.Seating` now drives the whole
  arc through the real `ServeFront` — serve, walk, sit, eat, leave — and pins the
  gate in the middle: the eating clock runs only once the guest has actually
  reached the chair, never while they are still crossing the room. The three
  takeaway branches (packed order, failed roll, full room) are each proven to
  leave no reservation behind, and the three ways a seated guest can be taken
  away from the table — stranded with no route, recycled into the pool, or left
  as a record whose customer is gone — each give the chair back. No production
  change was needed: the tests found no defect, which is the result rather than
  an absence of one. Nobody has watched the flow in a running game; see
  `KNOWN_LIMITATIONS.md`.

  What it did find was a defect in the *suite*. `Cigkofte.Queue` opened the shop
  and immediately counted the people in it, and `StartDay` rolls the day's event
  — one of which ("Okul Çıkışı") puts two customers straight into the queue. The
  RNG is seeded from the clock, so that roll lands on some runs and not others:
  `LeavingFromTheMiddleClosesTheGap` asked for four customers and found six,
  failing once in a full-suite run that had passed minutes earlier. The fix is
  `CigQueueTestsOpenEmptyShop`, which shows the lottery's arrivals out before the
  test's own people arrive. The assertions are unchanged — what changed is that
  they now start from a state the test chose rather than one the clock did.
- 4.5 regular arcs — **done.** Nine `Cigkofte.Regulars` acceptance tests now
  exercise the public production lifecycle rather than the extracted loyalty
  arithmetic: a good stranger serve can create one regular; low accuracy,
  existing identity and the 12-record cap cannot; a regular returns on a later
  day with the same identity, favourite order, visual seed and traits, but cannot
  visit twice on one day. The suite also pins the complete patience multiplier,
  angry-leave penalties and floor, actor-pool identity cleanup, and every regular
  field through a real `CaptureSave`/`ApplySave` round-trip.

  The regression pass exposed an existing production crash outside the regular
  arc. A party walkout can remove several companions while `UpdateSystem` is
  iterating the live queue, invalidating its next array index. The patience pass
  now walks a snapshot and skips customers already removed from the queue; a
  deterministic group test reproduces the old out-of-bounds failure and holds the
  fix. No regular-specific production change was needed. Nobody has watched a
  regular return in a running game; see `KNOWN_LIMITATIONS.md`.
- 4.6 queue fairness — **done**, shipped ahead of 4.5 in PR #42. The queue is a
  plain `TArray` and its entire promise is the array index: `FrontCustomer()` is
  `Queue[0]`, and every tick sends the customer at index i to `QueueSlot(i)`.
  Three different mechanisms reorder it — serving takes from the front, patience
  takes a `Remove(C)` from anywhere, and an unreachable customer is pulled out by
  `ReleaseCustomerOwnership` — so a hole or a shuffle in any one of them would
  show up as somebody at the counter watching the person behind them get served,
  with no error anywhere. Four `Cigkofte.Queue` tests hold it: nobody overtakes
  anybody when the front or the middle leaves, a customer leaving from the middle
  makes everyone behind step forward and no two people are ever sent to the same
  slot, the queue fills to `MaxQueue()` and never past it while still giving
  everyone their own position, and the patience clock runs only for customers who
  have actually arrived rather than for whoever is walking in.

  The one production change was a read-only `GetTargetForTest()` on the customer,
  because physical fairness is a claim about where people are standing and
  nothing else exposes it. The suite change that mattered is
  `CigQueueTestsOpenEmptyShop`: `StartDay` rolls the day's event, one of which
  ("Okul Çıkışı") puts two customers straight into the queue, and the RNG is
  seeded from the clock — so a test asking for four people found six on some runs
  and not others. The assertions did not move; what moved is that they now start
  from a state the test chose rather than one the clock did.

**Stage 4 is complete.** 4.1 through 4.6 are done and covered by
`Cigkofte.Groups`, `Cigkofte.Seating`, `Cigkofte.Regulars` and `Cigkofte.Queue`.

## Stage 5 — staff and automation

Explicitly gated behind 0.1. Expanding to three employees before the sale
pipeline is unified would triple the divergence.

## Stage 6 — business depth

Depends on 0.7 pricing and Stage 2 inventory batches.

## Stage 7 — events and streamable moments

Depends on 0.5 event lifecycle and Stage 1/3 for the visuals each event drives.

## Stage 8 — visual and audio polish

Depends on the systems being final enough that art is not thrown away.
Blocked on licensed assets — see ASSETS.md.

## Stage 9 — UI, accessibility, onboarding

Depends on Stage 1 and 4: the tutorial cannot teach an interaction that is still
changing.

## Stage 10 — Steam, demo, packaging

Last. Requires an App ID and Steamworks account access that this repository must
never contain.

## Co-op

Not started. Gated behind the demo QA checklist per the decision gate.
`COOP_FEASIBILITY.md` is written before any networking code.
