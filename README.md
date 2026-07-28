# Çiğköfte Simulator

*[Türkçe](README.tr.md)*

**Run a neighbourhood çiğköfte shop from nothing.** Knead the bulgur, judge the
isot, roll the wrap, and get it to the customer before their patience runs out —
then spend what you earn on the shop. A first-person cooking and business
simulator written **entirely in C++** on Unreal Engine 5.8.

> Çiğköfte is a Turkish street food: spiced bulgur kneaded by hand, wrapped in
> flatbread with herbs and pomegranate molasses. The shop, the menu and the
> neighbourhood are Turkish; the game itself is playable in **Turkish or
> English**, switchable in Settings at any time.

> An **EG Games** production.

![The shop, the seating area, the street, the square and the market](docs/demo.gif)

*Twelve seconds of the fixed tour route. Recorded from a packaged build with
`Scripts/Record-Demo.ps1`, which produces the same clip every time rather than
whatever the person holding the mouse managed on the day.*

![The ingredient stations, with the next customer's order on the right](docs/screenshots/02_mutfak.png)

*Bulgur, isot, tomato paste, water, spice — each measured separately, against the
order on the right and the recipe on the left.*

| | |
| --- | --- |
| ![The shop front and the street](docs/screenshots/01_servis.png) | ![The kneading counter](docs/screenshots/03_yogurma.png) |
| *The shop front, the awning and the queue coming off the street* | *Kneading: hold the rhythm with the left mouse button* |
| ![Tablet — shop upgrades](docs/screenshots/04_tablet_dukkan.png) | ![Tablet — quests](docs/screenshots/05_tablet_gorevler.png) |
| *Shop upgrades* | *Daily quests, story goals, regulars* |
| ![The seating area](docs/screenshots/06_salon.png) | |
| *Customers who eat in take a table* | |

## Playing it

A day is three minutes. It opens, customers arrive in order, they place an order,
and their patience bar fills while you work. At close, rent comes out, earnings
and reputation are settled, and the next day opens.

**The life of one wrap:**

1. **The mix** — measure from the bulgur, isot, paste, water and spice stations.
   The heat the customer asked for (mild / medium / hot) is a function of how much
   isot went in.
2. **Kneading** — look at the counter and press. Every stroke works the mix a
   little further.
3. **Chopping** — lettuce, parsley, tomato, pickle, onion, lemon, pomegranate
   molasses.
4. **Flatbread and rolling** — lay the bread, pick toppings with `1`–`7`, `8` for
   ayran, `9` for a side, `F` to roll it.
5. **Service** — on a plate or in a bag? Give them what they asked for, take the
   money and the XP.

**The menu is not only wraps.** The side station opens at level 3: içli köfte,
lentil soup, künefe and tea. A customer may ask for künefe alongside their wrap;
get it right and both the price and the score go up, forget it and the order
score drops. The supplier tab also sets **ingredient quality** — cheap stock
leaves more profit, but a gourmet customer can taste the difference, and good
stock costs more.

The closer the order matches what was asked for, the better the tip and the
reputation. Wrong heat, a missing topping or a late service makes them angry.

## A world that opens up

The game does not start with everything unlocked. Day one is a plain counter:
bulgur, isot, paste, water, spice, kneading, flatbread and service. Chopping,
packing, the fridge, the tea urn and the cat bowl stay locked — they read
"LEVEL N" and tell you when you touch them. Customers will not ask for what a
locked station does either: nobody orders toppings before chopping opens, and
nobody orders takeaway before packing does.

The neighbourhood grows the same way. The junctions at both ends of the main
street, and the roads running west from them, start behind construction
barriers. Each level lifts one, the district comes alive, and new delivery
addresses open with it:

| Level | District | What is there |
| --- | --- | --- |
| 2 | The market | Awned stalls, produce, market traders |
| 3 | Republic Square | Fountain, clock tower, benches, crowds |
| 4 | School and park | School building, flagpole, swings and slide, students |
| 5 | Industrial — supplier depot | Warehouses, loading ramp, barrels, a lorry |
| 6 | City stadium | Stand ring, floodlight masts, match-day crush |
| 7 | The seafront | Sea, sand, palms, parasols, the promenade railing |

The sun drops as the day runs and the street lights come on in the evening; rain
wets the ground, greys the sky and streaks the screen; a heatwave turns the world
yellow; a power cut kills the shop lights.

## Mastery, prestige and achievements

Every level grants a **skill point**, spent from the tablet's Skills tab:

| Skill | Effect |
| --- | --- |
| Fast Hands | Kneading strokes 15% more effective per rank |
| Sharp Knife | One fewer chop stroke per rank |
| Friendly Face | Tips and reputation up 12% per rank |
| Clean Worker | Dirt accumulates 15% slower per rank |
| Strong Constitution | Energy drain down 20% per rank |
| Quick Feet | Run speed up 8% per rank |
| Hard Bargainer | Stock costs down 8% per rank |
| Kitchen Master | Dough quality up 5% per rank |

Reach level 10 with 20,000 lira in the till and you can **hand the shop on**
(prestige). Day, level, money and every investment reset; skills, achievements
and the unlocked districts stay, and each handover leaves a permanent **12%
income bonus** and three skill points.

There are **20 achievements**, from the first wrap to a thousand, from the first
delivery to five stars, from a fully equipped shop to handing it on. They sit in
the tablet's Achievements tab with your lifetime statistics.

## Systems

Eighteen independent systems are what make this a business rather than a
click-and-sell loop:

| System | What it does |
| --- | --- |
| **Customers** | 14 traits — Impatient, Gourmet, Influencer, Tourist, Student, Regular, Secret Critic… each orders differently and reacts differently. |
| **Orders and recipes** | Heat, portion, topping mask, ayran, bag or plate. New recipes unlock with level. |
| **Economy** | Pricing, rent, suppliers, stock costs, the daily profit and loss. |
| **Stock** | 14 ingredients, supplier orders, shortages and price-hike events. |
| **Hygiene** | Counters get dirty, gloves get changed, hands get washed, dishes pile up. A fastidious customer notices. |
| **Reputation and reviews** | Customers score you and write reviews; the average drives footfall. |
| **Rivals** | The other shops in the neighbourhood run promotions and take customers. |
| **Events** | School turnout, match day, rain, heat, power cuts, council inspections, a street festival, an isot price hike, and more. |
| **Quests** | Daily and chained goals, with rewards. |
| **Staff** | Hire, pay wages, hand work over. |
| **Delivery** | The car opens at level 4; deliver to addresses across the city. |
| **Progression** | 10 levels, XP, titles, lifetime statistics. |
| **Skills** | 8 mastery skills, a point per level, and the prestige loop. |
| **Achievements** | 20 achievements and a statistics screen. |
| **The shop** | 13 upgrades — big fridge, air conditioning, sound system, second till, outdoor seating, a second branch… each changes both the world and the play. |
| **The cat** | The shop has a cat. Name it, feed it, pet it. |
| **Saving** | Versioned save system, `F5` / `F9`. |
| **Audio** | Kneading, chopping, the till, jingles — all event-driven. |

The tablet (`T`) manages stock, recipes, shop upgrades, suppliers, rivals,
reviews, quests and staff.

## Controls

Keyboard and mouse or **gamepad**; the HUD hints follow whichever you last used.

| Key | Gamepad | Action |
| --- | --- | --- |
| `W A S D` | Left stick | Move |
| `Shift` | L3 | Run |
| `Space` | Y | Jump |
| `Mouse` | Right stick | Look |
| `E` | A | Interact (station / car / pet the cat / deliver) |
| `LMB` | RT | Kneading stroke |
| `1` – `7` | LB/RB to select, B to apply | Add or remove a topping |
| `8` | same | Ayran on or off |
| `9` | same | Add a side (içli köfte / soup / künefe / tea) |
| `F` | X | Roll the wrap and finish it |
| `G` | R3 | Put the rolled wrap on the shelf |
| `T` | Select | Tablet (stock, shop, skills, achievements…) |
| `O` | — | Settings |
| `Esc` / `P` | Start | Pause |
| `F5` / `F9` | — | Save / load |
| `F1` | — | Debug HUD |
| `R` | — | Restart after game over |
| `↑ ↓ ← →` + `Enter` | D-pad + A | Menu and tablet navigation (tabs: LB/RB) |

## Building it

You need **Unreal Engine 5.8** and Visual Studio 2022 with the C++ desktop
workload.

```bash
git clone https://github.com/adilalperenciftci/CigkofteSimulator.git
cd CigkofteSimulator
```

Either right-click `CigkofteSimulator.uproject` → **Generate Visual Studio
project files** and build the solution, or build from the command line:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    CigkofteSimulatorEditor Win64 Development `
    -project="$PWD\CigkofteSimulator.uproject" -WaitMutex
```

After that, double-click the `.uproject`. There is no level to open — the world
is built at runtime, so pressing Play builds the shop.

### About the asset packs

> **The repository builds and runs with no extra downloads.** None of the packs
> below are needed to compile. `CigMeshLibrary` returns `nullptr` when an asset
> is missing and the caller falls back to an `/Engine/BasicShapes` primitive, so
> the game is playable end to end as boxes. Missing assets are logged as
> `Warning` on the `LogCig` channel, not as errors.

What is in the repository and what is not:

| Folder | Status | Source |
| --- | --- | --- |
| `Content/LowPoly/` | **In the repo** | Kenney Food Kit + Furniture Kit (CC0) |
| `Content/Audio/` | **In the repo** | Kenney Interface/Impact/RPG/Music Jingles (CC0) |
| `Content/dukkan/`, `Content/Fab/`, `Content/ModellerEnistem/`, `Content/ModellerEnistem2/` | Not in the repo | Fab / Unreal Marketplace store content |
| `Content/CityPark/` | Not in the repo | City park scene pack (Fab) |
| `Content/Scene_Bazaar_Vol1/` | Not in the repo | Bazaar scene pack (Fab) |
| `Content/CitySampleBuildings/`, `Content/ModularBuildingSet/`, `Content/MMSupermarket/`, `Content/Scene_Banquet/` | Not in the repo | Building and interior packs (Fab) |
| `Content/Cat_Animation_Pack/` | Not in the repo | Cat model and animations (Fab) |
| `Content/Characters/`, `Content/MC_Sample/` | Not in the repo | Character meshes and motion capture (Fab) |

The heavy packs are in `.gitignore`: individual `.uasset` files exceed GitHub's
100 MB limit and their licences do not permit redistribution. If you own them,
download them from your own Fab library and copy them under `Content/` with the
folder names in the table — there is nothing to change in code, because mesh
paths resolve against exactly those names (`World/CigMeshLibrary.cpp`).

## Architecture

`ACigkofteGameMode` is a coordinator; the gameplay is split across 18 systems
deriving from `UCigSystem` (a `UObject`), foldered under
`Source/CigkofteSimulator/`:

```
Core/        shared types, logging, upgrade definitions
Game/        GameMode + day loop + the system base class
World/       runtime world builder, stations, mesh library
Cooking/  Orders/  Customers/  Economy/  Inventory/
Progression/  Quests/  Events/  Delivery/  Hygiene/  Staff/  Cat/
Player/  Vehicles/  UI/  Save/  Audio/  Debug/
```

There is no Blueprint gameplay logic; everything including the HUD is drawn from
C++. The world mixes `/Engine/BasicShapes` primitives with Kenney low-poly
meshes — when an asset cannot be found the system falls back to a primitive, so
the project runs with assets missing.

### Balance numbers live in data

Skills, shop upgrades, customer traits, stock items and achievements are read
from `Config/Balance/*.csv`:

| File | What it tunes |
| --- | --- |
| `Skills.csv` | Skill name, maximum rank, effect per rank |
| `Upgrades.csv` | Upgrade price and unlock level |
| `Traits.csv` | Trait pool weight, first day seen, patience and tip effects |
| `Stock.csv` | Ingredient base price, starting stock, order quantity |
| `Achievements.csv` | Which statistic an achievement watches, and its threshold |

Rows map to `FTableRowBase` USTRUCTs (`Core/CigBalanceTypes.h`), so the same
columns can be made into a `UDataTable` asset in the editor if wanted. At runtime
the CSV is read directly, because `UDataTable`'s CSV import is editor-only.

If a file is missing, malformed or short a row, the C++ defaults in
`Core/CigBalance.cpp` apply — delete all of them and the game plays with the same
balance. With the game running, `CigReloadBalance` re-reads the tables, so trying
a number out does not need a rebuild.

### Customer dialogue: a production pipeline, not a runtime API

Customer lines are not requested from an API while playing. They are **generated
in bulk during development and shipped as data**, so the runtime cost is zero,
there is no latency, the game works offline, and there is no moderation risk.

The state space is finite: 5 moods × 15 dominant traits × 2⁵ (VIP, regular,
ayran, hygiene, patience) = **2,400 buckets**. At four variants each, about 9,600
lines.

```
1. In the editor console:  CigGenerateDialogue  → Saved/Dialogue/prompts.jsonl (2,400 prompts)
2. python Tools/generate_dialogue.py            → Config/Dialogue/Lines.csv
3. The CSV is reviewed and committed
```

Step two is a separate script because it costs money, can take hours and can be
interrupted; it resumes where it stopped (`--limit N` runs a small trial first)
and expects `ANTHROPIC_API_KEY` in the environment.

The table is bilingual (`TR`, `EN`), so English comes off the same line for free.
If a bucket is missing the game falls back to lines held in the text table. A
reviewed seed table ships in the repository.

### Randomness is deterministic

Every die roll that affects play runs through one `FRandomStream`
(`Core/CigRandomSubsystem.h`). The seed and the stream position are written to
the save: the same seed plays the same day the same way, and loading does not
rewind the stream. To reproduce a bug report exactly, read the seed with `CigSeed`
and hand it back with `CigSetSeed <n>`. World decoration, the cat's fur colour,
traffic and which dialogue line is picked are deliberately outside that stream —
they do not change game state.

`F1` opens the debug HUD during development, and the console has dozens of exec
commands: `StartDayNow`, `AddMoney`, `SpawnCustomer`, `UnlockAllUpgrades`,
`SetTimeScale`, `CigSeed`, `CigSetSeed`, `CigReloadBalance` and more
(`Debug/CigCheatManager.h`). The screenshots above are produced by `CigShots`:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
    "$PWD\CigkofteSimulator.uproject" -game -windowed -ResX=1600 -ResY=900 -ExecCmds="CigShots"
```

## Licence and credits

All of the code is original and **all rights are reserved** — the repository is
publicly viewable but it is not open source; see [LICENSE](LICENSE) for terms.

The art and audio assets in the repository come from **Kenney** (CC0, public
domain) and fall outside that licence — see [CREDITS.md](CREDITS.md).
