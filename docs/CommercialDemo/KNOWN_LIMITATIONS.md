# Known limitations

Genuine, non-blocking limitations of the current branch. Defects that are simply
not fixed yet belong in `PLAN.md`, not here.

## Visual

- **The counters overhang their interaction boxes.** Fixed the knee-high units
  they replaced — the counters now stand at 90cm — but they do it by letting the
  model run to 210cm inside a 90cm box. Where the player can stand and interact
  is unchanged, which is the point, and it means the visible counter and the
  volume it answers to are no longer the same shape. Fine while the stations are
  250 apart; it would need revisiting if a layout ever put two closer than that.

- **Signage pops in and out at 700uu.** Fixed the overlap it replaced — the far
  counter row's names no longer project across the near row in wide shots — but
  the boundary is a hard visibility switch, not a fade. `UTextRenderComponent`
  has no opacity to animate without a custom material, so a player walking the
  length of the shop will see names appear rather than fade up. Chosen over
  leaving the overlap, which was wrong in every screenshot; worth revisiting if
  the pop reads badly in motion, which nobody has watched yet.

## Verification

- **The preparation stations have never been driven by hand.** One PIE session
  exists and is recorded in `QA.md`: a full 180-second day through Slate's real
  input path, with committed screenshots. What it could not do is work a station,
  because the tooling driving it cannot hold a key down — so kneading rhythm,
  chopping and wrap assembly have been exercised by automation and by the
  screenshot pass, and never by a person with their hands on the controls.
- **No branch since that session has had a human playthrough.** Stage 2, the
  localization pass and the performance work were verified by automation,
  packaged smoke tests and deterministic screenshots.
- **Stage 3.3 has no human build-mode/layout-usability playtest.** Automation
  proves the authored shop's consequence counts and stable-ID lifecycle, but no
  person has judged placement feel, fixture reachability, crate readability or
  whether the shop layout is comfortable with keyboard or gamepad.
- **Nobody has listened to the ambience beds.** Format, loop-seam arithmetic, the
  looping flag and path resolution are all verified; whether the night bed reads
  as night, and whether the layer sits right against the Kenney one-shots, is not.
- Slice 0.7 made footfall respond to the pricing policy for the first time. The
  net price is unchanged, but `GunlukTalepCarpani` now sees a 0.8–1.25 multiplier
  it never saw, and with elasticity around 1.4 the expensive policy now measurably
  thins the queue. That is a balance change, and it has not been played.

## Dialogue

- The bucket key carries one bit for "the order was correct", so the offline
  line table cannot tell a missing ayran from the wrong spice — both land in the
  same `a0` bucket. Widening the key would multiply the 2400 buckets and
  invalidate the generated table, so it is deliberate. The AI prompt does name
  the specific mistake; only the offline fallback is coarse.
- The seed table covers 21 of 2400 buckets. Everything else falls back to the
  canned mood lines, which is the designed behaviour until the generator is run
  with an API key (see `AI/CigDialogueTable.h`).

## Audio

- The night ambience is the daytime street recording low-passed rather than a
  separate night field recording, because the licensed pack has no night city
  bed. It reads as the same street later in the evening, which is what the layer
  needs, but a real night recording would carry detail a filter cannot invent.
  Sources and edits are in `CREDITS.md`.
- `CarEngine` is mapped to a metal one-shot and `CatMeow` is unmapped. Both are
  documented in `ASSETS.md` rather than disguised with pitch shifting.

- **The focus highlight is invisible on most stations.** `SetHighlighted` tints
  `Top`, the coloured primitive tub. With the prop pack installed that component
  is hidden on every station except the five ingredient ones, so on the rest the
  player gets no feedback about what they are aiming at. Tinting the imported mesh
  instead would mean driving a material this project did not author and cannot
  assume the parameters of; an overlay material would be a new asset. Left as a
  limitation rather than guessed at.

## Coverage

- The 153 automation tests cover pure formulas, tables, data integrity and one
  end-to-end scenario (`Cigkofte.DayFlow.OneDayFromStockToSave`: stock through
  dough, wrap, customer, sale, day end and save/load). What they do not cover is
  anything that needs a renderer or a real input device — interaction tracing,
  animation, audio mixing and UI layout are all outside the harness.
- The count above is checked rather than copied: `Tools/check_sources.py` derives
  it from the test macros and fails when a document disagrees.
