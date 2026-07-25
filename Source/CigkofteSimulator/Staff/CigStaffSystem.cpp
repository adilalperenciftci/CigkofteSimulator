#include "Staff/CigStaffSystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Customers/CigCustomerSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Orders/CigOrderSystem.h"
#include "Cooking/CigCookingSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Core/CigRandomSubsystem.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* GApprenticeNames[] = { TEXT("Yusuf"), TEXT("Berat"), TEXT("Ali"), TEXT("Ceren"), TEXT("Mert"), TEXT("Esra") };

}
#include "Core/CigSpawnUtils.h"

FString UCigStaffSystem::TaskName(ECigStaffTask T)
{
	switch (T)
	{
	case ECigStaffTask::Dograma:  return CigText::Get(TEXT("staff.task.chop"));
	case ECigStaffTask::Temizlik: return CigText::Get(TEXT("staff.task.clean"));
	case ECigStaffTask::Kasa:     return CigText::Get(TEXT("staff.task.cashier"));
	case ECigStaffTask::Paket:    return CigText::Get(TEXT("staff.task.pack"));
	case ECigStaffTask::Stok:     return CigText::Get(TEXT("staff.task.stock"));
	default:                      return CigText::Get(TEXT("common.unknown"));
	}
}

FString UCigStaffSystem::SpecName(ECigStaffSpec S)
{
	switch (S)
	{
	case ECigStaffSpec::ServisUzmani:   return CigText::Get(TEXT("staff.spec.service"));
	case ECigStaffSpec::DogramaUzmani:  return CigText::Get(TEXT("staff.spec.chop"));
	case ECigStaffSpec::HijyenUzmani:   return CigText::Get(TEXT("staff.spec.hygiene"));
	case ECigStaffSpec::PaketUzmani:    return CigText::Get(TEXT("staff.spec.pack"));
	case ECigStaffSpec::CigkofteUstasi: return CigText::Get(TEXT("staff.spec.master"));
	default:                            return CigText::Get(TEXT("staff.spec.none"));
	}
}

void UCigStaffSystem::Hire()
{
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (!Eco)
	{
		return;
	}
	if (Apprentice.bHired)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.staff.alreadyhired"), *Apprentice.Name, Apprentice.Salary));
		return;
	}
	if (Prog && Prog->Level < 3)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.staff.needlevel")), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	if (!Eco->TrySpend(400))
	{
		GM->AddMessage(CigText::Get(TEXT("msg.staff.nomoney")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	Apprentice = FCigApprentice();
	Apprentice.bHired = true;
	Apprentice.Name = GApprenticeNames[Rng().PickIndex(UE_ARRAY_COUNT(GApprenticeNames))];

	RestoreNPC();

	GM->AddMessage(CigText::Format(TEXT("msg.staff.hired"), *Apprentice.Name), FLinearColor(0.4f, 1.f, 0.4f));
	if (GM->Progression)
	{
		GM->Progression->AddXP(10);
	}
	Bus().StaffHired.Broadcast();
}

void UCigStaffSystem::RestoreNPC()
{
	if (!Apprentice.bHired || ApprenticeNPC)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		ApprenticeNPC = World->SpawnActor<ACigkofteCustomer>(FVector(250.f, -450.f, 0.f), FRotator::ZeroRotator, CigAlwaysSpawnParams());
		if (ApprenticeNPC)
		{
			ApprenticeNPC->InitVisuals(Rng().Rand());
			if (ApprenticeNPC->BodyMID)
			{
				ApprenticeNPC->BodyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.9f, 0.88f));
			}
			ApprenticeNPC->OrderText->SetText(FText::FromString(CigText::Format(TEXT("label.staff.apprentice"), *Apprentice.Name.ToUpper())));
			ApprenticeNPC->OrderText->SetTextRenderColor(FColor::White);
			ApprenticeNPC->OrderText->SetVisibility(true);
			UpdateNPCPosition();
			if (!ApprenticeNPC->bAmbient)
			{
				ApprenticeNPC->InitAmbient(FVector2D(150.f, -560.f), FVector2D(380.f, -420.f));
			}
		}
	}
}

