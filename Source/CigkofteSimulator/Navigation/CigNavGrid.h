#pragma once

#include "CoreMinimal.h"
#include "Placement/CigPlacementTypes.h"

// Measured reachability over the shop floor.
//
// Stage 3.3 gave every placement a physical rectangle. A rectangle says what the
// floor is occupied by; it says nothing about whether a body can get from one
// side of the room to the other, and the two answers diverge the moment two
// legally placed objects leave a gap narrower than the thing walking through it.
// Nothing here infers reachability from a rectangle: the grid is rasterised from
// the placement records, inflated by the agent's own radius, and then actually
// searched.
//
// This is deliberately not a navmesh. There is no authored map to build one on -
// the shop is spawned at runtime into /Engine/Maps/Entry - and the floor is a
// single plane, so the only question a navmesh would answer that this does not
// is vertical traversal, which this game has none of. What matters is that the
// answer is measured rather than assumed, and that it is checked against real
// engine collision (see Cigkofte.Navigation.Collision tests).

enum class ECigPathFailure : uint8
{
	None = 0,
	NotBuilt,
	StartOutsideBounds,
	GoalOutsideBounds,
	// The endpoint is inside the floor but standing in something. Callers that
	// can legitimately hit this (a customer whose seat has just been moved) are
	// expected to resolve it through FindNearestWalkable rather than to retry.
	StartBlocked,
	GoalBlocked,
	// Both endpoints are standable and no route joins them.
	NoRoute
};

struct FCigPathResult
{
	bool bSuccess = false;
	// World-space XY waypoints, start first and goal last. Empty on failure: a
	// partial path is not returned, because a caller that walked one would stop
	// against the obstacle and look like it had arrived.
	TArray<FVector2D> Points;
	float Length = 0.f;
	ECigPathFailure Failure = ECigPathFailure::None;
	// Cells taken off the open set. Recorded so a query that becomes expensive is
	// visible as a number rather than as a frame-time complaint.
	int32 CellsSearched = 0;

	int32 PointCount() const { return Points.Num(); }
};

// Pure and deterministic: no UWorld, no actors, no collision scene. The same
// inputs give the same path on every run and on every machine, which is what
// makes the reachability assertions in the test suite worth anything.
class FCigNavGrid
{
public:
	// Blocking is taken from each record's physical rectangle only. A use area is
	// where a body stands to work the object, so blocking it would make every
	// station unreachable by construction.
	//
	// StaticObstacles carries what the placement authority does not own - the shop
	// shell. Walls are not placements: nothing can move them, and registering them
	// as records would make them candidates for a move that must never be offered.
	void Build(const FCigPlacementBounds& InBounds, const TArray<FCigPlacementRecord>& Records,
		const TArray<FCigPlacementRect>& StaticObstacles,
		float InAgentRadius, float InCellSize = 25.f);
	void Reset();

	bool IsBuilt() const { return bBuilt; }
	float AgentRadius() const { return Radius; }
	float CellSize() const { return Cell; }
	int32 CellCountX() const { return CountX; }
	int32 CellCountY() const { return CountY; }
	int32 WalkableCellCount() const;

	bool IsWalkable(const FVector2D& World) const;

	// Nearest standable point, searched outwards in rings so the result is the
	// closest one rather than the first one found in scan order. Returns false
	// when the whole floor is blocked or World is outside the bounds entirely.
	bool FindNearestWalkable(const FVector2D& World, FVector2D& OutWorld,
		float MaxSearchDistance = 400.f) const;

	FCigPathResult FindPath(const FVector2D& Start, const FVector2D& Goal) const;

	// True when a straight line between the two points stays walkable. Used to
	// simplify a path, and on its own by the tests that assert a route is direct.
	bool HasClearLine(const FVector2D& From, const FVector2D& To) const;

private:
	bool bBuilt = false;
	FCigPlacementBounds Bounds;
	float Radius = 40.f;
	float Cell = 25.f;
	int32 CountX = 0;
	int32 CountY = 0;
	FVector2D Origin = FVector2D::ZeroVector;
	// One byte per cell rather than a bitfield: the whole shop is a few thousand
	// cells, and indexed reads in the inner A* loop are the hot path.
	TArray<uint8> Blocked;

	int32 Index(int32 X, int32 Y) const { return Y * CountX + X; }
	bool IsValidCell(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < CountX && Y < CountY; }
	bool IsBlockedCell(int32 X, int32 Y) const;
	FVector2D CellCenter(int32 X, int32 Y) const;
	bool WorldToCell(const FVector2D& World, int32& OutX, int32& OutY) const;
};
