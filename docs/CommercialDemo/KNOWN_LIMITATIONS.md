# Known limitations

Genuine, non-blocking limitations of the current branch. Defects that are simply
not fixed yet belong in `PLAN.md`, not here.

## Visual

- **Station signage overlaps at shallow angles.** Each station draws its own name
  in world space, and standing back to see the whole counter puts several of them
  on top of one another — "BULGUR" lands across the fridge, "BAHARAT" across
  "BULAŞIK". Legible when the player is working at a station, which is when it
  matters, and wrong in every wide shot, which is what a store page is made of.
  Found in the `CigTour` frames taken to check the lighting change, not by
  anybody playing.

## Verification

- **Nobody has played the game on this branch.** Every check is static analysis,
  a compile, headless automation or log inspection. No PIE session and no human
  looking at a screen. So the player-facing consequences of these changes — the
  prices on the tablet, the renamed event, the new review wording, the fourteen
  dialogue lines — are unseen.
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

## Coverage

- The 52 automation tests cover pure formulas, tables and data integrity. There is
  no end-to-end scenario test yet that opens a day, prepares a wrap, serves a
  customer and asserts the resulting money, reputation and review.

## Platform

- No Steam integration. The platform abstraction in Stage 10 has not started, so
  achievements are local only.

## Content

- Districts unlock by level and are decorated from primitives plus optional Fab
  packs. With no packs installed the outer districts read as stylised blocks.
