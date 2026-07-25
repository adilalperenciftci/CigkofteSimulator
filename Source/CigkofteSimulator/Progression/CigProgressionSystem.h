#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigProgressionSystem.generated.h"

// XP, levels, reputation, titles and lifetime stats.
UCLASS()
class UCigProgressionSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	void AddXP(int32 Amount);
	void AddRep(float Delta);

	int32 XPForNext() const;
	FString PopularityTitle() const;

	int32 XP = 0;
	int32 Level = 1;
	float Rep = 50.f;

	static constexpr int32 MaxLevel = 10;

	// --- Lifetime stats ---
	int32 TotalServed = 0;
	int32 TotalDeliveries = 0;
	int32 TotalEarned = 0;
	int32 BestDayEarnings = 0;
	int32 TotalAngryCustomers = 0;
	int32 TotalPerfectOrders = 0;
};
