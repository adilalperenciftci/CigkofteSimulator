#include "Save/CigLayoutLoad.h"

#include "Navigation/CigNavGrid.h"
#include "Navigation/CigNavLayout.h"

namespace
{
	FCigLayoutLoadReport Fail(ECigLayoutLoadFailure Failure, FName StableId, const FString& Diagnostic)
	{
		FCigLayoutLoadReport Report;
		Report.bAccepted = false;
		Report.Failure = Failure;
		Report.FailedStableId = StableId;
		Report.Diagnostic = Diagnostic;
		return Report;
	}
}

TArray<FCigProtectedZone> CigLayoutLoad::ShopProtectedZones()
{
	// The same four UCigPlacementSystem::OnInit configures. Listed here rather
	// than read off the live system so a candidate is validated against the shop's
	// rules even when there is no live system - which is the case in a test, and
	// would also be the case if a load ever ran before the systems were built.
	return {
		CigPlacementLayout::EntranceZone(),
		CigPlacementLayout::QueueZone(),
		CigPlacementLayout::ServiceRouteZone(),
		CigPlacementLayout::PlayerRouteZone()
	};
}

FCigLayoutLoadReport CigLayoutLoad::BuildCandidate(
	const TArray<FCigSavePlacement>& Saved,
	const FCigPlacementBounds& Bounds,
	const TArray<FCigProtectedZone>& ProtectedZones,
	const TArray<FVector2D>& SeatStands,
	FCigPlacementAuthority& OutCandidate)
{
	OutCandidate.ResetRecords();
	OutCandidate.Configure(Bounds);
	for (const FCigProtectedZone& Zone : ProtectedZones)
	{
		OutCandidate.AddProtectedZone(Zone);
	}

	FCigLayoutLoadReport Report;

	// 1. Every record on its own terms, before any of them touch the floor. A
	// record that cannot be understood must not be allowed to fail later as an
	// overlap and be reported as one.
	TSet<FName> Seen;
	Seen.Reserve(Saved.Num());
	for (const FCigSavePlacement& One : Saved)
	{
		const ECigSavePlacementFault Fault = CigSavePlacement::Validate(One);
		if (Fault != ECigSavePlacementFault::None)
		{
			FCigLayoutLoadReport Bad = Fail(ECigLayoutLoadFailure::RecordFault, One.StableId,
				FString::Printf(TEXT("kayit okunamadi: %s"), CigSavePlacement::FaultText(Fault)));
			Bad.RecordFault = Fault;
			return Bad;
		}

		// Before registration, because the authority would treat the second one as
		// a move of the first and quietly keep one record where the file has two.
		bool bAlready = false;
		Seen.Add(One.StableId, &bAlready);
		if (bAlready)
		{
			return Fail(ECigLayoutLoadFailure::DuplicateStableId, One.StableId,
				TEXT("ayni kararli kimlik iki kez yaziyor"));
		}
	}

	// 2. The floor. Bounds, overlap and protected zones are the authority's own
	// rules, and it applies exactly the same ones here as it does to a live
	// placement - there is no second, weaker path for loading.
	for (const FCigSavePlacement& One : Saved)
	{
		const FCigPlacementResult Result = OutCandidate.TryRegister(CigSavePlacement::ToRequest(One));
		if (!Result.bAccepted)
		{
			FCigLayoutLoadReport Bad = Fail(ECigLayoutLoadFailure::Rejected, One.StableId,
				FString::Printf(TEXT("%s reddedildi (kod %d, catisan: %s, bolge: %s)"),
					*One.StableId.ToString(), (int32)Result.Failure,
					*Result.ConflictingStableId.ToString(), *Result.ProtectedZoneId.ToString()));
			Bad.PlacementFailure = Result.Failure;
			Bad.ConflictingStableId = Result.ConflictingStableId;
			Bad.RegisteredCount = Report.RegisteredCount;
			return Bad;
		}
		++Report.RegisteredCount;
	}

	// 3. Reachability. Every placement can be individually legal and still leave a
	// gap narrower than a customer, which no rectangle rule can see.
	TArray<FCigPlacementRect> Shell;
	for (const CigNavLayout::FCigShellWall& Wall : CigNavLayout::ShellWalls())
	{
		Shell.Add(Wall.Rect);
	}

	FCigNavGrid Grid;
	Grid.Build(CigNavLayout::NavBounds(), OutCandidate.GetRecords(), Shell,
		CigNavLayout::CustomerAgentRadius(), CigNavLayout::NavCellSize());

	const FVector2D Street = CigNavLayout::StreetApproach();
	const FVector QueueFrontLocation = CigPlacementLayout::QueueFront();
	const FVector2D QueueFront(QueueFrontLocation.X, QueueFrontLocation.Y);
	const FVector2D Service = CigPlacementLayout::ServiceRouteZone().Center;

	auto CheckRoute = [&Grid](FName RouteId, const FVector2D& From, const FVector2D& To,
		FCigLayoutLoadReport& OutBad) -> bool
	{
		if (Grid.FindPath(From, To).bSuccess)
		{
			return true;
		}
		OutBad = Fail(ECigLayoutLoadFailure::RouteClosed, NAME_None,
			FString::Printf(TEXT("zorunlu rota kapali: %s"), *RouteId.ToString()));
		OutBad.ClosedRouteId = RouteId;
		return false;
	};

	FCigLayoutLoadReport Bad;
	if (!CheckRoute(TEXT("route.queue"), Street, QueueFront, Bad)) { return Bad; }
	if (!CheckRoute(TEXT("route.counter"), QueueFront, Service, Bad)) { return Bad; }

	for (int32 Index = 0; Index < SeatStands.Num(); ++Index)
	{
		// The goal is the nearest standable point, not the chair: a chair sits
		// inside its own table's footprint, so asking for the seat centre would
		// fail every table in the shop.
		FVector2D Stand = SeatStands[Index];
		const FName RouteId(*FString::Printf(TEXT("route.seat.%d"), Index));
		if (!Grid.FindNearestWalkable(SeatStands[Index], Stand))
		{
			Bad = Fail(ECigLayoutLoadFailure::RouteClosed, NAME_None,
				FString::Printf(TEXT("koltuk cevresi kapali: %s"), *RouteId.ToString()));
			Bad.ClosedRouteId = RouteId;
			return Bad;
		}
		if (!CheckRoute(RouteId, QueueFront, Stand, Bad)) { return Bad; }
	}

	Report.bAccepted = true;
	Report.Failure = ECigLayoutLoadFailure::None;
	return Report;
}

const TCHAR* CigLayoutLoad::FailureText(ECigLayoutLoadFailure Failure)
{
	switch (Failure)
	{
	case ECigLayoutLoadFailure::None:              return TEXT("sorun yok");
	case ECigLayoutLoadFailure::RecordFault:       return TEXT("kayit hatali");
	case ECigLayoutLoadFailure::DuplicateStableId: return TEXT("yinelenen kararli kimlik");
	case ECigLayoutLoadFailure::Rejected:          return TEXT("yerlesim reddedildi");
	case ECigLayoutLoadFailure::RouteClosed:       return TEXT("zorunlu rota kapali");
	}
	return TEXT("bilinmeyen");
}
