# Navigation authority

**Decision: `FCigNavGrid` keeps placement policy and customer locomotion (option A).**
Dynamic Recast was measured and is not reachable in this project's architecture
without first giving the project an authored map. The grid is selected on
measurement, not by default.

Status: accepted. Branch `spike/runtime-recast-navmesh`. Supersedes the
"grid by default, unmeasured" position recorded in `KNOWN_LIMITATIONS.md`.

## Measured environment

| | |
|---|---|
| Engine | UE 5.8.0, `++UE5+Release-5.8-CL-55116800`, launcher install |
| Machine | Windows 11 26200, Intel i5-11400H, 6C/12T, 15.75 GB |
| Project | `CigkofteSimulator.uproject`, map `/Engine/Maps/Entry` |
| Base commit | `2fed3a6` (master at the time of the spike) |
| Probe | `ACigRecastProbe`, `-CigNavExperiment`, report written and process exits with the failure count |

Three environments, run separately:

1. Editor binary, `UnrealEditor-Cmd -game` (development aid only).
2. **Packaged Development**, `Build/WindowsDemo`.
3. **Packaged Shipping**, `Build/Windows-Shipping`.

PIE was deliberately not used as evidence. `UNavigationSystemV1::GetRuntimeGenerationType`
returns `Dynamic` unconditionally for any non-game world
(`NavigationSystem.cpp:5312`), so an editor world is dynamic whatever the
configuration says.

## Exact experiment configuration

```ini
[/Script/NavigationSystem.NavigationData]
RuntimeGeneration=Dynamic
[/Script/NavigationSystem.RecastNavMesh]
RuntimeGeneration=Dynamic
[/Script/NavigationSystem.NavigationSystemV1]
bGenerateNavigationOnlyAroundNavigationInvokers=True
bWholeWorldNavigable=True
```

Plus a `UNavigationInvokerComponent` registered at the shop centre with a
generation radius of 2040 cm, covering `CigNavLayout::NavBounds()`.

## Results

| Measurement | Editor `-game` | Packaged Development | Packaged Shipping |
|---|---|---|---|
| Navigation system present | yes | yes | yes |
| World type / is game world | 1 / yes | 1 / yes | 1 / yes |
| Active-tiles generation enabled | yes | yes | yes |
| Invoker registered | yes | yes | yes |
| **Navigation data instances** | **0** | **0** | **0** |
| **`ARecastNavMesh` created** | **no** | **no** | **no** |
| Runtime generation mode | n/a — no nav data to ask | n/a | n/a |
| Initial generation time | n/a | n/a | n/a |
| First successful query | **never** | **never** | **never** |
| `street_to_queue` | fail | fail | fail |
| `queue_to_service` | fail | fail | fail |
| `service_to_exit` | fail | fail | fail |
| `queue_to_seat` | fail | fail | fail |
| unreachable destination | fail (as expected, but for the wrong reason) | fail | fail |
| Average / worst query latency | n/a | n/a | n/a |
| Rebuild after table move / restore / remove | n/a | n/a | n/a |
| Rebuild after crate add / remove | n/a | n/a | n/a |
| Memory impact | none — nothing was created | none | none |
| Probe exit status | 13 | 13 | 13 |

The probe does report `build_*_ms` numbers between 0.86 and 7.77 ms for each
mutation. **They are not rebuild times and must not be quoted as such.** With no
navigation data registered, `IsNavigationBuildInProgress()` is false and
`GetNumRemainingBuildTasks()` is zero on the first poll, so those figures are the
cost of asking a navigation system with nothing in it whether it has finished.

## Why nothing was created

Read from the installed engine source, and confirmed by the runs above.

1. `UNavigationSystemV1::OnWorldInitDone` creates navigation data only inside the
   `else` of `if (IsThereAnywhereToBuildNavigation() == false)`
   (`NavigationSystem.cpp:1375`). The `true` branch does not merely skip creation —
   it unregisters and kills any navigation data that does exist.
