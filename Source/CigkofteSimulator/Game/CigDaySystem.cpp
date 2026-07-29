#include "Game/CigDaySystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "World/CigWorldBuilder.h"
#include "Economy/CigEconomySystem.h"
#include "Core/CigLog.h"

void UCigDaySystem::UpdateSystem(float DeltaSeconds)
{
	switch (Phase)
	{
	case ECigPhase::Playing:
		TimeLeft -= DeltaSeconds;
		if (GM && GM->WorldBuilder)
		{
			GM->WorldBuilder->UpdateSun(DayProgress());
		}
		if (TimeLeft <= 0.f)
		{
			EndDay();
		}
		break;

	case ECigPhase::Summary:
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			StartDay(true);
		}
		break;

	default:
		break;
	}
}

void UCigDaySystem::StartDay(bool bAdvanceDay)
{
	if (bAdvanceDay)
	{
		Day++;
	}
	// The morning starts in preparation, not in service. The day's hooks still
	// fire here - stock arrives, energy resets, staff turn up - because those are
	// things that happen overnight, not things that happen when the door opens.
	Phase = ECigPhase::Opening;
	TimeLeft = DayLength;
	DayEarnings = 0;
	DayServed = 0;
	DayMissed = 0;

	if (GM)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.day.prep"), Day), FLinearColor(0.9f, 0.9f, 0.4f));
		GM->BroadcastDayStart(Day);
	}
	UE_LOG(LogCig, Log, TEXT("Gün %d hazırlık"), Day);
}

void UCigDaySystem::OpenShop()
{
	if (Phase != ECigPhase::Opening)
	{
		return;
	}
	Phase = ECigPhase::Playing;
	if (GM)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.day.started"), Day), FLinearColor(0.9f, 0.9f, 0.4f));
		GM->PlaySound(ECigSound::DayStart);
	}
	UE_LOG(LogCig, Log, TEXT("Gün %d açıldı"), Day);
}

void UCigDaySystem::EndDay()
{
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;

	LastRent = 100 + Day * 50;
	if (Eco)
	{
		Eco->Money -= LastRent;

		if (Eco->bOwnHouse)
		{
			Eco->Money += 150;
			GM->AddMessage(CigText::Get(TEXT("msg.day.rentincome")), FLinearColor(0.4f, 1.f, 0.4f));
		}
	}

	if (GM)
	{
		GM->bTabletOpen = false;
		GM->PlaySound(ECigSound::DayEnd);
		GM->BroadcastDayEnd(Day);
	}

	if (Eco && Eco->Money < 0)
	{
		Phase = ECigPhase::GameOver;
		UE_LOG(LogCig, Log, TEXT("İflas: gün %d, kira %d"), Day, LastRent);
	}
	else
	{
		Phase = ECigPhase::Summary;
		PhaseTimer = 8.f;
	}
}
