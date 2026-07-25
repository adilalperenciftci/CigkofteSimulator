#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Game/CigkofteGameMode.h"
#include "World/CigWorldBuilder.h"
#include "Economy/CigEconomySystem.h"
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
	if (Rep >= 90.f) return TEXT("Çiğköfte Efsanesi");
	if (Rep >= 80.f) return TEXT("Şehir Efsanesi");
	if (Rep >= 60.f) return TEXT("Mahalle Gururu");
	if (Rep >= 40.f) return TEXT("Esnaf");
	if (Rep >= 20.f) return TEXT("Çaylak");
	return TEXT("Tanınmıyor");
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
		case 2: Unlock = TEXT("Ayran satışı, doğrama tezgâhı ve semt pazarı açıldı!"); break;
		case 3: Unlock = TEXT("Paketleme açıldı, meydan yolu serbest. Çırak da işe alınabilir."); break;
		case 4: Unlock = TEXT("Araba anahtarı sende! Buzdolabı ve okul-park bölgesi açıldı."); break;
		case 5: Unlock = TEXT("Emlak vakti! Tabletten (T) satılık evi alabilirsin. Tedarikçi deposu yolu açıldı."); break;
		case 6: Unlock = TEXT("Yeni şube yatırımı tablette! Şehir stadı da artık ulaşılabilir."); break;
		case 7: Unlock = TEXT("Sahil kordonu açıldı — yazlık kalabalık ve yüksek bahşiş."); break;
		default: Unlock = TEXT("Ustalık artıyor."); break;
		}

		if (GM)
		{
			GM->AddMessage(FString::Printf(TEXT("SEVİYE %d! %s"), Level, *Unlock), FLinearColor(0.4f, 0.9f, 1.f));
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				if (GM->WorldBuilder)
				{
					GM->WorldBuilder->SpawnFloatText(Pawn->GetActorLocation() + FVector(0.f, 0.f, 150.f), FString::Printf(TEXT("SEVİYE %d!"), Level), FColor(100, 220, 255), 44.f);
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
