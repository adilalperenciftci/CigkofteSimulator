#include "Customers/CigCustomerSystem.h"
#include "Core/CigText.h"
#include "Customers/CigkofteCustomer.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "Cooking/CigCookingSystem.h"
#include "Orders/CigOrderSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Events/CigEventSystem.h"
#include "Economy/CigPricingSystem.h"
#include "Economy/CigReviewSystem.h"
#include "Economy/CigSaleSystem.h"
#include "Economy/CigRivalSystem.h"
#include "Economy/CigInspectionSystem.h"
#include "Economy/CigSocialSystem.h"
#include "World/CigWorldBuilder.h"
#include "Placement/CigPlacementTypes.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigBalance.h"
#include "Core/CigLog.h"
#include "AI/CigOfflineDialogueProvider.h"
#include "Engine/GameInstance.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

namespace
{
	const FVector GCustomerSpawn(-1750.f, 1200.f, 0.f);
	const FVector GCustomerExit(-1750.f, -1200.f, 0.f);

	const TCHAR* GLoyalNames[] = {
		TEXT("Ayşe Teyze"), TEXT("Mehmet Abi"), TEXT("Zeynep"), TEXT("Hasan Usta"),
		TEXT("Elif"), TEXT("Kemal Bey"), TEXT("Fatma Hanım"), TEXT("Emre"),
		TEXT("Selin"), TEXT("İbrahim Amca"), TEXT("Deniz"), TEXT("Hülya Abla")
	};

}
#include "Core/CigSpawnUtils.h"

int32 UCigCustomerSystem::MaxQueue() const
{
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	return 5 + ((Eco && Eco->HasUpgrade(ECigUpgrade::IkinciKasa)) ? 2 : 0);
}

ACigkofteCustomer* UCigCustomerSystem::FrontCustomer() const
{
	return Queue.Num() > 0 ? Queue[0].Get() : nullptr;
}

FVector UCigCustomerSystem::QueueSlot(int32 Index) const
{
	return CigPlacementLayout::QueueFront()
		- FVector(CigPlacementLayout::QueueSpacing() * Index, 0.f, 0.f);
}

ACigkofteCustomer* UCigCustomerSystem::AcquireCustomer(const FVector& SpawnPos)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Take the first valid actor from the pool. Invalid ones (the level may have
	// been reloaded) are dropped quietly.
	while (Pool.Num() > 0)
	{
		ACigkofteCustomer* Reused = Pool.Pop(EAllowShrinking::No);
		if (IsValid(Reused))
		{
			Reused->Reactivate(SpawnPos);
			Live.Add(Reused);
			UE_LOG(LogCig, Verbose, TEXT("Musteri havuzdan alindi (havuz %d, sahnede %d)"), Pool.Num(), Live.Num());
			return Reused;
		}
	}

	ACigkofteCustomer* Fresh = World->SpawnActor<ACigkofteCustomer>(SpawnPos, FRotator::ZeroRotator, CigAlwaysSpawnParams());
	if (Fresh)
	{
		Live.Add(Fresh);
	}
	return Fresh;
}

void UCigCustomerSystem::RecycleFinished()
{
	for (int32 i = Live.Num() - 1; i >= 0; --i)
	{
		ACigkofteCustomer* C = Live[i].Get();
		if (!IsValid(C))
		{
			Live.RemoveAtSwap(i);
			continue;
		}
		if (!C->bAwaitingRecycle)
		{
			continue;
		}

		// Every reference in the scene has to be cleared, otherwise an actor taken
		// from the pool keeps showing up in its old role (in the queue, or as the
		// inspector).
		Queue.Remove(C);
		Seated.RemoveAll([C](const FSeatedGuest& G) { return G.Customer.Get() == C; });
		if (Inspector.Get() == C)
		{
			Inspector = nullptr;
		}

		Live.RemoveAtSwap(i);
		Pool.Add(C);
		UE_LOG(LogCig, Verbose, TEXT("Musteri havuza dondu (havuz %d, sahnede %d)"), Pool.Num(), Live.Num());
	}
}

