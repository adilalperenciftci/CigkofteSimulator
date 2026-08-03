#include "Navigation/CigPedRegion.h"

namespace
{
	// Pushing a point out of a disc has two answers and only one of them is
	// usually available: a lane is 60 wide and 13000 long, so radially out of a
	// tree is nearly always out of the lane as well. Sliding along the lane's long
	// axis is the answer that stays walkable.
	FVector2D PushOutOfObstacle(const FCigPedObstacle& Obstacle, const FCigPedLane& Lane,
		const FVector2D& Point, float AgentRadius)
	{
		const float Reach = Obstacle.Radius + AgentRadius;
		const FVector2D Extent = Lane.Extent();
		const bool bLongInY = Extent.Y >= Extent.X;

		// Distance along the long axis needed to clear the disc, given how far off
		// its centre-line the point already is on the short axis.
		const float Off = bLongInY ? (Point.X - Obstacle.Center.X) : (Point.Y - Obstacle.Center.Y);
		const float Along = bLongInY ? (Point.Y - Obstacle.Center.Y) : (Point.X - Obstacle.Center.X);
		const float Span = FMath::Sqrt(FMath::Max(0.f, Reach * Reach - Off * Off));

		// Leave by the nearer end, so a pedestrian steps round a tree rather than
		// walking the length of the street to avoid it.
		const float Target = (Along >= 0.f) ? (Span + 1.f) : -(Span + 1.f);
		FVector2D Result = Point;
		if (bLongInY)
		{
			Result.Y = Obstacle.Center.Y + Target;
		}
		else
		{
			Result.X = Obstacle.Center.X + Target;
		}
		return Lane.Clamp(Result);
	}
}

int32 FCigPedRegion::AddLane(const FVector2D& Min, const FVector2D& Max)
{
	FCigPedLane Lane;
	Lane.Min = FVector2D(FMath::Min(Min.X, Max.X), FMath::Min(Min.Y, Max.Y));
	Lane.Max = FVector2D(FMath::Max(Min.X, Max.X), FMath::Max(Min.Y, Max.Y));
	if (!Lane.IsValid())
	{
		// A lane with no width is not a thin lane, it is a bug in whoever built
		// it, and storing it would silently pin every pedestrian to a line.
		return INDEX_NONE;
	}
	return Lanes.Add(Lane);
}

void FCigPedRegion::AddObstacle(const FVector2D& Center, float Radius)
{
	if (Radius <= 0.f)
	{
		return;
	}
	FCigPedObstacle Obstacle;
	Obstacle.Center = Center;
	Obstacle.Radius = Radius;
	Obstacles.Add(Obstacle);
}

int32 FCigPedRegion::FindLane(const FVector2D& Point) const
{
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		if (Lanes[Index].Contains(Point))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FCigPedRegion::NearestLane(const FVector2D& Point) const
{
	int32 Best = INDEX_NONE;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		// Distance to the lane itself, not to its centre: a pedestrian standing at
		// one end of a long strip belongs to that strip, not to whichever strip
		// happens to be centred nearer.
		const float Dist = FVector2D::DistSquared(Point, Lanes[Index].Clamp(Point));
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Index;
		}
	}
	return Best;
}

bool FCigPedRegion::IsWalkable(int32 LaneIndex, const FVector2D& Point, float AgentRadius) const
{
	if (!Lanes.IsValidIndex(LaneIndex) || !Lanes[LaneIndex].Contains(Point))
	{
		return false;
	}
	for (const FCigPedObstacle& Obstacle : Obstacles)
	{
		if (Obstacle.Contains(Point, AgentRadius))
		{
			return false;
		}
	}
	return true;
}

FVector2D FCigPedRegion::ClampToLane(int32 LaneIndex, const FVector2D& Point, float AgentRadius) const
{
	if (!Lanes.IsValidIndex(LaneIndex))
	{
		return Point;
	}
	const FCigPedLane& Lane = Lanes[LaneIndex];
	FVector2D Result = Lane.Clamp(Point);

	// Bounded rather than "until clear": leaving one disc can enter the next, and
	// a street with obstacles closer together than an agent is wide has no clear
	// point at all. A few passes fixes the real cases; the rest stays inside the
	// lane, which is the guarantee that matters.
	constexpr int32 MaxPasses = 4;
	for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
	{
		bool bMoved = false;
		for (const FCigPedObstacle& Obstacle : Obstacles)
		{
			if (Obstacle.Contains(Result, AgentRadius))
			{
				Result = PushOutOfObstacle(Obstacle, Lane, Result, AgentRadius);
				bMoved = true;
			}
		}
		if (!bMoved)
		{
			break;
		}
	}
	return Result;
}

FVector2D FCigPedRegion::PickTarget(int32 LaneIndex, FRandomStream& Stream, float AgentRadius) const
{
	if (!Lanes.IsValidIndex(LaneIndex))
	{
		return FVector2D::ZeroVector;
	}
	const FCigPedLane& Lane = Lanes[LaneIndex];

	// Rejection, with a fixed budget. An unbounded retry would be a hang on a
	// lane whose obstacles happen to cover it, and this runs when a pedestrian
	// arrives rather than every frame.
	constexpr int32 MaxAttempts = 8;
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const FVector2D Candidate(
			Stream.FRandRange(Lane.Min.X, Lane.Max.X),
			Stream.FRandRange(Lane.Min.Y, Lane.Max.Y));
		if (IsWalkable(LaneIndex, Candidate, AgentRadius))
		{
			return Candidate;
		}
	}
	return ClampToLane(LaneIndex, Lane.Center(), AgentRadius);
}

FCigPedRegion FCigPedRegion::MainStreet()
{
	using namespace CigStreet;

	FCigPedRegion Region;

	// West pavement: trees on the far edge, lamps on the kerb, walk between them.
	Region.AddLane(
		FVector2D(WestTreeX + TreeClearance + PedRadius, MinY),
		FVector2D(WestLampX - LampClearance - PedRadius, MaxY));

	// East pavement: the same, mirrored - the kerb is the west edge here.
	Region.AddLane(
		FVector2D(EastLampX + LampClearance + PedRadius, MinY),
		FVector2D(EastTreeX - TreeClearance - PedRadius, MaxY));

	return Region;
}

FCigPedRegion FCigPedRegion::SingleLane(const FVector2D& Min, const FVector2D& Max)
{
	FCigPedRegion Region;
	Region.AddLane(Min, Max);
	return Region;
}
