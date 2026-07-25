#include "Hygiene/CigHygieneSystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Cat/CigCatSystem.h"
#include "Progression/CigSkillSystem.h"

float UCigHygieneSystem::OverallHygiene() const
{
	// A weighted average of the dirt; hands weigh heaviest.
	const float Dirt =
		HandDirt * 0.30f +
		CounterDirt * 0.20f +
		ChopDirt * 0.10f +
		TrashFill * 0.15f +
		DishPile * 0.15f +
		CatFur * 0.10f;
	return FMath::Clamp(100.f - Dirt, 0.f, 100.f);
}

FString UCigHygieneSystem::WorstProblem() const
{
	float Worst = 35.f; // eşik altındaki sorunları söyleme
	FString Result;
	struct { float V; const TCHAR* Hint; } Items[] = {
		{ HandDirt,    TEXT("Ellerini yıka (Lavabo)") },
		{ CounterDirt, TEXT("Tezgahı sil (Temizlik)") },
		{ ChopDirt,    TEXT("Doğrama alanını sil (Temizlik)") },
		{ TrashFill,   TEXT("Çöpü boşalt (Çöp)") },
		{ DishPile,    TEXT("Bulaşıkları yıka (Bulaşık)") },
		{ CatFur,      TEXT("Kedi tüylerini süpür (Temizlik)") }
	};
	for (const auto& It : Items)
	{
		if (It.V > Worst)
		{
			Worst = It.V;
			Result = It.Hint;
		}
	}
	return Result;
}

void UCigHygieneSystem::OnDayStart(int32 Day)
{
	// Morning routine: hands start clean, some of yesterday's dirt remains.
	HandDirt = 0.f;
	CounterDirt *= 0.5f;
	ChopDirt *= 0.5f;
}

void UCigHygieneSystem::OnServeMade(bool bPlate)
{
	// The TemizIsci skill slows every kind of dirt build-up.
	const float SkillMult = (GM && GM->Skills) ? GM->Skills->DirtMult() : 1.f;
	HandDirt = FMath::Min(100.f, HandDirt + 10.f * SkillMult);
	CounterDirt = FMath::Min(100.f, CounterDirt + 5.f * SkillMult);
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	const float TrashMult = (Eco && Eco->HasUpgrade(ECigUpgrade::BuyukCop)) ? 0.5f : 1.f;
	TrashFill = FMath::Min(100.f, TrashFill + 6.f * TrashMult * SkillMult);
	if (bPlate)
	{
		DishPile = FMath::Min(100.f, DishPile + 9.f * SkillMult);
	}
}

void UCigHygieneSystem::WashHands()
{
	HandDirt = 0.f;
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (Eco && Eco->HasUpgrade(ECigUpgrade::IyiLavabo))
	{
		CounterDirt = FMath::Max(0.f, CounterDirt - 15.f);
	}
	if (GM)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.hygiene.hands")), FLinearColor(0.4f, 0.8f, 1.f));
	Bus().Washed.Broadcast();
	}
}

void UCigHygieneSystem::CleanSurfaces()
{
	const bool bHadWork = CounterDirt > 5.f || ChopDirt > 5.f || CatFur > 5.f;
	CounterDirt = 0.f;
	ChopDirt = 0.f;
	CatFur = 0.f;
	if (GM)
	{
		GM->AddMessage(CigText::Get(bHadWork ? TEXT("msg.hygiene.cleaned") : TEXT("msg.hygiene.alreadyclean")), FLinearColor(0.4f, 0.9f, 0.9f));
	Bus().Cleaned.Broadcast();
	}
}

void UCigHygieneSystem::WashDishes()
{
	if (DishPile <= 1.f)
	{
		if (GM)
		{
			GM->AddMessage(CigText::Get(TEXT("msg.hygiene.nodishes")), FLinearColor(0.8f, 0.8f, 0.8f));
		}
		return;
	}
	DishPile = 0.f;
	if (GM)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.hygiene.dishesdone")), FLinearColor(0.4f, 0.9f, 0.9f));
	Bus().Cleaned.Broadcast();
	}
}

void UCigHygieneSystem::EmptyTrash()
{
	if (TrashFill <= 1.f)
	{
		if (GM)
		{
			GM->AddMessage(CigText::Get(TEXT("msg.hygiene.binempty")), FLinearColor(0.8f, 0.8f, 0.8f));
		}
		return;
	}
	TrashFill = 0.f;
	if (GM)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.hygiene.binemptied")), FLinearColor(0.4f, 0.9f, 0.9f));
	Bus().Cleaned.Broadcast();
	}
}

void UCigHygieneSystem::UpdateSystem(float DeltaSeconds)
{
	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	if (!Days || !Days->IsPlaying())
	{
		return;
	}

	// Passive build-up: a small increase per second, slowed by the hygiene equipment.
	PassiveTimer += DeltaSeconds;
	if (PassiveTimer >= 5.f)
	{
		PassiveTimer = 0.f;
		const UCigEconomySystem* Eco = GM->Economy.Get();
		const float Mult = (Eco && Eco->HasUpgrade(ECigUpgrade::HijyenEkipmani)) ? 0.65f : 1.f;
		CounterDirt = FMath::Min(100.f, CounterDirt + 0.8f * Mult);

		// An unhappy cat sheds
		const UCigCatSystem* CatSys = GM->CatSys.Get();
		if (CatSys && CatSys->Happiness() < 40.f)
		{
			CatFur = FMath::Min(100.f, CatFur + 1.5f);
		}

		// A full bin attracts flies, so the counter gets dirty faster
		if (TrashFill > 80.f)
		{
			CounterDirt = FMath::Min(100.f, CounterDirt + 1.2f);
		}
	}
}