ECigTrait UCigCustomerSystem::PickPooledTrait(int32 Day) const
{
	// Pool: traits with Weight > 0 whose day has come. Weights come from
	// Config/Balance/Traits.csv; all 1s gives a uniform spread.
	float Total = 0.f;
	const int32 Count = CigBalance::TraitCount();
	for (int32 i = 0; i < Count; ++i)
	{
		const FCigTraitRow& R = CigBalance::Trait(i);
		if (R.Weight > 0.f && Day >= R.MinDay)
		{
			Total += R.Weight;
		}
	}
	if (Total <= 0.f)
	{
		return ECigTrait::None;
	}

	float Roll = Rng().FRand() * Total;
	for (int32 i = 0; i < Count; ++i)
	{
		const FCigTraitRow& R = CigBalance::Trait(i);
		if (R.Weight <= 0.f || Day < R.MinDay)
		{
			continue;
		}
		Roll -= R.Weight;
		if (Roll <= 0.f)
		{
			return (ECigTrait)CigBalance::TraitMaskOfIndex(i);
		}
	}
	return ECigTrait::None; // floating point remainder; unreachable in practice
}

ECigTrait UCigCustomerSystem::RollTraits(int32 Day) const
{
	// Every customer draws one trait from the pool, and a second one with this
	// chance. It lives here rather than in the table because it belongs to
	// customer generation, not to the traits themselves.
	constexpr float SecondTraitChance = 0.35f;

	ECigTrait Traits = PickPooledTrait(Day);
	if (Rng().Chance(SecondTraitChance))
	{
		Traits |= PickPooledTrait(Day);
	}

	// Rare traits arrive on their own rolls, independent of the pool.
	const int32 Count = CigBalance::TraitCount();
	for (int32 i = 0; i < Count; ++i)
	{
		const FCigTraitRow& R = CigBalance::Trait(i);
		if (R.RareChance > 0.f && Day >= R.MinDay && Rng().Chance(R.RareChance))
		{
			Traits |= (ECigTrait)CigBalance::TraitMaskOfIndex(i);
		}
	}

	// Clear out contradictions
	if (EnumHasAnyFlags(Traits, ECigTrait::Impatient) && EnumHasAnyFlags(Traits, ECigTrait::Patient))
	{
		Traits &= ~ECigTrait::Impatient;
	}
	return Traits;
}

float UCigCustomerSystem::NextCustomerInterval() const
{
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;

	const float Base = Rng().FRandRange(9.f, 14.f);
	const float Rep = Prog ? Prog->Rep : 50.f;
	const float RepFactor = 1.25f - Rep / 200.f;
	const float DayFactor = FMath::Pow(0.93f, (float)((Days ? Days->Day : 1) - 1));

	float Mult = 1.f;
	if (Eco && Eco->HasUpgrade(ECigUpgrade::YeniTabela))
	{
		Mult *= 0.87f;
	}
	if (GM && GM->Events)
	{
		Mult /= FMath::Max(0.2f, GM->Events->SpawnMult());
	}
	if (GM && GM->Reviews)
	{
		Mult /= FMath::Max(0.5f, GM->Reviews->SpawnRateMult());
	}
	if (GM && GM->Rivals)
	{
		Mult /= FMath::Max(0.4f, GM->Rivals->PlayerPullMult());
	}
	if (GM && GM->Pricing)
	{
		// This is the gap between the interval and the demand curve: the curve
		// returns customers-per-day, the interval is seconds-between-customers,
		// so demand divides rather than multiplies.
		Mult /= GM->Pricing->GunlukTalepCarpani();
	}
	if (GM && GM->Social)
	{
		// Followers, today's campaign and a viral day all arrive as one number.
		Mult /= GM->Social->GunlukCarpan();
	}

	return FMath::Max(3.5f, Base * RepFactor * DayFactor * Mult);
}

