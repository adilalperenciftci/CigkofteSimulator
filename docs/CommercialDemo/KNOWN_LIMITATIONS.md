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

- **The ticket's abbreviations have never been read by a player.** Stage 4.2 put
  the toppings on the customer's own label, because they are 30 of the 100 points
  `ScoreWrap` awards — more than the spice or the portions — and the label had
  never carried them. They appear as three-letter codes, since seven spelled-out
  toppings is a paragraph over somebody's head. What is tested is that the codes
  are unique in every language, which matters because the obvious first-letter
  scheme collides in both (Marul/Maydanoz, Lettuce/Lemon) and collides
  differently in each. What is **not** tested is whether `MRL+SGN` is something a
  player learns in a minute or a cipher they resent: nobody has read one. If they
  do not land, the fallback is fewer codes rather than longer ones — the label
  has a width budget before it overruns the body it sits above.

- **Nobody has watched a customer get impatient.** Stage 4.1 gives urgency a
  second channel that is a shape rather than a colour — a lean that grows with
  the patience bands, plus a sway — because the label tint it sits beside is
  small text in one hue: unreadable from where the player actually stands, and
  invisible to anyone who cannot separate red from green. `CigCustomerReadout`
  decides the pose and is tested without a world, so what *is* verified is the
  precedence: leaving outranks everything, a seated or not-yet-arrived customer
  never plays urgency, and the bands hand over at full strength.
  What no test here can answer is whether 14° of lean reads at queue distance,
  whether the sway looks like impatience or like a wobble, and whether a row of
  five customers reads as five people rather than one animation. The numbers were
  chosen by reasoning about silhouettes, not by looking.

- **The focus highlight is invisible on most stations.** `SetHighlighted` tints
  `Top`, the coloured primitive tub. With the prop pack installed that component
  is hidden on every station except the five ingredient ones, so on the rest the
  player gets no feedback about what they are aiming at. Tinting the imported mesh
  instead would mean driving a material this project did not author and cannot
  assume the parameters of; an overlay material would be a new asset. Left as a
  limitation rather than guessed at.

## Hygiene and rivals

- **The cleaning hint is Turkish only.** `UCigHygieneSystem::WorstProblem`
  returns hard-coded strings — `TEXT("Ellerini yıka (Lavabo)")` and its five
  siblings — rather than going through `CigText`. They are not `LOCTEXT`, so the
  static localization check does not see them, and the HUD shows Turkish with the
  interface set to English. Found while writing the system's first tests;
  recorded rather than fixed, because moving six strings into `Config/Text` is a
  string-table change with its own review rather than part of adding coverage.

- **A rule the tests now make visible: closing the weakest rival hurts.**
  `PlayerPullMult` divides the rivals' strength by how many are open, so it is an
  average rather than a total. Driving the failing shop on the street out of
  business raises the average of the ones left, and the player's pull *drops*.
  `Cigkofte.Rivals.ClosingTheWeakestRivalMakesThingsWorseForThePlayer` pins it in
  both directions — losing the strongest rival helps, which is what makes it an
  average and not a one-way bug. Whether it reads as "the survivors are the tough
  ones" or as punishing the player for winning is a design decision; the test
  exists so whoever makes it can see the behaviour, not to assert it is right.

## Coverage

- The 255 automation tests cover pure formulas, tables, data integrity and one
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

## Shop identity

- **The rename has never been used by a person.** F2 on the shop tab is wired to
  hand the keyboard to a field: `BeginTextEntry` sets the gate that stops the
  character polling raw keys and switches to `GameAndUI` with the box focused,
  and commit or closing the tablet gives it back. Automation covers the gate
  (`CigInput::Scope`) and the name rules, and it covers neither of the things
  that actually matter here — that F2 moves focus, that typing does not walk the
  player, that Enter returns input to the game, that the cursor ends up where it
  should. Those need somebody to press the keys. **This is the first change in the
  project to call `SetInputMode`, and it is unverified.**
- **The name is not translated, deliberately.** A name given in Turkish stays that
  name when the interface is switched to English, so the default is a fixed string
  rather than a text key. The consequence is that the default board reads
  `CIGKOFTECI` in both languages.
- **Length is counted in UTF-16 code units, not graphemes.** `ğ` costs one and an
  emoji costs two. That is the same measure the text renderer and the save field
  use, so it is consistent rather than correct.

## Packaging

- **A local plugin can still change the editor, just not the game.** `Sentry` is
  now `"Enabled": false` in the `.uproject`, which keeps it out of the cook, the
  build and both packages — but only because it is named there. The guard
  (`Test-CigLocalPlugins.ps1`) fails release validation when an untracked plugin
  can reach a packaged Win64 game; it does **not** stop one from loading in the
  editor, and an editor that loads a plugin a clean clone does not have is still
  an editor other people cannot reproduce. `Tools/LocalPluginPolicy.json` records
  the decision per plugin rather than preventing it.
