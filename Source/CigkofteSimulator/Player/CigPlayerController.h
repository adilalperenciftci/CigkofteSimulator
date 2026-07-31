#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CigPlayerController.generated.h"

// The project cheat manager is assigned only outside Shipping; Development
// packages retain deterministic screenshot and benchmark commands.
UCLASS()
class ACigPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACigPlayerController();

	virtual void BeginPlay() override;
};
