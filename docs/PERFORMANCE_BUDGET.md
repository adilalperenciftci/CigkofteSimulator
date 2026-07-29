# Performance budget

Measured, not estimated. The route, the seed and the dwell time are fixed by
`CigBench` in the game, so two runs differ because the build changed and not
because the camera was somewhere else.

## How a run is taken

```powershell
.\Scripts\Measure-Performance.ps1 -SecondsPerStop 6 -Label <what-changed>
```

Five viewpoints — shop interior, seating, street, square, market — held for six
seconds each, on seed 1337 with every district open, the stock full and every
upgrade bought. That last part is deliberate: a budget measured on an empty first
day would be met by a build that stutters as soon as the player earns anything.

Not Unreal Insights. A `.utrace` is a binary that needs the Insights GUI to read,
so a number taken from one cannot be pasted into this file, diffed against last
week's, or checked by anyone who was not at the machine. The CSV profiler writes
the same frame timings as text.

Every run records the resolution, the build commit and the label. The capture
itself stays in the package's `Saved/Profiling/CSV` and is not committed.

## Results

Both runs: `1920x1080`, packaged **Development**, five stops at six seconds.
Hardware is the development machine — a baseline to compare against, not
minimum-spec figures.

- **before** — `stage1-rt-disabled`, 1580 frames
- **after** — `shadow-casting-off-on-fills`, 2482 frames, same route and seed

| Area | Target | Before | After | Verdict |
|---|---:|---:|---:|---|
| Frame rate (avg) | 60 | 55.4 FPS | **88.3 FPS** | met |
| Frame budget (avg) | 16.67 ms | 18.04 ms | **11.33 ms** | met |
| Game thread (avg) | ≤ 8 ms | 7.28 ms | **6.78 ms** | met |
| Render thread (avg) | ≤ 8 ms | 18.01 ms | **11.31 ms** | still over |
| GPU frame (avg) | ≤ 16.67 ms | 12.33 ms | **9.76 ms** | met |
| 1% low | ≥ 45 FPS | 22.4 FPS | **47.1 FPS** | met |
| Worst frame | — | 65.11 ms | **38.79 ms** | |
| Draw calls — shadow depths | to be set | 361 avg / 1113 peak | **182 avg** / 1104 peak | |
| Draw calls — base pass | to be set | 46 avg / 418 peak | 59 avg / 494 peak | |
| Draw calls — total (RHI) | to be set | — | 485 avg / 1281 peak | |
| Primitives drawn | to be set | — | 2.37 M avg / 8.27 M peak | |
| Static mesh actors | to be set | 626 | 626 | unchanged |
| GPU memory | ≤ 70% of the target GPU's VRAM | — | **1930 MB of 3366 MB = 57.3%** | met |
| System memory | safe headroom on a 16 GB machine | — | **1.76 GB peak** | met |
| Texture streaming pool | — | — | 10.9 MB avg / 23.0 MB peak | |
| Shader hitches | 0 critical hitches in play | — | not measurable here | the capture starts after the world is built |
| Load time | main-flow target to be set | — | TBD | not yet measured |
| Shipping build size | release target to be set | — | TBD | Development archive is 2.2 GB |

## What the numbers said, and what was done about it

The first run was **render-thread bound by a wide margin**. Render thread 18.01 ms
against a frame time of 18.04 ms means everything else was waiting on it: the game
thread finished in 7.28 ms and the GPU in 12.33 ms, both inside budget. Optimising
either would have bought nothing, which is the whole reason for measuring before
changing anything.

The cause was in the draw call split. Shadow depths averaged 361 calls against the
base pass's 46 — the scene was being drawn roughly eight times over, for shadows.

Six movable point lights were casting: four room fills at 2800uu, the counter key
light, and the door light. A movable point light renders six cube faces of shadow
depth for every primitive inside its radius, and 626 static mesh actors are in
range. Four overlapping fills in one room produce shadows that largely cancel one
another, and the sun is what gives the scene its shape.

Casting was turned off on the four fills and the door light. The counter key light
kept its shadows: it sits directly over the surface the player works at, where a
bowl casting onto the counter is the reason the light exists, and its radius stops
before the walls.

Result: shadow draw calls halved, and the render thread came down 37%. The frame
budget and the 1% low both moved from missed to met, and the worst frame in the
route dropped from 65.11 ms to 38.79 ms.

Two things worth noting rather than glossing over:

- **The render thread is still over its 8 ms target** at 11.31 ms. It is now
  within the overall frame budget, so this is a target to revisit rather than a
  defect — but the budget row says "still over" because it is.
- **Base pass draw calls went up slightly**, 46 to 59. Frames that were being
  spent on shadows are now spent drawing more of the scene per second, so a
  per-frame average taken over 2482 frames is not comparing like with like against
  one taken over 1580. The peak (418 → 494) moved for the same reason.

## Memory and scene cost

Both memory rows are comfortable and neither is where the time is going.

