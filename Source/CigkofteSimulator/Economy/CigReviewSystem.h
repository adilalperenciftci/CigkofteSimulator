#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "CigReviewSystem.generated.h"

// A single online review.
struct FCigReview
{
	// Identifies the review for as long as it exists. Reviews are inserted at the
	// front, so an index cannot be held across a day.
	int32 Id = 0;

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
	//
	// The price is not passed in: it is read from the pricing system at the
	// moment of the serve, the same effective price the customer was charged.
	// It used to arrive as the cheap/normal/expensive policy setting, so the
	// price stars answered a toggle rather than the bill.
	void RecordServe(float Quality, float Accuracy, float PatienceFrac, float Hygiene, ECigTrait Traits);
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

	// Next value handed out by PushReview. Persisted so IDs stay unique across
	// a save.
	int32 NextReviewId = 1;

	// Null when the review has been trimmed off the end of the list.
	const FCigReview* YorumBul(int32 Id) const;

	// The only way a review enters the list; it assigns the ID and enforces the
	// twelve-entry cap.
	void PushReview(const FString& Author, const FString& Text, int32 Stars, int32 Day);

private:
	struct FDayServeData
	{
		float Quality;
		float Accuracy;
		float PatienceFrac;
		float Hygiene;
		uint16 Traits;
	};
	TArray<FDayServeData> DayServes;
	int32 DayAngry = 0;
	int32 DayInfluencerAngry = 0;

	void BlendCategory(float& Cat, float Sample, float Weight = 0.15f);
	float ComputeAtmosphere() const;
};
