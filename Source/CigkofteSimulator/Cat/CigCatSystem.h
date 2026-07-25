#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigCatSystem.generated.h"

class ACigCat;

// The shop mascot cat's needs, happiness and bonuses.
UCLASS()
class UCigCatSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;
	virtual void UpdateSystem(float DeltaSeconds) override;
	virtual void OnDayStart(int32 Day) override;

	void Pet();
	void Feed();     // MamaKabi station: 20 TL, hunger resets
	void SetCatName(const FString& NewName);

	float Happiness() const;

	FString CatName = TEXT("Pamuk");
	float Food = 70.f;      // 0 starving, 100 full
	float Attention = 60.f; // 0 sulking, 100 delighted
	float PetCooldown = 0.f;

	UPROPERTY() TObjectPtr<ACigCat> Cat;

private:
	float NeedTimer = 0.f;
	bool bHungryWarned = false;
};
