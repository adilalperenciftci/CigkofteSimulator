# Visual style guide

One sentence to hold everything to: **a warm, colourful, readable stylised
low-poly Turkish street-food shop.**

The test for any asset is not "is this good" but "does this belong next to a
Kenney fridge and a bowl of bulgur, under one light, in the same frame".

## The constraint that comes first

This project already ships mixed-fidelity packs, and they are kept apart by
distance rather than by taste (see `VISUAL_AUDIT.md`). That separation is the
rule, not an accident:

| Zone | Fidelity | Packs |
|---|---|---|
| Shop interior, everything the player touches | Stylised low-poly, flat or lightly textured | Kenney `LowPoly`, `dukkan/Geometries` |
| Street immediately outside | Mid — simple forms, restrained detail | `CityPark` |
| Distant backdrop, market props walked past | Whatever reads at distance | `Scene_Bazaar_Vol1`, `CitySampleBuildings` |

A photoscanned prop on the counter would break this. A flat-shaded cube in the
middle of the photoreal street would too, but less, because nobody looks there.

## Colour

Warm, saturated, food-forward. The shop should feel like late afternoon indoors.

- **Wood** `#8B5E3C` → `#C89B6A` — counters, shelves, boards
- **Terracotta / isot red** `#B8391F` → `#E2603A` — the signature accent, the
  dough, the signage
- **Warm off-white** `#F2E6D2` — walls, lavaş, paper wrapping
- **Herb green** `#4E7A34` → `#7FA85A` — lettuce, parsley, plants
- **Steel** `#9AA3A8` — sinks, fridge trim, tools
- **Deep brown** `#3A2A1E` — shadow tone, floor grout, contrast anchor

Kept out: cool blue-grey interiors, neon, high-contrast black. The only cool
colour in frame should be daylight coming in from the street, which is what
makes the interior read as warm by comparison.

## Materials

One master material with instances, not a shader per asset. Parameters: base
colour, roughness, a subtle grunge mask, and an emissive scalar left at zero for
everything except signage.

Surface families to cover: wood, stainless steel, tile, plastic tub, glass,
cloth, food, paper packaging. Eight instances, not eighty. Shared instances mean
shared draw batches; a unique material per prop is how a shop of forty objects
becomes a shop of forty draw calls.

No parallax, no tessellation, no clearcoat. Roughness variation and colour do
the work.

## Models

- Silhouette first: each station has to be identifiable from across the room
  without reading its label. If two stations only differ by colour, the model is
  wrong.
- Chunky, slightly exaggerated proportions. Rounded corners over sharp bevels.
- Triangle budget: props 200–2000, stations 1000–5000, characters 5000–15000.
- No baked high-poly detail on interior props. Detail comes from silhouette and
  colour, not from normal maps.

## Textures

- Props: 512 or 1024. **A napkin does not get a 4K texture.**
- Stations and hero props: 1024, 2048 only if it is genuinely read up close.
- Characters: 1024 for the body atlas.
- Prefer untextured flat-shaded meshes with vertex colour where the pack already
  works that way — Kenney assets do, and mixing a textured version of the same
  object next to them is worse than either alone.

## Characters

Stylised, simplified humans: simple shapes, no facial detail beyond suggestion,
readable clothing colour blocks. Body types and ages should vary enough that a
queue of six does not look like six copies.

The UE5 mannequin skeleton is the retarget target, because that is what most
animation packs ship against — but Manny and Quinn themselves are placeholder
only. They are sci-fi mannequins and will read as a different game if they end
up in the final shot.

## UI

- One icon style throughout: flat, two-tone, thick strokes, no gradients, no
  photographic or 3D-rendered icons mixed in.
- Icons must survive being drawn at 24 px.
- Body text no smaller than 16 px at 1080p after `UIScale`. The current HUD is
  below that in places.
- Panels: dark translucent plate, warm accent bar on the leading edge — the
  existing HUD language, kept.
- Station labels move from floating capitals to shop signage in the world.
  Floating debug text stays available behind the debug toggle, not in the
  default view.

## Lighting

- Warm interior key over the counter, roughly 3000–3500K, the brightest thing in
  frame after the food.
- Cooler daylight from the street doorway, roughly 6000K, for contrast.
- A soft fill so no corner is unreadably dark. No pitch black interiors.
- Bloom subtle — enough to lift the signage, not enough to haze the food.
- Shadows: dynamic on the interior key only; static or none for background
  dressing.

## VFX

Meaningful moments only, never ambient loops:

- chopping — a few crumbs, short-lived
- kneading — dust puff on the stroke
- serving — a small warm sparkle on a perfect wrap
- cleaning — brief foam and water
- cold drinks — light condensation

If a Niagara system is running while nothing is happening, it is wrong.

## Performance budget

Measured against the current baseline before any of this starts, and re-measured
after each milestone:

- Frame time must not regress more than 20% at any milestone.
- Draw calls: interior under 600.
- Skeletal meshes on screen: 8 customers plus player plus apprentice plus cat.
  Beyond that, animation update rate optimisation is mandatory.
- Texture memory: prefer 512/1024; every 2048 needs a reason.
- Collision off on anything the player cannot reach or touch.
- LODs on anything over 2000 triangles, or Nanite where the pack already
  supports it and the mesh is static.

## Not this

- Photoreal food on the counter.
- Realistic humans.
- Branded logos, real product packaging, political or religious symbols.
- Mobile-grade flat colour with no lighting response.
- Horror-adjacent lighting, heavy vignette, desaturated grading.
- Mixing three icon styles in one screen.
