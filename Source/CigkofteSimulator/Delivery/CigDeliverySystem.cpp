#include "Delivery/CigDeliverySystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "World/CigWorldBuilder.h"
#include "Orders/CigOrderSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigReviewSystem.h"
#include "Economy/CigSaleSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Events/CigEventSystem.h"
#include "Vehicles/CigCar.h"
#include "Core/CigRandomSubsystem.h"
#include "Engine/StaticMeshActor.h"

void UCigDeliverySystem::OnDayStart(int32 Day)
{
	NextOrderTimer = Rng().FRandRange(25.f, 45.f);
	DayDelivered = 0;
}

void UCigDeliverySystem::OnDayEnd(int32 Day)
{
	for (int32 i = Orders.Num() - 1; i >= 0; --i)
	{
		Orders.RemoveAt(i);
	}
	RebuildBeacons();
}

void UCigDeliverySystem::SpawnOrder()
{
	UCigWorldBuilder* WB = GM ? GM->WorldBuilder.Get() : nullptr;
	if (!WB || WB->Addresses.Num() == 0 || Orders.Num() >= MaxOrders)
	{
		return;
	}

	const FCigAddress& Addr = WB->Addresses[Rng().PickIndex(WB->Addresses.Num())];
	const int32 Day = GM->Days ? GM->Days->Day : 1;

	FCigDeliveryOrder O;
	O.Pos = Addr.Pos;
	O.Address = Addr.Label;
	O.Spec = GM->Orders ? GM->Orders->MakeOrderSpec(Day, ECigTrait::None, true) : FCigOrderSpec();
	O.Spec.bPacked = true;
	O.MaxTime = 75.f + Day * 2.f;
	O.TimeLeft = O.MaxTime;
	O.BaseReward = 100 + Day * 5;
	Orders.Add(O);
	RebuildBeacons();

	GM->AddMessage(CigText::Format(TEXT("msg.delivery.new"), *O.Address, *UCigOrderSystem::DescribeSpec(O.Spec)), FLinearColor(1.f, 0.9f, 0.2f));
}

void UCigDeliverySystem::SpawnBulkOrder()
{
	UCigWorldBuilder* WB = GM ? GM->WorldBuilder.Get() : nullptr;
	if (!WB || WB->Addresses.Num() == 0)
	{
		return;
	}
	const FCigAddress& Addr = WB->Addresses[Rng().PickIndex(WB->Addresses.Num())];

	FCigDeliveryOrder O;
	O.Pos = Addr.Pos;
	O.Address = Addr.Label;
	O.Spec = FCigOrderSpec();
	O.Spec.bPacked = true;
	O.Spec.Portion = 2;
	O.MaxTime = 120.f;
	O.TimeLeft = O.MaxTime;
	O.BaseReward = 350;
	O.bBulk = true;
	Orders.Add(O);
	RebuildBeacons();

	GM->AddMessage(CigText::Format(TEXT("msg.delivery.bulk"), *O.Address, O.BaseReward), FLinearColor(1.f, 0.85f, 0.3f));
}

void UCigDeliverySystem::RebuildBeacons()
{
	UCigWorldBuilder* WB = GM ? GM->WorldBuilder.Get() : nullptr;
	if (!WB)
	{
		return;
	}

	// Hide surplus beacons and create the missing ones (pooling)
	while (Beacons.Num() < Orders.Num())
	{
		AStaticMeshActor* B = WB->SpawnBox(FVector(0.f, 0.f, 2000.f), FVector(0.5f, 0.5f, 40.f), FLinearColor(1.f, 0.85f, 0.1f));
		if (!B)
		{
			return;
		}
		B->SetActorEnableCollision(false);
		Beacons.Add(B);
	}
	for (int32 i = 0; i < Beacons.Num(); ++i)
	{
		if (AStaticMeshActor* B = Beacons[i].Get())
		{
			if (i < Orders.Num())
			{
				B->SetActorLocation(Orders[i].Pos + FVector(0.f, 0.f, 2000.f));
				B->SetActorHiddenInGame(false);
			}
			else
			{
				B->SetActorHiddenInGame(true);
			}
		}
	}
}

void UCigDeliverySystem::ExpireOrder(int32 Index)
{
	if (GM->Progression)
	{
		GM->Progression->AddRep(-5.f);
	}
	if (GM->Days)
	{
		GM->Days->RegisterMissed();
	}
	GM->AddMessage(CigText::Format(TEXT("msg.delivery.expired"), *Orders[Index].Address), FLinearColor(1.f, 0.3f, 0.3f));
	Orders.RemoveAt(Index);
	RebuildBeacons();
}

