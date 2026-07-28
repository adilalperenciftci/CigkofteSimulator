# Çiğköfte Simulator — asset sources and licences

An **EG Games** production.

## Art and audio (Kenney)

The low-poly models and sound effects below are published by **Kenney**
(https://kenney.nl) under **Creative Commons CC0 1.0 (public domain)**. CC0
permits commercial use and requires no attribution; it is given here anyway.

### Models (`Content/LowPoly/`)
- **Food Kit** — https://kenney.nl/assets/food-kit
  (flatbread/taco, sub, plate, tomato, onion, lemon, lettuce, glass, bottle, …)
- **Furniture Kit** — https://kenney.nl/assets/furniture-kit
  (table, chair, fridge, hob, sink, cupboard, bin, plant pot, radio, sofa)

### Audio (`Content/Audio/`)
- **Interface Sounds** — https://kenney.nl/assets/interface-sounds (UI click / confirm / error)
- **Impact Sounds** — https://kenney.nl/assets/impact-sounds (kneading)
- **RPG Audio** — https://kenney.nl/assets/rpg-audio (chopping, knife, coins, containers, cloth)
- **Music Jingles** — https://kenney.nl/assets/music-jingles (menu / day start / day end)

Licence text: https://creativecommons.org/publicdomain/zero/1.0/

## Ambience (Gregor Quendel)

`Content/Audio/S_AmbStreet`, `S_AmbNight` and `S_AmbRain` derive from:

- **Free City & Nature Sounds** — Gregor Quendel (Cinematic Sound Design), Fab.
  Licence: **Creative Commons Attribution 4.0 (CC BY 4.0)**,
  https://creativecommons.org/licenses/by/4.0/

CC BY 4.0 requires attribution and a statement of what was changed. The changes:

- `S_AmbStreet` — 45 seconds from 1:18 of
  `WAV_City_Ambience_Traffic_Street_Cars_and_tram.wav`; made seamless with a tail
  crossfade and peak-normalised.
- `S_AmbNight` — 45 seconds from 3:10 of the same recording (its quietest
  stretch), pushed back with a 900 Hz low-pass and looped the same way. The night
  bed coming off the same street as the day bed is deliberate.
- `S_AmbRain` — 43 seconds from 0:03 of
  `WAV_Rain_Dropping_on_various_textures.wav`, same loop treatment.

## Engine content
- `/Engine/BasicShapes` primitive meshes and the `Roboto` font ship with Unreal
  Engine; the world and the characters are built at runtime from a mix of those
  primitives and the Kenney meshes.

## Code
Every gameplay system is original C++. There is no Blueprint gameplay logic.
