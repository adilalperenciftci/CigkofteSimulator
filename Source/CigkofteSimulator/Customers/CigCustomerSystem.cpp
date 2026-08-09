#include "Customers/CigCustomerSystem.h"
#include "Core/CigText.h"
#include "Customers/CigkofteCustomer.h"
#include "Customers/CigLoyalty.h"
#include "Customers/CigCustomerGroup.h"
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

#if WITH_DEV_AUTOMATION_TESTS
	if (FailAcquireAfterForTest >= 0)
	{
		if (FailAcquireAfterForTest == 0)
		{
			return nullptr;
		}
		--FailAcquireAfterForTest;
	}
#endif

	// Take the first valid actor from the pool. Invalid ones (the level may have
	// been reloaded) are dropped quietly.
	while (Pool.Num() > 0)
	{
		ACigkofteCustomer* Reused = Pool.Pop(EAllowShrinking::No);
		if (IsValid(Reused))
		{
			// Re-handed on every reuse rather than only on spawn: a pooled actor
			// can outlive the systems it was first given.
			Reused->SetNavSystem(GM ? GM->Nav.Get() : nullptr);
			Reused->Reactivate(SpawnPos);
			Live.Add(Reused);
			UE_LOG(LogCig, Verbose, TEXT("Musteri havuzdan alindi (havuz %d, sahnede %d)"), Pool.Num(), Live.Num());
			return Reused;
		}
	}

	ACigkofteCustomer* Fresh = World->SpawnActor<ACigkofteCustomer>(SpawnPos, FRotator::ZeroRotator, CigAlwaysSpawnParams());
	if (Fresh)
	{
		Fresh->SetNavSystem(GM ? GM->Nav.Get() : nullptr);
		Live.Add(Fresh);
	}
	return Fresh;
}

void UCigCustomerSystem::ReleaseCustomerOwnership(ACigkofteCustomer* C)
{
	Queue.Remove(C);
	for (int32 i = Seated.Num() - 1; i >= 0; --i)
	{
		if (Seated[i].Customer.Get() != C)
		{
			continue;
		}
		if (Seated[i].SeatIndex >= 0 && GM && GM->WorldBuilder)
		{
			GM->WorldBuilder->ReleaseSeat(Seated[i].SeatIndex);
		}
		Seated.RemoveAt(i);
	}
}

void UCigCustomerSystem::RecoverStrandedCustomers()
{
	for (int32 i = Live.Num() - 1; i >= 0; --i)
	{
		ACigkofteCustomer* C = Live[i].Get();
		if (!IsValid(C) || !C->bNavStranded || C->bAwaitingRecycle)
		{
			continue;
		}

		// Whatever they were holding, they are not going to use it. A seat
		// reserved by a customer who cannot reach it is a chair the shop has lost.
		ReleaseCustomerOwnership(C);

		if (!C->bStrandRecoveryAttempted)
		{
			// One try at walking out the way they came. Leave() repaths, so this
			// clears the flag by itself when the exit is still reachable - which
			// it usually is, because what stranded them was a target inside the
			// shop rather than the shop being sealed.
			C->bStrandRecoveryAttempted = true;
			C->Leave(false, GCustomerExit);
			if (!C->bNavStranded)
			{
				++StrandedRecovered;
				continue;
			}
		}

		// Sent home and still stuck: the exit is unreachable too. Recycling is
		// the only bounded end - retrying forever would leave a body standing in
		// the shop that the player has no way to clear.
		UE_LOG(LogCig, Warning, TEXT("Musteri rotasiz kaldi ve havuza alindi (sebep %d)"),
			(int32)C->NavFailure);
		C->Deactivate();
		++StrandedRecycled;
	}
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
		//
		// Dropping the Seated record is not the same as releasing the chair: the
		// reservation lives in the world builder and Seated is only a view of it.
		// Removing the record alone left the seat marked occupied forever, so a
		// shop that recycled a seated customer lost a chair for the rest of the
		// day.
		ReleaseCustomerOwnership(C);
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

bool UCigCustomerSystem::CanAdmitArrival() const
{
	// Everything InitializeArrival will dereference, asked once and up front. It
	// is separate from the initialisation itself so a party can establish that all
	// of its members can be set up before the first one is - half a party with an
	// order and half without is not a state anything downstream knows how to read.
	return GetWorld() != nullptr
		&& GM != nullptr
		&& GM->Days.Get() != nullptr
		&& GM->Orders.Get() != nullptr;
}

void UCigCustomerSystem::InitializeArrival(ACigkofteCustomer* C, bool bForceVIP, bool bForceInfluencer)
{
	// Split out of SpawnCustomer so that acquiring a body and giving it an
	// identity are two separate steps. Everything in here has consequences
	// outside the actor - it draws from the shared random stream, marks a regular
	// as having visited today, and prints a message - so it must not run for a
	// customer that might yet have to be handed back.
	const UCigDaySystem* Days = GM->Days.Get();
	const UCigProgressionSystem* Prog = GM->Progression.Get();
	UCigOrderSystem* Orders = GM->Orders.Get();

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

ACigkofteCustomer* UCigCustomerSystem::SpawnCustomer(bool bForceVIP, bool bForceInfluencer)
{
	if (!CanAdmitArrival())
	{
		return nullptr;
	}

	ACigkofteCustomer* C = AcquireCustomer(GCustomerSpawn);
	if (!C)
	{
		return nullptr;
	}

	InitializeArrival(C, bForceVIP, bForceInfluencer);
	return C;
}

void UCigCustomerSystem::ReturnUnusedCustomer(ACigkofteCustomer* C)
{
	// A body that was acquired and then not needed. It never reached the queue and
	// was never initialised, so this is a straight hand-back rather than a
	// customer leaving: no reputation, no counters, no message.
	if (!C)
	{
		return;
	}
	Live.Remove(C);
	C->Deactivate();
	Pool.Add(C);
}

int32 UCigCustomerSystem::SpawnGroup(int32 Size)
{
	if (Size <= 0 || !CanAdmitArrival())
	{
		return 0;
	}

	// Every body first, and only then any identities. A party is all of its
	// members or none of them, and that has to hold against the pool running out
	// as much as it holds against the queue being full - the difference between
	// the two is invisible from where the player is standing.
	TArray<ACigkofteCustomer*> Arrived;
	Arrived.Reserve(Size);
	for (int32 i = 0; i < Size; ++i)
	{
		ACigkofteCustomer* C = AcquireCustomer(GCustomerSpawn);
		if (!C)
		{
			// Nothing has happened yet that the rest of the game can see: no order
			// was rolled, no regular was marked as having visited, no message was
			// printed. Handing the bodies back is therefore a complete undo, which
			// is the whole reason acquisition comes before initialisation.
			for (ACigkofteCustomer* Unused : Arrived)
			{
				ReturnUnusedCustomer(Unused);
			}
			return 0;
		}
		Arrived.Add(C);
	}

	// Consumed only now that the party is certain to exist, so a failed attempt
	// does not burn an id and leave a gap in the sequence.
	const int32 Id = NextGroupId++;
	for (ACigkofteCustomer* C : Arrived)
	{
		InitializeArrival(C, /*bForceVIP=*/false, /*bForceInfluencer=*/false);
		C->GroupId = Id;
		C->GroupSize = Arrived.Num();
	}
	GroupsAdmitted++;
	if (GM)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.customer.group.arrived"), Arrived.Num()),
			FLinearColor(0.8f, 0.95f, 1.f));
	}
	return Arrived.Num();
}

