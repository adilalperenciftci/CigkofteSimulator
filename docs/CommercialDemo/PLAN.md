# Commercial demo — dependency-ordered plan

Ordered by dependency, not by calendar. A slice is only started once everything
it relies on is correct, because a feature built on a broken pipeline has to be
rebuilt rather than fixed.

## Stage 0 — correctness and shared pipelines

Nothing in later stages is worth building until sales, events, reviews and saves
are internally consistent. Stage 0 adds no player-facing feature by design.

| # | Slice | Depends on | State |
|---|---|---|---|
| 0.2 | Staff packaging keeps real dough spice | — | done |
| 0.3 | Complaint risk feeds the inspector roll | — | done |
| 0.5 | Every event ends through one lifecycle | — | done |
| 0.6 | Stable review IDs for the reply queue | — | done |
| 0.10 | Build/test/package scripts, honest CI | — | done |
| 0.1 | One authoritative sale pipeline | 0.2 | done |
| 0.4 | Unified bulk-order contracts | 0.1 | done |
| 0.7 | PriceScore from real per-product prices | 0.1 | todo |
| 0.8 | Dialogue context uses the delivered wrap | 0.1 | todo |
| 0.9 | Save migration tests | 0.1, 0.4 | todo |

0.1 is sequenced after 0.2 deliberately: the spice defect lives in the staff sale
path, and fixing it first means the extraction has one correct behaviour to
preserve rather than two divergent ones to reconcile.

## Stage 1 — tactile food preparation

The single most important stage for the sales pitch. Depends on Stage 0.1 only
in that the finished product must already be authoritative.

- 1.1 Mixture visual state driven from food state, cached material instances
- 1.2 Ingredient pouring with procedural utensil motion
- 1.3 Kneading progression: pitch, cohesion, deformation, completion
- 1.4 Chopping states and pooled fragments
- 1.5 Wrap assembly surface, data-driven topping placement
- 1.6 Readable recoverable failures
- 1.7 Deterministic tests for visual state derivation

## Stage 2 — opening, closing, physical inventory

Depends on Stage 1 for the preparation loop it wraps around.

- 2.1 Opening routine, 2.2 physical stock, 2.3 storage rules,
  2.4 batch spoilage, 2.5 closing routine

## Stage 3 — shop customization and build mode

Depends on Stage 2 storage actors existing as placeable objects.

- 3.1 single placement authority, 3.2 categories, 3.3 layout consequences,
  3.4 path validation, 3.5 persistence by stable ID, 3.6 shop identity

## Stage 4 — customer life and readability

Depends on Stage 3 for seating and pathing.

- 4.1 body language, 4.2 order ticket, 4.3 groups, 4.4 seating,
  4.5 regular arcs, 4.6 queue fairness

## Stage 5 — staff and automation

Explicitly gated behind 0.1. Expanding to three employees before the sale
pipeline is unified would triple the divergence.

## Stage 6 — business depth

Depends on 0.7 pricing and Stage 2 inventory batches.

## Stage 7 — events and streamable moments

Depends on 0.5 event lifecycle and Stage 1/3 for the visuals each event drives.

## Stage 8 — visual and audio polish

Depends on the systems being final enough that art is not thrown away.
Blocked on licensed assets — see ASSETS.md.

## Stage 9 — UI, accessibility, onboarding

Depends on Stage 1 and 4: the tutorial cannot teach an interaction that is still
changing.

## Stage 10 — Steam, demo, packaging

Last. Requires an App ID and Steamworks account access that this repository must
never contain.

## Co-op

Not started. Gated behind the demo QA checklist per the decision gate.
`COOP_FEASIBILITY.md` is written before any networking code.