void UCigCustomerSystem::SpawnCustomer(bool bForceVIP, bool bForceInfluencer)
{
	UWorld* World = GetWorld();
	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	UCigOrderSystem* Orders = GM ? GM->Orders.Get() : nullptr;
	if (!World || !Days || !Orders)
	{
		return;
	}

	ACigkofteCustomer* C = AcquireCustomer(GCustomerSpawn);
	if (!C)
	{
		return;
	}

	const int32 Day = Days->Day;
	const int32 Level = Prog ? Prog->Level : 1;

	ECigTrait Traits = RollTraits(Day);
	if (bForceInfluencer)
	{
		Traits |= ECigTrait::Influencer;
	}

	// Chance of a regular showing up
	FCigLoyalCustomer* Loyal = nullptr;
	if (Loyals.Num() > 0 && Rng().Chance(0.30f))
	{
		FCigLoyalCustomer& Pick = Loyals[Rng().PickIndex(Loyals.Num())];
		if (Pick.LastVisitDay != Day)
		{
			Loyal = &Pick;
		}
	}

	if (Loyal)
	{
		Traits = (ECigTrait)Loyal->Traits | ECigTrait::Regular;
		C->LoyalId = Loyal->Id;
		C->LoyalName = Loyal->Name;
		C->InitVisuals(Loyal->Seed);
		C->Spec = Loyal->Favorite;
		// Indecisive regulars sometimes stray from their favourite
		if (EnumHasAnyFlags(Traits, ECigTrait::Indecisive) && Rng().Chance(0.4f))
		{
			C->Spec = Orders->MakeOrderSpec(Day, Traits, Level >= 2);
		}
		Loyal->LastVisitDay = Day;
		Loyal->Visits++;
		GM->AddMessage(CigText::Format(TEXT("msg.customer.regular.arrived"), *Loyal->Name, Loyal->Visits), FLinearColor(0.7f, 1.f, 0.9f));
	}
	else
	{
		C->InitVisuals(Rng().Rand());
		C->Spec = Orders->MakeOrderSpec(Day, Traits, Level >= 2);
	}

	C->Traits = Traits;
	C->bVIP = bForceVIP || (Level >= 3 && Rng().Chance(0.15f));

	// Patience calculation
	float Patience = FMath::Max(25.f, 55.f - 2.5f * Day);
	Patience *= CigBalance::TraitPatienceMult((uint16)Traits);
	if (Loyal)                                          { Patience *= 1.3f + Loyal->Trust / 300.f; }
	const UCigEconomySystem* Eco = GM->Economy.Get();
	if (Eco)
	{
		if (Eco->HasUpgrade(ECigUpgrade::Klima))        { Patience *= 1.2f; }
		if (Eco->HasUpgrade(ECigUpgrade::MuzikSistemi)) { Patience *= 1.08f; }
	}
	// People waiting for something they consider overpriced get impatient sooner,
	// and give a bargain more rope. That is about the bill, not about which
	// policy the shop happens to be set to.
	if (const UCigPricingSystem* Fiyatlar = GM->Pricing.Get())
	{
		if (Fiyatlar->PahaliGoruluyor())     { Patience *= 0.85f; }
		else if (Fiyatlar->UygunFiyatli())   { Patience *= 1.15f; }
	}
	if (GM->Events)
	{
		Patience *= GM->Events->PatienceMult();
	}
	if (C->bVIP)
	{
		Patience *= 0.6f;
	}
	C->MaxPatience = Patience;
	C->Patience = Patience;
	C->ApplyOrderVisuals();

	Queue.Add(C);
}

