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

	// How much of this item - or, for cold goods, of the cold pool - is already
	// on its way. Capacity is checked against the shelf plus this, or two orders
	// placed back to back both pass and both arrive.
	int32 PendingAmountFor(int32 Item) const;

	// Moves what fits from a crate into the pantry and destroys it when empty.
	// Returns how much moved, so the caller can tell a full fridge from a
	// successful unload without asking twice.
	int32 UnloadCrate(class ACigStockCrate* Crate);

	// Deliveries standing in the shop, waiting to be put away.
	UPROPERTY() TArray<TWeakObjectPtr<class ACigStockCrate>> Crates;

	// Puts away anything still standing about, at the quality it has by then.
	virtual void OnDayEnd(int32 Day) override;

	// --- Chopping ---
	void ChopPress();

	int32 Stock[CigStockCount] = { 0 };
	float StockQuality[CigStockCount] = { 0.f };

	TArray<FCigPendingOrder> PendingOrders;

	int32 Garnish = 0;      // chopped garnish ready to use (lettuce based)
	int32 ChopCombo = 0;
	static constexpr int32 MaxGarnish = 10;

private:
	void SpawnCrate(const FCigPendingOrder& Order);
};
