#include "Navigation/CigNavGrid.h"

#include "Algo/Reverse.h"

namespace
{
	// Octile distance. Admissible for 8-connected movement with a sqrt(2)
	// diagonal, so A* keeps its optimality guarantee.
	constexpr float DiagonalStep = 1.41421356f;

	float Octile(int32 FromX, int32 FromY, int32 ToX, int32 ToY)
	{
		const int32 DeltaX = FMath::Abs(ToX - FromX);
		const int32 DeltaY = FMath::Abs(ToY - FromY);
		const int32 Small = FMath::Min(DeltaX, DeltaY);
		const int32 Large = FMath::Max(DeltaX, DeltaY);
		return static_cast<float>(Large - Small) + DiagonalStep * static_cast<float>(Small);
	}
}

void FCigNavGrid::Reset()
{
	bBuilt = false;
	CountX = 0;
	CountY = 0;
	Blocked.Reset();
}

void FCigNavGrid::Build(const FCigPlacementBounds& InBounds, const TArray<FCigPlacementRecord>& Records,
	const TArray<FCigPlacementRect>& StaticObstacles, float InAgentRadius, float InCellSize)
{
	Reset();

	Bounds = InBounds;
	Radius = FMath::Max(0.f, InAgentRadius);
	Cell = FMath::Max(1.f, InCellSize);

	const FVector2D Size = Bounds.HalfExtent * 2.f;
	CountX = FMath::Max(1, FMath::FloorToInt(Size.X / Cell));
	CountY = FMath::Max(1, FMath::FloorToInt(Size.Y / Cell));
	Origin = Bounds.Center - Bounds.HalfExtent;
	Blocked.Init(0, CountX * CountY);

	// The agent is a disc, not a point. Inflating each obstacle by the radius and
	// then walking cell centres is the standard configuration-space reduction: it
	// means a gap only stays open when a body of that width actually fits, which
	// is the entire difference between this and asking whether rectangles overlap.
	TArray<FCigPlacementRect> Obstacles;
	Obstacles.Reserve(Records.Num() + StaticObstacles.Num());
	for (const FCigPlacementRecord& Record : Records)
	{
		Obstacles.Add(Record.Consequence.PhysicalRect);
	}
	Obstacles.Append(StaticObstacles);

	for (const FCigPlacementRect& Rect : Obstacles)
	{
		if (Rect.HalfExtent.X <= 0.f || Rect.HalfExtent.Y <= 0.f)
		{
			continue;
		}

		const FVector2D Inflated = Rect.HalfExtent + FVector2D(Radius, Radius);
		const FVector2D Min = Rect.Center - Inflated;
		const FVector2D Max = Rect.Center + Inflated;

		const int32 MinX = FMath::Max(0, FMath::FloorToInt((Min.X - Origin.X) / Cell));
		const int32 MinY = FMath::Max(0, FMath::FloorToInt((Min.Y - Origin.Y) / Cell));
		const int32 MaxX = FMath::Min(CountX - 1, FMath::CeilToInt((Max.X - Origin.X) / Cell));
		const int32 MaxY = FMath::Min(CountY - 1, FMath::CeilToInt((Max.Y - Origin.Y) / Cell));

		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FVector2D Center = CellCenter(X, Y);
				const FVector2D Delta = (Center - Rect.Center).GetAbs();
				if (Delta.X <= Inflated.X && Delta.Y <= Inflated.Y)
				{
					Blocked[Index(X, Y)] = 1;
				}
			}
		}
	}

	// The wall is as solid as anything standing against it. Without this an agent
	// would be allowed to clip the perimeter by up to its own radius, and a route
	// that only exists by doing so is not a route.
	const int32 Margin = FMath::CeilToInt(Radius / Cell) - 1;
	if (Margin >= 0)
	{
		for (int32 Y = 0; Y < CountY; ++Y)
		{
			for (int32 X = 0; X < CountX; ++X)
			{
				if (X <= Margin || Y <= Margin || X >= CountX - 1 - Margin || Y >= CountY - 1 - Margin)
				{
					Blocked[Index(X, Y)] = 1;
				}
			}
		}
	}

	bBuilt = true;
}

FVector2D FCigNavGrid::CellCenter(int32 X, int32 Y) const
{
	return Origin + FVector2D((X + 0.5f) * Cell, (Y + 0.5f) * Cell);
}

bool FCigNavGrid::WorldToCell(const FVector2D& World, int32& OutX, int32& OutY) const
{
	OutX = FMath::FloorToInt((World.X - Origin.X) / Cell);
	OutY = FMath::FloorToInt((World.Y - Origin.Y) / Cell);
	return IsValidCell(OutX, OutY);
}

bool FCigNavGrid::IsBlockedCell(int32 X, int32 Y) const
{
	if (!IsValidCell(X, Y))
	{
		return true;
	}
	return Blocked[Index(X, Y)] != 0;
}