void UCigCustomerSystem::RemoveCustomer(ACigkofteCustomer* C, bool bAngry)
{
	Queue.Remove(C);
	if (bAngry && C)
	{
		UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
		const bool bWasVIP = C->bVIP;
		const bool bInfluencer = EnumHasAnyFlags(C->Traits, ECigTrait::Influencer);
		float RepLoss = bWasVIP ? 12.f : 6.f;
		if (bInfluencer)
		{
			RepLoss *= 2.f;
			GM->AddMessage(CigText::Get(TEXT("msg.customer.influencer.angry")), FLinearColor(1.f, 0.2f, 0.2f));
		}
		if (Prog)
		{
			Prog->AddRep(-RepLoss);
			Prog->TotalAngryCustomers++;
		}
		if (GM->Days)
		{
			GM->Days->RegisterMissed();
		}
		if (GM->Reviews)
		{
			GM->Reviews->RecordAngryLeave(bInfluencer);
			if (bInfluencer && GM->Social)
			{
				GM->Social->FenomenAyrildi(false);
			}
		}
		if (C->LoyalId >= 0)
		{
			if (FCigLoyalCustomer* Loyal = FindLoyal(C->LoyalId))
			{
				Loyal->Trust = FMath::Max(0.f, Loyal->Trust - 15.f);
				Loyal->Satisfaction = FMath::Max(0.f, Loyal->Satisfaction - 20.f);
				Loyal->RememberedMistakes++;
			}
		}
		GM->PlaySound(ECigSound::CustomerAngry);
		GM->RequestFlash(FLinearColor(1.f, 0.25f, 0.2f), 0.5f);
		GM->AddMessage(bWasVIP
			? CigText::Get(TEXT("msg.customer.vip.leftangry"))
			: CigText::Get(TEXT("msg.customer.leftangry")), FLinearColor(1.f, 0.3f, 0.3f));
	}
	if (C)
	{
		C->Leave(bAngry, GCustomerExit);
	}
}

FCigLoyalCustomer* UCigCustomerSystem::FindLoyal(int32 Id)
{
	for (FCigLoyalCustomer& L : Loyals)
	{
		if (L.Id == Id)
		{
			return &L;
		}
	}
	return nullptr;
}

void UCigCustomerSystem::MaybeCreateLoyal(ACigkofteCustomer* C, float Accuracy, float Tip)
{
	if (!C || C->LoyalId >= 0 || Accuracy < 80.f || Loyals.Num() >= 12)
	{
		return;
	}
	if (!Rng().Chance(0.25f))
	{
		return;
	}

	FCigLoyalCustomer L;
	L.Id = NextLoyalId++;
	L.Name = GLoyalNames[Rng().PickIndex(UE_ARRAY_COUNT(GLoyalNames))];
	L.Seed = C->VisualSeed;
	L.Favorite = C->Spec;
	L.Visits = 1;
	L.Satisfaction = 70.f;
	L.Trust = 55.f;
	L.LastVisitDay = GM->Days ? GM->Days->Day : 1;
	L.AvgTip = Tip;
	L.Traits = (uint16)C->Traits;
	Loyals.Add(L);
	GM->AddMessage(CigText::Format(TEXT("msg.customer.regular.new"), *L.Name), FLinearColor(0.6f, 1.f, 0.8f));
}

