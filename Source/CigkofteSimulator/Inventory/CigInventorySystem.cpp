#include "Inventory/CigInventorySystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Events/CigEventSystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "World/CigWorldBuilder.h"
#include "World/CigkofteStation.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigBalance.h"
#include "Inventory/CigStorage.h"
#include "Player/CigkoftePlayerCharacter.h"
#include "Kismet/GameplayStatics.h"

void UCigInventorySystem::OnInit()
{
	// Starting amounts from Config/Balance/Stock.csv (or the defaults without it).
	for (int32 i = 0; i < CigStockCount; ++i)
	{
		Stock[i] = CigBalance::Stock(i).StartAmount;
		StockQuality[i] = 1.f;
	}
}

bool UCigInventorySystem::HasStock(int32 Item, int32 Amount) const
{
	return Item >= 0 && Item < CigStockCount && Stock[Item] >= Amount;
}

void UCigInventorySystem::Consume(int32 Item, int32 Amount)
{
	if (Item >= 0 && Item < CigStockCount)
	{
		Stock[Item] = FMath::Max(0, Stock[Item] - Amount);
	}
}

void UCigInventorySystem::Add(int32 Item, int32 Amount, float Quality)
{
	if (Item < 0 || Item >= CigStockCount || Amount <= 0)
	{
		return;
	}
	// Quality blends into a weighted average with the existing stock
	const float OldTotal = (float)Stock[Item];
	StockQuality[Item] = (StockQuality[Item] * OldTotal + Quality * Amount) / FMath::Max(1.f, OldTotal + Amount);
	Stock[Item] += Amount;
}

int32 UCigInventorySystem::PendingAmountFor(int32 Item) const
{
	// Cold goods share one pool, so an inbound crate of tomatoes takes room from
	// a crate of lettuce. Anything cold counts against anything cold; dry goods
	// only count against themselves.
	const bool bCold = CigStorage::ClassOf(Item) == ECigStorageClass::Cold;

	int32 Sum = 0;
	for (const FCigPendingOrder& O : PendingOrders)
	{
		const bool bMatches = bCold
			? CigStorage::ClassOf(O.Item) == ECigStorageClass::Cold
			: O.Item == Item;
		if (bMatches)
		{
			Sum += O.Amount;
		}
	}
	return Sum;
}

float UCigInventorySystem::AverageIngredientQuality() const
{
	float Sum = 0.f;
	for (int32 i = 0; i < (int32)ECigIngredient::COUNT; ++i)
	{
		Sum += StockQuality[i];
	}
	return FMath::Clamp(Sum / (float)ECigIngredient::COUNT, 0.6f, 1.4f);
}

int32 UCigInventorySystem::OrderCost(int32 Item) const
{
	if (Item < 0 || Item >= CigStockCount)
	{
		return 0;
	}
	float Cost = (float)CigBalance::Stock(Item).BaseCost;
	if (GM && GM->Economy)
	{
		Cost *= GM->Economy->SupplierPriceMult();
		Cost *= GM->Economy->IngredientTierCostMult();
	}
	if (GM && GM->Events)
	{
		Cost *= GM->Events->StockCostMult();
	}
	if (GM && GM->Skills)
	{
		Cost *= GM->Skills->StockCostMult(); // the PazarlikUstasi skill
	}
	return FMath::RoundToInt(Cost);
}

