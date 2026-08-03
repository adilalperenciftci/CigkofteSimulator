# Dynamic Recast NavMesh: what the engine source says, before any measurement

**Status: research only. No experiment has been run and nothing has been
decided.** `NAVIGATION_AUTHORITY.md` does not exist yet and must not be written
from this file — it needs measurements, and this contains none.

Engine: UE 5.8.0 (`++UE5+Release-5.8-CL-55116800`), installed at
`C:\Program Files\Epic Games\UE_5.8`. Every claim below cites the file and line
it was read from, in that install.

## The constraint that shapes the question

The game has no map. `Config/DefaultEngine.ini` sets both `GameDefaultMap` and
`EditorStartupMap` to `/Engine/Maps/Entry`, and `UCigWorldBuilder` spawns the
shop, the street, the districts and every prop at runtime.

So there is no authored `ANavMeshBoundsVolume`, no `ARecastNavMesh` actor saved
in a level, and no serialized navigation data to cook. The usual answer — place a
bounds volume, build the navmesh in the editor, ship the tiles — is not available
without giving the project a map first. The question is therefore narrower than
"should we use a navmesh": it is whether one can be created and generated
entirely at runtime, in a map the project does not own, and whether that survives
cook and Shipping.

## What the source says is possible

**Navigation data is created without bounds, if and only if it is dynamic.**
`UNavigationSystemV1::SpawnMissingNavigationDataInLevel`
(`Runtime/NavigationSystem/Private/NavigationSystem.cpp:4636`):

```cpp
if (NavWorld->WorldType != EWorldType::Editor && NavDataCDO->GetRuntimeGenerationMode() == ERuntimeGenerationType::Static)
{
    // if we're not in the editor, and specified navigation class is configured
    // to be static, and there're no navigation bounds present, then we don't want to create an instance
    if (!IsThereAnywhereToBuildNavigation())
    {
        continue;
    }
}
```

The skip is guarded on `Static`. A nav data class configured for dynamic runtime
generation is spawned in a game world even with no navigation bounds present,
which is exactly this project's starting state.

**Auto-creation is on by default.** `BaseEngine.ini:3039` sets
`[/Script/NavigationSystem.NavigationSystemV1] bAutoCreateNavigationData=true`,
and `UNavigationSystemV1` calls `SpawnMissingNavigationData()` behind that flag
(`NavigationSystem.cpp:1408`). Nothing in this project overrides it.

**The geometry octree exists at runtime only when something needs it.**
`RequiresNavOctree()` (`NavigationSystem.cpp:5290`) returns true unconditionally
for non-game worlds, and for a game world only if some registered nav data
`SupportsRuntimeGeneration()`. The octree is what gathers collision geometry into
the navmesh build, so runtime generation and runtime geometry gathering stand or
fall together.

## The finding that decides how the experiment must be run

`UNavigationSystemV1::GetRuntimeGenerationType()` (`NavigationSystem.cpp:5312`):

```cpp
// We always use ERuntimeGenerationType::Dynamic in editor worlds
if (!World->IsGameWorld())
{
    return ERuntimeGenerationType::Dynamic;
}
```

and `IsNavigationBuilt` (`NavigationSystem.cpp:2568`) treats `GEditor != nullptr`
as equivalent to non-static generation.

**PIE is not evidence.** An editor world is dynamic whatever the configuration
says, so a navmesh that generates perfectly in PIE proves nothing about the
packaged game. Any measurement that is going to be believed has to come from a
Development package and be repeated in Shipping — the same rule the packaging
work on this project has already had to learn twice.

## What still has to be measured

Nothing below has been run. Listed so the experiment cannot quietly shrink to
the easy half.

- Whether an `ARecastNavMesh` is actually created in a packaged game with no
  authored bounds, or only in PIE.
- Whether runtime-spawned bounds (`ANavMeshBoundsVolume` at runtime, or
  `UNavigationSystemV1::AddNavigationBounds`) are honoured after the world is
  already built.
- Initial generation latency, and how much of it is on the game thread. The shop
  is built during startup; a stall there is a stall the player sees.
- Query latency for the routes that matter: pavement to service point, round the
  counter, to every seat, and out.
- Rebuild latency after a table moves, a table is removed, and a delivery crate
  appears and is taken away — the four mutations placement actually performs.
- Agreement with `FCigNavGrid` on the same start/goal pairs: not only whether
  both succeed, but path shape and length, because a route that succeeds and
  hugs a counter is a customer clipping a counter.
- Behaviour in Development **and** Shipping packages, separately.
- Determinism. The grid is deterministic and its tests depend on that; a navmesh
  built from async tile generation may not be, and the cost of losing it is the
  placement test suite.

## Options that will be compared

- **A.** `FCigNavGrid` keeps both placement reachability and customer locomotion.
  Costs nothing, changes nothing, and stays a private model of the world that the
  engine does not check.
- **B.** Recast takes both. Uses the engine's own geometry, and makes placement
  validation depend on an async build.
- **C.** Split: grid owns deterministic placement validation and required-route
  policy, Recast owns live locomotion.

C is the one that looks attractive and is the one most likely to produce two
systems disagreeing about the same shop, so the authority rules have to be
written before it is chosen, not after.

## Why nothing was decided here

The mission for this branch is to measure. The research above says the experiment
is worth running — the engine does not rule out runtime generation in a mapless
game — and says how it has to be run to mean anything. It does not say whether
the result is better than the grid, and this file must not be read as though it
did.
