// Deliveries that arrive as objects instead of as numbers.
//
// Stock used to teleport: an order's timer ran out and the count went up wherever
// the player happened to be standing. A crate makes the delivery a thing in the
// room, and that turns one addition into four branching cases - it fits, it
// half-fits, it does not fit at all, and nobody ever came to get it. These tests
// pin all four, because three of them only happen to a player who is busy.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Crate; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Tests/CigTestShop.h"
#include "Inventory/CigInventorySystem.h"
#include "Inventory/CigStockCrate.h"
#include "Inventory/CigStorage.h"
#include "Placement/CigPlacementSystem.h"
#include "Core/CigSpawnUtils.h"
#include "Core/CigkofteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	ACigStockCrate* Kasa(FCigTestShop& Shop, int32 Item, int32 Amount, float Quality)
	{
		static int32 PlacementSerial = 0;
		if (!Shop.GM->Placement)
		{
			return nullptr;
		}
		FCigPlacementRequest Request;
		Request.StableId = FName(*FString::Printf(TEXT("crate.test.%08d"), ++PlacementSerial));
		Request.Category = ECigPlacementCategory::Storage;
		Request.Lifetime = ECigPlacementLifetime::Transient;
		Request.Footprint = UCigPlacementSystem::StockCrateFootprint();
		Request.UseSpec = UCigPlacementSystem::StockCrateUseSpec();
		Request.Context = ECigPlacementContext::Delivery;
		const FCigPlacementResult Candidate = Shop.GM->Placement->FindFirstValidPlacement(
			Request, CigPlacementLayout::DeliverySpots());
		if (!Candidate.bAccepted)
		{
			return nullptr;
		}
		Request.CandidateTransform = Candidate.NormalizedTransform;
		const FCigPlacementResult Registered = Shop.GM->Placement->RegisterPlacement(Request);
		if (!Registered.bAccepted)
		{
			return nullptr;
		}
		ACigStockCrate* C = Shop.World->SpawnActor<ACigStockCrate>(
			Registered.NormalizedTransform.GetLocation(), Registered.NormalizedTransform.Rotator(),
			CigAlwaysSpawnParams());
		if (C)
		{
			C->PlacementId = Request.StableId;
			C->Setup(Item, Amount, Quality);
		}
		else
		{
			Shop.GM->Placement->RemovePlacement(Request.StableId);
		}
		return C;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCrateUnloadMovesStock,
	"Cigkofte.Crate.UnloadingMovesTheGoodsAndTheirQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCrateUnloadMovesStock::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }

	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Inv)
	{
		AddError(TEXT("Envanter sistemi yok."));
		return false;
	}

	// A dry good, so the shared cold pool cannot interfere with the arithmetic.
	const int32 Item = (int32)ECigIngredient::Isot;
	Inv->Stock[Item] = 0;
	Inv->StockQuality[Item] = 1.f;

	ACigStockCrate* C = Kasa(Shop, Item, 8, 0.6f);
	if (!C)
	{
		AddError(TEXT("Kasa spawn edilemedi."));
		return false;
	}

	TestEqual(TEXT("Sekiz kalemin tamamı boşalmalı"), Inv->UnloadCrate(C), 8);
	TestEqual(TEXT("Stok sekize çıkmalı"), Inv->Stock[Item], 8);

	// The crate's quality is what arrives, not a flat 1.0. A bad supplier's
	// delivery has to still be a bad delivery after it has been carried indoors.
	TestTrue(FString::Printf(TEXT("Kalite kasanın kalitesini almalı (%.2f)"), Inv->StockQuality[Item]),
		FMath::IsNearlyEqual(Inv->StockQuality[Item], 0.6f, 0.01f));

	// An emptied crate is gone. Left standing it would be an interactable that
	// gives nothing, and the prompt would still offer it.
	TestTrue(TEXT("Boşalan kasa yok edilmeli"), !IsValid(C));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCratePartialUnloadKeepsTheRest,
	"Cigkofte.Crate.AFullShelfTakesWhatFitsAndTheCrateKeepsTheRest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCratePartialUnloadKeepsTheRest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }

	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Inv) { AddError(TEXT("Envanter sistemi yok.")); return false; }

	const int32 Item = (int32)ECigIngredient::Bulgur;
	// Three units short of the shelf limit, with ten on the way.
	Inv->Stock[Item] = CigStorage::DryCapacityPerItem - 3;

	ACigStockCrate* C = Kasa(Shop, Item, 10, 1.f);
	if (!C) { AddError(TEXT("Kasa spawn edilemedi.")); return false; }

	TestEqual(TEXT("Yalnız sığan üç kalem boşalmalı"), Inv->UnloadCrate(C), 3);
	TestEqual(TEXT("Raf tam kapasitede olmalı"), Inv->Stock[Item], CigStorage::DryCapacityPerItem);

	// The remainder is neither lost nor silently absorbed: it is still in the room.
	TestTrue(TEXT("Kısmen boşalan kasa ayakta kalmalı"), IsValid(C));
	TestEqual(TEXT("Kasada yedi kalem kalmalı"), C->Amount, 7);

	// And a second attempt against a full shelf moves nothing rather than
	// overflowing it - the case a player hits by pressing E again.
	TestEqual(TEXT("Dolu rafa ikinci deneme hiçbir şey taşımamalı"), Inv->UnloadCrate(C), 0);
	TestEqual(TEXT("Raf kapasiteyi aşmamalı"), Inv->Stock[Item], CigStorage::DryCapacityPerItem);
	TestEqual(TEXT("Kasa içeriği değişmemeli"), C->Amount, 7);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCrateWiltsWhileItStands,
	"Cigkofte.Crate.PerishablesLoseQualityOnTheFloorAndDryGoodsDoNot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCrateWiltsWhileItStands::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }

	ACigStockCrate* Soguk = Kasa(Shop, CigStockMarul, 9, 1.f);
	ACigStockCrate* Kuru = Kasa(Shop, (int32)ECigIngredient::Bulgur, 9, 1.f);
	if (!Soguk || !Kuru) { AddError(TEXT("Kasa spawn edilemedi.")); return false; }

	// Three minutes on the floor, which is most of a shift.
	Soguk->AgeBy(180.f);
	Kuru->AgeBy(180.f);

	TestTrue(FString::Printf(TEXT("Marul kalite kaybetmeli (%.2f)"), Soguk->Quality),
		Soguk->Quality < 0.99f);
	TestTrue(FString::Printf(TEXT("Bulgur kalite kaybetmemeli (%.2f)"), Kuru->Quality),
		FMath::IsNearlyEqual(Kuru->Quality, 1.f));

	// It has a floor. An ignored crate is a loss, not a total write-off - and a
	// quality of zero would feed a review the game has no other way to produce.
	Soguk->AgeBy(60.f * 60.f);
	TestTrue(FString::Printf(TEXT("Kalite tabanın altına düşmemeli (%.2f)"), Soguk->Quality),
		Soguk->Quality >= 0.45f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCrateOvernightIsNotLost,
	"Cigkofte.Crate.AnIgnoredDeliveryIsPutAwayOvernightAtTheQualityItHas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCrateOvernightIsNotLost::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }

	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Inv) { AddError(TEXT("Envanter sistemi yok.")); return false; }

	const int32 Item = (int32)ECigIngredient::Isot;
	Inv->Stock[Item] = 0;

	ACigStockCrate* C = Kasa(Shop, Item, 6, 0.8f);
	if (!C) { AddError(TEXT("Kasa spawn edilemedi.")); return false; }
	Inv->Crates.Add(C);

	Inv->OnDayEnd(1);

	// The player paid for it. Deleting it for being ignored would be a harsher
	// rule than this game plays anywhere else; the cost was the quality it lost.
	TestEqual(TEXT("Gece kalan kasa kilere girmeli"), Inv->Stock[Item], 6);
	TestTrue(TEXT("Kalitesini yanında getirmeli"),
		FMath::IsNearlyEqual(Inv->StockQuality[Item], 0.8f, 0.01f));
	TestEqual(TEXT("Ertesi güne kasa taşınmamalı"), Inv->Crates.Num(), 0);
	TestTrue(TEXT("Kasa aktörü yok edilmeli"), !IsValid(C));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCrateArrivesInsteadOfTeleporting,
	"Cigkofte.Crate.ADeliveryLandsInTheRoomRatherThanInThePantry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCrateArrivesInsteadOfTeleporting::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }

	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Inv) { AddError(TEXT("Envanter sistemi yok.")); return false; }

	const int32 Item = (int32)ECigIngredient::Isot;
	const int32 Once = Inv->Stock[Item];

	FCigPendingOrder O;
	O.Item = Item;
	O.Amount = 5;
	O.Quality = 1.f;
	O.TimeLeft = 0.1f;
	Inv->PendingOrders.Add(O);

	Inv->UpdateSystem(0.2f);

	// This is the whole point of the feature: the timer running out must not move
	// the numbers. If this ever passes with the stock raised, the crate has been
	// bypassed and the supplier tab is a vending machine again.
	TestEqual(TEXT("Teslimat anında stok artmamalı"), Inv->Stock[Item], Once);
	TestEqual(TEXT("Sipariş kuyruğu boşalmalı"), Inv->PendingOrders.Num(), 0);
	TestEqual(TEXT("Dükkânda bir kasa durmalı"), Inv->Crates.Num(), 1);

	ACigStockCrate* C = Inv->Crates.Num() > 0 ? Inv->Crates[0].Get() : nullptr;
	if (!C) { AddError(TEXT("Kasa oluşmadı.")); return false; }

	TestEqual(TEXT("Kasa siparişin kalemini taşımalı"), C->Item, Item);
	TestEqual(TEXT("Kasa siparişin miktarını taşımalı"), C->Amount, 5);

	// Not at the origin, which is where it stood when the body was the root
	// component and Setup's relative location silently replaced the spawn point.
	TestTrue(TEXT("Kasa dünya merkezinde durmamalı"),
		!C->GetActorLocation().IsNearlyZero());

	// And unloading it gets the player what they ordered.
	TestEqual(TEXT("Boşaltma beş kalem taşımalı"), Inv->UnloadCrate(C), 5);
	TestEqual(TEXT("Stok siparişi kadar artmalı"), Inv->Stock[Item], Once + 5);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