void UCigCustomerSystem::ServeFront()
{
	ACigkofteCustomer* C = FrontCustomer();
	UCigOrderSystem* Orders = GM ? GM->Orders.Get() : nullptr;
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	UCigHygieneSystem* Hyg = GM ? GM->Hygiene.Get() : nullptr;
	if (!Orders || !Eco || !Prog)
	{
		return;
	}

	if (!C || !C->bArrived || C->bLeaving)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.customer.none")), FLinearColor(0.8f, 0.8f, 0.8f));
		return;
	}
	if (!Orders->Wrap.bActive || !Orders->Wrap.bWrapped)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.customer.nowrap")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	const FCigWrapBuild& Wrap = Orders->Wrap;
	const float PrepTime = GetWorld() ? GetWorld()->GetTimeSeconds() - Wrap.StartTime : 0.f;
	const FCigOrderScore Score = UCigOrderSystem::ScoreWrap(Wrap, C->Spec, PrepTime);

	// Quality: dough quality plus the hygiene effect
	const float Hygiene = Hyg ? Hyg->OverallHygiene() : 100.f;
	const bool bDirty = Hygiene < 50.f;
	const float Quality = Wrap.DoughQuality * (bDirty ? 0.7f : 1.f);

	UCigSaleSystem* Satis = GM->Sales.Get();
	if (!Satis)
	{
		return;
	}

	// Pricing, the combo, the tip and every counter this sale moves belong to the
	// sale pipeline. What stays here is the part that is about this customer
	// rather than about the sale.
	FCigSatisTalebi Talep;
	Talep.Kaynak = ECigSatisKaynagi::Oyuncu;
	Talep.Wrap = Wrap;
	Talep.Accuracy = Score.Accuracy;
	Talep.Quality = Quality;
	Talep.Traits = C->Traits;
	Talep.bVIP = C->bVIP;
	Talep.bSadik = C->LoyalId >= 0;
	Talep.SabirKesri = C->MaxPatience > 0.f ? C->Patience / C->MaxPatience : 1.f;

	const FCigSatisSonucu Sonuc = Satis->SatisiIsle(Talep);
	const int32 FinalPrice = Sonuc.Toplam;
	const int32 Tip = Sonuc.Bahsis;

	if (Sonuc.Kombo >= 2)
	{
		GM->SpawnFloatText(FVector(-680.f, 0.f, 300.f), CigText::Format(TEXT("float.combo"), Sonuc.Kombo), FColor(255, 160, 40), 36.f);
	}

	// A squeamish customer is reacting to the counter rather than to the wrap, so
	// this stays with the customer instead of moving into the pipeline.
	const bool bInfluencer = EnumHasAnyFlags(C->Traits, ECigTrait::Influencer);
	if (bDirty && EnumHasAnyFlags(C->Traits, ECigTrait::HygieneSensitive))
	{
		Prog->AddRep(-4.f);
		GM->AddMessage(CigText::Get(TEXT("msg.customer.hygienecomplaint")), FLinearColor(1.f, 0.5f, 0.3f));
	}

	// --- Loyalty ---
	UpdateLoyaltyAfterServe(C, Eco, Score.Accuracy, Tip);

	// A happy family customer brings friends along
	if (EnumHasAnyFlags(C->Traits, ECigTrait::Family) && Score.Accuracy >= 75.f && Rng().Chance(0.5f))
	{
		CustomerTimer = FMath::Min(CustomerTimer, 3.f);
		GM->AddMessage(CigText::Get(TEXT("msg.customer.familychain")), FLinearColor(0.7f, 1.f, 0.7f));
	}

	// --- Messages ---
	GM->PlaySound(ECigSound::Cash);
	GM->RequestFlash(Score.Accuracy >= 85.f ? FLinearColor(0.3f, 1.f, 0.4f) : FLinearColor(1.f, 0.85f, 0.3f), 0.45f);
	GM->SpawnFloatText(FVector(-680.f, 0.f, 220.f),
		CigText::Format(TEXT("float.customer.earn"), FinalPrice, C->bVIP ? *CigText::Get(TEXT("float.customer.vip")) : TEXT(""), Tip > 0 ? *CigText::Get(TEXT("float.customer.tip")) : TEXT("")),
		C->bVIP ? FColor(255, 215, 60) : FColor(120, 255, 120), 40.f);
	GM->AddMessage(CigText::Format(TEXT("msg.customer.served"), FinalPrice, FMath::RoundToInt(Score.Accuracy), *CigQualityName(Quality)), FLinearColor(0.4f, 1.f, 0.4f));

	// The customer's line is produced by the AI/offline dialogue system below;
	// here we only surface concrete mistakes as teaching feedback.
	if (Score.Accuracy < 90.f && Score.Notes.Num() > 0)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.customer.notes"), *FString::Join(Score.Notes, TEXT(", "))), FLinearColor(1.f, 0.7f, 0.4f));
	}
	if (bInfluencer && GM->Social)
	{
		GM->Social->FenomenAyrildi(Score.Accuracy >= 70.f);
	}

	if (bInfluencer && Score.Accuracy >= 85.f)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.customer.influencer.good")), FLinearColor(0.9f, 0.6f, 1.f));
	}

	// Ask the AI (or the offline fallback) for the line the customer will say.
	// Asynchronous: the game does not wait; the reply lands in the message feed.
	RequestServeDialogue(C, Wrap, Score.Accuracy, Quality, FinalPrice, Tip > 0);

	// The wrap has been handed over.
	Orders->Wrap = FCigWrapBuild();

	// Seat the customer at a table, or send them off happy.
	FinishCustomerVisit(C);
}

