# Known limitations

Genuine, non-blocking limitations of the current branch. Defects that are simply
not fixed yet belong in `PLAN.md`, not here.

## Audio

- The ambience layer resolves three loop assets and stays silent because none are
  imported yet. Selection, volume and weather switching are implemented and logged
  once. The owner's Fab library already contains suitable packs; see `ASSETS.md`
  for the import procedure. This is a content step, not a code gap.
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
