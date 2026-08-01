#include "Placement/CigPlacementSystem.h"
#include "Core/CigText.h"

void UCigPlacementSystem::OnInit()
{
	Authority.Configure(CigPlacementLayout::ShopBounds());
	Authority.AddProtectedZone(CigPlacementLayout::EntranceZone());
	Authority.AddProtectedZone(CigPlacementLayout::QueueZone());
	Authority.AddProtectedZone(CigPlacementLayout::ServiceRouteZone());
	Authority.AddProtectedZone(CigPlacementLayout::PlayerRouteZone());
}

FCigPlacementResult UCigPlacementSystem::ValidatePlacement(const FCigPlacementRequest& Request) const
{
	return Authority.Validate(Request);
}

FCigPlacementResult UCigPlacementSystem::RegisterPlacement(const FCigPlacementRequest& Request)
{
	return Authority.TryRegister(Request);
}

FCigPlacementResult UCigPlacementSystem::FindFirstValidPlacement(const FCigPlacementRequest& BaseRequest,
	const TArray<FTransform>& OrderedCandidates) const
{
	return Authority.FindFirstValid(BaseRequest, OrderedCandidates);
}

bool UCigPlacementSystem::RemovePlacement(FName StableId)
{
	return Authority.Remove(StableId);
}

const FCigPlacementRecord* UCigPlacementSystem::FindPlacement(FName StableId) const
{
	return Authority.Find(StableId);
}

bool UCigPlacementSystem::AddProtectedZone(const FCigProtectedZone& Zone)
{
	return Authority.AddProtectedZone(Zone);
}

FCigPlacementFootprint UCigPlacementSystem::StockCrateFootprint()
{
	// ACigStockCrate's repository-owned primitive is 60 x 45. Placement keeps a
	// ten-centimetre handling margin regardless of whether a future optional mesh
	// is installed.
	return FCigPlacementFootprint::FromOptionalMeshSize(TOptional<FVector2D>(), FVector2D(60.f, 45.f), 10.f);
}

FString UCigPlacementSystem::FailureText(ECigPlacementFailure Failure)
{
	const TCHAR* Key = TEXT("msg.placement.invalid");
	switch (Failure)
	{
	case ECigPlacementFailure::OutsideShopBounds: Key = TEXT("msg.placement.outside"); break;
	case ECigPlacementFailure::Overlap: Key = TEXT("msg.placement.overlap"); break;
	case ECigPlacementFailure::BlocksEntrance: Key = TEXT("msg.placement.entrance"); break;
	case ECigPlacementFailure::BlocksQueue: Key = TEXT("msg.placement.queue"); break;
	case ECigPlacementFailure::BlocksServiceRoute: Key = TEXT("msg.placement.service"); break;
	case ECigPlacementFailure::BlocksStationAccess: Key = TEXT("msg.placement.station"); break;
	case ECigPlacementFailure::BlocksPlayerRoute: Key = TEXT("msg.placement.route"); break;
	case ECigPlacementFailure::UnsupportedRotation: Key = TEXT("msg.placement.rotation"); break;
	case ECigPlacementFailure::InvalidFloor: Key = TEXT("msg.placement.floor"); break;
	case ECigPlacementFailure::DuplicateStableId: Key = TEXT("msg.placement.duplicate"); break;
	case ECigPlacementFailure::NoDeliverySpotAvailable: Key = TEXT("msg.placement.deliveryfull"); break;
	case ECigPlacementFailure::UnknownCategory:
	case ECigPlacementFailure::UnknownLifetime:
	case ECigPlacementFailure::UnknownContext:
	case ECigPlacementFailure::InvalidIgnoreStableId:
	case ECigPlacementFailure::InvalidClassification:
	case ECigPlacementFailure::CategoryMismatch:
	case ECigPlacementFailure::LifetimeMismatch:
		Key = TEXT("msg.placement.classification");
		break;
	default: break;
	}
	return CigText::Get(Key);
}
