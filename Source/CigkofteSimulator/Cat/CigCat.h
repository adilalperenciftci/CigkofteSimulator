#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CigCat.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UMaterialInstanceDynamic;

// The shop cat: wanders inside and purrs when petted.
// With Cat_Animation_Pack it uses a skeletal, animated model; without it, it
// falls back to a simple cat built from primitives.
UCLASS()
class ACigCat : public AActor
{
	GENERATED_BODY()

public:
	ACigCat();

	virtual void Tick(float DeltaSeconds) override;

	void Init();

	// Whether the animated model loaded (the primitive parts are then hidden).
	bool bSkeletal = false;

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> SkelBody;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimWalk;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimSit;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimIdle;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Tail;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> EarL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> EarR;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FurMID;

private:
	FVector Target = FVector::ZeroVector;
	float StateTimer = 0.f;
	bool bSitting = false;
	float TailPhase = 0.f;
	bool bPlayingSit = false; // tracks which animation is running

	void PickTarget();
	// Loads the animated model when available and hides the primitive parts.
	void TrySetupSkeletalCat();
	void PlayCatAnim(bool bSit);
};
