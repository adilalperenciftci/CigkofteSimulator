#pragma once

#include "CoreMinimal.h"

// Where a street pedestrian is allowed to be.
//
// Ambient pedestrians used to be given a rectangle and left to interpolate
// across it. The rectangle they were given on the main street was
// X in [-2250, -1450], and the carriageway is X in [-2150, -1450]: the wander
// box was the road. They walked in both lanes of moving traffic, and through the
// trees and lamp posts, which are spawned with collision disabled and so are not
// caught by the sweep in ACigkofteCustomer::StepTowards.
//
// So this is not a pathfinder and does not try to be one. The pavements are two
// straight strips with a line of trees down the outer edge and a line of lamp
// posts down the kerb; the walkable part is the clear span between them, which
// the world builder already knows because it put them there. A lane is that
// span, and containment is by construction: a pedestrian's position is clamped
// into its lane every step, so no frame rate and no step length can put one in
// the road.
//
// What this deliberately is not: a model of the whole city. District pedestrians
// still get a single rectangle around a district centre, because the interiors
// of six districts are not modelled and pretending otherwise in a type would be
// worse than the rectangle.

// The main street, in one place.
//
// CigWorldBuilder::BuildCity spawns the road, the pavements, the trees and the
// lamp posts from these, and MainStreet() below derives the lanes from the same
// values. Written as edges rather than as the centre-and-scale pairs SpawnBox
// takes, because "the pavement is 200 wide with furniture on both edges" is the
// fact that matters and a scale of 2 does not say it.
namespace CigStreet
{
	// Carriageway. Cars drive at CarLaneWest and CarLaneEast; no pedestrian lane
	// may overlap any of it.
	constexpr float RoadMinX = -2150.f;
	constexpr float RoadMaxX = -1450.f;
	constexpr float CarLaneWest = -1950.f;
	constexpr float CarLaneEast = -1650.f;

	// Pavements, outside the kerbs on both sides.
	constexpr float WestPavementMinX = -2350.f;
	constexpr float WestPavementMaxX = -2150.f;
	constexpr float EastPavementMinX = -1450.f;
	constexpr float EastPavementMaxX = -1250.f;

	// Street furniture stands on lines, not scattered: trees down the outer edge
	// of each pavement, lamp posts down the kerb. Both are spawned with collision
	// disabled, so keeping pedestrians off them is geometry's job here rather
	// than the collision sweep's.
	constexpr float WestTreeX = -2350.f;
	constexpr float EastTreeX = -1250.f;
	constexpr float WestLampX = -2150.f;
	constexpr float EastLampX = -1450.f;

	// Clearances. Generous rather than measured off a mesh: the packs are
	// optional, the primitive fallbacks are thinner than the meshes, and a lane
	// that is too narrow costs nothing but single file.
	constexpr float TreeClearance = 55.f;
	constexpr float LampClearance = 25.f;
	constexpr float PedRadius = 30.f;

	// How far along the street pedestrians are kept. The pavements run to the
	// district gates; this is the stretch either side of the shop.
	constexpr float MinY = -6500.f;
	constexpr float MaxY = 6500.f;
}

// One walkable strip, axis-aligned. Long in one axis, narrow in the other.
struct FCigPedLane
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;

	bool IsValid() const { return Max.X > Min.X && Max.Y > Min.Y; }
	FVector2D Center() const { return (Min + Max) * 0.5f; }
	FVector2D Extent() const { return Max - Min; }

	bool Contains(const FVector2D& Point) const
	{
		return Point.X >= Min.X && Point.X <= Max.X && Point.Y >= Min.Y && Point.Y <= Max.Y;
	}

	FVector2D Clamp(const FVector2D& Point) const
	{
		return FVector2D(FMath::Clamp(Point.X, Min.X, Max.X), FMath::Clamp(Point.Y, Min.Y, Max.Y));
	}
};

// A prop inside a lane that a pedestrian must walk round rather than through.
// A disc because street furniture is round and because the cheap test is what
// gets run every step.
struct FCigPedObstacle
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 0.f;

	bool Contains(const FVector2D& Point, float AgentRadius) const
	{
		const float Reach = Radius + AgentRadius;
		return FVector2D::DistSquared(Point, Center) < Reach * Reach;
	}
};

struct FCigPedRegion
{
	TArray<FCigPedLane> Lanes;
	TArray<FCigPedObstacle> Obstacles;

	bool IsEmpty() const { return Lanes.Num() == 0; }

	// Adds a lane, rejecting a degenerate one rather than storing a rectangle
	// that Clamp would collapse to a point.
	int32 AddLane(const FVector2D& Min, const FVector2D& Max);
	void AddObstacle(const FVector2D& Center, float Radius);

	// The lane holding Point, or INDEX_NONE. Used to place a pedestrian that was
	// spawned before it was told where it may walk.
	int32 FindLane(const FVector2D& Point) const;

	// Nearest lane by centre distance. Never INDEX_NONE for a non-empty region,
	// so a pedestrian always has somewhere legal to be.
	int32 NearestLane(const FVector2D& Point) const;

	bool IsWalkable(int32 LaneIndex, const FVector2D& Point, float AgentRadius) const;

	// Puts Point inside the lane and outside every obstacle. This is the
	// containment guarantee: it is applied to the pedestrian's own position, not
	// only to the target it is walking at.
	FVector2D ClampToLane(int32 LaneIndex, const FVector2D& Point, float AgentRadius) const;

	// A walkable point inside the lane, chosen from Stream so a fixed seed gives
	// a fixed route. Falls back to the clamped lane centre rather than looping.
	FVector2D PickTarget(int32 LaneIndex, FRandomStream& Stream, float AgentRadius) const;

	// The main street outside the shop: two pavement lanes and the furniture that
	// stands on them. Built from the same numbers CigWorldBuilder spawns the
	// street with, so the two cannot drift apart silently.
	static FCigPedRegion MainStreet();

	// A district, which is a rectangle and is honest about it: one lane, no
	// obstacles, no claim that the interior is modelled.
	static FCigPedRegion SingleLane(const FVector2D& Min, const FVector2D& Max);
};