TArray<ACigkofteCustomer*> UCigCustomerSystem::PartyMembersOf(const ACigkofteCustomer* C) const
{
	TArray<ACigkofteCustomer*> Out;
	if (!C)
	{
		return Out;
	}
	if (C->GroupId < 0)
	{
		Out.Add(const_cast<ACigkofteCustomer*>(C));
		return Out;
	}
	for (const TObjectPtr<ACigkofteCustomer>& Other : Queue)
	{
		if (Other && Other->GroupId == C->GroupId)
		{
			Out.Add(Other.Get());
		}
	}
	return Out;
}

void UCigCustomerSystem::DetachFromWaitingParty(ACigkofteCustomer* C)
{
	if (!C || C->GroupId < 0)
	{
		return;
	}

	TArray<ACigkofteCustomer*> Remaining;
	for (const TObjectPtr<ACigkofteCustomer>& Other : Queue)
	{
		if (Other && Other.Get() != C && Other->GroupId == C->GroupId)
		{
			Remaining.Add(Other.Get());
		}
	}

	C->GroupId = -1;
	C->GroupSize = 1;
	const int32 RemainingSize = Remaining.Num();
	for (ACigkofteCustomer* Member : Remaining)
	{
		Member->GroupSize = RemainingSize;
		if (RemainingSize < CigCustomerGroup::MinSize)
		{
			Member->GroupId = -1;
			Member->GroupSize = 1;
		}
	}
}

