#include "Placement/CigPlacementSystem.h"
#include "Core/CigText.h"
#include "Game/CigEventBus.h"

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
	FCigPlacementResult Result = Authority.TryRegister(Request);
	if (Result.bAccepted && Result.bStateChanged)
	{
		FCigPlacementChange Change;
		Change.StableId = Request.StableId;
		Change.Mutation = Request.Context == ECigPlacementContext::MoveExisting
			? ECigPlacementMutation::Moved : ECigPlacementMutation::Added;
		Change.Category = Request.Category;
		Change.Lifetime = Request.Lifetime;
		Bus().PlacementChanged.Broadcast(Change);
	}
	return Result;
}

FCigPlacementResult UCigPlacementSystem::FindFirstValidPlacement(const FCigPlacementRequest& BaseRequest,
	const TArray<FTransform>& OrderedCandidates) const
{
	return Authority.FindFirstValid(BaseRequest, OrderedCandidates);
}

bool UCigPlacementSystem::RemovePlacement(FName StableId)
{
	const FCigPlacementRecord* Existing = Authority.Find(StableId);
	if (!Existing)
	{
		return false;
	}
	FCigPlacementChange Change;
	Change.StableId = Existing->StableId;
	Change.Mutation = ECigPlacementMutation::Removed;
	Change.Category = Existing->Category;
	Change.Lifetime = Existing->Lifetime;
	if (!Authority.Remove(StableId))
	{
		return false;
	}
	Bus().PlacementChanged.Broadcast(Change);
	return true;
}

const FCigPlacementRecord* UCigPlacementSystem::FindPlacement(FName StableId) const
{
	return Authority.Find(StableId);
}

bool UCigPlacementSystem::TryGetPlacementConsequence(FName StableId,
	FCigPlacementConsequence& OutConsequence) const
{
	return Authority.TryGetConsequence(StableId, OutConsequence);
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

FCigPlacementUseSpec UCigPlacementSystem::StockCrateUseSpec()
{
	FCigPlacementUseSpec Result;
	// The player approaches from the shop interior (+X). This is deterministic
	// floor geometry, not a claim that navmesh can reach the rectangle.
	Result.Size = FVector2D(100.f, 80.f);
	Result.CenterOffset = FVector2D(90.f, 0.f);
	Result.FunctionalCapacity = 1;
	return Result;
}

FString UCigPlacementSystem::FailureText(ECigPlacementFailure Failure)
{
	const TCHAR* Key = TEXT("msg.placement.invalid");
	switch (Failure)
	{
	case ECigPlacementFailure::OutsideShopBounds: Key = TEXT("msg.placement.outside"); break;
	case ECigPlacementFailure::FunctionalAreaOutsideShop: Key = TEXT("msg.placement.useoutside"); break;
	case ECigPlacementFailure::Overlap: Key = TEXT("msg.placement.overlap"); break;
	case ECigPlacementFailure::BlocksFunctionalClearance: Key = TEXT("msg.placement.clearance"); break;
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
	case ECigPlacementFailure::ConsequenceMismatch:
		Key = TEXT("msg.placement.classification");
		break;
	case ECigPlacementFailure::InvalidConsequence:
		Key = TEXT("msg.placement.consequence");
		break;
	default: break;
	}
	return CigText::Get(Key);
}
