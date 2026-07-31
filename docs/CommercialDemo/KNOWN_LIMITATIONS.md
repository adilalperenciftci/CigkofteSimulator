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

- The 96 automation tests cover pure formulas, tables, data integrity and one
  end-to-end scenario (`Cigkofte.DayFlow.OneDayFromStockToSave`: stock through
  dough, wrap, customer, sale, day end and save/load). What they do not cover is
  anything that needs a renderer or a real input device — interaction tracing,
  animation, audio mixing and UI layout are all outside the harness.
- The count above is checked rather than copied: `Tools/check_sources.py` derives
  it from the test macros and fails when a document disagrees.

## Platform

- No Steam integration. The platform abstraction in Stage 10 has not started, so
  achievements are local only.

## Content

- Districts unlock by level and are decorated from primitives plus optional Fab
  packs. With no packs installed the outer districts read as stylised blocks.