void UCigStaffSystem::CycleTask()
{
	if (!Apprentice.bHired)
	{
		return;
	}
	Apprentice.Task = (ECigStaffTask)(((int32)Apprentice.Task + 1) % (int32)ECigStaffTask::COUNT);
	GM->AddMessage(CigText::Format(TEXT("msg.staff.taskchanged"), *Apprentice.Name, *TaskName(Apprentice.Task)), FLinearColor(0.7f, 0.9f, 1.f));
	UpdateNPCPosition();
}

void UCigStaffSystem::GiveRaise()
{
	if (!Apprentice.bHired || !Apprentice.bWantsRaise)
	{
		return;
	}
	Apprentice.Salary += 50;
	Apprentice.bWantsRaise = false;
	Apprentice.Morale = FMath::Min(100.f, Apprentice.Morale + 30.f);
	Apprentice.DaysSinceRaise = 0;
	GM->AddMessage(CigText::Format(TEXT("msg.staff.raise"), *Apprentice.Name, Apprentice.Salary), FLinearColor(0.5f, 1.f, 0.5f));
}

void UCigStaffSystem::UpdateNPCPosition()
{
	if (!ApprenticeNPC)
	{
		return;
	}
	FVector2D Lo(150.f, -560.f);
	FVector2D Hi(380.f, -420.f);
	switch (Apprentice.Task)
	{
	case ECigStaffTask::Dograma:  Lo = FVector2D(500.f, 650.f); Hi = FVector2D(700.f, 850.f); break;
	case ECigStaffTask::Temizlik: Lo = FVector2D(150.f, 250.f); Hi = FVector2D(350.f, 750.f); break;
	case ECigStaffTask::Kasa:     Lo = FVector2D(-550.f, -150.f); Hi = FVector2D(-400.f, 150.f); break;
	case ECigStaffTask::Paket:    Lo = FVector2D(-450.f, 150.f); Hi = FVector2D(-250.f, 350.f); break;
	case ECigStaffTask::Stok:     Lo = FVector2D(700.f, -450.f); Hi = FVector2D(920.f, 450.f); break;
	default: break;
	}
	ApprenticeNPC->InitAmbient(Lo, Hi);
}

void UCigStaffSystem::GainXP(int32 Amount)
{
	Apprentice.XP += Amount;
	const int32 Needed = Apprentice.Level * 40;
	if (Apprentice.XP >= Needed && Apprentice.Level < 6)
	{
		Apprentice.XP -= Needed;
		Apprentice.Level++;
		GM->AddMessage(CigText::Format(TEXT("msg.staff.levelup"), *Apprentice.Name, Apprentice.Level), FLinearColor(0.6f, 0.9f, 1.f));

		// At level 3 a specialism comes from whichever job was done most
		if (Apprentice.Level == 3 && Apprentice.Spec == ECigStaffSpec::Yok)
		{
			int32 BestTask = 0;
			for (int32 i = 1; i < (int32)ECigStaffTask::COUNT; ++i)
			{
				if (Apprentice.TaskCounts[i] > Apprentice.TaskCounts[BestTask])
				{
					BestTask = i;
				}
			}
			switch ((ECigStaffTask)BestTask)
			{
			case ECigStaffTask::Kasa:     Apprentice.Spec = ECigStaffSpec::ServisUzmani; break;
			case ECigStaffTask::Dograma:  Apprentice.Spec = ECigStaffSpec::DogramaUzmani; break;
			case ECigStaffTask::Temizlik: Apprentice.Spec = ECigStaffSpec::HijyenUzmani; break;
			case ECigStaffTask::Paket:    Apprentice.Spec = ECigStaffSpec::PaketUzmani; break;
			default:                      Apprentice.Spec = ECigStaffSpec::CigkofteUstasi; break;
			}
			GM->AddMessage(CigText::Format(TEXT("msg.staff.newspec"), *Apprentice.Name, *SpecName(Apprentice.Spec)), FLinearColor(1.f, 0.85f, 0.4f));
		}
	}
}