void UCigCustomerSystem::UpdateLoyaltyAfterServe(ACigkofteCustomer* C, UCigEconomySystem* Eco, float Accuracy, int32 Tip)
{
	if (!C) { return; }

	if (C->LoyalId >= 0)
	{
		if (FCigLoyalCustomer* Loyal = FindLoyal(C->LoyalId))
		{
			Loyal->Satisfaction = FMath::Clamp(Loyal->Satisfaction + (Accuracy - 60.f) / 5.f, 0.f, 100.f);
			Loyal->Trust = FMath::Clamp(Loyal->Trust + (Accuracy >= 80.f ? 4.f : -6.f), 0.f, 100.f);
			Loyal->AvgTip = (Loyal->AvgTip + Tip) * 0.5f;
			if (Accuracy >= 90.f)
			{
				// Their favourite order was made perfectly: bonus
				if (Eco) { Eco->Earn(15); }
				GM->AddMessage(CigText::Format(TEXT("msg.customer.regular.tip"), *C->LoyalName), FLinearColor(0.6f, 1.f, 0.8f));
			}
			else if (Accuracy < 50.f)
			{
				Loyal->RememberedMistakes++;
				GM->AddMessage(CigText::Format(TEXT("msg.customer.regular.memory"), *C->LoyalName), FLinearColor(1.f, 0.7f, 0.4f));
			}
		}
	}
	else
	{
		MaybeCreateLoyal(C, Accuracy, (float)Tip);
	}
}

void UCigCustomerSystem::FinishCustomerVisit(ACigkofteCustomer* C)
{
	if (!C) { return; }

	// A dine-in customer sits down and eats if there is a free seat.
	bool bSatDown = false;
	if (!C->Spec.bPacked && GM->WorldBuilder && Rng().Chance(0.7f))
	{
		const int32 SeatIdx = GM->WorldBuilder->ReserveSeat();
		if (SeatIdx >= 0)
		{
			const UCigWorldBuilder::FCigSeat& Seat = GM->WorldBuilder->Seats[SeatIdx];
			Queue.Remove(C);
			C->GoToSeat(Seat.Pos, Seat.Yaw);
			FSeatedGuest G;
			G.Customer = C;
			G.SeatIndex = SeatIdx;
			G.EatTimer = Rng().FRandRange(7.f, 12.f);
			Seated.Add(G);
			bSatDown = true;
		}
	}
	if (!bSatDown)
	{
		RemoveCustomer(C, false); // takeaway, or no seat - they leave happy
	}
}

