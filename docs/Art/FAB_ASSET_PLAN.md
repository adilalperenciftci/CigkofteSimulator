# Fab asset plan

Researched 2026-07-27 against the signed-in Fab library (2.6K products: 1.7K 3D,
509 materials, 14 animations, 13 VFX, 7 UI). Nothing was purchased, downloaded or
added to the library during this pass — this is the shortlist, and A2 acts on it.

The order of preference is the one that matters: **already installed in the
project → already owned in the library → free on Fab → paid (recommendation
only)**. Most of what this project needs is in the first two buckets.

## Already installed, already used

Do not re-acquire. See `VISUAL_AUDIT.md` for counts.

`LowPoly` (Kenney), `dukkan/Geometries`, `CityPark`, `Scene_Bazaar_Vol1`,
`CitySampleBuildings`, `Cat_Animation_Pack`, `Audio`.

## Already installed, not used by any code

| Pack | uassets | Why it matters |
|---|---|---|
| `Characters` (UE5 Manny/Quinn) | 145 | Skeleton + locomotion + `ABP_Manny`. The retarget target, not the final look. |
| `MMSupermarket` | 657 | Shelving, fridges, checkout — shop fittings |
| `Scene_Banquet` | 621 | Tables, chairs, tableware |
| `ModularBuildingSet` | 315 | Shop exterior, walls, doors, windows |

Three of those four are shop furniture that nobody has looked at yet. Checking
them before importing anything new is the cheapest work available.

## Will use — in the library, free to download

| Asset | Publisher | For |
|---|---|---|
| **Stylized Catcafe 110 Asset Pack** | China Capture | **The single best match in the library.** Wooden crates, brick wall, chairs, tables, counter dressing, warm orange palette, stylised. This is the shop interior. |
| **Food FREE - Low Poly 3D Models Pack** | ithappy | Vegetables, produce, food props for the prep stations |
| **Animals FREE - Low Poly 3D Models Pack** | ithappy | Candidate stylised cat, matching the food pack's style |
| **50 Free Stylized Icons** | Hexagram | UI icons — one consistent flat style, which is exactly what the HUD lacks |
| **Game Animation Sample** | Epic Games | Large UE5 locomotion set on the UE5 skeleton: idle, walk, run, turns, starts and stops |
| **Animation Starter Pack** | Epic Games | Idle, walk, run, plus reaction poses |
| **MC Sample Animation Pack** | MoCap Central | Extra mocap idles and gestures |
| **Free Animation Library** | voxel vision | Gesture and reaction filler |
| **60 FREE Finger Poses Animations Pack** | Attaku | Hand poses for holding a wrap, a glass, cash |
| **Modern Street Props - 16 Pieces** | Infinite Polygon Studio | Pavement dressing outside the shop |
| **Wooden props environment pack** | JessyStorm's Assets | Crates, shelving, rustic dressing |

## Characters — decide in A4, all in the library

| Asset | Publisher | Note |
|---|---|---|
| **Stylized Characters — Modular Stylized Character Creator (Adventurer)** | Polyart Studio | Modular means variety from one skeleton, which is what a queue of six customers needs. Fantasy-themed clothing is the risk. |
| **Quantum Modular Character Free Sample** | Quantum Assets | Free sample; check how many bodies it actually contains |
| **Adventure Characters (Pack)** | Bugrimov Maksim | Fallback |
| **Stylized Fantasy Characters** | JustCreate | Fantasy costume — likely too themed for a street-food shop |

None of these is a Turkish street-food customer. The realistic outcome is
modular civilians recoloured into plain clothing, not a perfect match.

## Alternatives held in reserve

`Stylized Eastern Village` (AleksandrIvanov) and `Stylized Village Fatpack`
(Meshingun) for exterior and street if `ModularBuildingSet` proves wrong.
`Low Poly Nature: Essentials`, `Stylized Low Poly Nature Lite` for planting.

## Will not use

- `Stylized Fantasy Provencal`, `Stylized Egypt`, `Stylized Lake Village`,
  `Stylized Windmill Valley` — wrong setting, and each drags in its own palette.
- Every sci-fi pack in the library (`Sci-Fi Corridor`, `Sci-FI Troopers`,
  `Science Fiction Desert City Kit`, `Sci-Fi Panels and Pipes`).
- Combat and magic VFX (`Mixed Magic`, `Slash Trail FX`, `Melee Weapon Aura`,
  `Advanced Portals`) — this game has no combat.
- `Quixel Megascans` furniture fabrics — photoreal 4K material on a low-poly
  chair is exactly the fidelity mix the style guide forbids.

## Gaps nothing in the library fills

Honest list, because these will not be solved by downloading:

1. **Turkish street-food props** — lavaş, dürüm, ayran glass, çiğköfte portions,
   isot tubs. No pack ships these. They come from Kenney/dukkan substitutes now
   and would need authoring to be right.
2. **Preparation animations** — kneading, chopping, rolling a wrap, portioning.
   No general animation pack contains these; they are specific to this game.
   Plan for Control Rig or hand-authored montages, not a purchase.
3. **Food VFX** — the library's 13 VFX products are all combat. Crumbs, dough
   dust, condensation and cleaning foam are small Niagara systems to author.

## Paid — recommendation only, not bought

Nothing is recommended for purchase yet. The library already covers the shop,
the icons and the locomotion; buying before those are integrated would be
guessing. If a gap survives A2–A5, it will be listed here with a price.

## What A2 does with this

1. Inspect `MMSupermarket`, `Scene_Banquet`, `ModularBuildingSet` locally first —
   already on disk, zero download.
2. Download **Stylized Catcafe** and **Food FREE** through the signed-in library.
3. Verify UE 5.8 compatibility, triangle counts and texture sizes against the
   budget in `VISUAL_STYLE_GUIDE.md` before anything enters `Content/External`.
4. Write a licence record per pack under `AssetWork/Licenses/` before use.
