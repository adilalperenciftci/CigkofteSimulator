#include "Progression/CigAchievementSystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Staff/CigStaffSystem.h"
#include "Cat/CigCatSystem.h"
#include "World/CigWorldBuilder.h"
#include "Core/CigUpgrades.h"
#include "Core/CigLog.h"
#include "Kismet/GameplayStatics.h"

int32 UCigAchievementSystem::UnlockedCount() const
{
	int32 N = 0;
	for (int32 i = 0; i < (int32)ECigAchievement::COUNT; ++i)
	{
		N += Unlocked[i] ? 1 : 0;
	}
	return N;
}

void UCigAchievementSystem::UpdateSystem(float DeltaSeconds)
{
	// The conditions derive from stats, so checking once a second is enough.
	CheckTimer -= DeltaSeconds;
	if (CheckTimer > 0.f)
	{
		return;
	}
	CheckTimer = 1.f;
	Evaluate();
}

void UCigAchievementSystem::Grant(ECigAchievement A)
{
	const int32 Idx = (int32)A;
	if (Unlocked[Idx])
	{
		return;
	}
	Unlocked[Idx] = true;

	if (!GM)
	{
		return;
	}
	const FCigAchievementRow& D = CigAchievementDef(A);
	GM->AddMessage(CigText::Format(TEXT("msg.achievement.unlocked"),
		*CigBalance::AchievementName((int32)A), *CigBalance::AchievementDesc((int32)A), UnlockedCount(), (int32)ECigAchievement::COUNT), FLinearColor(1.f, 0.85f, 0.35f));
	GM->PlaySound(ECigSound::QuestComplete);
	if (GM->WorldBuilder)
	{
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			GM->WorldBuilder->SpawnFloatText(Pawn->GetActorLocation() + FVector(0.f, 0.f, 185.f),
				CigText::Format(TEXT("float.achievement"), *CigBalance::AchievementName((int32)A)), FColor(255, 215, 90), 36.f);
		}
	}
}

bool CigAchievementMet(const FCigAchievementRow& Row, const FCigAchievementStats& Stats, bool* bOutUnknownStat)
{
	if (bOutUnknownStat)
	{
		*bOutUnknownStat = false;
	}

	// Boolean conditions reduce to 1/0, so one threshold comparison covers everything.
	const FString& Stat = Row.Stat;
	float Value;
	if      (Stat == TEXT("TotalServed"))        { Value = (float)Stats.TotalServed; }
	else if (Stat == TEXT("TotalEarned"))        { Value = (float)Stats.TotalEarned; }
	else if (Stat == TEXT("TotalPerfectOrders")) { Value = (float)Stats.TotalPerfectOrders; }
	else if (Stat == TEXT("TotalDeliveries"))    { Value = (float)Stats.TotalDeliveries; }
	else if (Stat == TEXT("Rep"))                { Value = Stats.Rep; }
	else if (Stat == TEXT("Level"))              { Value = (float)Stats.Level; }
	else if (Stat == TEXT("Money"))              { Value = (float)Stats.Money; }
	else if (Stat == TEXT("Day"))                { Value = (float)Stats.Day; }
	else if (Stat == TEXT("Hygiene"))            { Value = Stats.Hygiene; }
	else if (Stat == TEXT("CatHappy"))           { Value = Stats.CatHappy; }
	else if (Stat == TEXT("PrestigeCount"))      { Value = (float)Stats.PrestigeCount; }
	else if (Stat == TEXT("OwnHouse"))           { Value = Stats.bOwnHouse ? 1.f : 0.f; }
	else if (Stat == TEXT("AllUpgrades"))        { Value = Stats.bAllUpgrades ? 1.f : 0.f; }
	else if (Stat == TEXT("ApprenticeHired"))    { Value = Stats.bApprenticeHired ? 1.f : 0.f; }
	else
	{
		if (bOutUnknownStat)
		{
			*bOutUnknownStat = true;
		}
		return false;
	}

	return Value >= Row.Threshold;
}

FCigAchievementStats UCigAchievementSystem::GatherStats() const
{
	FCigAchievementStats S;
	if (!GM)
	{
		return S;
	}

	if (const UCigProgressionSystem* Prog = GM->Progression.Get())
	{
		S.TotalServed = Prog->TotalServed;
		S.TotalEarned = Prog->TotalEarned;
		S.TotalPerfectOrders = Prog->TotalPerfectOrders;
		S.TotalDeliveries = Prog->TotalDeliveries;
		S.Rep = Prog->Rep;
		S.Level = Prog->Level;
	}
	if (const UCigEconomySystem* Eco = GM->Economy.Get())
	{
		S.Money = Eco->Money;
		S.bOwnHouse = Eco->bOwnHouse;

		S.bAllUpgrades = true;
		for (int32 i = 0; i < (int32)ECigUpgrade::COUNT; ++i)
		{
			S.bAllUpgrades &= Eco->UpgradeOwned[i];
		}
	}
	if (const UCigDaySystem* Days = GM->Days.Get())
	{
		S.Day = Days->Day;
	}
	if (const UCigHygieneSystem* Hyg = GM->Hygiene.Get())
	{
		S.Hygiene = Hyg->OverallHygiene();
	}
	if (const UCigStaffSystem* Staff = GM->Staff.Get())
	{
		S.bApprenticeHired = Staff->Apprentice.bHired;
	}
	if (const UCigCatSystem* CatSys = GM->CatSys.Get())
	{
		// The cat has to be both fed and attended to; the lower value decides.
		S.CatHappy = FMath::Min(CatSys->Food, CatSys->Attention);
	}
	if (const UCigSkillSystem* Skills = GM->Skills.Get())
	{
		S.PrestigeCount = Skills->PrestigeCount;
	}
	return S;
}

void UCigAchievementSystem::Evaluate()
{
	if (!GM)
	{
		return;
	}

	const FCigAchievementStats Stats = GatherStats();
	for (int32 i = 0; i < (int32)ECigAchievement::COUNT; ++i)
	{
		if (Unlocked[i])
		{
			continue;
		}

		const FCigAchievementRow& Row = CigAchievementDef((ECigAchievement)i);
		bool bUnknownStat = false;
		if (CigAchievementMet(Row, Stats, &bUnknownStat))
		{
			Grant((ECigAchievement)i);
		}
		else if (bUnknownStat && !bWarnedUnknownStat[i])
		{
			bWarnedUnknownStat[i] = true;
			UE_LOG(LogCig, Warning, TEXT("Başarım '%s': bilinmeyen Stat '%s' — bu başarım hiç açılmayacak."),
				*Row.Key, *Row.Stat);
		}
	}
}