- Placement tests prove deterministic rectangular occupancy, category-specific
  use/approach rectangles, functional capacity, atomic move/remove and declared
  crate alternatives. Reachability across those rectangles is now measured rather
  than assumed — see the navigation section below. Placement save/load
  persistence is Stage 3.5 and shop identity is Stage 3.6; neither is implemented
  here.

## Packaging

- **The shipped package carries Sentry's crash handler.** Both Development and
  Shipping archives contain
  `Plugins/Sentry/Binaries/Win64/crashpad_handler.exe`. `Plugins/Sentry` is
  gitignored and local-only because `CRASH_PRIVACY.md` requires explicit consent,
  a privacy policy, a retention period and a DSN before any endpoint is enabled,
  and none of those exist. A plugin dropped into `Plugins/` is enabled by default
  whether or not the `.uproject` names it, so a clean checkout produces a package
  without these binaries and this machine does not. No DSN is configured, so
  nothing is transmitted — but the binary should not be in a release archive.
  Release blocker; needs its own branch.
- **A Development archive is not a release artefact.** `PackageDemo.ps1` does not
  pass `-nodebuginfo`, so it carries a PDB. `Package-Windows.ps1` does unless
  `-IncludeSymbols` is given, which is why Shipping has none.
- **Editor tooling is excluded from the cook by a command-line argument, not by
  the project descriptor.** `ModelContextProtocol` and `AllToolsets` remain
  enabled in the committed `.uproject` on purpose. Packaging outside
  `PackageDemo.ps1` and `Package-Windows.ps1` — a raw `RunUAT` invocation, or a
  future Steam staging script — will hit the original cook failure unless it also
  uses `Get-CigCookPluginExclusionArg`.

## Navigation

Stage 3.4 measures reachability with an A* search over an occupancy grid
rasterised from the placement records and the shop shell, inflated by the agent's
own radius. What that is and is not:

- **It is not a navmesh.** There is no authored map to build one on: the shop is
  spawned at runtime into `/Engine/Maps/Entry`. The floor is a single plane and
  the game has no vertical traversal, so the one thing a navmesh would add that
  this does not have is unused. The grid is cross-checked against real engine
  collision with the player's own capsule
  (`Cigkofte.Navigation.Collision.TheEngineAgreesWithTheGrid`), which is what
  stops it being a private opinion about a shop it never touched.
- **The street is not modelled.** The navigable region covers the shop and the
  pavement the queue stands on. Beyond it the grid assumes open ground, and
  `BuildCity` has put real geometry out there. Nothing outside can be moved by
  the player, so nothing outside can block a route, which is why the boundary is
  drawn where it is — but a grid answer taken from the deep street is not
  trustworthy and nothing should ask for one.
- **Street pedestrians still walk through things.** Ambient wanderers are
  deliberately left on direct movement, because pathing them would mean pathing
  across the unmodelled street.
- **A customer with no route stops.** It used to walk straight at the target,
  which was the wrong trade: the fallback fires on exactly the layouts the
  measured navigation exists to catch. They now stop, release the seat and queue
  slot they were holding, and `UCigCustomerSystem::RecoverStrandedCustomers`
  gives them one attempt to walk out before recycling them through the pool. The
  attempt is bounded and counted (`StrandedRecovered` / `StrandedRecycled`); a
  customer is never left standing in the shop with no way for the player to clear
  them.
- **Movement is swept against static world geometry, not against other
  customers.** A sphere sweep on `ECC_WorldStatic` stops a customer short of
  anything the grid did not know about and triggers one repath. Customers pass
  through each other by design — the visible body is `QueryOnly` so a queue
  cannot deadlock on itself.
- **The player is not path-constrained.** They are a real `ACharacter` with a
  capsule and move against engine collision, which is stricter than the grid. The
  grid answers questions *about* the player's width; it does not steer them.
- **Ambient street pedestrians are still not hardened.** They wander the
  unmodelled street on direct movement and can cross authored static geometry.
  Constraining them needs either authored pavement lanes or a modelled street
  region, and neither is done. Open.
- **The Dynamic Recast NavMesh comparison has not been run.** The decision to
  measure first and evaluate NavMesh afterwards still stands; the evaluation
  itself is outstanding, so `docs/Architecture/NAVIGATION_AUTHORITY.md` does not
  exist yet and the grid is the authority by default rather than by measurement.
  Open.
- **No human has walked the shop since this landed.** Every claim here is from
  automation. See the QA section.

## Platform

- No Steam integration. The platform abstraction in Stage 10 has not started, so
  achievements are local only.

## Content

- Districts unlock by level and are decorated from primitives plus optional Fab
  packs. With no packs installed the outer districts read as stylised blocks.
