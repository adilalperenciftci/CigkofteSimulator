#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "Core/CigUpgrades.h"
#include "CigEconomySystem.generated.h"

// A supplier definition: the price, quality, speed and reliability trade-off.
struct FCigSupplier
{
	const TCHAR* Name;
	const TCHAR* Desc;
	float PriceMult;
	float Quality;
	float DeliverTime;
	float Reliability;
};

constexpr int32 CigSupplierCount = 5;

// Money, pricing policy, station upgrades, shop upgrades, suppliers and the house.
UCLASS()
class UCigEconomySystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnDayEnd(int32 Day) override;

	static const FCigSupplier& Supplier(int32 Index);

	// The supplier's name and blurb, from the text table when it has them. Same
	// arrangement as the recipes: the literals in the table are the fallback.
	static FString SupplierName(int32 Index);
	static FString SupplierDesc(int32 Index);

	// --- Money ---
	bool TrySpend(int32 Cost);
	void Earn(int32 Amount);

	// --- Pricing policy ---
	void CyclePricePolicy();
	FString PricePolicyName() const;
	float PolicyPriceMult() const;

	// --- Station upgrades (gloves / isot / advertising) ---
	int32 GloveCost() const;
	int32 IsotCost() const;
	int32 AdCost() const;
	FString UpgradeText(ECigStation Type) const;
	void BuyStationUpgrade(ECigStation Type);

	// --- Shop upgrades ---
	bool HasUpgrade(ECigUpgrade U) const { return UpgradeOwned[(int32)U]; }
	bool BuyUpgrade(ECigUpgrade U);

	// --- Supplier ---
	void CycleSupplier();
	float SupplierPriceMult() const;
	float SupplierQuality() const;
	float SupplierDeliverTime() const;
	float SupplierReliability() const;
	void AddSupplierRelation(float Delta);
	float RelationDiscount(int32 SupplierIdx) const;

	// --- Ingredient tier: cheap / normal / premium ---
	// Moves cost and incoming stock quality together; a gourmet customer notices.
	void CycleIngredientTier();
	FString IngredientTierName() const;
	float IngredientTierCostMult() const;
	float IngredientTierQualityMult() const;

	// --- House & car ---
	void BuyHouse();
	void RefuelCar();
	void RepairCar();

	int32 Money = 400;
	int32 PricePolicy = 1; // 0 cheap, 1 normal, 2 expensive

	int32 GloveLevel = 0;
	int32 IsotLevel = 0;
	int32 AdCount = 0;

	bool bOwnHouse = false;
	bool UpgradeOwned[(int32)ECigUpgrade::COUNT] = { false };

	int32 CurrentSupplier = 0;
	float SupplierRelation[CigSupplierCount] = { 0.f };
	int32 IngredientTier = 1; // 0 cheap, 1 normal, 2 premium
};