void UCigCustomerSystem::RemoveCustomer(ACigkofteCustomer* C, bool bAngry)
{
	// A party walks out together. Whoever ran out of patience first speaks for
	// all of them, so the consequences below are charged once with a multiplier
	// rather than once per person: four separate angry customers would cost four
	// separate reputation hits for one afternoon.
	//
	// Collected before anybody is removed from the queue, and the group is cleared
	// on each of them, so the recursive call takes the ordinary single-customer
	// path and cannot come back round here.
	int32 PartySize = 1;
	TArray<ACigkofteCustomer*> Companions;
	if (bAngry && C && C->GroupId >= 0)
	{
		const TArray<ACigkofteCustomer*> Party = PartyMembersOf(C);
		PartySize = FMath::Max(1, Party.Num());
		for (ACigkofteCustomer* Member : Party)
		{
			Member->GroupId = -1;
			Member->GroupSize = 1;
			if (Member != C)
			{
				Companions.Add(Member);
			}
		}
	}
	else
	{
		// A served takeaway customer leaves happy on their own. Their companions
		// still have independent orders, so they remain in the queue rather than
		// being mistaken for an angry party walkout.
		DetachFromWaitingParty(C);
	}

	Queue.Remove(C);
	if (bAngry && C)
	{
		UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
		const bool bWasVIP = C->bVIP;
		const bool bInfluencer = EnumHasAnyFlags(C->Traits, ECigTrait::Influencer);
		float RepLoss = (bWasVIP ? 12.f : 6.f) * CigCustomerGroup::WalkoutRepMult(PartySize);
		if (bInfluencer)
		{
			RepLoss *= 2.f;
			GM->AddMessage(CigText::Get(TEXT("msg.customer.influencer.angry")), FLinearColor(1.f, 0.2f, 0.2f));
		}
		// Reputation and the review are charged once, with the multiplier: they
		// describe how badly one afternoon went, and a party is one afternoon.
		// The counters below are not opinions, they are headcounts - four people
		// did leave and four orders did go unserved - so those move by the party
		// size. Conflating the two would either understate the loss or make a
		// single party read as a catastrophe.
		if (Prog)
		{
			Prog->AddRep(-RepLoss);
			Prog->TotalAngryCustomers += PartySize;
		}
		if (GM->Days)
		{
			for (int32 i = 0; i < PartySize; ++i)
			{
				GM->Days->RegisterMissed();
			}
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
		if (PartySize > 1)
		{
			GM->AddMessage(CigText::Format(TEXT("msg.customer.group.leftangry"), PartySize),
				FLinearColor(1.f, 0.3f, 0.3f));
		}
		else
		{
			GM->AddMessage(bWasVIP
				? CigText::Get(TEXT("msg.customer.vip.leftangry"))
				: CigText::Get(TEXT("msg.customer.leftangry")), FLinearColor(1.f, 0.3f, 0.3f));
		}
	}
	if (C)
	{
		C->Leave(bAngry, GCustomerExit);
	}

	// The rest of the party follows them out. Their group is already cleared, so
	// each of these is an ordinary removal: no second reputation hit, no second
	// message, and no chance of recursing back into the party branch.
	for (ACigkofteCustomer* Member : Companions)
	{
		RemoveCustomer(Member, false);
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
			// The arithmetic lives in CigLoyalty, where it can be read and argued
			// with without serving a customer in a running shop.
			const FCigLoyaltyOutcome Outcome = CigLoyalty::AfterServe(
				Accuracy, Loyal->Satisfaction, Loyal->Trust, Loyal->AvgTip, Tip);

			Loyal->Satisfaction = Outcome.Satisfaction;
			Loyal->Trust = Outcome.Trust;
			Loyal->AvgTip = Outcome.AvgTip;

			if (Outcome.bPerfectBonus)
			{
				// Their favourite order was made perfectly: bonus
				if (Eco) { Eco->Earn(15); }
				GM->AddMessage(CigText::Format(TEXT("msg.customer.regular.tip"), *C->LoyalName), FLinearColor(0.6f, 1.f, 0.8f));
			}
			else if (Outcome.bRemembersMistake)
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
			// Sitting down is leaving the queue, so the party they were waiting with
			// is now one person smaller. Without this the friends still in line would
			// keep counting the seated customer as one of them, and the next walkout
			// would be priced for a party that is no longer standing there.
			DetachFromWaitingParty(C);
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
	//
	// Recovery runs first, because it is what turns a stranded customer into a
	// recyclable one; running it after would leave them standing for a frame.
	RecoverStrandedCustomers();
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
		// Most arrivals are one person. When they are not, whether the party comes
		// in is CigCustomerGroup's answer rather than one made here - the queue's
		// capacity has to mean the same thing to a party as it does to the timer
		// that got us this far.
		bool bSpawned = false;
		if (Rng().Chance(CigCustomerGroup::GroupChance))
		{
			const int32 Size = CigCustomerGroup::SizeFromRoll(Rng().FRand());
			const FCigGroupIntake Intake =
				CigCustomerGroup::JudgeIntake(Size, Queue.Num(), MaxQueue());
			if (Intake.IsAdmitted())
			{
				// Zero back means the pool refused, not that the party was turned
				// away - the group was handed back whole and nobody arrived. Falling
				// through to the single-customer path below is the right answer:
				// the timer fired, somebody should walk in, and one person is what
				// the shop can manage right now.
				bSpawned = SpawnGroup(Intake.Admitted) > 0;
			}
			else if (Intake.Refusal == ECigGroupRefusal::WouldSplit)
			{
				// They will not split up, so they leave - and the player is told
				// why, because a group visibly walking away from a shop with two
				// free spaces is otherwise unexplained. No reputation cost: they
				// never queued, and nothing went wrong that the player did.
				GroupsTurnedAway++;
				GM->AddMessage(CigText::Format(TEXT("msg.customer.group.noroom"), Size),
					FLinearColor(1.f, 0.85f, 0.5f));
				bSpawned = true;
			}
		}
		if (!bSpawned)
		{
			SpawnCustomer();
		}
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
