# External assets

Every third-party file used by the shipped demo must appear here with its licence.
Nothing is added to the repository without an explicit commercial-use permission.

## Currently in the repository

| Asset set | Author | Source | Licence | Obtained | Files | Attribution |
|---|---|---|---|---|---|---|
| Food Kit, Furniture Kit | Kenney | kenney.nl | CC0 1.0 | pre-existing | `Content/LowPoly/Food`, `Content/LowPoly/Furniture` | not required, credited in `CREDITS.md` |
| Interface / Impact / RPG / Music Jingles | Kenney | kenney.nl | CC0 1.0 | pre-existing | `Content/Audio/S_*.uasset` | not required, credited in `CREDITS.md` |

## Referenced but deliberately not committed

Listed in `.gitignore`; the game falls back to engine primitives when absent.
Their licences do not permit redistribution from this repository.

Present on the development machine as of 2026-07-26 (so the game already runs
with real art locally): `dukkan` (317 MB), `CityPark` (3.3 GB),
`Scene_Bazaar_Vol1` (6.1 GB), `CitySampleBuildings` (15 GB), `ModularBuildingSet`
(732 MB), `MMSupermarket` (835 MB), `Scene_Banquet`, `Cat_Animation_Pack`,
`Characters`. Absent: `Fab`, `ModellerEnistem`, `ModellerEnistem2`.

`Content/dukkan`, `Content/Fab`, `Content/ModellerEnistem`,
`Content/ModellerEnistem2`, `Content/CityPark`, `Content/Scene_Bazaar_Vol1`,
`Content/CitySampleBuildings`, `Content/ModularBuildingSet`,
`Content/MMSupermarket`, `Content/Scene_Banquet`, `Content/Cat_Animation_Pack`,
`Content/Characters`.

## Available in the owner's Fab library — not yet imported

Checked 2026-07-26 in the account's Fab library (`fab.com/library`, audio filter:
11 items). These are already owned, so nothing has to be sourced externally. Each
still needs its Fab licence recorded here once imported, since Fab "free" listings
carry a specific licence rather than CC0 by default.

| Pack | Publisher | Covers |
|---|---|---|
| Free City Ambiences | rawAmbience | `S_AmbStreet`, `S_AmbNight` |
| Free Thunder Sounds | Gregor Quendel | `S_AmbRain` |
| Free City & Nature Sounds | Gregor Quendel | street and outdoor layers |
| Free Crowd Cheering Sounds | Gregor Quendel | match-day crowd (Stage 7) |
| 50 Free Game Sounds Pack | PlaceHolder Inc | car engine, dishes, general gaps |

### Import procedure

The ambience layer looks these paths up by name and stays silent when they are
absent, so importing requires no code change:

| Expected path | Source |
|---|---|
| `/Game/Audio/S_AmbStreet` | Free City Ambiences — daytime street loop |
| `/Game/Audio/S_AmbNight` | Free City Ambiences — night loop |
| `/Game/Audio/S_AmbRain` | Free Thunder Sounds — rain loop |

1. Add the packs to the project from the Epic Games Launcher, or download the
   source audio from the Fab listing.
2. Place or import the chosen loops as `Content/Audio/S_AmbStreet`,
   `S_AmbNight`, `S_AmbRain`. UE imports `.wav`, `.ogg`, `.flac` and `.aiff`;
   it does not import `.mp3`, and no converter is installed on this machine.
3. Confirm the layer resolves: the log line `Ortam sesi yok:` must stop appearing.
4. Record each pack's exact Fab licence in the table above before committing.

### Redistribution

Fab licences generally permit use in a shipped game but **not** redistribution of
the source assets. Cooked audio in a packaged build is fine; committing the raw
pack contents to this public repository is not. Import the loops, verify the
licence, and add `Content/Audio/S_Amb*` to the repository only if the licence
allows it — otherwise treat them like the other Fab packs and gitignore them.

## Externally verified alternative (only if the library packs cannot be used)

`Rain in the Gutter Loop` by Ogrebane, opengameart.org, licence stated as **CC0**,
`rain-gutter-loop_0.mp3`, 1.5 MB. Verified 2026-07-26. Needs conversion to a
UE-importable format, which this machine cannot do without ffmpeg.

## Prohibited

Ripped assets, trademarked restaurant or club branding, football club marks, and
any file whose licence cannot be produced on request.
