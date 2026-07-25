#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigBalance.h"
#include "CigAchievementSystem.generated.h"

// Achievements: derived from game stats, unlocked when the condition holds.
enum class ECigAchievement : uint8
{
	IlkDurum = 0,
	YuzDurum,
	BinDurum,
	IlkBinLira,
	OnBinLira,
	KasaDolu,
	MukemmelOn,
	MukemmelYuz,
	IlkTeslimat,
	ElliTeslimat,
	BesYildiz,
	TemizMutfak,
	OnuncuGun,
	OtuzuncuGun,
	SonSeviye,
	EvSahibi,
	KediDostu,
	CirakPatronu,
	TumDukkan,
	Devretme,
	COUNT
};

// Name, description, the stat to watch and the threshold come from
// Config/Balance/Achievements.csv; without the file the defaults in
// CigBalance.cpp are used.
inline const FCigAchievementRow& CigAchievementDef(ECigAchievement A)
{
	return CigBalance::Achievement((int32)A);
}

// A snapshot of the stats the achievement conditions read. It is gathered from
// the systems and handed to evaluation with no side effects, so the condition
// logic can be tested without standing up a game (see Tests/CigBalanceTests.cpp).
struct FCigAchievementStats
{
	int32 TotalServed = 0;
	int32 TotalEarned = 0;
	int32 TotalPerfectOrders = 0;
	int32 TotalDeliveries = 0;
	float Rep = 0.f;
	int32 Level = 1;
	int32 Money = 0;
	int32 Day = 1;
	float Hygiene = 0.f;
	bool bOwnHouse = false;
	bool bAllUpgrades = false;
	bool bApprenticeHired = false;
	float CatHappy = 0.f;   // the lower of food and attention
	int32 PrestigeCount = 0;
};

// Resolves a row's Stat column and compares it against the threshold. An
// unrecognised stat name returns false and raises bOutUnknownStat, which the
// caller turns into a warning so a typo does not quietly become an achievement
// that never unlocks.
bool CigAchievementMet(const FCigAchievementRow& Row, const FCigAchievementStats& Stats, bool* bOutUnknownStat = nullptr);

// Polls the conditions at a regular interval and announces any unlock.
UCLASS()
class UCigAchievementSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;

	bool IsUnlocked(ECigAchievement A) const { return Unlocked[(int32)A]; }
	int32 UnlockedCount() const;

	bool Unlocked[(int32)ECigAchievement::COUNT] = { false };

private:
	void Evaluate();
	void Grant(ECigAchievement A);
	FCigAchievementStats GatherStats() const;

	float CheckTimer = 0.f;

	// Keeps the unknown-stat warning from repeating every second.
	bool bWarnedUnknownStat[(int32)ECigAchievement::COUNT] = { false };
};
