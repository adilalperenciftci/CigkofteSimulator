#include "Navigation/CigNavSystem.h"

#include "Game/CigEventBus.h"
#include "Game/CigkofteGameMode.h"
#include "Navigation/CigNavLayout.h"
#include "Placement/CigPlacementSystem.h"
#include "World/CigWorldBuilder.h"

void UCigNavSystem::OnInit()
{
	Bus().PlacementChanged.AddUObject(this, &UCigNavSystem::HandlePlacementChanged);
	bDirty = true;
}

void UCigNavSystem::HandlePlacementChanged(const FCigPlacementChange& Change)
{
	// Every mutation matters, including a decoration: a decoration occupies the
	// floor exactly as a station does, and the one thing navigation cares about
	// is what stands on the floor. Filtering by category here would make a sofa
	// dropped in the doorway invisible to pathing.
	MarkDirty();
}

void UCigNavSystem::MarkDirty()
{
	bDirty = true;
}

void UCigNavSystem::GatherObstacles(TArray<FCigPlacementRecord>& OutRecords,
	TArray<FCigPlacementRect>& OutStatic) const
{
	OutRecords.Reset();
	OutStatic.Reset();

	if (GM && GM->Placement)
	{
		OutRecords = GM->Placement->PlacementRecords();
	}

	for (const CigNavLayout::FCigShellWall& Wall : CigNavLayout::ShellWalls())
	{
		OutStatic.Add(Wall.Rect);
	}
}

void UCigNavSystem::EnsureBuilt() const
{
	if (!bDirty && CustomerGrid.IsBuilt() && PlayerGrid.IsBuilt())
	{
		return;
	}

	TArray<FCigPlacementRecord> Records;
	TArray<FCigPlacementRect> Static;
	GatherObstacles(Records, Static);

	const FCigPlacementBounds Bounds = CigNavLayout::NavBounds();
	const float Cell = CigNavLayout::NavCellSize();
	CustomerGrid.Build(Bounds, Records, Static, CigNavLayout::CustomerAgentRadius(), Cell);
	PlayerGrid.Build(Bounds, Records, Static, CigNavLayout::PlayerAgentRadius(), Cell);

	bDirty = false;
	++RebuildCounter;
}

FCigPathResult UCigNavSystem::FindCustomerPath(const FVector& Start, const FVector& Goal) const
{
	EnsureBuilt();
	++QueryCounter;
	return CustomerGrid.FindPath(FVector2D(Start.X, Start.Y), FVector2D(Goal.X, Goal.Y));
}

FCigPathResult UCigNavSystem::FindPlayerPath(const FVector& Start, const FVector& Goal) const
{
	EnsureBuilt();
	++QueryCounter;
	return PlayerGrid.FindPath(FVector2D(Start.X, Start.Y), FVector2D(Goal.X, Goal.Y));
}

bool UCigNavSystem::IsCustomerStandable(const FVector& World) const
{
	EnsureBuilt();
	return CustomerGrid.IsWalkable(FVector2D(World.X, World.Y));
}

bool UCigNavSystem::IsPlayerStandable(const FVector& World) const
{
	EnsureBuilt();
	return PlayerGrid.IsWalkable(FVector2D(World.X, World.Y));
}

bool UCigNavSystem::FindCustomerStand(const FVector& World, FVector& OutWorld) const
{
	EnsureBuilt();
	FVector2D Found = FVector2D::ZeroVector;
	if (!CustomerGrid.FindNearestWalkable(FVector2D(World.X, World.Y), Found))
	{
		return false;
	}
	OutWorld = FVector(Found.X, Found.Y, World.Z);
	return true;
}

