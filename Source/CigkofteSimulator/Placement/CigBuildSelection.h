#pragma once

#include "CoreMinimal.h"
#include "Placement/CigPlacementTypes.h"

// What the player has picked out to move.
//
// Selection looks like a rendering concern and is not one. Whether a thing can be
// selected is a question about the placement authority - what the record is, and
// whether this project is willing to let a player move it - and the answer has to
// be the same whether it is reached by a line trace, by a save being reloaded, or
// by a test with no world at all. So the decision lives here, as a value, and the
// trace that produced the actor stays in the character where it belongs.
//
// The refusals are named rather than collapsed into "no". A player who looks at a
// delivery crate and gets silence learns nothing; one who is told it is a delivery
// learns why the shop's furniture behaves differently from the day's traffic.

enum class ECigBuildSelectionFault : uint8
{
	None = 0,
	// Looking at nothing, or at something no placement owns: a wall, the floor,
	// a pedestrian. The overwhelmingly common case, and not an error.
	NotAPlacement,
	// The visual registry knows this actor but the authority has no record for it.
	// That is a defect rather than a refusal - the two went out of step - and it is
	// given its own name so it can never be read as "the player aimed badly".
	Orphaned,
	// A real placement that build mode does not move. Delivery crates arrive and
	// leave on the day's own schedule; letting a player rearrange them would put
	// the shop's layout and the day's traffic into the same editing surface.
	NotInstalled
};

struct FCigBuildSelection
{
	FName StableId;
	ECigPlacementCategory Category = ECigPlacementCategory::Unknown;
	ECigPlacementLifetime Lifetime = ECigPlacementLifetime::Unknown;
	ECigBuildSelectionFault Fault = ECigBuildSelectionFault::NotAPlacement;

	// A selection is usable only when nothing refused it and it names something.
	// Both halves are checked: an id with a fault, or no fault with no id, are
	// each a bug in whoever built it rather than a state to act on.
	bool IsValid() const
	{
		return Fault == ECigBuildSelectionFault::None && !StableId.IsNone();
	}
};

namespace CigBuildSelection
{
	// The decision, given what the registry said and what the authority holds.
	//
	// Record is the authority's answer for StableId and may be null; passing it in
	// rather than looking it up keeps this callable without a game. StableId is
	// NAME_None when the trace hit something no placement owns.
	FCigBuildSelection Resolve(FName StableId, const FCigPlacementRecord* Record);

	// What the player reads. Text keys, so it follows the interface language.
	FString Describe(const FCigBuildSelection& Selection);

	FString CategoryText(ECigPlacementCategory Category);
	FString FaultText(ECigBuildSelectionFault Fault);
}
