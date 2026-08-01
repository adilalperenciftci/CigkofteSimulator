#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Placement/CigPlacementTypes.h"
#include "CigPlacementSystem.generated.h"

// Single source of truth for shop-floor placement validity. It owns records and
// protected geometry; callers supply explicit footprints instead of inspecting
// world collision or scanning actors.
UCLASS()
class UCigPlacementSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;

	FCigPlacementResult ValidatePlacement(const FCigPlacementRequest& Request) const;
	FCigPlacementResult RegisterPlacement(const FCigPlacementRequest& Request);
	FCigPlacementResult FindFirstValidPlacement(const FCigPlacementRequest& BaseRequest,
		const TArray<FTransform>& OrderedCandidates) const;
	bool RemovePlacement(FName StableId);
	const FCigPlacementRecord* FindPlacement(FName StableId) const;
	bool AddProtectedZone(const FCigProtectedZone& Zone);

	int32 PlacementCount() const { return Authority.RecordCount(); }
	int32 PlacementCountByCategory(ECigPlacementCategory Category) const { return Authority.CountByCategory(Category); }
	int32 PlacementCountByLifetime(ECigPlacementLifetime Lifetime) const { return Authority.CountByLifetime(Lifetime); }
	int32 ProtectedZoneCount() const { return Authority.ProtectedZoneCount(); }
	const TArray<FCigPlacementRecord>& PlacementRecords() const { return Authority.GetRecords(); }

	static FCigPlacementFootprint StockCrateFootprint();
	static FString FailureText(ECigPlacementFailure Failure);

private:
	FCigPlacementAuthority Authority;
};