void UCigCustomerSystem::RequestServeDialogue(ACigkofteCustomer* C, const FCigWrapBuild& Teslim,
	float Accuracy, float Quality, int32 FinalPrice, bool bTipped)
{
	if (!C || !GM) { return; }

	// Fill in the context.
	FCigDialogueContext Ctx;
	Ctx.Traits = (uint16)C->Traits;
	Ctx.bVIP = C->bVIP;
	Ctx.Accuracy = Accuracy;
	Ctx.Quality = Quality;
	Ctx.Hygiene = (GM->Hygiene) ? GM->Hygiene->OverallHygiene() : 100.f;
	Ctx.PatienceFrac = C->MaxPatience > 0.f ? FMath::Clamp(C->Patience / C->MaxPatience, 0.f, 1.f) : 1.f;
	// The order on one side, the food on the other. Copying the delivered side
	// from the requested side made every service look correct, which silenced
	// every line written for a wrong order.
	Ctx.RequestedSpice = C->Spec.Spice;
	Ctx.ServedSpice = Teslim.Spice;
	Ctx.bWantedAyran = C->Spec.bWantsAyran;
	Ctx.bGotAyran = Teslim.bAyran;
	Ctx.FinalPrice = FinalPrice;
	Ctx.bTipped = bTipped;
	if (C->LoyalId >= 0)
	{
		Ctx.bRegular = true;
		Ctx.CustomerName = C->LoyalName;
		if (const FCigLoyalCustomer* Loyal = FindLoyal(C->LoyalId))
		{
			Ctx.PastVisits = Loyal->Visits;
			Ctx.RememberedMistakes = Loyal->RememberedMistakes;
		}
	}

	// Resolved here and now, from data.
	//
	// This used to hand the context to a subsystem that issued an HTTP request to
	// a hosted model and fell back to the table when it failed, which is why the
	// call was asynchronous and the game mode was captured weakly. The shipped
	// game does not make that request, so the line is simply looked up - which
	// also means it cannot arrive after the customer has left.
	const FString Line = FCigOfflineDialogueProvider::PickLine(Ctx);
	if (!Line.IsEmpty())
	{
		GM->AddMessage(CigText::Format(TEXT("msg.customer.dialogue"), *Line),
			FLinearColor(0.85f, 0.85f, 0.85f));
	}
}

void UCigCustomerSystem::OnDayStart(int32 Day)
{
	CustomerTimer = 3.f;

	// One roll, and never a certainty: the complaint warning at day start only
	// means something if the visit it warns about might still not happen.
	const UCigInspectionSystem* Denetim = GM ? GM->Inspection.Get() : nullptr;
	const float Sans = Denetim ? Denetim->BugunkuDenetimSansi() : 0.f;
	InspectorTimer = (Day >= 2 && Rng().Chance(Sans)) ? Rng().FRandRange(30.f, 120.f) : -1.f;
}

void UCigCustomerSystem::OnDayEnd(int32 Day)
{
	SendEveryoneHome();
}

// Everyone out: the queue, the tables and the inspector.
//
// Split out of OnDayEnd because the two moments stopped being the same one.
// The clock running out shuts the door, and the books are closed a window
// later - so the shop has to empty when it shuts, not when the takings are
// counted, or the closing minute is spent scrubbing around a queue of people
// standing frozen at the counter.
void UCigCustomerSystem::SendEveryoneHome()
{
	for (int32 i = Queue.Num() - 1; i >= 0; --i)
	{
		if (ACigkofteCustomer* C = Queue[i].Get())
		{
			C->Leave(false, GCustomerExit);
		}
	}
	Queue.Empty();

	// Clear seated customers and free the seats
	for (FSeatedGuest& G : Seated)
	{
		if (GM->WorldBuilder)
		{
			GM->WorldBuilder->ReleaseSeat(G.SeatIndex);
		}
		if (ACigkofteCustomer* C = G.Customer.Get())
		{
			C->Leave(false, GCustomerExit);
		}
	}
	Seated.Empty();

	if (Inspector && IsValid(Inspector))
	{
		Inspector->Leave(false, GCustomerExit);
	}
	Inspector = nullptr;
}

void UCigCustomerSystem::TriggerInspectorNow()
{
	InspectorTimer = 0.01f;
}

