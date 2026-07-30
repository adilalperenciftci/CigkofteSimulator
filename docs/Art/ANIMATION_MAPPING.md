# Animation mapping

What each animation state needs, where it comes from, and — the part worth
reading — which ones no pack will ever contain.

## Current reality

The game has no character animation at all. Customers, the player and the
apprentice are static primitives that slide between positions. The only skeletal
mesh is the cat, and only when `Cat_Animation_Pack` is installed
(`ACigCat::TrySetupSkeletalCat`, three sequences: idle, walk, sit).

## Skeleton decision — revised once MC_Sample was actually installed

**Customers run on MC_Sample's own skeleton** (`SKM_MCUE5v2_Skeleton`), with the
UE5 mannequin as the fallback when that pack is absent.

The listing says MC_Sample is "rigged to the standard UE5 Mannequin" and the
bone hierarchy may well match, but every sequence in it references its own
`USkeleton` asset. `PlayAnimation` across two different skeleton assets does not
play — it fails silently and leaves the mesh in bind pose. Caught by querying a
loaded sequence's `Skeleton` property before wiring anything, which is the only
reason it did not ship as a queue of frozen customers.

So body and animation come from the same pack, always. Two bodies
(`SKM_MCUE5v2`, `SKM_MCUE5Fv2`) chosen by seed, and if MC_Sample is missing the
code falls back to Manny/Quinn with mannequin locomotion and no sit or
reactions.

**Retarget target for anything new: the UE5 mannequin skeleton**
(`Content/Characters`, already installed).

It is what `Game Animation Sample`, `Animation Starter Pack` and most Fab and
Mixamo animation packs ship against, so it is the one choice that avoids
retargeting every source separately. Manny and Quinn themselves stay placeholder;
whichever stylised character pack wins in A4 gets retargeted onto this skeleton,
not the other way round.

Pipeline per source pack: IK Rig on source → IK Rig on target → IK Retargeter →
export retargeted sequences into `Content/CigkofteSimulator/Art/Animations/`.
Vendor files are never edited in place.

## Customers

Movement is driven by `CharacterMovement` and the NavMesh, so walk and run must
be **in place**; root motion would fight the movement component.

| State | Source | Root motion | Loop | Notes |
|---|---|---|---|---|
| WalkingToQueue | Game Animation Sample / Starter Pack walk | In place | Yes | Blend space with speed |
| Waiting | **Done** — three MC_Sample idles by seed | In place | Yes | LookAround, ScratchArm, Conv_Talk. Six customers on one idle moved in lockstep and read as a bug. |
| Ordering | Free Animation Library gesture | In place | No | Talk/point gesture |
| WaitingForFood | Idle variants, impatience | In place | Yes | Check watch / phone — **gesture packs may not have these; see gaps** |
| ReceivingFood | Finger Poses + reach | In place | No | Hand alignment matters, see below |
| Paying | Reach gesture | In place | No | Cash or card hand-off |
| Happy | **Done** — `am_Stand_React_Excited_01` | In place | No | Plays while still at the counter; walking wins once they turn to leave. |
| Angry | **Done** — `am_Stand_Emotion_Frustrated_01_All` | In place | No | Same rule — stomping while gliding to the door reads as broken. |
| WalkingToSeat | Same walk | In place | Yes | |
| Sitting | **Done** — `am_SitPiano_Play_01` | In place | Yes | A seated piano performance: seated, hands forward, which at a table reads as leaning over food. |
| Eating | Covered by the sit loop | In place | Yes | No separate hand-to-mouth animation; the seated pose carries it for now. |
| Leaving | Walk | In place | Yes | |

## Staff and player

Only the apprentice has a body here. The player is in first person, so the staff
NPC is the one figure in the shop anybody ever sees do the job — which is why
these are wired to `ECigStaffTask` and nowhere else.

| State | Source | Notes |
|---|---|---|
| Idle | MC_Sample idles | At the counter |
| Kneading | **Done** — `am_StandDrillLow_01_Drill` | Standing over a surface, both hands working something at waist height, repeating |
| Chopping | **Done** — same clip | Same motion at this distance; see below |
| Cleaning | **Done** — same clip | Also a pair of hands worked over a surface |
| AddingIngredient / restock | **Done** — `am_Vend_Start` | Reaching into a machine is the shape of reaching into a tub |
| Serving | **Done** — `am_Vend_Success_GrabItem` | Taking the item out and offering it forward |
| Wrapping / packing | **Done** — `am_SpellBook_02_Read_Loop_01` | Both hands in front, working over something held between them |
| BuildingWrap | Covered by the wrap loop | No separate topping-placing motion |

### Why these clips

All four were already in the project, on the right skeleton, licensed with
MC_Sample. Nothing was downloaded and nothing needed retargeting.

They are chosen by **what the motion is**, not by what the clip was named for. A
drill held low and a knife worked on a board are the same silhouette from across
a shop; a vending machine and an ingredient tub are both something you reach into
and take from. Four clips cover five jobs because two of the jobs genuinely look
alike, and splitting them would need animations that do not exist.

This is option 2 from the list below — retarget an approximate clip — chosen over
option 3 because the apprentice is watched from several metres away across a
counter, where the difference between an approximate motion and an authored one
is smaller than the difference between an approximate motion and none.

## The cat

Already working where the pack is installed: idle, walk, sit. Missing from the
requested list: run, eating, sleeping, meowing, approaching the player. Check
`Animals FREE` (ithappy) and the existing `Cat_Animation_Pack` contents before
assuming these need authoring.

## The gaps, stated plainly

**No general animation pack contains kneading çiğköfte, rolling a dürüm or
portioning bulgur.** They are specific to this game and will not appear by
downloading more packs — which is the finding, not a research failure. What
exists are motions of the right shape, and the staff table above is now filled
from them.

Still genuinely missing: the two customer states, WaitingForFood impatience
(checking a watch or a phone) and a distinct ReceivingFood reach.

Realistic options, cheapest first:

1. **Procedural, no animation asset.** The kneading station already pulses the
   dough on each stroke. A hand or utensil moved by Control Rig or a simple
   timeline, driven by the same state, reads as motion without a mocap clip.
2. **Retarget an approximate mocap clip.** A generic "reach and place" or
   "hammer" motion retimed for kneading. Cheap, and honest enough at the camera
   distance this game uses.
3. **Author montages by hand in Control Rig.** Best result, most work, and only
   worth it for kneading and wrapping — the two the player watches.

Sitting and eating are worth solving properly because A6 makes them visible for
the whole time a customer occupies a table.

## Rules for integration

- Vendor sequences are never modified; retargeted copies live in the project's
  own `Art/Animations` folder.
- Anim notifies are for **visual sync only**. No money, state change or progress
  may depend on a notify firing — the gameplay systems already own that, and a
  missing animation must not stop a sale.
- If an animation is absent, the state falls back to idle and the game continues.
  This is the same rule the mesh library already follows.
- Seat animations align to the chair transform, not the other way round.
- Foot sliding is checked against the movement speed the blend space assumes.
