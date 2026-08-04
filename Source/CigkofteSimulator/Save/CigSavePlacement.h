#pragma once

#include "CoreMinimal.h"
#include "Placement/CigPlacementTypes.h"
#include "CigSavePlacement.generated.h"

// One installed placement, as it goes into the save file.
//
// Authored inputs only. FCigPlacementRecord also carries a
// FCigPlacementConsequence, and that is derived: FCigPlacementConsequencePolicy
// recomputes it from the fields below. Writing it down would create a second
// authority that a later policy change could silently contradict - a save whose
// stored consequence says a table seats two while the policy of the day says
// four. It is left out so there is exactly one way to know.
//
// The record is otherwise self-describing, which is what decided the persistence
// model: footprint and use spec travel with it, so a saved placement can be
// re-registered without consulting any definition table. Nothing here needs to
// exist in code for the record to be understood.
USTRUCT()
struct FCigSavePlacement
{
	GENERATED_BODY()

	UPROPERTY() FName StableId;
	UPROPERTY() uint8 Category = 0;
	UPROPERTY() uint8 Lifetime = 0;
	UPROPERTY() FTransform Transform = FTransform::Identity;

	// --- Footprint ---
	UPROPERTY() FVector2D FootprintSize = FVector2D(100.f, 100.f);
	UPROPERTY() FVector2D FootprintOffset = FVector2D::ZeroVector;
	UPROPERTY() float ClearanceMargin = 0.f;
	UPROPERTY() uint8 RotationPolicy = 0;
	UPROPERTY() float FixedYawDegrees = 0.f;

	// --- Use spec ---
	// Decoration leaves this empty on purpose; functional categories must have it.
	// Stored rather than re-derived from the category, because two stations of the
	// same category can have different approach sides and the shop already relies
	// on that (MamaKabi and YanUrun approach opposite their labels).
	UPROPERTY() FVector2D UseSize = FVector2D::ZeroVector;
	UPROPERTY() FVector2D UseOffset = FVector2D::ZeroVector;
	UPROPERTY() float UseYawDegrees = 0.f;
	UPROPERTY() int32 FunctionalCapacity = 0;
};

// Why a saved placement cannot be used. Per-record and pure; the transactional
// rules that decide what happens to the *layout* when one record is faulty are
// step 3 and live with the loader.
enum class ECigSavePlacementFault : uint8
{
	None = 0,
	MissingStableId,
	UnknownCategory,
	// Only Installed is written. A Transient in the file means either a crate was
	// persisted by mistake or the file was edited; either way it must not be
	// registered, because the delivery system would then create a second one.
	NotInstalled,
	NonFiniteTransform,
	NonUniformScale,
	InvalidFootprint,
	InvalidUseSpec
};

namespace CigSavePlacement
{
	// Installed records only, ordered by stable ID so the same shop always writes
	// the same bytes. The authority stores records in registration order, which is
	// world-build order, and that is not something a save file should depend on.
	TArray<FCigSavePlacement> Capture(const TArray<FCigPlacementRecord>& Records);

	FCigSavePlacement FromRecord(const FCigPlacementRecord& Record);

	// The request that re-registers this placement. Context is WorldRegistration:
	// a load is restoring authored layout, not accepting a build-mode edit, and
	// the two are validated differently.
	FCigPlacementRequest ToRequest(const FCigSavePlacement& Saved);

	// Everything that can be decided about one record without looking at the rest
	// of the layout. Overlap, duplicate IDs and route closure need the whole
	// candidate set and belong to the loader.
	ECigSavePlacementFault Validate(const FCigSavePlacement& Saved);

	const TCHAR* FaultText(ECigSavePlacementFault Fault);
}
