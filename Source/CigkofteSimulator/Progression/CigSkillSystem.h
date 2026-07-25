#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigBalance.h"
#include "CigSkillSystem.generated.h"

// Mastery skills. Each level-up grants 1 point, a prestige grants 3.
enum class ECigSkill : uint8
{
	HizliEl = 0,      // kneading strokes are more effective
	KeskinBicak,      // chopping needs fewer strokes
	Guleryuzlu,       // more tips and reputation
	TemizIsci,        // dirt builds up more slowly
	DayanikliBunye,   // energy drains more slowly
	HizliAyak,        // faster running
	PazarlikUstasi,   // cheaper stock
	MutfakUstasi,     // better dough quality
	COUNT
};

// Name, description, max rank and the per-rank effect coefficient come from
// Config/Balance/Skills.csv; without the file the defaults in CigBalance.cpp
// are used.
inline const FCigSkillRow& CigSkillDef(ECigSkill S)
{
	return CigBalance::Skill((int32)S);
}

// Shortcut to the per-rank coefficient; the effect formulas are in CigSkillSystem.cpp.
inline float CigSkillEffect(ECigSkill S)
{
	return CigSkillDef(S).EffectPerRank;
}

// Skill points, ranks and the prestige (hand-over) loop.
UCLASS()
class UCigSkillSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	// --- Points & ranks ---
	void GrantPoint(int32 Amount = 1);
	bool CanUpgrade(ECigSkill S) const;
	bool Upgrade(ECigSkill S);
	int32 RankOf(ECigSkill S) const { return Rank[(int32)S]; }

	// --- Gameplay effects (read by the other systems) ---
	float KneadMult() const;        // HizliEl
	int32 ChopReduction() const;    // KeskinBicak
	float TipRepMult() const;       // Guleryuzlu
	float DirtMult() const;         // TemizIsci
	float EnergyDrainMult() const;  // DayanikliBunye
	float RunSpeedMult() const;     // HizliAyak
	float StockCostMult() const;    // PazarlikUstasi
	float DoughQualityMult() const; // MutfakUstasi

	// --- Prestige (handing the shop over) ---
	static constexpr int32 PrestigeLevel = 10;
	static constexpr int32 PrestigeMoney = 20000;
	bool CanPrestige() const;
	FString PrestigeRequirementText() const;
	void DoPrestige();
	// Every shop handed over leaves a permanent +12% income bonus.
	float PrestigeEarnMult() const { return 1.f + 0.12f * (float)PrestigeCount; }
	FString PrestigeTitle() const;

	int32 Points = 0;
	int32 Spent = 0;
	int32 Rank[(int32)ECigSkill::COUNT] = { 0 };
	int32 PrestigeCount = 0;
};
