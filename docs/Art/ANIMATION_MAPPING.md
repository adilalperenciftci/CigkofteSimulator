# Animation mapping

What each animation state needs, where it comes from, and — the part worth
reading — which ones no pack will ever contain.

## Current reality

The game has no character animation at all. Customers, the player and the
apprentice are static primitives that slide between positions. The only skeletal
mesh is the cat, and only when `Cat_Animation_Pack` is installed
(`ACigCat::TrySetupSkeletalCat`, three sequences: idle, walk, sit).

## Skeleton decision

**Target: the UE5 mannequin skeleton** (`Content/Characters`, already installed).

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
| Waiting | Starter Pack idle + MC Sample idles | In place | Yes | Vary per customer so a queue is not six clones |
| Ordering | Free Animation Library gesture | In place | No | Talk/point gesture |
| WaitingForFood | Idle variants, impatience | In place | Yes | Check watch / phone — **gesture packs may not have these; see gaps** |
| ReceivingFood | Finger Poses + reach | In place | No | Hand alignment matters, see below |
| Paying | Reach gesture | In place | No | Cash or card hand-off |
| Happy | Starter Pack reaction | In place | No | |
| Angry | Starter Pack reaction | In place | No | |
| WalkingToSeat | Same walk | In place | Yes | |
| Sitting | **Gap** | In place | No | Sit-down transition, aligned to chair transform |
| Eating | **Gap** | In place | Yes | Hand-to-mouth loop |
| Leaving | Walk | In place | Yes | |

## Staff and player

| State | Source | Notes |
|---|---|---|
| Idle | Starter Pack | At the counter |
| AddingIngredient | **Gap** | Reach into a tub and pour |
| Kneading | **Gap** | The signature motion of this game |
| Chopping | **Gap** | Repeating downstroke |
| BuildingWrap | **Gap** | Placing toppings |
| Wrapping | **Gap** | Rolling the flatbread |
| Packing | **Gap** | Bag or plate |
| Serving | Reach + Finger Poses | Hand-off, shares with customer's ReceivingFood |
| Cleaning | **Gap** | Wiping motion |

## The cat

Already working where the pack is installed: idle, walk, sit. Missing from the
requested list: run, eating, sleeping, meowing, approaching the player. Check
`Animals FREE` (ithappy) and the existing `Cat_Animation_Pack` contents before
assuming these need authoring.

## The gaps, stated plainly

Nine of the staff states and two customer states have no source. This is not a
research failure — **no general animation pack contains kneading çiğköfte,
rolling a dürüm or portioning bulgur.** They are specific to this game and will
not appear by downloading more packs.

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