GPU memory peaks at 1930 MB against a 3366 MB budget — 57.3%, inside the 70%
target. System memory peaks at 1.76 GB, which leaves a large margin on a 16 GB
machine. The texture streaming pool sits at 11 MB, because the shop's textures
are small and mostly resident rather than streamed.

Primitives drawn averages 2.37 M with an 8.27 M peak, and total RHI draw calls
485 average against a 1281 peak.

Note that these are read from the capture taken after the shadow change, so the
memory figures have no before/after pair. They were not measured earlier, which
is a gap in the record and not a change in the numbers.

## Per stop

Sliced on the `CigBench` events, same capture. This corrects a guess made in an
earlier version of this file — "cheap at four stops and expensive at one" — which
was written before anything read the events and turns out to be wrong.

| Stop | Frame avg | p99 | Worst | Frames | New PSOs | Frames > 25 ms |
|---|---:|---:|---:|---:|---:|---:|
| dukkan (shop interior) | **13.44 ms** | 24.50 | 36.42 | 446 | 2 | 3 |
| oturma (seating) | **13.36 ms** | 24.51 | 35.84 | 449 | 17 | 3 |
| cadde (street) | 9.94 ms | 12.99 | 14.13 | 604 | 0 | 0 |
| meydan (square) | 10.42 ms | 12.87 | 16.30 | 576 | 0 | 0 |
| pazar (market) | 10.11 ms | 13.19 | 38.79 | 407 | 0 | 1 |

Two views are expensive, not one, and they are the two indoors: the shop and the
seating area, at roughly 13.4 ms against the outdoor districts' 10 ms. That is
the finding that matters, because the shop interior is where the entire game is
played. The open-world districts the player visits between deliveries are the
cheap part.

The p99 tells the same story more sharply. At both interior stops it is 24.5 ms —
41 FPS — so the 1% low target is not missed evenly across the route, it is missed
**in the shop**. Outdoors the p99 sits at 13 ms and never moves.

Six of the seven frames over 25 ms are at those two stops. The seventh is the
38.79 ms worst frame at the market, which is otherwise the steadiest view in the
route: an isolated hitch rather than a load.

PSO compilation does not explain it. Seventeen new graphics PSOs were encountered
at the seating stop against three slow frames, and two at the shop against the
same three — the counts and the hitches do not line up, so the interior cost is
steady-state work and not first-sight shader compilation.

## The shop interior, and one attempt that failed

Per-stop slicing said the two indoor views cost 13.4 ms against 10 outdoors, and
the scene columns said why: the shop was drawing **523 shadow depth calls a
frame** against the street's 53.

**First attempt: stop the small props casting.** Tubs, dough, scoops, chop
fragments, topping pieces and the signage. Reasonable on the face of it — a
movable point light renders six cube faces per primitive in its radius, and the
counter light's radius is full of small objects.

It did nothing. 523 → 495 shadow calls, and the frame went 13.44 → 13.48 ms,
which is noise. The props were not the cost. What that ruled out was useful: if
removing eighty small primitives moves the number by five percent, the cost is
the large ones, and the only thing making them cast six times over is the light.

**Second attempt: the counter key light stops casting.** It was the one point
light still doing so, kept on the argument that a bowl casting onto the counter
is the reason the light is there. The argument was fine; the measurement beat it.

| | before | small props | key light off |
|---|---:|---:|---:|
| dukkan frame | 13.44 ms | 13.48 ms | **11.11 ms** |
| dukkan shadow calls | 522.9 | 495.0 | **402.7** |
| oturma frame | 13.36 ms | 13.72 ms | **9.69 ms** |
| oturma shadow calls | 256.4 | 252.7 | **147.2** |
| GPU memory peak | 1930 MB | 1934 MB | **1807 MB** |

The shop is 17% faster and the seating area 27%. In the room the game is played
in that is 74 → 90 FPS; in the seating area 75 → 103. GPU memory came down 123 MB
with the shadow maps.

Checked in a screenshot: the sun still shadows the interior, the counters still
have shape, and the warm light still tints the counter without casting. What is
lost is the pool of shade under each tub, which nobody was looking at.

The small-prop change is kept even though it bought nothing measurable. Signage
casting shadows was never intentional, and a pea of lettuce casting one is not
either — but it is recorded here as what it was, which is a failed optimisation
rather than a win.

## Still open

- The shop interior is still the most expensive view at 11.11 ms, but it is now
  inside the 16.67 budget with room. The render thread across the route is 10.5 ms
  against its own 8 ms target — the next thing, if there is a next thing here.
- Load time and shipping build size have no numbers yet.
- No minimum-spec machine has been tested. Everything above is one developer
  machine at 1080p, and the GPU budget in particular is that card's budget.
- Shader hitches are invisible to this route by construction: the capture starts
  after the world is built, so first-run compilation is excluded on purpose.
- The lighting change has not been looked at by a human. The numbers are certain;
  whether the shop still reads the way it did is a judgement nobody has made yet.
