#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "CigDeliverySystem.generated.h"

class AStaticMeshActor;

// An active delivery order.
struct FCigDeliveryOrder
{
	FVector Pos = FVector::ZeroVector;
	FString Address;
	FCigOrderSpec Spec;
	float TimeLeft = 75.f;
	float MaxTime = 75.f;
	int32 BaseReward = 100;
	bool bBulk = false; // bulk order (asks for 2 packages)
};

// Full delivery service: concurrent orders, cooling, fuel and damage, delivery score.
UCLASS()
class UCigDeliverySystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;
	virtual void OnDayStart(int32 Day) override;
	virtual void OnDayEnd(int32 Day) override;

	// The player pressed E near a door; delivers if a matching order exists.
	// Returning true means the interaction was consumed.
	bool TryDeliverAt(const FVector& PlayerPos);

	// Debug / events: create an order immediately.
	void SpawnOrder();
	void SpawnBulkOrder();

	TArray<FCigDeliveryOrder> Orders;
	static constexpr int32 MaxOrders = 3;
	static constexpr float DeliverRadius = 350.f;

	// Delivery stats for the day
	int32 DayDelivered = 0;
	float LastScore = 0.f;

private:
	float NextOrderTimer = 40.f;
	UPROPERTY() TArray<TObjectPtr<AStaticMeshActor>> Beacons;

	void RebuildBeacons();
	void ExpireOrder(int32 Index);
};
