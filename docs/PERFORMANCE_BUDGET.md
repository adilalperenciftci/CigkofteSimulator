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
| Render thread (avg) | ≤ 8 ms | 18.01 ms | 9.3 – 10.6 ms | **target is mis-specified — see below** |
| GPU frame (avg) | ≤ 16.67 ms | 12.33 ms | **9.76 ms** | met |
| 1% low | ≥ 45 FPS | 22.4 FPS | **47.1 FPS** | met |
| Worst frame | — | 65.11 ms | **38.79 ms** | |
| Draw calls — shadow depths | to be set | 361 avg / 1113 peak | **89 avg** / 390 peak | |
| Draw calls — base pass | to be set | 46 avg / 418 peak | 59 avg / 494 peak | |
| Draw calls — total (RHI) | to be set | 424 avg | **279 avg** / 1085 peak | |
| Primitives drawn | to be set | — | 2.37 M avg / 8.27 M peak | |
| Static mesh actors | to be set | 626 | 626 | unchanged |
| GPU memory | ≤ 70% of the target GPU's VRAM | — | **1930 MB of 3366 MB = 57.3%** | met |
| System memory | safe headroom on a 16 GB machine | — | **1.76 GB peak** | met |
| Texture streaming pool | — | — | 10.9 MB avg / 23.0 MB peak | |
| Shader hitches | 0 critical hitches in play | — | not measurable here | the capture starts after the world is built |
| Load time | main-flow target to be set | — | **3.9 s to a playable shop** | measured, Development |
| Shipping build size | release target to be set | — | **Development 2282 MB**; Shipping not yet built | content is 1430 MB of it |

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

## The render-thread row, and why it is not a target

Two changes were made to chase the 8 ms render-thread target. Both cut real work
and neither moved the number, which is the finding.

**The sun's cascades.** Every point light had already stopped casting, so the
directional light was the only shadow caster left, and a movable one defaults to
three cascades over 20000 units - the whole map. Two cascades over 7000 kept the
shadows where they are read and dropped them from the far end of the street.
Shadow-depth draw calls went 137 to 89 on the route, and 372 to 162 at the shop.
The render thread went 9.46 to 9.54 ms. Wrong direction, inside noise.

**Static world decoration.** The world is assembled at runtime, so all 629 props
were spawned Movable and left that way; a movable primitive has its mesh draw
commands rebuilt rather than cached. `FinalizeStaticProps` settles them to Static
once the building is over. Total draw calls went 371 to 279 and occlusion tests
114 to 33 - a 72% cut. The render thread went 9.54 to 9.34 ms.

| | before | cascades | + static props |
| --- | --- | --- | --- |
| Shadow-depth draws | 137.5 | 88.7 | **85.4** |
| Occlusion tests | 118.4 | 113.9 | **32.6** |
| Total RHI draws | 424.1 | 371.1 | **279.3** |
| 1% low | 52.1 FPS | 50.9 FPS | **67.5 FPS** |
| Render thread | 9.46 ms | 9.54 ms | 9.34 ms |

Draw calls fell by a third and occlusion tests by nearly three quarters. The
render thread did not follow. In every run it sits within 0.02 ms of the frame
time - 9.46/9.48, 9.54/9.55, 9.34/9.35, 10.60/10.62 - regardless of how much work
it was given. A thread that tracks the frame that closely is being measured while
it waits, not while it works.

So the `≤ 8 ms` row is not a target this counter can meet by reducing
render-thread work, because the counter is not reporting render-thread work. It
should be replaced by something that is: GPU frame time, which is measured
separately and sits at 8.2 ms, or a frame-time budget, which is met at 94-107 FPS.
The row is left in the table with its verdict rewritten rather than deleted,
because deleting a target you failed to meet is how it stops being tracked.

**Run-to-run variance is about 1.3 ms** on this machine, larger than any of the
deltas above. The draw-call counts are exact and repeatable; the millisecond
figures are not, and none of the timing comparisons here should be read as
better than that.

## What the resolution label was actually saying

Nothing, until now.

`ApplySettings` ran at startup and re-applied the saved video settings over
whatever the command line had just asked for. `Measure-Performance.ps1` passes
`-ResX`/`-ResY` and printed them as the report's resolution, so a deliberate
experiment - the same build at 1920x1080 and at 1280x720 - came back with GPU
times 0.03 ms apart. That is what a resolution comparison looks like when the
resolution never changed.

Two fixes. The command line now wins over the saved resolution, and the report
reads the resolution back out of the run's own log instead of repeating the
request. It immediately reported `1600x900 (istenen 1920x1080 kirpildi)`, because
the run is windowed on a 1536x864 desktop and 1080p does not fit - which is true,
was true before, and had never appeared in a report.

Every resolution label in this document written before this point is a statement
about a command line, not about a frame.

## Load time and build size

Measured on the Development package, which carries 374 MB of debug symbols and a
331 MB executable that a Shipping build does not.

| | |
| --- | --- |
| Engine initialisation | 3.23 s |
| Map load and world build | 0.76 s |
| Process start to a playable shop | **~3.9 s** |
| Archive | 2282 MB in 105 files |
| — cooked content (`.ucas`) | 1430 MB |
| — debug symbols (`.pdb`) | 374 MB |
| — executable | 331 MB |
| — engine DLLs | 96 MB |

The world is built from code into `/Engine/Maps/Entry`, so there is one map load
and it includes constructing the shop and the city: 0.76 s for all of it.

Content is 1430 MB and is the only part a Shipping build keeps at full size. It
comes from the asset packs - the bazaar scene, the mannequins, the cat pack, the
supermarket and the modular building set - not from anything authored here. A
Shipping figure is still not measured; the number a player would download is
somewhere near 1.5 GB, and until that build is made this row says Development.


## Still open

- The shop interior is still the most expensive view, at 9.98 ms, and it is inside
  the 16.67 budget with room.
- The render-thread target was chased twice and is not reachable through
  render-thread work; the row needs replacing with a counter that measures work.
  See above.
- Shipping build size has no number: only Development has been packaged.
- No minimum-spec machine has been tested. Everything above is one developer
  machine at 1080p, and the GPU budget in particular is that card's budget.
- Shader hitches are invisible to this route by construction: the capture starts
  after the world is built, so first-run compilation is excluded on purpose.
- The lighting change has not been looked at by a human. The numbers are certain;
  whether the shop still reads the way it did is a judgement nobody has made yet.
