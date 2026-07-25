#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CigFloatText.generated.h"

class UTextRenderComponent;

// A floating label that rises and fades (+65 TL and so on).
UCLASS()
class ACigFloatText : public AActor
{
	GENERATED_BODY()

public:
	ACigFloatText();

	virtual void Tick(float DeltaSeconds) override;

	void Init(const FString& Text, const FColor& Color, float WorldSize);

	UPROPERTY() TObjectPtr<UTextRenderComponent> TextComp;

private:
	float Life = 1.6f;
};
