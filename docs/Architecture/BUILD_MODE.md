# Build mode: what the audit found before any code

Branch `feat/build-mode`, from master `2c65deb`.

Status: **audit and plan. No code has been written.**

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
4. **Build mode: selection.** Look at a placement, see it highlighted, with its
   stable ID and category readable.
5. **Build mode: ghost preview.** A candidate transform validated every frame
   through `ValidatePlacement` plus `WouldCloseRequiredRoute`, drawn in the colour
   of its verdict, with the failure named — the authority already returns
   `ECigPlacementFailure` and the conflicting stable ID.
6. **Commit and cancel.** Commit goes through `RegisterPlacement` with
   `MoveExisting` and `IgnoreStableId`; cancel restores the previous transform
   from the visual registry.
7. **Remove and restore**, using the same paths Stage 3.5's load already exercises.
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
