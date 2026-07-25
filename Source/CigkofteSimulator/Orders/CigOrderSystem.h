#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "CigOrderSystem.generated.h"

// The wrap the player builds step by step on the counter.
struct FCigWrapBuild
{
	bool bActive = false;
	int32 Portions = 0;
	float DoughQuality = 0.f;    // effective quality of the dough added, freshness included
	ECigSpice Spice = ECigSpice::Orta;
	int32 Recipe = 0;
	uint8 ToppingMask = 0;
	bool bAyran = false;
	bool bPacked = false;        // false = plate
	bool bWrapped = false;
	ECigSide Side = ECigSide::Yok; // side added to the tray
	float StartTime = 0.f;       // when assembly began (world seconds)

	bool HasTopping(ECigTopping T) const { return (ToppingMask & (1 << (uint8)T)) != 0; }
};

// A package put on the shelf, for delivery.
struct FCigPackagedWrap
{
	FCigWrapBuild Build;
	float Temp = 100.f; // heat and freshness; falls over time
};

// The breakdown of an order accuracy evaluation.
struct FCigOrderScore
{
	float Accuracy = 0.f; // 0-100
	TArray<FString> Notes;
};

// The wrap assembly flow and the order accuracy calculation.
UCLASS()
class UCigOrderSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;

	// --- Wrap assembly steps ---
	void StartWrap();                 // lay down the flatbread
	void AddPortion();                // add one unit of dough
	void ToggleTopping(ECigTopping T);
	void ToggleAyran();
	void TogglePack();
	void CycleSide();                 // sides counter: cycle through the menu items
	void FinishWrap();                // roll it
	void ShelfWrap();                 // move it to the delivery shelf
	void DiscardWrap();

	// Computes order accuracy (what was built against what was asked for).
	static FCigOrderScore ScoreWrap(const FCigWrapBuild& Build, const FCigOrderSpec& Spec, float PrepSeconds);

	// Generates a random order; traits influence its complexity.
	FCigOrderSpec MakeOrderSpec(int32 Day, ECigTrait Traits, bool bAllowAyran) const;

	static FString DescribeSpec(const FCigOrderSpec& Spec);
	FString DescribeWrap() const;

	FCigWrapBuild Wrap;
	TArray<FCigPackagedWrap> Shelf;
	static constexpr int32 MaxShelf = 4;

	// The counter visual for the wrap being built (flatbread -> filled -> rolled
	// -> takeaway/plate).
	UPROPERTY() TObjectPtr<class AStaticMeshActor> WrapVisual;
	void UpdateWrapVisual();
	// Where that visual sits in the world; derived from the flatbread station.
	FVector WrapVisualPos() const;
};
