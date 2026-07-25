#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CigBalanceTypes.generated.h"

// Balance tables: the data form of the skill, shop upgrade, customer trait,
// stock item and achievement definitions.
//
// Why: these numbers used to be constants in C++, so trying out a balance
// change meant a full rebuild. The source is now `Config/Balance/*.csv` - it is
// text, so it diffs in git and `CigReloadBalance` re-reads it in a live game.
//
// Rows derive from FTableRowBase, so a UDataTable asset with the exact same
// columns can be authored in the editor if wanted. At runtime the CSV is read
// directly because UDataTable's CSV import is editor-only.
//
// Every table keeps a complete default set on the C++ side: if the file is
// missing, malformed or short a row, the game runs on that default
// (see Core/CigBalance.h).

// --- Skills ---
USTRUCT(BlueprintType)
struct FCigSkillRow : public FTableRowBase
{
	GENERATED_BODY()

	// Key matching the enum name (e.g. "HizliEl"). Row order does not matter.
	UPROPERTY(EditAnywhere, Category = "Yetenek") FString Key;
	UPROPERTY(EditAnywhere, Category = "Yetenek") FString Name;
	UPROPERTY(EditAnywhere, Category = "Yetenek") FString Desc;
	UPROPERTY(EditAnywhere, Category = "Yetenek") int32 MaxRank = 3;

	// Effect coefficient per rank. How it is applied varies by skill and stays
	// in code: some are "1 + K*rank" (HizliEl), others "K^rank" (TemizIsci).
	// Only K is tuned in the table.
	UPROPERTY(EditAnywhere, Category = "Yetenek") float EffectPerRank = 0.15f;
};

// --- Shop upgrades ---
USTRUCT(BlueprintType)
struct FCigUpgradeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Geliştirme") FString Key;
	UPROPERTY(EditAnywhere, Category = "Geliştirme") FString Name;
	UPROPERTY(EditAnywhere, Category = "Geliştirme") FString Desc;
	UPROPERTY(EditAnywhere, Category = "Geliştirme") int32 Cost = 500;
	UPROPERTY(EditAnywhere, Category = "Geliştirme") int32 MinLevel = 1;
};

// --- Customer traits ---
USTRUCT(BlueprintType)
struct FCigTraitRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Özellik") FString Key;
	UPROPERTY(EditAnywhere, Category = "Özellik") FString Name;

	// Weight in the normal pool. 0 = never enters the pool; only RareChance or
	// a special rule (regulars) can assign it. All 1s means a uniform spread.
	UPROPERTY(EditAnywhere, Category = "Özellik") float Weight = 1.f;

	// The day this trait starts showing up.
	UPROPERTY(EditAnywhere, Category = "Özellik") int32 MinDay = 1;

	// A separate roll per customer, independent of the pool. 0 = unused.
	UPROPERTY(EditAnywhere, Category = "Özellik") float RareChance = 0.f;

	// Patience duration multiplier (1 = no effect).
	UPROPERTY(EditAnywhere, Category = "Özellik") float PatienceMult = 1.f;

	// Delta added to the chance of tipping.
	UPROPERTY(EditAnywhere, Category = "Özellik") float TipChanceDelta = 0.f;

	// Forces the tip rate to this value outright. 0 = leave it alone.
	UPROPERTY(EditAnywhere, Category = "Özellik") float TipMultOverride = 0.f;
};

// --- Stock items ---
USTRUCT(BlueprintType)
struct FCigStockRow : public FTableRowBase
{
	GENERATED_BODY()

	// Index into the stock array (the CigStock* constants in Core/CigkofteTypes.h).
	UPROPERTY(EditAnywhere, Category = "Stok") int32 Index = 0;

	// Only there to keep the CSV readable. The name shown in game comes from
	// CigStockName(); localization hangs off that, so it is not duplicated here.
	UPROPERTY(EditAnywhere, Category = "Stok") FString Label;

	// Pack price before supplier, event and skill multipliers are applied.
	UPROPERTY(EditAnywhere, Category = "Stok") int32 BaseCost = 50;

	// Amount on hand when a new game starts.
	UPROPERTY(EditAnywhere, Category = "Stok") int32 StartAmount = 0;

	// Amount that arrives per order.
	UPROPERTY(EditAnywhere, Category = "Stok") int32 OrderAmount = 10;
};

// --- Achievements ---
USTRUCT(BlueprintType)
struct FCigAchievementRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Başarım") FString Key;
	UPROPERTY(EditAnywhere, Category = "Başarım") FString Name;
	UPROPERTY(EditAnywhere, Category = "Başarım") FString Desc;

	// Which stat to watch. The valid values, and where each is read from, live
	// in StatValue() in Progression/CigAchievementSystem.cpp:
	// TotalServed, TotalEarned, TotalPerfectOrders, TotalDeliveries, Rep, Level,
	// Money, Day, Hygiene, OwnHouse, AllUpgrades, ApprenticeHired, CatHappy,
	// PrestigeCount. An unknown name does not disable the row quietly - it warns
	// and the achievement never unlocks, so a typo gets noticed.
	UPROPERTY(EditAnywhere, Category = "Başarım") FString Stat;

	// The achievement unlocks once the stat reaches this value (>=).
	UPROPERTY(EditAnywhere, Category = "Başarım") float Threshold = 1.f;
};
