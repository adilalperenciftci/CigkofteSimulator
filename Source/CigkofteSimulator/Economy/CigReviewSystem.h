#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "CigReviewSystem.generated.h"

// A single online review.
struct FCigReview
{
	FString Author;
	FString Text;
	int32 Stars = 3;
	int32 Day = 1;
};

// Online reviews and the five-category shop score.
UCLASS()
class UCigReviewSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnDayEnd(int32 Day) override;

	// Collects data while serving; it turns into reviews at end of day.
	void RecordServe(float Quality, float Accuracy, float PatienceFrac, int32 PricePolicy, float Hygiene, ECigTrait Traits);
	void RecordAngryLeave(bool bInfluencer);
	void RecordDelivery(float Score);

	// Overall score (0-5 stars).
	float ShopScore() const;
	float SpawnRateMult() const;

	// Category scores, 0-5
	float FoodScore = 3.f;
	float ServiceScore = 3.f;
	float PriceScore = 3.f;
	float HygieneScore = 3.f;
	float AtmosphereScore = 2.5f;

	TArray<FCigReview> Reviews; // newest first, at most 12

private:
	struct FDayServeData
	{
		float Quality;
		float Accuracy;
		float PatienceFrac;
		float Hygiene;
		int32 PricePolicy;
		uint16 Traits;
	};
	TArray<FDayServeData> DayServes;
	int32 DayAngry = 0;
	int32 DayInfluencerAngry = 0;

	void BlendCategory(float& Cat, float Sample, float Weight = 0.15f);
	void PushReview(const FString& Author, const FString& Text, int32 Stars, int32 Day);
	float ComputeAtmosphere() const;
};