- **The stray-artefact scan matches names, not provenance.** It looks for path
  fragments derived from the cook exclusion list and the local plugin policy
  (`Toolset`, `ModelContextProtocol`, `Sentry`, `crashpad`, …). A future plugin
  whose files carry none of those strings would need a policy entry before the
  scan could see it, and a legitimate game asset containing one of those words in
  its path would be a false positive.
- **Neither guard covers engine plugins.** Both look under the project's
  `Plugins/`. An engine plugin enabled by default is outside their scope.
- **A Development archive is not a release artefact.** `PackageDemo.ps1` does not
  pass `-nodebuginfo`, so it carries a PDB. `Package-Windows.ps1` does unless
  `-IncludeSymbols` is given, which is why Shipping has none.
- **Editor tooling is excluded from the cook by a command-line argument, not by
  the project descriptor.** `ModelContextProtocol`, `EditorToolset` and `SlateInspectorToolset` remain
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
- **Street pedestrians are contained, not navigated.** Ambient wanderers are
  still on direct movement — they have no route and ask the grid for nothing.
  What they now have is a region: `FCigPedRegion` gives the main street a lane on
  each pavement, and a pedestrian's position is clamped into its lane every step.
  That is a containment guarantee rather than a path, and it is deliberately the
  smaller claim.
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
- **Only the main street has pedestrian lanes.** The two pavements outside the
  shop are modelled, because they are two straight strips whose furniture stands
  on known lines. The six districts are not: each still gets one rectangle around
  its centre, and a pedestrian inside it has no route around the stalls — only
  containment and blocked-step recovery. Modelling district interiors is open.

  One detail in this entry was **wrong and is corrected**: it used to say the
  stalls' "collision is off". It is not. Both stall paths ask for it — the bazaar
  pack's mesh through `SpawnProp(..., bCollision=true)`, and the primitive
  fallback through `SetActorEnableCollision(true)` on its table — and the
  components report profile `BlockAll` with a `Block` response to `ECC_Pawn`.
  Whether a walker nonetheless passes through one is **unverified**: an attempt
  to pin it with a capsule at the stall coordinates found nothing there, and the
  diagnostic could not separate "the stall is elsewhere" from "the stall does not
  block" before the attempt was abandoned. The flag is not the explanation; what
  is, is still open.
- **Street furniture blocks now, with a shape this project chose.** Trees and
  lamp posts block the player. What does the blocking is a narrow invisible
  cylinder at the trunk, not the imported mesh's own collision — and that
  distinction was learned the hard way. Giving the CityPark tree meshes collision
  closed the east pedestrian lane, because their hulls are wider than the visible
  trunk and the lane inset was chosen for a trunk. It failed on roughly one run
  in three, since the trees carry a random Y jitter. An optional asset's
  collision also differs between the six trunk meshes and is absent entirely when
  the pack is not installed, so relying on it means the street blocks differently
  on different machines.

  The tree *crown* still does not block: it is a sphere centred at 300 with the
  player's head at about 180, so stopping somebody walking past a tree they are
  clearly beside would be a worse wrong than walking through one.

  **What is proven is narrower than it looks.** Only the *west* tree line is
  asserted. Putting the flag back one prop type at a time showed that a blocked
  sample on the east line comes from the buildings behind `EastPavementMaxX`, and
  one on the lamp line from the road edge at `RoadMinX` — both assertions passed
  whether or not the collision was there, so they were removed. The lamp posts'
  blocker is therefore **unmeasured**: it is the same call as the trees', and the
  reason to believe it is the code rather than a test.

- **A building's collision reaches into the east pedestrian lane.** Found by the
  furniture test before it was narrowed: sampling the lane centre at roughly
  x=-1350 is blocked, on some runs, by an actor whose origin is near x=-578 —
  some 770 units away, so its hull is very large. Ambient pedestrians walk that
  lane and sweep against `WorldStatic`, so they would stop there and repath.
  This predates the furniture work and is not caused by it; it is recorded rather
  than fixed because the fix is a building-placement change and nobody has
  watched what the street looks like today.
- **The grid is the navigation authority by measurement, and Recast is not
  available.** `docs/Architecture/NAVIGATION_AUTHORITY.md` records the experiment.
  No `ARecastNavMesh` is created in this project in any environment measured —
  editor `-game`, packaged Development or packaged Shipping — because
  `UNavigationSystemV1::OnWorldInitDone` only creates navigation data when
  navigation bounds already exist, and a game with no authored map cannot have
  any: the API to register them from game code is protected, and
  `bWholeWorldNavigable` has had its config property commented out by Epic as
  broken. What stays open is the condition, not the question: if the project ever
  gains an authored map for another reason, the hybrid option becomes measurable
  for the first time.
- **No human has walked the shop since this landed.** Every claim here is from
  automation. See the QA section.

## Platform

- No Steam integration. The platform abstraction in Stage 10 has not started, so
  achievements are local only.

## Content

- Districts unlock by level and are decorated from primitives plus optional Fab
  packs. With no packs installed the outer districts read as stylised blocks.
