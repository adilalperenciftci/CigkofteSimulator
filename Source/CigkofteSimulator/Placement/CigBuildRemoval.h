#pragma once

#include "CoreMinimal.h"
#include "Placement/CigPlacementTypes.h"

// Whether a placement may be taken off the floor.
//
// Moving something is always reversible - the shop is a different shape and that
// is all. Removing is not the same kind of act: it takes capability away, and a
// player who removes their only preparation station has broken the shop from
// inside a mode that exists to improve it.
//
// So removal has a rule that moving does not need, and the rule is about function
// rather than taste: a category that carries functional capacity must keep some.
// You can take away one of two tables. You cannot take away the last one.
//
// Nothing here asks the navigation grid. Removing only ever opens floor, and a
// route that was walkable before cannot be closed by taking an obstacle away.

enum class ECigBuildRemovalFault : uint8
{
	None = 0,
	// Nothing selected, or the selection is not something build mode owns.
	NotSelectable,
	// The authority has no record for it. As with selection, a defect rather than
	// a refusal, and named so it cannot be read as the player aiming badly.
	NoRecord,
	// Taking this away would leave the shop unable to do something it must do.
	// The message names the category, because "you cannot remove that" without
	// saying why is what teaches players to stop reading messages.
	LastOfItsFunction
};

struct FCigBuildRemovalVerdict
{
	FName StableId;
	ECigPlacementCategory Category = ECigPlacementCategory::Unknown;
	ECigBuildRemovalFault Fault = ECigBuildRemovalFault::NotSelectable;

	bool IsAllowed() const
	{
		return Fault == ECigBuildRemovalFault::None && !StableId.IsNone();
	}
};

namespace CigBuildRemoval
{
	// The decision.
	//
	// CategoryCapacityTotal is what the shop has in this category now, including
	// the record being judged; passing both in rather than looking them up keeps
	// this callable with no game. A category whose total capacity is zero carries
	// no function at all - decoration - and is always removable.
	FCigBuildRemovalVerdict Judge(const FCigPlacementRecord* Record, int32 CategoryCapacityTotal);

	FString Describe(const FCigBuildRemovalVerdict& Verdict);
}