int32 FCigNavGrid::WalkableCellCount() const
{
	int32 Count = 0;
	for (uint8 Value : Blocked)
	{
		Count += (Value == 0) ? 1 : 0;
	}
	return Count;
}

bool FCigNavGrid::IsWalkable(const FVector2D& World) const
{
	if (!bBuilt)
	{
		return false;
	}
	int32 X = 0;
	int32 Y = 0;
	if (!WorldToCell(World, X, Y))
	{
		return false;
	}
	return !IsBlockedCell(X, Y);
}

bool FCigNavGrid::FindNearestWalkable(const FVector2D& World, FVector2D& OutWorld,
	float MaxSearchDistance) const
{
	if (!bBuilt)
	{
		return false;
	}

	int32 StartX = FMath::Clamp(FMath::FloorToInt((World.X - Origin.X) / Cell), 0, CountX - 1);
	int32 StartY = FMath::Clamp(FMath::FloorToInt((World.Y - Origin.Y) / Cell), 0, CountY - 1);

	if (!IsBlockedCell(StartX, StartY))
	{
		OutWorld = CellCenter(StartX, StartY);
		return true;
	}

	const int32 MaxRing = FMath::Max(1, FMath::CeilToInt(MaxSearchDistance / Cell));
	// Expanding rings rather than a flood fill: the answer wanted is the closest
	// standable point, and a scan-order search returns whichever one the loop
	// happened to reach first, which moves a recovering customer sideways for no
	// reason a player could follow.
	for (int32 Ring = 1; Ring <= MaxRing; ++Ring)
	{
		float BestDistanceSquared = TNumericLimits<float>::Max();
		bool bFound = false;
		FVector2D Best = FVector2D::ZeroVector;

		for (int32 OffsetY = -Ring; OffsetY <= Ring; ++OffsetY)
		{
			for (int32 OffsetX = -Ring; OffsetX <= Ring; ++OffsetX)
			{
				// Only the shell of this ring; the interior was covered already.
				if (FMath::Max(FMath::Abs(OffsetX), FMath::Abs(OffsetY)) != Ring)
				{
					continue;
				}
				const int32 X = StartX + OffsetX;
				const int32 Y = StartY + OffsetY;
				if (IsBlockedCell(X, Y))
				{
					continue;
				}
				const FVector2D Center = CellCenter(X, Y);
				const float DistanceSquared = FVector2D::DistSquared(Center, World);
				// The < is what makes this deterministic: an equidistant later cell
				// never displaces an earlier one, so scan order decides ties in a
				// fixed way rather than an arbitrary one.
				if (DistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = DistanceSquared;
					Best = Center;
					bFound = true;
				}
			}
		}

		if (bFound)
		{
			OutWorld = Best;
			return true;
		}
	}

	return false;
}

bool FCigNavGrid::HasClearLine(const FVector2D& From, const FVector2D& To) const
{
	if (!bBuilt)
	{
		return false;
	}

	const float Distance = FVector2D::Distance(From, To);
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return IsWalkable(From);
	}

	// Sampled at half a cell so no cell along the line can be stepped over.
	const int32 Steps = FMath::CeilToInt(Distance / (Cell * 0.5f));
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const float Alpha = static_cast<float>(Step) / static_cast<float>(Steps);
		if (!IsWalkable(FMath::Lerp(From, To, Alpha)))
		{
			return false;
		}
	}
	return true;
}

