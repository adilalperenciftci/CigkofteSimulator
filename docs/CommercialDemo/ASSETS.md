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

`Content/dukkan`, `Content/Fab`, `Content/ModellerEnistem`,
`Content/ModellerEnistem2`, `Content/CityPark`, `Content/Scene_Bazaar_Vol1`,
`Content/CitySampleBuildings`, `Content/ModularBuildingSet`,
`Content/MMSupermarket`, `Content/Scene_Banquet`, `Content/Cat_Animation_Pack`,
`Content/Characters`.

## Required and still missing

These block Stage 8 and parts of the definition of done. They cannot be authored
in this environment.

| Need | Used by | Expected path | Status |
|---|---|---|---|
| Street ambience loop | `UCigAudioSubsystem::TickAmbience` | `/Game/Audio/S_AmbStreet` | missing, layer silent |
| Night ambience loop | same | `/Game/Audio/S_AmbNight` | missing, layer silent |
| Rain ambience loop | same | `/Game/Audio/S_AmbRain` | missing, layer silent |
| Car engine | `ECigSound::CarEngine` | `/Game/Audio/S_Car*` | currently mapped to a metal one-shot |
| Cat meow | `ECigSound::CatMeow` | `/Game/Audio/S_Cat*` | unmapped, silent |

Kenney and other CC0 libraries carry suitable ambience beds. Importing them at the
paths above requires no code change.

## Prohibited

Ripped assets, trademarked restaurant or club branding, football club marks, and
any file whose licence cannot be produced on request.
