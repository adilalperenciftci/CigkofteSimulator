# Visual audit — where the game actually is

Read before planning any art work. Written against the running build on
2026-07-27, from the code that builds the world and from a PIE session, not from
memory.

The headline is not what the plan assumed. This project is **not** a box of
primitives waiting for assets. It already loads five third-party packs and
spawns real meshes for most of the environment. What is primitive is a short,
specific list, and it is the list the player looks at most.

## What is already real

`CigMeshLibrary` resolves meshes by path with a primitive fallback, and these
packs are installed on this machine:

| Pack | uassets | Used by |
|---|---|---|
| `CitySampleBuildings` | 10682 | street backdrop |
| `dukkan/Geometries` | 1507 | plates, bowls, glasses, greens, bread, tables |
| `CityPark` | 1010 | props, buildings, trees, park square |
| `MMSupermarket` | 657 | not referenced by code |
| `Scene_Banquet` | 621 | not referenced by code |
| `Scene_Bazaar_Vol1` | 547 | market stalls, produce, sacks, crates |
| `ModularBuildingSet` | 315 | not referenced by code |
| `Characters` | 145 | **not referenced by code** — UE5 Manny/Quinn |
| `LowPoly` (Kenney) | 64 | fridge, stove, sink, cabinets, food props, plants |
| `Cat_Animation_Pack` | 21 | the cat, when present |

`CigWorldBuilder` spawns kitchen furniture, plants, a radio, food containers, a
cutting board, trash cans and street dressing from these. That work is done and
should not be redone.

## What is still primitive

This is the actual gap, in the order the player notices it.

1. **Every station.** `ACigkofteStation::Setup` builds each one from
   `/Engine/BasicShapes` — a cube base, a cylinder top, a sphere for dough, and
   a floating `UTextRenderComponent` label. Bulgur, isot, salça, su, baharat,
   yoğurma, servis, lavabo, temizlik: twenty-two stations, all the same two
   shapes in different colours. The player spends the whole game looking at
   these, and the labels are the only thing telling them apart.
2. **Every customer.** `ACigkofteCustomer` is a cylinder body, a sphere head, two
   cylinder arms, a cone hat, a cube bag and two sphere glasses. No skeleton, no
   animation — they slide to the queue and slide away. There is no walk cycle, no
   idle, no reaction, no eating.
3. **The player character and the apprentice.** Same construction.
4. **The counter and the wrap being built.** `UCigOrderSystem::WrapVisual` is a
   single static mesh actor; the toppings the player adds are not visible on it.
5. **The dough ball.** Now derives its colour and size from the batch (1.1), but
   it is still a sphere — there is no texture, no surface, no grain.

## Animation

There is effectively none. The only skeletal mesh in the game is the cat, and
only when `Cat_Animation_Pack` is installed — `ACigCat::TrySetupSkeletalCat`
loads a mesh and three sequences (idle, walk, sit) and returns early without a
warning when they are absent, leaving a primitive cat standing there.

`Content/Characters` holds the UE5 mannequins with idle, walk, run, jump, fall
and a locomotion blend space, plus `ABP_Manny`. Nothing in the game references
them. They are the obvious retarget target — the UE5 skeleton is what most Fab
and Mixamo animation packs ship against — but Manny and Quinn are stylised
sci-fi mannequins and will not sit next to Kenney low-poly furniture without
looking like two different games.

## UI

The HUD is drawn entirely in Canvas C++ (`CigkofteHUD.cpp`, 1091 lines) with a
resolution-derived `UIScale`. There are no UMG widget assets in Content at all;
the tablet is built in C++ too. Consequences seen in the PIE session:

- The day header, money, level, popularity and hygiene are readable but plain —
  flat rectangles and text, no iconography.
- The recipe panel lists ingredients as coloured squares plus counts. It works,
  but at 1080p the text is small and the panel competes with the world.
- Station labels float in the air in capital letters (`BULGUR`, `İSOT`, `SU`).
  They read as debug output, not as shop signage.
- No main menu art beyond text on the world; no logo.

## Lighting

The shop reads as evenly lit and flat in the PIE screenshots. There is no warm
interior key, no contrast between inside and the street, and no light drawing
attention to the counter or the food. Nothing is unreadably dark, which is the
one thing in its favour.

## VFX

None found. No Niagara systems, no particles on chopping, kneading, serving or
cleaning.

## Things that can be fixed without touching gameplay

- Station meshes: `Setup` takes the type; swapping the mesh per type changes no
  interaction, no collision volume and no coordinate.
- Customer visuals: the actor's components are cosmetic; the queue slot,
  patience and serve logic never read them.
- Floating debug labels: replace with signage or move behind a debug toggle.
- Lighting and post-process: no gameplay reads them.
- UI iconography and text size: `CigkofteHUD` is presentation only.
- Wrap toppings: `FCigWrapBuild` already carries `ToppingMask`; the visual
  simply does not read it.

## Things that must not move

Station world coordinates, interaction radii, queue slots, seat transforms, the
NavMesh, and the fallback path in `CigMeshLibrary` — every loader returns
nullptr and the caller draws a primitive, which is what keeps the game running
on a machine without the packs. That fallback is load-bearing: every pack in the
table above except `Audio` and `LowPoly` is excluded from the repository by
`.gitignore`, because they carry uassets over GitHub's 100 MB limit. A clone of
this project renders the shop from primitives and that has to keep working.

## The style problem nobody has named yet

The packs already in use do not belong to the same art direction. Kenney's
`LowPoly` furniture is flat-shaded, untextured and cartoonish.
`Scene_Bazaar_Vol1` is Megascans-grade photoscanned produce at full texture
resolution — 70-90 MB for a single spice pile. `CitySampleBuildings` is from
Epic's photoreal city demo. They are currently kept apart by distance: the
realistic packs dress the street and the market the player walks past, while the
shop interior is Kenney and primitives. That separation is doing real work, and
any new asset has to respect it or the seam becomes visible.
