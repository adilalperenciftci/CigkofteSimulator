#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CigCar.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

// The decorative car that drives up and down the street; the player's car is
// this class too.
UCLASS()
class ACigCar : public AActor
{
	GENERATED_BODY()

public:
	ACigCar();

	virtual void Tick(float DeltaSeconds) override;

	void Init(float LaneX, float Dir, const FLinearColor& Color, float InSpeed);

	// The player's car: it stays parked and never joins the traffic loop.
	void SetParked(const FVector& Pos, float Yaw, const FLinearColor& Color);
	// Toggles body collision so it does not catch the trace while driving.
	void SetInteractCollision(bool bEnabled);

	bool bAutoDrive = true;

	// State of the player's car (for the delivery loop).
	float Fuel = 100.f;   // 0-100
	float Damage = 0.f;   // 0-100; yüksek hasar teslimat puanını düşürür

	UPROPERTY() TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Cabin;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMID;

private:
	float Speed = 700.f;
	float MoveDir = 1.f;
};
