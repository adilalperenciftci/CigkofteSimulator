#include "Cat/CigCatSystem.h"
#include "Core/CigText.h"
#include "Cat/CigCat.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "World/CigWorldBuilder.h"
#include "Engine/World.h"

void UCigCatSystem::OnInit()
{
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Cat = World->SpawnActor<ACigCat>(FVector(-200.f, 400.f, 0.f), FRotator::ZeroRotator, P);
		if (Cat)
		{
			Cat->Init();
		}
	}
}

float UCigCatSystem::Happiness() const
{
	return (Food + Attention) * 0.5f;
}

void UCigCatSystem::Pet()
{
	if (PetCooldown > 0.f)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.cat.nopet"), *CatName), FLinearColor(0.8f, 0.8f, 0.8f));
		return;
	}
	PetCooldown = 45.f;
	Attention = FMath::Min(100.f, Attention + 25.f);
	if (GM->Progression)
	{
		GM->Progression->AddRep(1.f);
	}
	GM->PlaySound(ECigSound::CatMeow);
	if (Cat && GM->WorldBuilder)
	{
		GM->WorldBuilder->SpawnFloatText(Cat->GetActorLocation() + FVector(0.f, 0.f, 90.f), CigText::Get(TEXT("float.cat.purr")), FColor(255, 200, 220), 26.f);
	}
	GM->AddMessage(CigText::Format(TEXT("msg.cat.petted"), *CatName), FLinearColor(1.f, 0.8f, 0.9f));
}

void UCigCatSystem::Feed()
{
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (!Eco)
	{
		return;
	}
	if (Food > 85.f)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.cat.full"), *CatName), FLinearColor(0.8f, 0.8f, 0.8f));
		return;
	}
	if (!Eco->TrySpend(20))
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cat.nomoney")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}
	Food = 100.f;
	bHungryWarned = false;
	GM->PlaySound(ECigSound::CatMeow);
	GM->AddMessage(CigText::Format(TEXT("msg.cat.fed"), *CatName), FLinearColor(1.f, 0.8f, 0.9f));
}

void UCigCatSystem::SetCatName(const FString& NewName)
{
	if (!NewName.IsEmpty())
	{
		CatName = NewName;
		GM->AddMessage(CigText::Format(TEXT("msg.cat.renamed"), *CatName), FLinearColor(1.f, 0.8f, 0.9f));
	}
}

void UCigCatSystem::OnDayStart(int32 Day)
{
	// A happy cat starts the day with a bonus
	if (Happiness() >= 70.f)
	{
		if (GM->Progression)
		{
			GM->Progression->AddRep(2.f);
		}
		GM->AddMessage(CigText::Format(TEXT("msg.cat.happyday"), *CatName), FLinearColor(1.f, 0.85f, 0.9f));
	}
	else if (Happiness() < 30.f)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.cat.sadday"), *CatName), FLinearColor(1.f, 0.7f, 0.5f));
	}
}

void UCigCatSystem::UpdateSystem(float DeltaSeconds)
{
	PetCooldown = FMath::Max(0.f, PetCooldown - DeltaSeconds);

	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	if (!Days || !Days->IsPlaying())
	{
		return;
	}

	NeedTimer += DeltaSeconds;
	if (NeedTimer >= 10.f)
	{
		NeedTimer = 0.f;
		Food = FMath::Max(0.f, Food - 2.f);
		Attention = FMath::Max(0.f, Attention - 1.5f);

		if (Food < 25.f && !bHungryWarned)
		{
			bHungryWarned = true;
			GM->AddMessage(CigText::Format(TEXT("msg.cat.hungry"), *CatName), FLinearColor(1.f, 0.8f, 0.6f));
			GM->PlaySound(ECigSound::CatMeow);
		}
	}
}
