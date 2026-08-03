#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Navigation/CigNavGrid.h"
#include "CigNavSystem.generated.h"

struct FCigPlacementChange;

// A route the shop has to keep open, and what the grid said about it.
struct FCigRouteReport
{
	FName RouteId;
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D Goal = FVector2D::ZeroVector;
	FCigPathResult Result;
};

// Owns the walkable picture of the shop and answers paths against it.
//
// This is the production subscriber to UCigEventBus::PlacementChanged. It exists
// because navigation genuinely has state to invalidate, not to give the event a
// listener: a grid is rasterised from the placement records, so the moment a
// record moves or goes away the grid describes a shop that is no longer there.
//
// Invalidation is a dirty flag rather than an immediate rebuild. World build
// registers on the order of thirty fixtures in one pass, and rebuilding per
// registration would be thirty full rasterisations to produce the one grid the
// first query actually needs.
UCLASS()
class UCigNavSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;

	// Paths are XY. The shop floor is one plane, so Z is carried through from the
	// caller's own start rather than sampled.
	FCigPathResult FindCustomerPath(const FVector& Start, const FVector& Goal) const;
	FCigPathResult FindPlayerPath(const FVector& Start, const FVector& Goal) const;

	bool IsCustomerStandable(const FVector& World) const;
	bool IsPlayerStandable(const FVector& World) const;

	// Nearest point a customer can stand. A seat sits inside its own table's
	// footprint by construction - a chair is at a table - so the walkable target
	// for "go and sit there" is beside the seat, not on it.
	bool FindCustomerStand(const FVector& World, FVector& OutWorld) const;

	// Every route the shop must keep open, measured rather than assumed.
	TArray<FCigRouteReport> AuditRequiredRoutes() const;
	bool AreRequiredRoutesOpen() const;

	// Would accepting this candidate close a required route? Answered on a grid
	// built with the candidate included, which is the only way to know: a
	// candidate can pass every rectangle rule in the placement authority and
	// still leave a gap narrower than a customer.
	bool WouldCloseRequiredRoute(const FCigPlacementRecord& Candidate, FName& OutRouteId) const;

	// Observability. Both are read by tests that assert the rebuild is not
	// happening more often than the layout actually changes.
	int32 RebuildCount() const { return RebuildCounter; }
	bool IsDirty() const { return bDirty; }
	int32 QueryCount() const { return QueryCounter; }

	void MarkDirty();

private:
	void HandlePlacementChanged(const FCigPlacementChange& Change);
	void EnsureBuilt() const;
	// Records plus the shop shell, which the placement authority does not own.
	void GatherObstacles(TArray<FCigPlacementRecord>& OutRecords, TArray<FCigPlacementRect>& OutStatic) const;
	TArray<FCigRouteReport> AuditRoutesOn(const FCigNavGrid& Grid) const;

	mutable FCigNavGrid CustomerGrid;
	mutable FCigNavGrid PlayerGrid;
	mutable bool bDirty = true;
	mutable int32 RebuildCounter = 0;
	mutable int32 QueryCounter = 0;
};