2. `IsThereAnywhereToBuildNavigation()` (`NavigationSystem.cpp:2583`) returns true
   for exactly three things: `bWholeWorldNavigable`, a registered
   `FNavigationBounds`, or an `ANavMeshBoundsVolume` actor already present in the
   world.
3. `bWholeWorldNavigable` cannot be set. Its `UPROPERTY(config, EditAnywhere)` is
   commented out in `NavigationSystem.h:370`, above the comment
   *"removing it from edition since it's currently broken"*. The ini section above
   is inert, which is what the third run measured.
4. Registering bounds from game code is not public API. `AddNavigationBounds`,
   `AddNavigationBoundsUpdateRequest`, `RegisterNavData` and
   `SpawnMissingNavigationData` are all `protected` — each of those was a compile
   error in this experiment before it was rewritten around them. The engine's own
   `@TODO` beside `IsThereAnywhereToBuildNavigation` says as much: *"this should be
   made more flexible to be able to trigger this from game-specific code"*.
5. That leaves an `ANavMeshBoundsVolume` in the world at world-init time. This
   project has no map — `GameDefaultMap` is `/Engine/Maps/Entry` and
   `UCigWorldBuilder` spawns the shop, the street and every prop from
   `ACigkofteGameMode::InitGame`, which runs *after* world init. There is no point
   at which game code can put a volume in the world early enough, and a volume
   spawned at runtime has no brush to give it bounds.

So the blocker is not that Recast is slow, or unsuitable, or that dynamic
generation is unsupported. It is that **navigation bounds must exist before the
world this project builds exists.**

## Limitations of this experiment

- It does not show that Recast would perform badly. It shows it cannot be reached.
  Any statement about Recast's latency, rebuild cost or path quality in this shop
  would be invented, and none appears above.
- A navigation invoker was registered, and invokers do not satisfy
  `IsThereAnywhereToBuildNavigation`. A different arrangement — a
  `UNavigationSystemConfig` on world settings, a custom `UNavigationSystemV1`
  subclass with access to the protected API, or an authored map — was not tried.
  The first two are engine-level work; the third is the migration path below.
- Only Win64 was measured.
- Nobody watched a customer walk during the experiment. The probe queries paths;
  it does not move anybody.

## Option A — the grid keeps both

Placement reachability and customer locomotion stay with `FCigNavGrid`.

- **Determinism:** total. Pure value type, no `UWorld`, no actors, no collision
  scene; the placement suite depends on this.
- **Packaging:** already proven across Development and Shipping.
- **Rebuild cost:** lazy and synchronous. A burst of placement changes costs one
  rasterisation before the next query, not one per change.
- **Complexity:** already written and tested.
- **Debugging:** a rectangle list and a grid, inspectable from a test.
- **Save/load:** none. The grid is derived from placement records; nothing about
  it is persisted, which is what Stage 3.5 needs.
- **Testability:** high — this is where the current navigation tests live.
- **Procedural world:** native. It is built from the records the world builder
  registers.
- **Moving placements:** supported, and uniquely, *hypothetically* — see below.
- **Collision quality:** the grid is a model, and it is cross-checked against real
  engine collision with the player's capsule
  (`Cigkofte.Navigation.Collision.TheEngineAgreesWithTheGrid`).
- **Two authorities:** none.

## Option B — Recast takes both

**Not available.** Blocked before any of its merits could be measured, for the
reasons above. Adopting it would first require authoring a project map so a
`ANavMeshBoundsVolume` can exist at world init — which changes the project's
world-construction architecture, not its navigation.

Beyond the blocker, one property is worth recording because it would not go away
even if the blocker did: `UCigNavSystem::WouldCloseRequiredRoute` builds a grid
*with the candidate placement included* and searches it, synchronously, before the
placement is accepted. A navmesh cannot answer a hypothetical. The equivalent
would be to spawn the object, dirty the navmesh, wait for an asynchronous rebuild,
query, and then undo all of it — inside the placement validation that runs while
the player is dragging an object.

