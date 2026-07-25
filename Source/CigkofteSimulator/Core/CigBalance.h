#pragma once

#include "CoreMinimal.h"
#include "Core/CigBalanceTypes.h"

// Access to the balance tables. The first call installs the C++ defaults, then
// `Config/Balance/*.csv` overwrites them if present (see Core/CigBalanceTypes.h).
//
// Access is by index: the caller casts its own enum to int32. That keeps this
// header free of any system header, so anything - the HUD included - can read
// it. Indices match the corresponding enum values exactly, and static_assert in
// CigBalance.cpp holds the lengths together.
namespace CigBalance
{
	const FCigSkillRow& Skill(int32 Index);
	const FCigUpgradeRow& Upgrade(int32 Index);
	const FCigStockRow& Stock(int32 Index);
	const FCigAchievementRow& Achievement(int32 Index);

	// Indexed by the CigUrun* constants in Core/CigkofteTypes.h.
	const FCigPricingRow& Pricing(int32 Index);
	// Row 0 is the shop's street; 1..6 follow ECigDistrict.
	const FCigMahalleRow& Mahalle(int32 Index);
	int32 MahalleCount();

	FString SkillName(int32 Index);
	FString SkillDesc(int32 Index);
	FString UpgradeName(int32 Index);
	FString UpgradeDesc(int32 Index);
	FString TraitName(int32 Index);
	FString AchievementName(int32 Index);
	FString AchievementDesc(int32 Index);

	// Customer traits are a bit mask; "index" here means bit position
	// (0 = Impatient, 1 = Patient, ...). Use TraitIndexOfMask to go from a mask
	// back to an index.
	const FCigTraitRow& Trait(int32 Index);
	int32 TraitCount();

	// Index of a single-bit ECigTrait value; -1 for multiple bits or for 0.
	int32 TraitIndexOfMask(uint16 SingleBitMask);
	// The bit mask an index corresponds to.
	uint16 TraitMaskOfIndex(int32 Index);

	// --- Effects derived from a trait mask ---
	// All pure functions: they read only the mask and the table, so they can be
	// tested without standing up a game (see Tests/CigBalanceTests.cpp).

	// Product of the patience multipliers of every trait in the mask.
	float TraitPatienceMult(uint16 TraitMask);

	// Sum of the deltas added to tip chance.
	float TraitTipChanceDelta(uint16 TraitMask);

	// A value that dictates the tip rate outright; 0 = no trait interferes.
	// If several traits force a value, the highest one wins.
	float TraitTipMultOverride(uint16 TraitMask);

	// Re-reads the balance files. `CigReloadBalance` calls this from the console
	// so numbers can be tried out without restarting the game.
	void Reload();
}