void UCigCustomerSystem::UpdateInspector(float DeltaSeconds)
{
	UCigHygieneSystem* Hyg = GM ? GM->Hygiene.Get() : nullptr;
	UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;

	if (Inspector)
	{
		if (!IsValid(Inspector))
		{
			Inspector = nullptr;
			return;
		}
		if (Inspector->bArrived && !Inspector->bLeaving)
		{
			// The verdict belongs to the inspection system; all that happens
			// here is that the person who delivered it walks back out.
			bool bGecti = true;
			if (UCigInspectionSystem* Denetim = GM->Inspection.Get())
			{
				bGecti = Denetim->Denetle().bGecti;
			}
			Inspector->Leave(!bGecti, GCustomerExit);
	Bus().InspectorVisited.Broadcast(Hyg ? Hyg->OverallHygiene() : 100.f);
			Inspector = nullptr;
		}
		return;
	}

	if (InspectorTimer > 0.f)
	{
		InspectorTimer -= DeltaSeconds;
		if (InspectorTimer <= 0.f)
		{
			UWorld* World = GetWorld();
			if (!World)
			{
				return;
			}
			Inspector = AcquireCustomer(GCustomerSpawn);
			if (Inspector)
			{
				Inspector->InitVisuals(Rng().Rand());
				if (Inspector->BodyMID)
				{
					Inspector->BodyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.95f, 0.98f));
				}
				Inspector->OrderText->SetText(FText::FromString(CigText::Get(TEXT("label.inspector"))));
				Inspector->OrderText->SetTextRenderColor(FColor::White);
				Inspector->MaxPatience = 9999.f;
				Inspector->Patience = 9999.f;
				Inspector->SetTarget(FVector(-850.f, 250.f, 0.f));
				GM->AddMessage(CigText::Get(TEXT("msg.customer.inspector.incoming")), FLinearColor(1.f, 0.9f, 0.3f));
			}
		}
	}
}

void UCigCustomerSystem::UpdateSystem(float DeltaSeconds)
{
	// Before the phase check: customers who left at end of day must return to
	// the pool too, so hidden actors do not pile up in the scene.
	RecycleFinished();

	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	if (!Days || !Days->IsPlaying())
	{
		return;
	}

	// A shop the council has shut takes no customers. The day still runs, which
	// is the point: a closed day is when the counter gets scrubbed, the stock
	// gets ordered and the licence gets renewed. The penalty is the lost income,
	// not an unplayable day.
	if (GM->Inspection && GM->Inspection->KapaliMi())
	{
		return;
	}

	CustomerTimer -= DeltaSeconds;
	if (CustomerTimer <= 0.f && Queue.Num() < MaxQueue())
	{
		SpawnCustomer();
		CustomerTimer = NextCustomerInterval();
	}

	// Queue targets
	for (int32 i = 0; i < Queue.Num(); ++i)
	{
		if (ACigkofteCustomer* C = Queue[i].Get())
		{
			C->SetTarget(QueueSlot(i));
		}
	}

	// Patience
	for (int32 i = Queue.Num() - 1; i >= 0; --i)
	{
		ACigkofteCustomer* C = Queue[i].Get();
		if (!IsValid(C))
		{
			Queue.RemoveAt(i);
			continue;
		}
		if (C->bArrived)
		{
			if (!C->bArrivalNotified)
			{
				C->bArrivalNotified = true;
	Bus().CustomerArrived.Broadcast();
			}
			C->Patience -= DeltaSeconds;
			C->SetPatienceColor(C->Patience / C->MaxPatience);
			if (C->Patience <= 0.f)
			{
				RemoveCustomer(C, true);
			}
		}
	}

	// Seated customers: once their eating time is up they get up and leave,
	// freeing the seat.
	for (int32 i = Seated.Num() - 1; i >= 0; --i)
	{
		FSeatedGuest& G = Seated[i];
		ACigkofteCustomer* C = G.Customer.Get();
		if (!IsValid(C))
		{
			if (GM->WorldBuilder)
			{
				GM->WorldBuilder->ReleaseSeat(G.SeatIndex);
			}
			Seated.RemoveAt(i);
			continue;
		}
		if (C->IsSeated())
		{
			G.EatTimer -= DeltaSeconds;
			if (G.EatTimer <= 0.f)
			{
				if (GM->WorldBuilder)
				{
					GM->WorldBuilder->ReleaseSeat(G.SeatIndex);
				}
				C->Leave(false, FVector(-1750.f, -1200.f, 0.f));
				Seated.RemoveAt(i);
			}
		}
	}

	UpdateInspector(DeltaSeconds);
}