bool UCigDeliverySystem::TryDeliverAt(const FVector& PlayerPos)
{
	UCigOrderSystem* OrdersSys = GM ? GM->Orders.Get() : nullptr;
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (!OrdersSys || !Eco)
	{
		return false;
	}

	int32 Best = -1;
	float BestDist = DeliverRadius;
	for (int32 i = 0; i < Orders.Num(); ++i)
	{
		const float D = FVector::Dist2D(PlayerPos, Orders[i].Pos);
		if (D < BestDist)
		{
			BestDist = D;
			Best = i;
		}
	}
	if (Best < 0)
	{
		return false;
	}

	FCigDeliveryOrder& O = Orders[Best];
	const int32 NeedPacks = O.bBulk ? 2 : 1;
	if (OrdersSys->Shelf.Num() < NeedPacks)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.delivery.nopack"), NeedPacks), FLinearColor(1.f, 0.4f, 0.3f));
		return true;
	}

	// Use the freshest packages
	float TempSum = 0.f;
	float AccSum = 0.f;
	for (int32 p = 0; p < NeedPacks; ++p)
	{
		const FCigPackagedWrap& Pack = OrdersSys->Shelf[0];
		TempSum += Pack.Temp;
		const FCigOrderScore Score = UCigOrderSystem::ScoreWrap(Pack.Build, O.Spec, 30.f);
		AccSum += Score.Accuracy;
		OrdersSys->Shelf.RemoveAt(0);
	}
	const float AvgTemp = TempSum / NeedPacks;
	const float AvgAcc = AccSum / NeedPacks;

	// --- Delivery score ---
	const float SpeedFrac = FMath::Clamp(O.TimeLeft / O.MaxTime, 0.f, 1.f);
	ACigCar* Car = GM->PlayerCar.Get();
	const float DamageFrac = Car ? FMath::Clamp(1.f - Car->Damage / 100.f, 0.f, 1.f) : 1.f;

	const float Score =
		SpeedFrac * 30.f +
		(AvgAcc / 100.f) * 35.f +
		(AvgTemp / 100.f) * 25.f +
		DamageFrac * 10.f;
	LastScore = Score;

	int32 Reward = FMath::RoundToInt(O.BaseReward * FMath::Lerp(0.4f, 1.3f, Score / 100.f));
	const bool bFast = SpeedFrac > 0.6f;
	if (bFast)
	{
		Reward += 25; // speed tip
	}

	if (GM->Sales)
	{
		GM->Sales->GeliriKaydet(Reward);
	}
	else
	{
		Eco->Earn(Reward);
	}
	if (GM->Progression)
	{
		GM->Progression->AddRep(Score >= 70.f ? 2.f : -1.f);
		GM->Progression->AddXP(15);
		GM->Progression->TotalDeliveries++;
	}
	if (GM->Reviews)
	{
		GM->Reviews->RecordDelivery(Score);
	}
	Bus().Delivered.Broadcast();
	DayDelivered++;

	// Vehicle wear: a long trip does a little damage
	if (Car && GM->PlayerDriving())
	{
		Car->Damage = FMath::Min(100.f, Car->Damage + Rng().FRandRange(1.f, 4.f));
	}

	GM->PlaySound(ECigSound::Cash);
	GM->SpawnFloatText(O.Pos + FVector(0.f, 0.f, 220.f), CigText::Format(TEXT("float.money.plus"), Reward), FColor(120, 255, 120), 40.f);

	FString Msg = CigText::Format(TEXT("msg.delivery.done"), Reward, FMath::RoundToInt(Score));
	if (AvgTemp < 50.f)
	{
		Msg += CigText::Get(TEXT("msg.delivery.note.cold"));
	}
	if (AvgAcc < 60.f)
	{
		Msg += CigText::Get(TEXT("msg.delivery.note.miss"));
	}
	Msg += CigText::Get(TEXT("msg.delivery.done.suffix"));
	GM->AddMessage(Msg, FLinearColor(0.4f, 1.f, 0.4f));

	Orders.RemoveAt(Best);
	RebuildBeacons();
	return true;
}

void UCigDeliverySystem::UpdateSystem(float DeltaSeconds)
{
	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	if (!Days || !Days->IsPlaying())
	{
		return;
	}

	for (int32 i = Orders.Num() - 1; i >= 0; --i)
	{
		Orders[i].TimeLeft -= DeltaSeconds;
		if (Orders[i].TimeLeft <= 0.f)
		{
			ExpireOrder(i);
		}
	}

	// New order generation (from day 2 onwards)
	if (Days->Day >= 2 && Orders.Num() < MaxOrders)
	{
		NextOrderTimer -= DeltaSeconds * (GM->Events ? GM->Events->DeliveryMult() : 1.f);
		if (NextOrderTimer <= 0.f)
		{
			SpawnOrder();
			NextOrderTimer = Rng().FRandRange(30.f, 55.f);
		}
	}
}
