# Fab: what needs doing by hand

Everything else in the art pass has been done from packs already on disk. These
two need a person, because getting a Fab asset into a UE project goes through
the Epic Games Launcher, which is a desktop application and not reachable from
browser automation. The Fab web session was used successfully to research the
library and to press the download control; that control hands off to the
Launcher, and the handoff is where automation stops.

Nothing here is urgent. The shop, the stations, the walls and the customers are
all done without them. These close the animation gaps listed in
`docs/Art/ANIMATION_MAPPING.md`, and until they are done a seated customer idles
with straight legs and the staff have no preparation animations at all.

---

## 1. MC Sample Animation Pack — MoCap Central  ← do this one

- **Fab URL:** https://www.fab.com/listings/fba58a40-dc18-475a-b726-b04345f39697
- **Price:** free, already in your library
- **Licence:** Standard. Last updated 18 June 2026.
- **Engine:** 5.0 – 5.8.
- **Format: Asset package** — not a complete project, so it has a plain
  **Add to Project** and lands straight in this project's Content.

This is the better answer to the animation gaps, found after the Game Animation
Sample attempt below hit the complete-project wall:

- **Rigged to the standard UE5 Mannequin**, which is the skeleton the customers
  already use — so no IK Rig, no retargeter, no migration. The sequences load
  and play.
- 120+ animations, and the listing names the two gaps by name: **sitting** (at a
  piano, but a seated pose is a seated pose) and eating off the floor, which is
  a hand-to-mouth loop that can be retimed.
- Idle variety and emotional reactions — happy, sad, injured, drunk — which is
  what a queue of six identical idling customers needs.
- Clean start/end poses, so transitions blend without extra work.

Steps:

1. Epic Games Launcher → Unreal Engine → Library → Fab Library
2. Find **MC Sample Animation Pack** → **Add to Project**
3. Pick `CigkofteSimulator`, confirm the 5.8 version

Then tell me and I will wire sitting, eating, idle variety and the reaction
states, and add the cook entries.

---

## 2. Game Animation Sample — Epic Games

- **Fab URL:** https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016
- **Price:** free, already in your library ("Kütüphanem'e kaydedildi")
- **Licence:** Standard, UE-only content. Last updated 17 June 2026.
- **Engine:** 5.4 – 5.8, so 5.8 is covered.
- **Why this one:** 500+ game-ready animations built against the UE5 mannequin
  skeleton — the skeleton the customers already use. It is the single biggest
  source for the states that have no animation today.

**The catch:** it ships as a **Complete project**, not an asset pack. There is
no "Add to Project" for it. It has to be created as its own project and the
animations migrated across.

Steps:

1. Epic Games Launcher → Unreal Engine → Library → Fab Library
2. Find **Game Animation Sample**, click **Create Project**, pick UE 5.8
3. Open that project in the editor
4. Content Browser → select the animation folders you want → right-click →
   **Asset Actions → Migrate**
5. Target: `C:\Users\adila\Documents\Unreal Projects\CigkofteSimulator\Content`

What to migrate, in priority order — these are the gaps:

- **sit down / seated idle / stand up** (customers at tables, and the current
  stopgap drops the body 45cm and keeps idling)
- **eating or hand-to-mouth** loop
- idle variations, so a queue of six does not look like six copies
- reaction poses: happy, annoyed, checking a watch, looking at a phone

Once migrated, tell me the folder and I will wire the states and add the cook
entries.

---

## 3. Stylized Catcafe 110 Asset Pack — China Capture

- **Price:** free, already in your library
- **Why:** the best stylistic match in the library for the shop interior —
  wooden crates, brick, warm orange, stylised. See `docs/Art/FAB_ASSET_PLAN.md`.
- **Status: optional.** The shop was furnished from `MMSupermarket`, which was
  already on disk, so this is an upgrade rather than a need. Worth doing if the
  bakery-counter look ever feels too clean for a street shop.

Steps: Launcher → Fab Library → **Add to Project** → pick this project. Asset
packs like this one do have that option; only complete projects do not.

---

## Licence note worth keeping

Game Animation Sample's Fab listing carries **"Yapay zekâ ile kullanılmasına
izin ver: Hayır"** — the publisher has not permitted the content to be used with
AI. Read as the industry norm, that covers training or generating from the
asset, not shipping it inside a game, which is what the Standard Licence is for.
Recorded here rather than assumed away; if the pack is ever used for anything
beyond being animation data in the game, check the terms again.

---

## What was already tried

- Fab library searched over the whole 2.6K catalogue: food, kitchen, low poly,
  character, animation, stylized, props, icon, furniture, vfx, market.
- The download control was pressed on both packs. Neither produced a browser
  download or a dialog: every Fab listing here offers Unreal Engine format only,
  with no generic FBX to fetch directly, so the handoff always goes to the
  Launcher and out of the browser's reach.
- The Launcher was running headless with no window at all (`MainWindowHandle`
  0). Restarting it did open a window — it is open now, on the account, and
  should be showing the Fab listing after the last click.
- A desktop screenshot came back entirely black: the session is locked. That is
  the actual blocker. Nothing on the desktop can be driven until someone unlocks
  the machine, and browser automation cannot reach a native window regardless.
- No purchase was made, no licence accepted, no account setting changed.

Neither of these blocks any current work. Everything shipped so far came from
packs already on disk.