FCigPathResult FCigNavGrid::FindPath(const FVector2D& Start, const FVector2D& Goal) const
{
	FCigPathResult Result;
	if (!bBuilt)
	{
		Result.Failure = ECigPathFailure::NotBuilt;
		return Result;
	}

	int32 StartX = 0;
	int32 StartY = 0;
	if (!WorldToCell(Start, StartX, StartY))
	{
		Result.Failure = ECigPathFailure::StartOutsideBounds;
		return Result;
	}

	int32 GoalX = 0;
	int32 GoalY = 0;
	if (!WorldToCell(Goal, GoalX, GoalY))
	{
		Result.Failure = ECigPathFailure::GoalOutsideBounds;
		return Result;
	}

	if (IsBlockedCell(StartX, StartY))
	{
		Result.Failure = ECigPathFailure::StartBlocked;
		return Result;
	}
	if (IsBlockedCell(GoalX, GoalY))
	{
		Result.Failure = ECigPathFailure::GoalBlocked;
		return Result;
	}

	const int32 CellCount = CountX * CountY;
	const int32 StartIndex = Index(StartX, StartY);
	const int32 GoalIndex = Index(GoalX, GoalY);

	if (StartIndex == GoalIndex)
	{
		Result.bSuccess = true;
		Result.Points.Add(Start);
		Result.Points.Add(Goal);
		Result.Length = FVector2D::Distance(Start, Goal);
		return Result;
	}

	TArray<float> CostSoFar;
	CostSoFar.Init(TNumericLimits<float>::Max(), CellCount);
	TArray<int32> CameFrom;
	CameFrom.Init(INDEX_NONE, CellCount);
	TArray<uint8> Closed;
	Closed.Init(0, CellCount);

	// Entry is {priority, cost, cell}. The cell index is carried into the
	// comparison so that two cells with the same f-score always come off in the
	// same order - without it the path can differ between runs on a symmetric
	// layout, and a test asserting a route would be flaky rather than wrong.
	struct FOpenEntry
	{
		float Priority = 0.f;
		int32 CellIndex = INDEX_NONE;

		bool operator<(const FOpenEntry& Other) const
		{
			if (!FMath::IsNearlyEqual(Priority, Other.Priority))
			{
				return Priority < Other.Priority;
			}
			return CellIndex < Other.CellIndex;
		}
	};

	TArray<FOpenEntry> Open;
	Open.HeapPush({ Octile(StartX, StartY, GoalX, GoalY), StartIndex });
	CostSoFar[StartIndex] = 0.f;

	static const int32 OffsetsX[8] = { 1, -1, 0,  0, 1,  1, -1, -1 };
	static const int32 OffsetsY[8] = { 0,  0, 1, -1, 1, -1,  1, -1 };

	bool bReached = false;
	while (Open.Num() > 0)
	{
		FOpenEntry Current;
		Open.HeapPop(Current, EAllowShrinking::No);

		if (Closed[Current.CellIndex] != 0)
		{
			continue;
		}
		Closed[Current.CellIndex] = 1;
		++Result.CellsSearched;

		if (Current.CellIndex == GoalIndex)
		{
			bReached = true;
			break;
		}

		const int32 CurrentX = Current.CellIndex % CountX;
		const int32 CurrentY = Current.CellIndex / CountX;

		for (int32 Direction = 0; Direction < 8; ++Direction)
		{
			const int32 NextX = CurrentX + OffsetsX[Direction];
			const int32 NextY = CurrentY + OffsetsY[Direction];
			if (IsBlockedCell(NextX, NextY))
			{
				continue;
			}

			const bool bDiagonal = Direction >= 4;
			if (bDiagonal)
			{
				// Squeezing between two diagonally touching obstacles is a path on
				// a grid and a collision in the world. Both orthogonal neighbours
				// have to be open for the corner to be cuttable.
				if (IsBlockedCell(CurrentX + OffsetsX[Direction], CurrentY)
					|| IsBlockedCell(CurrentX, CurrentY + OffsetsY[Direction]))
				{
					continue;
				}
			}

			const int32 NextIndex = Index(NextX, NextY);
			if (Closed[NextIndex] != 0)
			{
				continue;
			}

			const float StepCost = bDiagonal ? DiagonalStep : 1.f;
			const float NewCost = CostSoFar[Current.CellIndex] + StepCost;
			if (NewCost < CostSoFar[NextIndex])
			{
				CostSoFar[NextIndex] = NewCost;
				CameFrom[NextIndex] = Current.CellIndex;
				Open.HeapPush({ NewCost + Octile(NextX, NextY, GoalX, GoalY), NextIndex });
			}
		}
	}

	if (!bReached)
	{
		Result.Failure = ECigPathFailure::NoRoute;
		return Result;
	}

	TArray<FVector2D> Reversed;
	for (int32 Walk = GoalIndex; Walk != INDEX_NONE; Walk = CameFrom[Walk])
	{
		Reversed.Add(CellCenter(Walk % CountX, Walk / CountX));
		if (Walk == StartIndex)
		{
			break;
		}
	}
	Algo::Reverse(Reversed);

	// The true endpoints replace the cell centres they were snapped to, so a
	// caller is given the position it asked for rather than one up to half a cell
	// away from it.
	if (Reversed.Num() > 0)
	{
		Reversed[0] = Start;
		Reversed.Last() = Goal;
	}

	// Greedy string pulling. A raw grid path is a staircase, and a body following
	// one visibly zig-zags across an empty room; keeping only the corners it has
	// to turn at leaves the same route without the steps.
	TArray<FVector2D>& Points = Result.Points;
	Points.Add(Reversed[0]);
	int32 Anchor = 0;
	for (int32 Probe = 2; Probe < Reversed.Num(); ++Probe)
	{
		if (!HasClearLine(Reversed[Anchor], Reversed[Probe]))
		{
			Points.Add(Reversed[Probe - 1]);
			Anchor = Probe - 1;
		}
	}
	Points.Add(Reversed.Last());

	for (int32 Point = 1; Point < Points.Num(); ++Point)
	{
		Result.Length += FVector2D::Distance(Points[Point - 1], Points[Point]);
	}

	Result.bSuccess = true;
	return Result;
}
