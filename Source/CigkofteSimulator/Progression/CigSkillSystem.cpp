#include "Progression/CigSkillSystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "World/CigWorldBuilder.h"
#include "Core/CigLog.h"

void UCigSkillSystem::GrantPoint(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	Points += Amount;
	if (GM)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.skill.pointsgained"), Amount),
			FLinearColor(0.75f, 0.9f, 1.f));
	}
}

bool UCigSkillSystem::CanUpgrade(ECigSkill S) const
{
	const int32 Idx = (int32)S;
	return Points > 0 && Idx >= 0 && Idx < (int32)ECigSkill::COUNT && Rank[Idx] < CigSkillDef(S).MaxRank;
}

bool UCigSkillSystem::Upgrade(ECigSkill S)
{
	if (!CanUpgrade(S))
	{
		if (GM)
		{
			GM->AddMessage(Points > 0 ? CigText::Get(TEXT("msg.skill.maxrank")) : CigText::Get(TEXT("msg.skill.nopoints")),
				FLinearColor(1.f, 0.6f, 0.3f));
		}
		return false;
	}
	Points--;
	Spent++;
	Rank[(int32)S]++;
	if (GM)
	{
		const FCigSkillRow& D = CigSkillDef(S);
		GM->AddMessage(CigText::Format(TEXT("msg.skill.upgraded"), *CigBalance::SkillName((int32)S), Rank[(int32)S], D.MaxRank, *CigBalance::SkillDesc((int32)S)),
			FLinearColor(0.5f, 1.f, 0.7f));
		GM->PlaySound(ECigSound::Success);
	}
	return true;
}

// --- Effects ---

// The coefficients come from the EffectPerRank column in
// Config/Balance/Skills.csv; the shape of the formula (additive or exponential
// decay) belongs to what the skill means, so it stays here.
namespace
{
	// "+K% per rank" - 1 + K * rank
	float AdditiveMult(ECigSkill S, int32 Rank)
	{
		return 1.f + CigSkillEffect(S) * (float)Rank;
	}
	// "each rank scales it to K" - K^rank
	float DecayMult(ECigSkill S, int32 Rank)
	{
		return FMath::Pow(CigSkillEffect(S), (float)Rank);
	}
}

float UCigSkillSystem::KneadMult() const        { return AdditiveMult(ECigSkill::HizliEl, Rank[(int32)ECigSkill::HizliEl]); }
int32 UCigSkillSystem::ChopReduction() const    { return FMath::RoundToInt(CigSkillEffect(ECigSkill::KeskinBicak) * (float)Rank[(int32)ECigSkill::KeskinBicak]); }
float UCigSkillSystem::TipRepMult() const       { return AdditiveMult(ECigSkill::Guleryuzlu, Rank[(int32)ECigSkill::Guleryuzlu]); }
float UCigSkillSystem::DirtMult() const         { return DecayMult(ECigSkill::TemizIsci, Rank[(int32)ECigSkill::TemizIsci]); }
float UCigSkillSystem::EnergyDrainMult() const  { return DecayMult(ECigSkill::DayanikliBunye, Rank[(int32)ECigSkill::DayanikliBunye]); }
float UCigSkillSystem::RunSpeedMult() const     { return AdditiveMult(ECigSkill::HizliAyak, Rank[(int32)ECigSkill::HizliAyak]); }
float UCigSkillSystem::StockCostMult() const    { return DecayMult(ECigSkill::PazarlikUstasi, Rank[(int32)ECigSkill::PazarlikUstasi]); }
float UCigSkillSystem::DoughQualityMult() const { return AdditiveMult(ECigSkill::MutfakUstasi, Rank[(int32)ECigSkill::MutfakUstasi]); }

// --- Prestige ---

bool UCigSkillSystem::CanPrestige() const
{
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	return Prog && Eco && Prog->Level >= PrestigeLevel && Eco->Money >= PrestigeMoney;
}

FString UCigSkillSystem::PrestigeRequirementText() const
{
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	return CigText::Format(TEXT("msg.skill.prestigereq"),
		Prog ? Prog->Level : 1, PrestigeLevel, Eco ? Eco->Money : 0, PrestigeMoney);
}

FString UCigSkillSystem::PrestigeTitle() const
{
	switch (PrestigeCount)
	{
	case 0:  return CigText::Get(TEXT("skill.prestige.0"));
	case 1:  return CigText::Get(TEXT("skill.prestige.1"));
	case 2:  return CigText::Get(TEXT("skill.prestige.2"));
	case 3:  return CigText::Get(TEXT("skill.prestige.3"));
	default: return CigText::Get(TEXT("skill.prestige.4"));
	}
}

void UCigSkillSystem::DoPrestige()
{
	if (!CanPrestige() || !GM)
	{
		if (GM)
		{
			GM->AddMessage(CigText::Format(TEXT("msg.skill.prestigeneed"), *PrestigeRequirementText()), FLinearColor(1.f, 0.6f, 0.3f));
		}
		return;
	}

	PrestigeCount++;
	GrantPoint(3);

	// Hand over the shop: day, level, cash and every investment reset.
	// Skills, prestige count, achievements and lifetime stats are kept.
	if (GM->Days)
	{
		GM->Days->Day = 1;
	}
	if (UCigEconomySystem* Eco = GM->Economy.Get())
	{
		Eco->Money = 1500;
		Eco->GloveLevel = 0;
		Eco->IsotLevel = 0;
		Eco->AdCount = 0;
		Eco->bOwnHouse = false;
		Eco->PricePolicy = 1;
		for (int32 i = 0; i < (int32)ECigUpgrade::COUNT; ++i)
		{
			Eco->UpgradeOwned[i] = false;
		}
	}
	if (UCigProgressionSystem* Prog = GM->Progression.Get())
	{
		Prog->Level = 1;
		Prog->XP = 0;
		Prog->Rep = 55.f;
	}
	if (UCigInventorySystem* Inv = GM->Inventory.Get())
	{
		Inv->OnInit();          // stock returns to its starting values
		Inv->PendingOrders.Reset();
		Inv->Garnish = 0;
		Inv->ChopCombo = 0;
	}
	if (UCigHygieneSystem* Hyg = GM->Hygiene.Get())
	{
		Hyg->CounterDirt = 10.f;
		Hyg->TrashFill = 15.f;
		Hyg->DishPile = 0.f;
	}
	// Stations lock again; districts that were opened stay open.
	if (GM->WorldBuilder)
	{
		GM->WorldBuilder->RefreshUnlocks(1, false);
	}

	GM->AddMessage(CigText::Format(TEXT("msg.skill.prestigedone"),
		PrestigeCount, FMath::RoundToInt((PrestigeEarnMult() - 1.f) * 100.f), *PrestigeTitle()), FLinearColor(1.f, 0.85f, 0.3f));
	GM->PlaySound(ECigSound::QuestComplete);
	UE_LOG(LogCig, Log, TEXT("Prestij %d yapıldı"), PrestigeCount);
}
