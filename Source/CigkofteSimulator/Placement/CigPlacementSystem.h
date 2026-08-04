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
	bool TryGetPlacementConsequence(FName StableId, FCigPlacementConsequence& OutConsequence) const;
	bool AddProtectedZone(const FCigProtectedZone& Zone);

	// Replaces the whole record set with a candidate that has already been
	// validated as a whole.
	//
	// This is the swap the transactional load exists to perform, and it is a swap
	// rather than a replay for a reason found the hard way: re-registering the
	// saved records one at a time asks the authority a different question than the
	// candidate was asked, and the answers differed. The authored service counter
	// stands in the entrance by design - accepted when the shop is built, refused
	// when the same record is offered to a floor whose history is not the same. A
	// layout that has been judged good must not be judged again on the way in.
	//
	// Publishes exactly one PlacementChanged, because the point of validating the
	// whole layout first is that nothing downstream has to see it arrive in pieces.
	void AdoptValidatedLayout(const FCigPlacementAuthority& Candidate);

	int32 PlacementCount() const { return Authority.RecordCount(); }
	int32 PlacementCountByCategory(ECigPlacementCategory Category) const { return Authority.CountByCategory(Category); }
	int32 PlacementCountByLifetime(ECigPlacementLifetime Lifetime) const { return Authority.CountByLifetime(Lifetime); }
	int32 PlacementConsequenceCount() const { return Authority.ConsequenceCount(); }
	int32 FunctionalCapacityByCategory(ECigPlacementCategory Category) const
	{
		return Authority.CountFunctionalCapacity(Category);
	}
	int32 InstalledLayoutConsequenceCount() const { return Authority.CountInstalledLayoutConsequences(); }
	int32 ProtectedZoneCount() const { return Authority.ProtectedZoneCount(); }
	const TArray<FCigPlacementRecord>& PlacementRecords() const { return Authority.GetRecords(); }

	static FCigPlacementFootprint StockCrateFootprint();
	static FCigPlacementUseSpec StockCrateUseSpec();
	static FString FailureText(ECigPlacementFailure Failure);

private:
	FCigPlacementAuthority Authority;
};
