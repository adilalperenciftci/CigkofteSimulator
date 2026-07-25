#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigHygieneSystem.generated.h"

// Hygiene across several areas: hands, counter, chopping board, bin, dishes and
// cat hair. Values are stored as dirt or fill level (0 clean, 100 disastrous).
UCLASS()
class UCigHygieneSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;
	virtual void OnDayStart(int32 Day) override;

	// Overall hygiene from 0-100 (100 = spotless). Read by the HUD and the inspector.
	float OverallHygiene() const;

	// Dirt added on serving (a dine-in order produces dishes).
	void OnServeMade(bool bPlate);

	// --- Temizlik eylemleri ---
	void WashHands();
	void CleanSurfaces();  // Temizlik istasyonu: tezgah + doğrama
	void WashDishes();
	void EmptyTrash();

	// Dirt areas
	float HandDirt = 0.f;
	float CounterDirt = 10.f;
	float ChopDirt = 0.f;
	float TrashFill = 20.f;
	float DishPile = 0.f;
	float CatFur = 0.f;

	// A short line naming the most urgent cleaning job (HUD hint).
	FString WorstProblem() const;

private:
	float PassiveTimer = 0.f;
};