void UCigStaffSystem::DoWork()
{
	UCigInventorySystem* Inv = GM->Inventory.Get();
	UCigHygieneSystem* Hyg = GM->Hygiene.Get();
	UCigCustomerSystem* Cust = GM->Customers.Get();
	UCigOrderSystem* Orders = GM->Orders.Get();
	UCigCookingSystem* Cook = GM->Cooking.Get();

	if (Apprentice.Energy < 10.f)
	{
		return; // on a break
	}

	const ECigStaffTask Task = Apprentice.Task;
	Apprentice.TaskCounts[(int32)Task]++;
	bool bWorked = false;

	switch (Task)
	{
	case ECigStaffTask::Dograma:
		if (Inv && Inv->Garnish < UCigInventorySystem::MaxGarnish && Inv->HasStock(CigStockMarul))
		{
			Inv->Consume(CigStockMarul);
			const int32 Amount = Apprentice.Spec == ECigStaffSpec::DogramaUzmani ? 2 : 1;
			Inv->Garnish = FMath::Min(UCigInventorySystem::MaxGarnish, Inv->Garnish + Amount);
			bWorked = true;
		}
		break;

	case ECigStaffTask::Temizlik:
		if (Hyg)
		{
			const float Power = Apprentice.Spec == ECigStaffSpec::HijyenUzmani ? 25.f : 12.f;
			Hyg->CounterDirt = FMath::Max(0.f, Hyg->CounterDirt - Power);
			Hyg->ChopDirt = FMath::Max(0.f, Hyg->ChopDirt - Power);
			Hyg->CatFur = FMath::Max(0.f, Hyg->CatFur - Power * 0.5f);
			bWorked = true;
		}
		break;

	case ECigStaffTask::Kasa:
		// Handles simple orders (one portion, few toppings) on their own
		if (Cust && Cook && Cook->Dough.IsValid())
		{
			ACigkofteCustomer* C = Cust->FrontCustomer();
			if (C && C->bArrived && !C->bLeaving && C->Spec.Portion == 1 && !C->bVIP)
			{
				int32 ToppingCount = 0;
				for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
				{
					if (C->Spec.WantsTopping((ECigTopping)i))
					{
						ToppingCount++;
					}
				}
				if (ToppingCount <= 2 && Cook->Dough.Servings >= 1)
				{
					const float Q = Cook->UseServings(1);
					const float BaseAcc = 55.f + Apprentice.Level * 5.f + (Apprentice.Spec == ECigStaffSpec::ServisUzmani ? 15.f : 0.f);
					const int32 Price = FMath::RoundToInt(55.f * FMath::Clamp(Q / 100.f, 0.3f, 1.f) * (BaseAcc / 100.f));
					if (GM->Economy)
					{
						GM->Economy->Earn(Price);
					}
					if (GM->Days)
					{
						GM->Days->RegisterSale(Price);
					}
					Cust->RemoveCustomer(C, false);
					GM->AddMessage(CigText::Format(TEXT("msg.staff.served"), *Apprentice.Name, Price), FLinearColor(0.7f, 0.95f, 0.8f));
					bWorked = true;
				}
			}
		}
		break;

	case ECigStaffTask::Paket:
		// If there is dough, prepares a simple package for the shelf
		if (Orders && Cook && Cook->Dough.IsValid() && Orders->Shelf.Num() < UCigOrderSystem::MaxShelf && GM->Inventory && GM->Inventory->HasStock(CigStockLavas))
		{
			GM->Inventory->Consume(CigStockLavas);
			const float Q = Cook->UseServings(1);
			FCigPackagedWrap P;
			P.Build.bActive = true;
			P.Build.bWrapped = true;
			P.Build.bPacked = true;
			P.Build.Portions = 1;
			P.Build.DoughQuality = Q * (Apprentice.Spec == ECigStaffSpec::PaketUzmani ? 1.f : 0.9f);
			P.Build.Spice = Cook->Dough.IsValid() ? Cook->Dough.Spice : ECigSpice::Orta;
			P.Temp = 100.f;
			Orders->Shelf.Add(P);
			GM->AddMessage(CigText::Format(TEXT("msg.staff.shelved"), *Apprentice.Name, Orders->Shelf.Num(), UCigOrderSystem::MaxShelf), FLinearColor(0.7f, 0.95f, 0.8f));
			bWorked = true;
		}
		break;

	case ECigStaffTask::Stok:
		// Finds the lowest stock item and orders it
		if (Inv && GM->Economy)
		{
			int32 Lowest = 0;
			for (int32 i = 1; i < CigStockCount; ++i)
			{
				if (Inv->Stock[i] < Inv->Stock[Lowest])
				{
					Lowest = i;
				}
			}
			if (Inv->Stock[Lowest] <= 3 && GM->Economy->Money >= Inv->OrderCost(Lowest) + 200)
			{
				Inv->OrderStock(Lowest);
				bWorked = true;
			}
		}
		break;

	default:
		break;
	}

	if (bWorked)
	{
		Apprentice.Energy = FMath::Max(0.f, Apprentice.Energy - 3.f);
		GainXP(3);
	}
}

