#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "CigInventorySystem.generated.h"

// A stock order in transit with the courier.
struct FCigPendingOrder
{
	int32 Item = 0;
	int32 Amount = 10;
	float TimeLeft = 25.f;
	int32 Supplier = 0;
	float Quality = 1.f;
};

// Stock, courier orders and chopping (garnish prep).
UCLASS()
class UCigInventorySystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;
	virtual void UpdateSystem(float DeltaSeconds) override;

	bool HasStock(int32 Item, int32 Amount = 1) const;
	void Consume(int32 Item, int32 Amount = 1);
	void Add(int32 Item, int32 Amount, float Quality = 1.f);

	// Orders from the active supplier; cost and lead time depend on them.
	void OrderStock(int32 Item);
	int32 OrderCost(int32 Item) const;

	// Average quality multiplier of main ingredients 0-4 (roughly 0.7 - 1.35).
	float AverageIngredientQuality() const;

	// --- Chopping ---
	void ChopPress();

	int32 Stock[CigStockCount] = { 0 };
	float StockQuality[CigStockCount] = { 0.f };

	TArray<FCigPendingOrder> PendingOrders;

	int32 Garnish = 0;      // chopped garnish ready to use (lettuce based)
	int32 ChopCombo = 0;
	static constexpr int32 MaxGarnish = 10;
};
