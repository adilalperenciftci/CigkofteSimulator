#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CigPlayerController.generated.h"

// A controller that keeps the cheat manager active in every build.
UCLASS()
class ACigPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACigPlayerController();

	virtual void BeginPlay() override;
};
