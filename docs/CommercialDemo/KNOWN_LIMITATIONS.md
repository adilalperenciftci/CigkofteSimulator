# Known limitations

Genuine, non-blocking limitations of the current branch. Defects that are simply
not fixed yet belong in `PLAN.md`, not here.

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
