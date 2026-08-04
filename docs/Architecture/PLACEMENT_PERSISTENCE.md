# Stage 3.5: what the save audit found before any schema was written

Branch `feat/stage3-placement-persistence`, from master `44bd304`.

Status: **audit and model decision. No schema change has been made.** The save
version is still 12 and `UCigSaveGame` is untouched.

## What was inspected

`CigSaveGame.h`, `CigSaveSubsystem.h/.cpp` (the whole v1→v12 migration chain),
`CigPlacementTypes.h/.cpp`, `CigPlacementSystem.cpp`, `CigNavSystem.cpp`,
`CigWorldBuilder.h/.cpp` (`RegisterFixturePlacement`, `BuildSeatingArea`,
`SpawnProp`, `BuiltProps`, `Seats`), `CigkofteGameMode.cpp` (`InitGame`,
`CreateSystems`, `CaptureSave`/`ApplySave` call sites).

## Three findings that change the shape of this stage

### 1. There is no player-facing build mode yet

`ECigPlacementContext::MoveExisting` and `BuildMode` appear in exactly four files:
`CigPlacementTypes.h`, `CigPlacementTypes.cpp`, `CigPlacementSystem.cpp` and
`CigPlacementTests.cpp`. Nothing in `Player/`, `UI/` or any input path references
either.

So the brief — "persist player-authored installed layout state" — describes
something a player cannot currently author. That does not make the stage
pointless: deliveries already create transient placements at runtime, and build
mode will need this machinery the day it lands. It does mean the stage has to be
described honestly as **the persistence and reconstruction machinery**, whose
player-visible effect arrives with build mode, rather than as a feature players
will see.

### 2. Placement records and presentation actors are not connected

`RegisterFixturePlacement` puts a record in the authority. The table mesh is
spawned separately by `SpawnProp`, which appends to a flat `BuiltProps` array.
There is no `StableId → AActor` mapping anywhere.

Consequence: restoring a moved table from a save would restore the record — and
therefore navigation, validation and seat capacity — while the mesh stayed where
the world builder put it. The record and the thing the player looks at would
disagree, which is worse than not persisting at all.

**Presentation reconstruction is therefore part of this stage, not a follow-up.**
It is also the largest single piece of work in it, and it was not visible from the
brief.

### 3. A record is self-describing, which decides the persistence model

`FCigPlacementRecord` carries `StableId`, `Category`, `Lifetime`, `Transform`,
`Footprint` and `UseSpec`. Every one of those is an authored input. The only
derived member is `Consequence`, which `FCigPlacementConsequencePolicy::Derive`
recomputes from the rest.

This means a saved record needs **no definition table to be reconstructed**. It
can be re-registered exactly as it is, and the consequence is derived on the way
in — which is the same path a live registration takes, so there is no second code
path that could drift.

## Model decision

Three were compared, as required.

| | A. Full installed snapshot | B. Delta from authored default | C. Player records + default fallback |
|---|---|---|---|
| Reconstruction | total and deterministic | needs the default to be byte-stable | needs both, plus merge rules |
| Authored default changes under a save | new fixtures never appear | new fixtures appear | new fixtures appear |
| Removed default fixture | absent from snapshot, nothing to do | needs a tombstone | needs a tombstone |
| Moved fixture | one record | delta entry | one record |
| Needs a definition table | **no** — records are self-describing | no | no |
| Save size | ~30 records | smaller | smaller |
| Failure modes | one: a record that will not re-register | several: tombstone vs missing, delta vs absent | most |
| Debugging | the file *is* the layout | the file is a diff against code | two sources to read |

**Selected: A, a full snapshot of `Installed` records.**

The deciding argument is finding 3 combined with the failure matrix. Because
records are self-describing, A needs no lookup and has exactly one failure mode
per record — it re-registers or it does not. B and C buy the ability for a content
update to introduce new default fixtures into existing saves, and pay for it with
tombstones and merge rules, in a game whose shop is about thirty fixtures.

The cost of A is stated rather than hidden: **a fixture added to the authored
default layout in a later patch will not appear in a save written before it.**
That is acceptable now and is the trigger for revisiting this decision.

## Version decision

**The save version must go to 13.** Not because a field is added — a new
`TArray<FCigSavePlacement>` would read back empty on a v12 save and be harmless —
but because an empty array is ambiguous between two states that must not be
confused:

- a v12 save, written before layout was persisted, whose layout is *unknown* and
  must fall back to the authored default; and
- a v13 save of a shop whose installed layout is genuinely empty.

Without the version bump the loader cannot tell "no layout recorded" from "no
layout". `MigrateV12ToV13` will be a real step: it records that the save predates
layout persistence, so load takes the authored default and does not treat the
absence as a player decision.

## What must not be persisted

Derived state, per the audit: `FCigPlacementConsequence` (recomputed by policy),
grid cells, `LayoutRevision`, customer paths, actor pointers, seat reservations,
queue slots, and `Transient` placements. Delivery crates are `Transient` and
belong to the delivery system, whose own state already persists; a crate written
into the layout would be duplicated on load, once by the delivery state and once
by the layout.

## Transactional load, as designed

Read to value types → validate outer format → version → migrate → validate stable
IDs and reject duplicates → validate enums → validate finite transforms → build a
candidate authority → derive consequences → validate overlap → validate required
routes through `UCigNavSystem` → **only then** replace the authoritative layout →
reconstruct presentation actors → invalidate navigation **once** → publish one
bounded completion notification.

A failed transaction keeps the previous layout and the previous save. Half the
default layout and half the saved layout is the state this ordering exists to
make impossible.

## Plan, in dependency order

1. `StableId → AActor` mapping in `UCigWorldBuilder`, with tests. Nothing else can
   be honest without it (finding 2).
2. `FCigSavePlacement` value type plus capture from the authority. Pure, tested
   against fixtures.
3. Transactional load with the failure matrix, rejecting the whole candidate.
4. Version 13 and `MigrateV12ToV13`, with a v12 fixture proving the fallback.
5. Presentation reconstruction driven by the mapping from step 1.
6. Bulk-load event boundary: one navigation invalidation, one station cache
   rebuild, one seating capacity rebuild.
7. Transient rules: crates excluded from layout, delivery state still authoritative.
8. Round-trip, mutation, rejection and idempotence tests.
9. Packages, both configurations, and a packaged round trip.

Step 1 is the next task and is where this branch resumes.
