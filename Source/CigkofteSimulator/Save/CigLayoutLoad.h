#pragma once

#include "CoreMinimal.h"
#include "Placement/CigPlacementTypes.h"
#include "Save/CigSavePlacement.h"

// Turning a saved layout into a layout the shop can actually stand in.
//
// The rule this exists to enforce is that a load is one transaction. Registering
// saved records straight into the live authority would leave a shop half authored
// default and half save the moment one record was refused - and every record
// after the bad one would be validated against a floor that is neither layout.
// So a candidate is built to one side, checked whole, and only then does anything
// swap. The swap itself belongs to the caller; this decides whether there is
// anything worth swapping to.
//
// Everything here is a value type. No UWorld, no actors, no save file: the same
// inputs give the same verdict, which is what makes the failure matrix testable
// rather than reachable only by corrupting a real save.

enum class ECigLayoutLoadFailure : uint8
{
	None = 0,
	// One record could not be understood on its own terms. RecordFault says which.
	RecordFault,
	// Two records claim the same stable ID. Registering both would silently keep
	// whichever came last and lose the other.
	DuplicateStableId,
	// The record is well-formed and the floor still refuses it: out of bounds,
	// overlapping another placement, or standing in a protected zone.
	// PlacementFailure carries the authority's own reason.
	Rejected,
	// Every placement is individually legal and a body cannot get across the shop.
	// This is the failure that rectangles cannot see and the grid exists for.
	RouteClosed
};

struct FCigLayoutLoadReport
{
	bool bAccepted = false;
	ECigLayoutLoadFailure Failure = ECigLayoutLoadFailure::None;

	// Which record went wrong. None when the failure is not about one record.
	FName FailedStableId;
	ECigSavePlacementFault RecordFault = ECigSavePlacementFault::None;
	ECigPlacementFailure PlacementFailure = ECigPlacementFailure::None;
	FName ClosedRouteId;

	// How many records were registered before the transaction was abandoned.
	// Diagnostic only: on failure none of them are kept.
	int32 RegisteredCount = 0;
	FString Diagnostic;
};

namespace CigLayoutLoad
{
	// Builds the candidate and reports whether it may replace the live layout.
	// OutCandidate is only meaningful when bAccepted is true; on failure it holds
	// however far the attempt got and must not be used.
	//
	// SeatStands are the points a seated customer walks to. They come from the
	// world builder in production and from the test in a test, rather than being
	// re-derived here: which chair belongs to which table is the world builder's
	// knowledge, and duplicating it would be a second authority.
	FCigLayoutLoadReport BuildCandidate(
		const TArray<FCigSavePlacement>& Saved,
		const FCigPlacementBounds& Bounds,
		const TArray<FCigProtectedZone>& ProtectedZones,
		const TArray<FVector2D>& SeatStands,
		FCigPlacementAuthority& OutCandidate);

	// The protected zones the shop is configured with. Shared so a loader and a
	// test cannot disagree about what they are validating against.
	TArray<FCigProtectedZone> ShopProtectedZones();

	const TCHAR* FailureText(ECigLayoutLoadFailure Failure);
}
