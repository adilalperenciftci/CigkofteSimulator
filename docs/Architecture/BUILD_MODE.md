# Build mode: what the audit found before any code

Branch `feat/build-mode`, from master `2c65deb`.

Status: **steps 1-7 done. Step 8 open.** Steps 1-3 landed in PR #26 (the modal
gate, `SetInputMode`, and the shop rename that used them); step 4, selection, in
PR #27; step 5, the ghost, in PR #28; step 6, commit and cancel, in PR #29;
step 7, remove and restore, on `feat/build-mode-remove`.

**The player can move, remove and restore the shop's furniture, and it holds for
the session.** Surviving a save is step 8.

The plan said a playtest belonged before step 7. It did not happen, and step 7
was built anyway on request. Seven steps, none played.

**Nobody has played any of it.** Steps 1-3 shipped with an explicit playtest
request that was not carried out before the branch merged, and step 4 adds a
second mode on top of the first. The plan's own note at the bottom of this file -
that a playtest belongs before step 7 rather than after - now covers four
unplayed steps rather than three.

## Why this is the next thing

Two finished stages have the same gap, and it is the same gap:

- **Stage 3.5** persists the installed layout. Nobody has moved a table, because
  there is no way to move one. Every run so far persists the authored default.
- **Stage 3.6** validates, stores and displays a shop name. Nobody can type one.

Both are machinery with no way for a player to reach it. Neither is worth more
until this exists, and both become real the day it does.

## What was inspected

`CigInput.h`, `CigkoftePlayerCharacter.cpp` (the input polling), `CigTabletWidget`,
`CigTabletData`, `CigPlacementSystem`, `CigPlacementTypes` (`MoveExisting`,
`BuildMode`, `IgnoreStableId`), `CigWorldBuilder` (`PlacementVisuals`,
`SetShopName`), `CigNavSystem::WouldCloseRequiredRoute`.

## Three findings

### 1. Input is polled, not bound

`CigInput` reads keys straight off the `APlayerController` every tick —
`PC->IsInputKeyDown`, `PC->WasInputKeyJustPressed`. There is no EnhancedInput and
no bound delegate, which is why nothing in the project has ever called
`SetInputMode`.

The consequence for a text field is specific and easy to miss: `WasInputKeyJustPressed`
reads raw key state, so it fires **whether or not a widget has focus**. Putting a
`UEditableTextBox` on screen without gating the polling gives a shop rename where
typing "W" also walks the player forward.

### 2. The gate already exists, in the right shape

`ACigkoftePlayerCharacter` already does this:

```cpp
if (Mode->bTabletOpen)
{
    PollTabletInput(PC, Mode);
    return; // no world interaction while the tablet is open
}
```

So a modal mode that swallows world input is an established pattern here rather
than a new idea. Build mode and text entry are two more of them, and text entry
has to gate one level deeper — past `PollTabletInput` as well.

### 3. The placement authority is already built for this

Nothing new is needed to validate a player's edit:

- `ECigPlacementContext::BuildMode` and `MoveExisting` exist and are the contexts
  the authority already judges differently from world registration — the reason
  Stage 3.5's swap had to be an assignment.
- `FCigPlacementRequest::IgnoreStableId` exists so a move ignores its own old
  record.
- `UCigNavSystem::WouldCloseRequiredRoute` answers "would accepting this close a
  route" **synchronously, on a hypothetical grid**. That is exactly the question a
  ghost preview needs on every drag, and it is the capability the navigation ADR
  recorded as the strongest structural argument for keeping the grid.
- `FCigPlacementVisualRegistry` already maps a `StableId` to its actors and moves
  them together, which is what a preview and a commit both need.

The authority work is done. What is missing is a way to drive it.

## Plan, in dependency order

1. **A modal input gate.** One flag, checked before the tablet gate, that
   suppresses gameplay polling. Tested by the flag rather than by playing.
2. **`SetInputMode` on entering a modal UI**, and back on leaving. The first time
   this project changes input mode; it affects movement and look, so it is its own
   step with its own verification.
3. **Shop rename**, as the smallest real user of steps 1 and 2: one text field,
   already-validated rules, already-tested rename path. It closes the Stage 3.6
   gap and proves the input work with the least new surface.
4. **Build mode: selection.** Look at a placement, see it named, with its stable
   ID and category readable. **Done** — `CigBuildSelection` decides, the registry
   answers "which placement is this actor" in reverse, and the HUD reads it.

   Two things came out of building it that the plan did not anticipate. The first
   is that refusals need names: a player looking at a delivery crate and getting
   silence learns nothing, so `NotAPlacement`, `Orphaned` and `NotInstalled` are
   three answers rather than one. Only `Installed` placements are selectable,
   because the save persists only those and build mode must not promise to
   remember a crate that will not survive the evening.

   The second is that "say nothing" cannot be a text key. `CigText` treats a blank
   value as a missing translation, falls back to Turkish, finds that blank too,
   then warns and returns something visible — so an empty `build.select.none`
   would have put both a message on screen and a warning in the log on every frame
   the player looked at a wall. Silence is returned directly instead. A test
   caught this rather than a playthrough.
