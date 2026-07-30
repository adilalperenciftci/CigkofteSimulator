#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Game/CigkofteGameMode.h"
#include "World/CigWorldBuilder.h"
#include "Economy/CigEconomySystem.h"
#include "Core/CigText.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Thresholds for levels 1..10
	const int32 GLevelThresholds[UCigProgressionSystem::MaxLevel] = { 0, 100, 250, 450, 700, 1000, 1400, 1900, 2500, 3200 };
}

int32 UCigProgressionSystem::XPForNext() const
{
	return Level >= MaxLevel ? -1 : GLevelThresholds[Level];
}

FString UCigProgressionSystem::PopularityTitle() const
{
	// This sits under the money in the corner of the screen the whole time, so it
	// was the most visible untranslated string in the game.
	if (Rep >= 90.f) return CigText::Get(TEXT("rep.title.legend"));
	if (Rep >= 80.f) return CigText::Get(TEXT("rep.title.city"));
	if (Rep >= 60.f) return CigText::Get(TEXT("rep.title.pride"));
	if (Rep >= 40.f) return CigText::Get(TEXT("rep.title.trader"));
	if (Rep >= 20.f) return CigText::Get(TEXT("rep.title.rookie"));
	return CigText::Get(TEXT("rep.title.unknown"));
}

void UCigProgressionSystem::AddRep(float Delta)
{
	if (Delta > 0.f)
	{
		const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
		if (Eco && Eco->HasUpgrade(ECigUpgrade::Dekorasyon))
		{
			Delta *= 1.1f;
		}
	}
	Rep = FMath::Clamp(Rep + Delta, 0.f, 100.f);
}

void UCigProgressionSystem::AddXP(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	XP += Amount;
	while (Level < MaxLevel && XP >= GLevelThresholds[Level])
	{
		Level++;
		FString Unlock;
		switch (Level)
		{
		case 2: Unlock = CigText::Get(TEXT("level.unlock.2")); break;
		case 3: Unlock = CigText::Get(TEXT("level.unlock.3")); break;
		case 4: Unlock = CigText::Get(TEXT("level.unlock.4")); break;
		case 5: Unlock = CigText::Get(TEXT("level.unlock.5")); break;
		case 6: Unlock = CigText::Get(TEXT("level.unlock.6")); break;
		case 7: Unlock = CigText::Get(TEXT("level.unlock.7")); break;
		default: Unlock = CigText::Get(TEXT("level.unlock.more")); break;
		}

		if (GM)
		{
			GM->AddMessage(CigText::Format(TEXT("level.up"), Level, *Unlock), FLinearColor(0.4f, 0.9f, 1.f));
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				if (GM->WorldBuilder)
				{
					GM->WorldBuilder->SpawnFloatText(Pawn->GetActorLocation() + FVector(0.f, 0.f, 150.f), CigText::Format(TEXT("level.float"), Level), FColor(100, 220, 255), 44.f);
				}
			}

			// Apply the stations and districts this level unlocks to the world.
			if (GM->WorldBuilder)
			{
				GM->WorldBuilder->RefreshUnlocks(Level, true);
			}
			// Each level grants one skill point.
			if (GM->Skills)
			{
				GM->Skills->GrantPoint(1);
			}
		}
	}
}
