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

// --- Pricing ---
USTRUCT(BlueprintType)
struct FCigPricingRow : public FTableRowBase
{
	GENERATED_BODY()

	// Index into the pricing table (the CigUrun* constants in Core/CigkofteTypes.h).
	UPROPERTY(EditAnywhere, Category = "Fiyat") int32 Index = 0;

	// Only there to keep the CSV readable; what the tablet shows comes from the
	// text table.
	UPROPERTY(EditAnywhere, Category = "Fiyat") FString Label;

	// List price, i.e. the price at a markup of 1.0.
	UPROPERTY(EditAnywhere, Category = "Fiyat") int32 TabanFiyat = 65;

	// How sharply demand answers a price change: footfall follows
	// (price ratio)^-Esneklik. At 0 price does not move footfall at all; at 2 a
	// 20% hike costs roughly a third of the queue. Staples should sit lower than
	// treats - nobody skips the wrap over five lira, they skip the kunefe.
	UPROPERTY(EditAnywhere, Category = "Fiyat") float Esneklik = 1.4f;

	// The range the tablet lets the markup move in. Kept per product because a
	// giveaway price on tea is harmless, on a two-portion wrap it is not.
	UPROPERTY(EditAnywhere, Category = "Fiyat") float MinCarpan = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Fiyat") float MaxCarpan = 2.f;
};

// --- Inspection parameters ---
// A flat key/value table rather than one row per concept: these are a dozen
// unrelated scalars (thresholds, fees, risk steps) that share no shape, and
// forcing them into columns would invent a structure that is not there.
USTRUCT(BlueprintType)
struct FCigInspectionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Denetim") FString Key;
	UPROPERTY(EditAnywhere, Category = "Denetim") FString Label;
	UPROPERTY(EditAnywhere, Category = "Denetim") float Deger = 0.f;
};

// --- Daily events ---
// Only the numbers live here. The event's name and its start/end announcements
// stay in Events/CigEventSystem.cpp: they are prose, not balance, and moving
// them would be a job for the text layer rather than this table.
USTRUCT(BlueprintType)
struct FCigEventRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Olay") FString Key;
	UPROPERTY(EditAnywhere, Category = "Olay") FString Label;

	UPROPERTY(EditAnywhere, Category = "Olay") int32 MinGun = 1;
	UPROPERTY(EditAnywhere, Category = "Olay") float Sans = 0.1f;

	// Seconds the event lasts; -1 means it runs to the end of the day.
	UPROPERTY(EditAnywhere, Category = "Olay") float Sure = -1.f;

	UPROPERTY(EditAnywhere, Category = "Olay") float SpawnCarpani = 1.f;
	UPROPERTY(EditAnywhere, Category = "Olay") float SabirCarpani = 1.f;
	UPROPERTY(EditAnywhere, Category = "Olay") float FiyatCarpani = 1.f;
	UPROPERTY(EditAnywhere, Category = "Olay") float TeslimatCarpani = 1.f;
	UPROPERTY(EditAnywhere, Category = "Olay") float StokCarpani = 1.f;

	// 0 leaves the event on the dice. Anything else pins it to the calendar and
	// it fires every N days regardless of Sans - that is what makes match day
	// something the player can plan around instead of hope for.
	UPROPERTY(EditAnywhere, Category = "Olay") int32 TakvimPeriyodu = 0;
};

// --- Staff archetypes ---
// What a candidate walks in with. Everything is a multiplier around 1 so that
// an apprentice hired before this table existed can be migrated to a flat 1 and
// keep behaving exactly as they did.
USTRUCT(BlueprintType)
struct FCigStaffRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Personel") FString Key;
	UPROPERTY(EditAnywhere, Category = "Personel") FString Label;

	// Shortens the gap between work ticks.
	UPROPERTY(EditAnywhere, Category = "Personel") float Hiz = 1.f;

	// Guards against mistakes: wrong garnish, dropped plate, missed customer.
	UPROPERTY(EditAnywhere, Category = "Personel") float Titizlik = 1.f;

	// Tips and reputation earned while working the counter.
	UPROPERTY(EditAnywhere, Category = "Personel") float GulerYuz = 1.f;

	// Daily wage the candidate asks for. The fast and the tidy cost more, which
	// is the whole trade-off the hiring screen is built on.
	UPROPERTY(EditAnywhere, Category = "Personel") int32 MaasBeklentisi = 100;
};

// --- Neighbourhood income ---
USTRUCT(BlueprintType)
struct FCigMahalleRow : public FTableRowBase
{
	GENERATED_BODY()

	// Row 0 is the shop's own street and always counts; rows 1..6 line up with
	// ECigDistrict and join the average as each district unlocks. That is what
	// makes the area richer as the map opens up.
	UPROPERTY(EditAnywhere, Category = "Mahalle") int32 Index = 0;
	UPROPERTY(EditAnywhere, Category = "Mahalle") FString Label;

	// What the area can afford. Above 1 the same price meets less resistance.
	UPROPERTY(EditAnywhere, Category = "Mahalle") float GelirCarpani = 1.f;
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
