#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CigStockCrate.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

// A delivery, standing in the shop until somebody puts it away.
//
// Stock used to teleport: an order's timer ran out and the numbers appeared in
// the pantry wherever the player happened to be. Nothing about a delivery was in
// the world, so the supplier tab was a vending machine with a delay on it.
//
// A crate is the smallest thing that makes a delivery an event. It arrives by the
// door, it is in the way, and it does not become stock until the player walks
// over and unloads it - which during service means breaking off from the counter,
// and during preparation is just part of the morning.
//
// It does not need a carry mechanic to do that job, and deliberately does not
// have one: attaching a crate to the player is a much larger feature and would
// buy nothing this does not already deliver.
UCLASS()
class ACigStockCrate : public AActor
{
	GENERATED_BODY()

public:
	ACigStockCrate();

	// Sets up what is inside and what it looks like. Called straight after spawn.
	void Setup(int32 InItem, int32 InAmount, float InQuality);

	void SetHighlighted(bool bOn);

	// What the prompt should say when the player is looking at it.
	FString GetPromptText() const;

	// Time spent standing out of storage, in seconds. Perishables lose quality
	// for it; dry goods do not care.
	void AgeBy(float DeltaSeconds);

	int32 Item = 0;
	int32 Amount = 0;

	// Blends into the pantry's weighted average on unload, the same way a
	// supplier's quality already does.
	float Quality = 1.f;

	// Stable runtime identity owned by the placement authority. The actor pointer
	// is deliberately not the identity and is never persisted.
	FName PlacementId;

private:
	// A plain root the body hangs off, rather than making the body the root.
	//
	// A root component's relative location *is* the actor's location, so setting
	// it in Setup silently threw away the spawn position: every crate stood at
	// the world origin, 90cm below the camera and off the bottom of the screen,
	// while the log insisted a delivery had arrived. It also means Setup can be
	// called again - which a part-unloaded crate does - without walking.
	UPROPERTY() TObjectPtr<USceneComponent> Pivot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMID;

	FLinearColor BaseColor = FLinearColor::White;
	bool bHighlighted = false;

	void RefreshLabel();
};