## Option C — grid owns placement policy, Recast owns locomotion

**Not available**, same blocker.

Recorded for the future because it is the option that looks most attractive and
carries the most risk: two systems describing the same shop. The measurement that
already exists says they would not agree. A seating fixture registers a
`160 × 280` footprint covering the table *and both chairs*, while the chairs are
spawned with `bCollision = false` (`UCigWorldBuilder::SpawnProp` defaults it to
false and the chair call site takes the default). Recast would see the table's
collision only. Customers would path between a table and its own chairs, and
placement would go on refusing layouts for a reason locomotion could not see.

If C is ever revisited, the collision authoring has to be fixed first, and the
authority rules have to be written before the code, not after.

## Selected model

**Option A.** `FCigNavGrid` is the single authority for:

- placement validity and required-route policy (`WouldCloseRequiredRoute`,
  `AuditRequiredRoutes`);
- customer locomotion paths (`FindCustomerPath`);
- standability queries for both agents.

Engine collision remains the authority for what a body may physically pass
through: `ACigkofteCustomer::StepTowards` sweeps against `ECC_WorldStatic` and the
player is a real `ACharacter` with a capsule. The grid answers *where a route is*;
collision answers *what stops a body*. Where they disagree, collision wins and the
customer repaths — that behaviour already exists and is unchanged by this decision.

Ambient street pedestrians are outside both: they use `FCigPedRegion`, which is
containment rather than a route. That distinction is deliberate and documented.

## Rejected alternatives

- **Dynamic Recast (B, C):** unreachable without an authored map. Measured in
  three environments.
- **Authoring a project map purely to enable a navmesh:** rejected for now. It
  would trade a working, deterministic, tested navigation model for one that
  cannot answer the hypothetical query placement validation is built on, and it
  would change world construction to solve a problem the grid does not have.
- **Subclassing `UNavigationSystemV1` to reach the protected API:** rejected as
  disproportionate. It is engine-adjacent work to obtain a system whose benefits
  over the grid have not been shown for a single-floor shop with no vertical
  traversal.

## Migration plan

None is executed. The conditions under which this decision should be reopened:

1. The project gains an authored map for another reason (streaming, lighting
   build, World Partition). At that point a `ANavMeshBoundsVolume` becomes
   available and option C becomes measurable for the first time.
2. The shop gains vertical traversal, multiple floors, or ramps — the one class of
   question a grid over a single plane genuinely cannot answer.
3. Placement stops needing a synchronous hypothetical, which would remove the
   strongest structural argument for the grid.

If reopened, the work is a **separate production branch from master**, not this
spike, and the collision authoring of chairs and street furniture is a prerequisite.

## Rollback plan

Nothing to roll back. The experiment is removed from this branch before the PR:
`ACigRecastProbe`, the `NavigationSystem` module dependency, the navigation
sections in `DefaultEngine.ini` and the guarded spawn in
`ACigkofteGameMode::InitGame` all go. Production navigation is untouched by this
branch — `UCigNavSystem`, `FCigNavGrid` and customer path following are the same
code before and after.

## Consequences

**For Stage 3.5 persistence.** Unchanged and unblocked. The grid is derived from
placement records and persists nothing of its own, so a save carries authoritative
placement inputs and the grid is rebuilt from them on load. Had Recast been
selected, load would have needed an asynchronous navmesh build to complete before
any route could be validated, and the transactional load in Stage 3.5 would have
had to wait on it. Stage 3.5 may now proceed.

**For customer movement.** Unchanged. Customers keep following `FCigNavGrid`
paths, keep repathing on `LayoutRevision` changes, and keep stopping and releasing
their seat and queue slot when no route exists.

**For placement validation.** Unchanged, and the reason it stays this way is now
recorded: the synchronous hypothetical grid in `WouldCloseRequiredRoute` is a
capability, not an implementation detail, and no navmesh-based design can offer it.
