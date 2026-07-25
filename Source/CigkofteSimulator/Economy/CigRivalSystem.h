#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigRivalSystem.generated.h"

// State of a rival business.
struct FCigRival
{
	FString Name;
	float Price = 1.f;       // price multiplier
	float Quality = 60.f;    // 0-100
	float Hygiene = 60.f;
	float Popularity = 50.f; // 0-100
	float AdPower = 0.f;     // campaign strength, decays over time
	int32 Capacity = 30;     // daily customer capacity
	int32 Attitude = 0;      // -1 hostile, 0 neutral, 1 respectful
	bool bOpen = true;
	int32 BadDays = 0;
};

// A simple daily rival simulation; it acts through the tablet report and the
// pull on customers.
UCLASS()
class UCigRivalSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;
	virtual void OnDayEnd(int32 Day) override;

	// Multiplier on the customer rate reaching the player (< 1 when rivals are strong).
	float PlayerPullMult() const;

	// How many rivals are still open?
	int32 OpenRivalCount() const;

	TArray<FCigRival> Rivals;
};