5. **Build mode: ghost preview.** A candidate transform validated through
   `ValidatePlacement` plus `WouldCloseRequiredRoute`, drawn in the colour of its
   verdict, with the failure named. **Done** — `CigBuildVerdict`.

   Three things the plan left open, decided here:

   **The order the two authorities are asked in.** A candidate can fail both at
   once — a slab across the doorway overlaps the entrance zone *and* closes the
   entrance route — and only one of those answers tells the player where to move
   their hands. So validation outranks the grid: a refused placement keeps its own
   reason, and the grid is consulted only when the rectangles were happy. This is
   also the cheaper order, but that is not why it was chosen.

   **"Every frame" turned out to be wrong.** `WouldCloseRequiredRoute` rebuilds a
   hypothetical grid, and re-running that while the player stands still is a cost
   with no answer attached. The verdict is recomputed when the candidate actually
   changes — position beyond the authority's own snap, or a rotation.

   **The real furniture does not move.** The ghost is separate geometry sized from
   the *footprint* rather than the mesh, because floor space is what the player is
   fighting for and the footprint is what the authority judges. That is also what
   lets step 5 stand alone: leaving the mode mid-move loses nothing but a box.

   The ghost is drawn at the verdict's `NormalizedTransform`, not the raw
   candidate. The authority snaps position and rotation, and a preview a few
   centimetres from where committing lands is the sort of difference nobody
   notices until a table will not fit and the ghost said it would.
6. **Commit and cancel.** **Done.** Commit goes through `RegisterPlacement` with
   `MoveExisting` and `IgnoreStableId`, and refuses when the verdict is not
   accepted — the ghost has been saying no, and a commit that went ahead anyway
   would make every red preview a suggestion.

   **Cancel turned out to need nothing.** The plan expected it to restore the
   previous transform from the visual registry, which is what it would have cost
   if the ghost had been the real furniture. Because step 5 made the ghost separate
   geometry, nothing is moved until commit, so cancel is just letting go. The step
   that looked like half the work was free.

   What was not free is what a move drags behind it. A record is only half a
   placement: the actors are found through the visual registry, and the *seats* are
   what customers walk to. `UCigWorldBuilder::FollowPlacement` now owns both, and
   the saved-layout path was moved onto it rather than left with its own copy —
   two implementations of "what follows a table" would mean fixing one and
   silently not the other, and the symptom is not a crash but customers walking to
   where a table used to be.

   Escape is not the cancel key. It is handled far above build mode in `PollInput`
   for the pause menu and never reaches here, so cancel is `X`. Whether anybody
   finds `X` is a playtest question; leaving cancel unbound would have been worse
   than binding it badly.
7. **Remove and restore.** **Done**, and not by the route the plan expected.

   The plan said "using the same paths Stage 3.5's load already exercises." The
   load path *destroys* actors, because a load's removal is final. A player taking
   a table off the floor has not destroyed it — the table is in the back room —
   and destroying it would mean putting it back required respawning meshes only
   `BuildWorld` knows how to make. So removal **stores**: actors hidden and
   uncollidable, record kept whole, seats lifted out.

   **Not everything can be removed**, and the rule is about capability rather than
   taste: a category carrying functional capacity must keep some. One of two
   tables, yes; the last one, no. The boundary is the *capacity* the record
   carries, not the count of records — a single table seating four is still the
   last of its function.

   **Seats leave the world rather than being flagged.** A flag would be a second
   meaning for a seat to have, and everything reading `Seats` — the customer
   system, the route audit, capacity — would have to learn it. Taken out, a stored
   chair is simply not there.

   Removal never asks the navigation grid, because taking an obstacle away only
   ever opens floor. That claim justifies skipping a check, so it is tested rather
   than assumed: everything removable is removed and the routes are still open.

   Restore re-registers with `BuildMode` context rather than `MoveExisting` — the
   id is not on the floor, so there is no record of its own to ignore — and it is
   judged again, because the shop it returns to is not the shop it left. A refused
   restore keeps the thing in storage rather than dropping it.
8. **Round-trip through the save**, which is where Stage 3.5 stops being
   theoretical: move a table in build mode, save, reload, and see it where it was
   left.

Step 3 is the first one that produces something a player can use, and it is
deliberately small: the rules and the rename are written and tested already, so it
tests the input work and nothing else.

## What cannot be verified without playing

Steps 2 through 6 change how the game reads input and what it draws while the
player is dragging an object. Automation can check the gate, the validation and
the commit; it cannot check that the controls feel right, that the ghost is
readable, or that the mouse is where the player expects it. That is a playtest,
and it should happen before step 7 rather than after.
