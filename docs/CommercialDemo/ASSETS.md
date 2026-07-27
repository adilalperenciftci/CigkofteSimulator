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

## The owner's Fab audio library

Checked 2026-07-26 in the account's Fab library (`fab.com/library`, audio filter:
11 items). Fab "free" does not mean one licence: each listing carries its own,
and only some of them permit redistribution. That distinction decided which pack
the shipped ambience actually came from.

| Pack | Publisher | Licence | Browser download | Redistributable |
|---|---|---|---|---|
| Free City & Nature Sounds | Gregor Quendel | CC BY 4.0 | `.unitypackage`, 403.89 MB | yes, with attribution |
| Free City Ambiences | rawAmbience | Fab Standard | `free_city_ambiences.zip`, 169.44 MB | no |
| Free Thunder Sounds | Gregor Quendel | CC BY 4.0 | none — Launcher only | yes, but unreachable |
| Free Crowd Cheering Sounds | Gregor Quendel | not yet checked | — | — |
| 50 Free Game Sounds Pack | PlaceHolder Inc | not yet checked | — | — |

The Fab website only offers a direct download when the publisher uploaded an
"additional file"; otherwise the pack is reachable solely through the Epic Games
Launcher or the Fab UE5 plugin. Free Thunder Sounds is in that second group, so
its rain material could not be fetched even though its licence is the permissive
one.

## Ambience beds — done

All three beds are in the repository and resolve at runtime.

| Path | Source | State |
|---|---|---|
| `/Game/Audio/S_AmbStreet` | Free City & Nature Sounds — traffic street, cars and tram | 45.0 s, stereo 44.1 kHz, looping |
| `/Game/Audio/S_AmbNight` | same recording, calmest stretch, low-passed | 45.0 s, stereo 44.1 kHz, looping |
| `/Game/Audio/S_AmbRain` | Free City & Nature Sounds — rain on various textures | 43.0 s, stereo 44.1 kHz, looping |

Everything came from the CC BY 4.0 pack rather than the two packs the paths were
originally sketched against. Fab Standard forbids redistributing the source
audio, so a `Free City Ambiences` bed could have shipped in a packaged build but
not in this public repository — which would have left the repo with two beds and
a hole. Attribution and the exact edits are recorded in `CREDITS.md`, as CC BY
requires.

The night bed is derived from the day recording rather than sourced separately.
The pack has no night city ambience, and deriving it has an advantage over
finding one: night is the same street heard later, so it should be the same
street. The calmest 45 s window (starting 190 s in, chosen by scoring every
window for loudness and spikiness) low-passed at 900 Hz gives distance and an
empty road without changing location.

### How they were produced

`Tools/make_ambience.py` rebuilds all three from the downloaded pack:

```
python Tools/make_ambience.py <city_nature_sounds_unity.unitypackage> <outdir>
```

Verified byte-identical to the committed assets' sources. No editor work and no
manual audio tool is involved, which is the point: the source pack is licensed
and cannot live in the repository, so without a script "where did S_AmbNight
come from" would have no answer.

What the script does, and what was checked:

1. `.unitypackage` is a gzipped tar, so the source WAVs are extracted with `tar`
   and mapped back to their real names through each entry's `pathname` file.
2. Loops are cut with a tail crossfade: the material just past the loop end is
   faded over the opening with an equal-power curve, so the last sample leads
   into the first. The script checks each wrap-point step against the file's own
   sample-to-sample motion and reports OK or CLICK — 118, 22 and 253 against
   p99.9 steps of 1112, 573 and 12122.
3. Imported as `USoundWave` through a headless `-run=pythonscript` commandlet
   with `looping = True`. That flag is not cosmetic: `TickAmbience` spawns each
   bed once and never retriggers it, so a one-shot import would play for
   45 seconds and then leave the rest of the day silent.
4. Verified by reloading the saved assets in a second commandlet and resolving
   the exact object paths `ResolveOrtam` passes to `LoadObject`. All three
   return `SoundWave`; the `Ortam sesi yok:` log line no longer fires.

**Not verified: nobody has listened to them.** Every check above is analytic —
format, seam arithmetic, the looping flag, path resolution. Whether the night bed
reads as night, and whether the layer sits at the right level against the Kenney
one-shots, needs a play session. Tracked in `KNOWN_LIMITATIONS.md`.

## Prohibited

Ripped assets, trademarked restaurant or club branding, football club marks, and
any file whose licence cannot be produced on request.
