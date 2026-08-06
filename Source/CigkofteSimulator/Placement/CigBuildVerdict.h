#pragma once

#include "CoreMinimal.h"
#include "Placement/CigPlacementTypes.h"

// Whether a proposed move would be allowed, and why not.
//
// Two authorities have to agree before a table can go somewhere, and they answer
// different questions. The placement authority reasons about rectangles: does this
// overlap a sofa, leave the shop, block the entrance zone. The navigation grid
// reasons about width: a candidate can pass every rectangle rule and still leave a
// gap narrower than a customer, which no rectangle test can see.
//
// This joins them, and the joining is the part worth writing down - the order the
// two are asked in changes what the player is told, and the wrong order tells them
// something true and useless.

enum class ECigBuildVerdictStatus : uint8
{
	// Both authorities agree.
	Accepted = 0,
	// The placement authority refused. Failure names which rule.
	Refused,
	// The rectangles were fine and the grid was not: accepting this would leave
	// somewhere in the shop unreachable. ClosedRouteId names which route.
	ClosesRoute
};

struct FCigBuildVerdict
{
	ECigBuildVerdictStatus Status = ECigBuildVerdictStatus::Refused;
	ECigPlacementFailure Failure = ECigPlacementFailure::None;
	FName ConflictingStableId;
	FName ClosedRouteId;
	// Where the placement would actually land. The authority snaps position and
	// rotation, so the ghost has to be drawn here rather than at the raw candidate
	// or it would sit a few centimetres from where committing puts it.
	FTransform NormalizedTransform = FTransform::Identity;

	bool IsAccepted() const { return Status == ECigBuildVerdictStatus::Accepted; }
};

namespace CigBuildVerdict
{
	// The request a move asks: the existing record's own geometry at a new
	// transform, ignoring itself.
	//
	// IgnoreStableId is the whole reason a move is expressible at all - without it
	// every table would overlap the record of where it currently stands.
	FCigPlacementRequest MakeMoveRequest(const FCigPlacementRecord& Existing,
		const FTransform& CandidateTransform);

	// The candidate as a record, so the navigation grid can be asked about it.
	// Consequence geometry is derived by the same policy the authority uses rather
	// than rebuilt here, because two derivations that drift would make the ghost
	// and the commit disagree.
	bool MakeCandidateRecord(const FCigPlacementRequest& Request,
		const FTransform& NormalizedTransform, FCigPlacementRecord& OutRecord);

	// The join, and the ordering decision.
	//
	// A refused placement keeps its own reason even when the route question was
	// also going to fail. "There is already something there" is actionable; "a
	// route would close" is true and leaves the player looking for a corridor that
	// was never the problem. So validation outranks the grid, and the grid is only
	// consulted - and only reported - when the rectangles were happy.
	FCigBuildVerdict Combine(const FCigPlacementResult& Validation,
		bool bClosesRoute, FName ClosedRouteId);

	// What the player reads while the ghost is on screen.
	FString Describe(const FCigBuildVerdict& Verdict);

	// Green when it would be accepted, red when it would not. Kept beside Describe
	// so the colour and the words can never disagree about the same verdict.
	FLinearColor Tint(const FCigBuildVerdict& Verdict);
}