TArray<FCigRouteReport> UCigNavSystem::AuditRoutesOn(const FCigNavGrid& Grid) const
{
	TArray<FCigRouteReport> Reports;

	const FVector2D Street = CigNavLayout::StreetApproach();
	const FVector QueueFrontLocation = CigPlacementLayout::QueueFront();
	const FVector2D QueueFront(QueueFrontLocation.X, QueueFrontLocation.Y);

	auto AddRoute = [&Reports, &Grid](FName RouteId, const FVector2D& Start, const FVector2D& Goal)
	{
		FCigRouteReport Report;
		Report.RouteId = RouteId;
		Report.Start = Start;
		Report.Goal = Goal;
		Report.Result = Grid.FindPath(Start, Goal);
		Reports.Add(Report);
	};

	// Reaching the counter. The queue runs back along the pavement, so this is
	// entirely outdoors: what it catches is a delivery crate parked across the
	// queue, which is a legal placement in front of the shop.
	AddRoute(TEXT("route.queue"), Street, QueueFront);

	// From the counter into the shop. Customers who are given a seat cross here,
	// and so does the player between the counter and the service area, so this is
	// the route that has to survive the shopfront being furnished.
	AddRoute(TEXT("route.counter"), QueueFront, CigPlacementLayout::ServiceRouteZone().Center);

	// One route per seat. The goal is the nearest standable point rather than the
	// seat itself: a chair is inside its table's footprint, so the seat centre is
	// blocked by construction and asking for it would fail every table in the shop.
	if (GM && GM->WorldBuilder)
	{
		for (int32 SeatIndex = 0; SeatIndex < GM->WorldBuilder->Seats.Num(); ++SeatIndex)
		{
			const UCigWorldBuilder::FCigSeat& Seat = GM->WorldBuilder->Seats[SeatIndex];
			const FVector2D SeatXY(Seat.Pos.X, Seat.Pos.Y);
			const FName RouteId(*FString::Printf(TEXT("route.seat.%d"), SeatIndex));
			FVector2D Stand = SeatXY;
			if (!Grid.FindNearestWalkable(SeatXY, Stand))
			{
				// No standable cell anywhere near the chair: the table has been
				// boxed in. Reported as a blocked goal rather than as a missing
				// route, because those need different fixes.
				FCigRouteReport Report;
				Report.RouteId = RouteId;
				Report.Start = QueueFront;
				Report.Goal = SeatXY;
				Report.Result.Failure = ECigPathFailure::GoalBlocked;
				Reports.Add(Report);
				continue;
			}
			AddRoute(RouteId, QueueFront, Stand);
		}
	}

	return Reports;
}

TArray<FCigRouteReport> UCigNavSystem::AuditRequiredRoutes() const
{
	EnsureBuilt();
	return AuditRoutesOn(CustomerGrid);
}

bool UCigNavSystem::AreRequiredRoutesOpen() const
{
	for (const FCigRouteReport& Report : AuditRequiredRoutes())
	{
		if (!Report.Result.bSuccess)
		{
			return false;
		}
	}
	return true;
}

bool UCigNavSystem::WouldCloseRequiredRoute(const FCigPlacementRecord& Candidate, FName& OutRouteId) const
{
	OutRouteId = NAME_None;

	TArray<FCigPlacementRecord> Records;
	TArray<FCigPlacementRect> Static;
	GatherObstacles(Records, Static);

	// A move is the candidate replacing its own old record, not standing beside
	// it. Leaving the old one in would make every move that shortened a gap look
	// like it closed one.
	Records.RemoveAll([&Candidate](const FCigPlacementRecord& Existing)
	{
		return Existing.StableId == Candidate.StableId;
	});
	Records.Add(Candidate);

	FCigNavGrid Hypothetical;
	Hypothetical.Build(CigNavLayout::NavBounds(), Records, Static,
		CigNavLayout::CustomerAgentRadius(), CigNavLayout::NavCellSize());

	for (const FCigRouteReport& Report : AuditRoutesOn(Hypothetical))
	{
		if (!Report.Result.bSuccess)
		{
			OutRouteId = Report.RouteId;
			return true;
		}
	}
	return false;
}