void UCigStaffSystem::UpdateSystem(float DeltaSeconds)
{
	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	if (!Apprentice.bHired || !Days || !Days->IsPlaying())
	{
		return;
	}

	const float Interval = 9.f - Apprentice.Level * 0.5f;
	WorkTimer += DeltaSeconds;
	if (WorkTimer >= Interval)
	{
		WorkTimer = 0.f;
		DoWork();
	}
}

void UCigStaffSystem::OnDayStart(int32 Day)
{
	if (!Apprentice.bHired)
	{
		return;
	}
	Apprentice.Energy = 100.f;
	UpdateNPCPosition();

	if (Apprentice.bWantsRaise)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.staff.askraise"), *Apprentice.Name), FLinearColor(1.f, 0.85f, 0.5f));
	}
}

void UCigStaffSystem::OnDayEnd(int32 Day)
{
	if (!Apprentice.bHired)
	{
		return;
	}

	UCigEconomySystem* Eco = GM->Economy.Get();
	if (Eco)
	{
		Eco->Money -= Apprentice.Salary;
		GM->AddMessage(CigText::Format(TEXT("msg.staff.salary"), Apprentice.Salary), FLinearColor(1.f, 0.8f, 0.6f));
	}

	// Morale: hard work lowers it, good earnings raise it
	if (Apprentice.Energy < 30.f)
	{
		Apprentice.Morale = FMath::Max(0.f, Apprentice.Morale - 10.f);
	}
	else
	{
		Apprentice.Morale = FMath::Min(100.f, Apprentice.Morale + 4.f);
	}

	Apprentice.DaysSinceRaise++;
	if (Apprentice.DaysSinceRaise >= 5 && !Apprentice.bWantsRaise)
	{
		Apprentice.bWantsRaise = true;
	}
	if (Apprentice.bWantsRaise)
	{
		Apprentice.Morale = FMath::Max(0.f, Apprentice.Morale - 6.f);
	}

	// High morale bonus: a small gift
	if (Apprentice.Morale >= 90.f && Rng().Chance(0.3f) && Eco)
	{
		Eco->Earn(40);
		GM->AddMessage(CigText::Format(TEXT("msg.staff.bonus"), *Apprentice.Name), FLinearColor(0.5f, 1.f, 0.5f));
	}

	// Low morale: risk of quitting
	if (Apprentice.Morale < 20.f)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.staff.quit"), *Apprentice.Name), FLinearColor(1.f, 0.3f, 0.3f));
		Apprentice = FCigApprentice();
		if (ApprenticeNPC && IsValid(ApprenticeNPC))
		{
			ApprenticeNPC->Destroy();
		}
		ApprenticeNPC = nullptr;
	}
}