void UCigInventorySystem::OrderStock(int32 Item)
{
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (!Eco || Item < 0 || Item >= CigStockCount)
	{
		return;
	}

	// Room before money. Paying for a delivery that has nowhere to go is the
	// worst version of this rule, and checking capacity first is also the order
	// the player thinks in: is there space, then can I afford it.
	//
	// Counted against what is already on the way as well as what is on the
	// shelf. Two orders placed back to back would otherwise both pass the check
	// and both arrive, which is the shape of every capacity bug.
	const int32 Ordered = CigBalance::Stock(Item).OrderAmount;
	const int32 Room = CigStorage::RoomFor(Stock, Item, Eco->HasUpgrade(ECigUpgrade::BuyukBuzdolabi))
		- PendingAmountFor(Item);
	if (Room <= 0)
	{
		const bool bCold = CigStorage::ClassOf(Item) == ECigStorageClass::Cold;
		GM->AddMessage(CigText::Format(bCold ? TEXT("msg.inventory.nofridgeroom") : TEXT("msg.inventory.noshelfroom"),
			*CigStockName(Item)), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}

	const int32 Cost = OrderCost(Item);
	if (!Eco->TrySpend(Cost))
	{
		GM->AddMessage(CigText::Format(TEXT("msg.inventory.nomoney"), *CigStockName(Item), Cost), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	FCigPendingOrder O;
	O.Item = Item;
	// A part-load rather than a refusal. Once the fridge is tight this is the
	// common case, and turning it away would cost the player the delivery as
	// well as the space they did have.
	O.Amount = FMath::Min(Ordered, Room);
	if (O.Amount < Ordered)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.inventory.partial"), O.Amount, *CigStockName(Item)),
			FLinearColor(1.f, 0.8f, 0.4f));
	}
	O.Supplier = Eco->CurrentSupplier;
	O.Quality = Eco->SupplierQuality() * Eco->IngredientTierQualityMult();
	O.TimeLeft = Eco->SupplierDeliverTime();
	if (GM->Events)
	{
		O.TimeLeft *= GM->Events->SupplyDelayMult();
	}

	// An unreliable supplier is sometimes late
	if (!Rng().Chance(Eco->SupplierReliability()))
	{
		O.TimeLeft *= 1.8f;
		GM->AddMessage(CigText::Get(TEXT("msg.inventory.delayed")), FLinearColor(1.f, 0.8f, 0.4f));
	}

	PendingOrders.Add(O);
	Eco->AddSupplierRelation(2.f);
	GM->AddMessage(CigText::Format(TEXT("msg.inventory.ordered"), O.Amount, *CigStockName(Item), Cost, FMath::CeilToInt(O.TimeLeft)), FLinearColor(0.6f, 0.9f, 1.f));
	Bus().StockOrdered.Broadcast();
}

void UCigInventorySystem::UpdateSystem(float DeltaSeconds)
{
	for (int32 i = PendingOrders.Num() - 1; i >= 0; --i)
	{
		PendingOrders[i].TimeLeft -= DeltaSeconds;
		if (PendingOrders[i].TimeLeft <= 0.f)
		{
			Add(PendingOrders[i].Item, PendingOrders[i].Amount, PendingOrders[i].Quality);
			if (GM)
			{
				GM->AddMessage(CigText::Format(TEXT("msg.inventory.arrived"), PendingOrders[i].Amount, *CigStockName(PendingOrders[i].Item)), FLinearColor(0.4f, 1.f, 0.4f));
			}
			PendingOrders.RemoveAt(i);
		}
	}
}

void UCigInventorySystem::ChopPress()
{
	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	if (!Days || !Days->CanWork())
	{
		return;
	}
	if (Garnish >= MaxGarnish)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.inventory.garnishfull"), MaxGarnish), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	if (!HasStock(CigStockMarul))
	{
		GM->AddMessage(CigText::Get(TEXT("msg.inventory.nolettuce")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	const UCigEconomySystem* Eco = GM->Economy.Get();
	int32 NeededChops = (Eco && Eco->HasUpgrade(ECigUpgrade::HizliDograma)) ? 2 : 4;
	if (GM->Skills)
	{
		NeededChops = FMath::Max(1, NeededChops - GM->Skills->ChopReduction()); // KeskinBicak
	}

	// Fatigue affects chopping too
	if (ACigkoftePlayerCharacter* Player = Cast<ACigkoftePlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->Energy = FMath::Max(0.f, Player->Energy - 0.5f);
	}

	GM->PlaySound(ECigSound::Chop);
	ChopCombo++;
	if (GM->Hygiene)
	{
		GM->Hygiene->ChopDirt = FMath::Min(100.f, GM->Hygiene->ChopDirt + 2.f);
	}

	// The board follows the count. Pushed after the increment and before the
	// completion branch resets it, so a finished garnish sweeps the board back to
	// a whole head rather than leaving the last stroke's fragments on it.
	ACigkofteStation* Board = GM->WorldBuilder ? GM->WorldBuilder->FindStation(ECigStation::Dograma) : nullptr;
	if (Board)
	{
		Board->SetChopState(ChopCombo, NeededChops);
	}

	if (ChopCombo >= NeededChops)
	{
		ChopCombo = 0;
		if (Board)
		{
			Board->SetChopState(0, NeededChops);
		}
		Consume(CigStockMarul);
		Garnish++;
		if (GM->Progression)
		{
			GM->Progression->AddXP(2);
		}
	Bus().Chopped.Broadcast();
		GM->AddMessage(CigText::Format(TEXT("msg.inventory.garnishready"), Garnish), FLinearColor(0.5f, 1.f, 0.5f));
	}
	else if (GM->WorldBuilder)
	{
		GM->WorldBuilder->SpawnFloatText(FVector(600.f, 750.f, 200.f), FString::Printf(TEXT("Doğra! %d/%d"), ChopCombo, NeededChops), FColor(180, 255, 180), 26.f);
	}
}
