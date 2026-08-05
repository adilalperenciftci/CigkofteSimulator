// The delivery system, on its own terms.
//
// It had no test that named it until now, which the August audit recorded as the
// gap that mattered most: delivery owns the transient placements a layout load has
// to reason about, and the defect Stage 3.5 introduced there was caught by the
// *placement* tests rather than by anything of delivery's own. A system whose
// correctness is only ever observed through another system's tests is one nobody
// is actually watching.
//
// These use a real shop, because every rule here is about what happens to five
// other systems: an expired order costs reputation and a missed day, a delivery
// consumes packages off the shelf and pays through the economy.

#include "Misc/AutomationTest.h"
#include "Delivery/CigDeliverySystem.h"
#include "Orders/CigOrderSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool BuildDeliveryShop(FCigTestShop& Shop, FAutomationTestBase& Test)
	{
		if (!Shop.Build(Test))
		{
			return false;
		}
		if (!Shop.GM->Delivery || !Shop.GM->Orders || !Shop.GM->Economy || !Shop.GM->WorldBuilder)
		{
			Test.AddError(TEXT("Teslimat icin gereken sistemler yok."));
			return false;
		}
		// Orders need addresses, and addresses come from the built world.
		Shop.GM->WorldBuilder->BuildWorld();
		return true;
	}

	// A package on the shelf, which is what a delivery consumes. Being on the shelf
	// *is* being packaged here — `FCigPackagedWrap` carries no separate flag.
	void PutPackageOnShelf(FCigTestShop& Shop, int32 Count = 1)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			FCigPackagedWrap Pack;
			Pack.Temp = 60.f;
			Shop.GM->Orders->Shelf.Add(Pack);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDeliverySpawnTest,
	"Cigkofte.Delivery.OrdersAreCappedAndCarryAnAddress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigDeliverySpawnTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildDeliveryShop(Shop, *this)) { return false; }

	UCigDeliverySystem* Del = Shop.GM->Delivery.Get();
	TestEqual(TEXT("Gun basinda siparis olmamali"), Del->Orders.Num(), 0);

	Del->SpawnOrder();
	if (Del->Orders.Num() == 0)
	{
		AddError(TEXT("Siparis olusmadi; dunyada adres yok."));
		return false;
	}
	TestFalse(TEXT("Siparisin adresi olmali"), Del->Orders[0].Address.IsEmpty());
	TestTrue(TEXT("Siparisin suresi olmali"), Del->Orders[0].TimeLeft > 0.f);
	// Packed on purpose: a delivery is a package, and the shelf check below counts
	// packages rather than loose wraps.
	TestTrue(TEXT("Teslimat siparisi paketli istemeli"), Del->Orders[0].Spec.bPacked);

	// The cap is what stops the beacon list and the HUD growing without bound.
	for (int32 i = 0; i < 10; ++i)
	{
		Del->SpawnOrder();
	}
	TestTrue(FString::Printf(TEXT("Siparis sayisi %d ile sinirli olmali"), UCigDeliverySystem::MaxOrders),
		Del->Orders.Num() <= UCigDeliverySystem::MaxOrders);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDeliveryTooFarTest,
	"Cigkofte.Delivery.DeliveringFromTooFarAwayDoesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigDeliveryTooFarTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildDeliveryShop(Shop, *this)) { return false; }

	UCigDeliverySystem* Del = Shop.GM->Delivery.Get();
	Del->SpawnOrder();
	if (Del->Orders.Num() == 0) { AddError(TEXT("Siparis olusmadi.")); return false; }

	PutPackageOnShelf(Shop);
	const int32 ShelfBefore = Shop.GM->Orders->Shelf.Num();
	const int32 MoneyBefore = Shop.GM->Economy->Money;

	// Far outside DeliverRadius. Returning false is what lets the interaction fall
	// through to whatever else is under the player's cursor.
	TestFalse(TEXT("Uzaktan teslimat tuketilmemeli"),
		Del->TryDeliverAt(Del->Orders[0].Pos + FVector(50000.f, 0.f, 0.f)));
	TestEqual(TEXT("Uzaktan teslimat rafi degistirmemeli"), Shop.GM->Orders->Shelf.Num(), ShelfBefore);
	TestEqual(TEXT("Uzaktan teslimat para vermemeli"), Shop.GM->Economy->Money, MoneyBefore);
	TestEqual(TEXT("Siparis durmali"), Del->Orders.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDeliveryNoPackageTest,
	"Cigkofte.Delivery.ArrivingWithAnEmptyShelfConsumesTheInteractionButNotTheOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigDeliveryNoPackageTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildDeliveryShop(Shop, *this)) { return false; }

	UCigDeliverySystem* Del = Shop.GM->Delivery.Get();
	Del->SpawnOrder();
	if (Del->Orders.Num() == 0) { AddError(TEXT("Siparis olusmadi.")); return false; }

	Shop.GM->Orders->Shelf.Reset();
	const int32 MoneyBefore = Shop.GM->Economy->Money;

	// True, not false: the player did reach the door and the game told them why
	// nothing happened. Returning false here would make the same key press fall
	// through and do something else instead.
	TestTrue(TEXT("Paketsiz varis etkilesimi tuketmeli"), Del->TryDeliverAt(Del->Orders[0].Pos));
	TestEqual(TEXT("Paketsiz varis siparisi silmemeli"), Del->Orders.Num(), 1);
	TestEqual(TEXT("Paketsiz varis para vermemeli"), Shop.GM->Economy->Money, MoneyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDeliverySuccessTest,
	"Cigkofte.Delivery.ASuccessfulDeliveryTakesThePackageAndPays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigDeliverySuccessTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildDeliveryShop(Shop, *this)) { return false; }

	UCigDeliverySystem* Del = Shop.GM->Delivery.Get();
	Del->SpawnOrder();
	if (Del->Orders.Num() == 0) { AddError(TEXT("Siparis olusmadi.")); return false; }

	PutPackageOnShelf(Shop);
	const int32 MoneyBefore = Shop.GM->Economy->Money;
	const int32 DeliveredBefore = Del->DayDelivered;

	TestTrue(TEXT("Kapida teslimat kabul edilmeli"), Del->TryDeliverAt(Del->Orders[0].Pos));

	TestEqual(TEXT("Teslim edilen siparis listeden cikmali"), Del->Orders.Num(), 0);
	TestEqual(TEXT("Paket raftan alinmali"), Shop.GM->Orders->Shelf.Num(), 0);
	TestTrue(TEXT("Teslimat para getirmeli"), Shop.GM->Economy->Money > MoneyBefore);
	TestEqual(TEXT("Gunluk teslimat sayaci artmali"), Del->DayDelivered, DeliveredBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDeliveryBulkTest,
	"Cigkofte.Delivery.ABulkOrderNeedsTwoPackagesAndTakesBoth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigDeliveryBulkTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildDeliveryShop(Shop, *this)) { return false; }

	UCigDeliverySystem* Del = Shop.GM->Delivery.Get();
	Del->SpawnBulkOrder();
	if (Del->Orders.Num() == 0) { AddError(TEXT("Toplu siparis olusmadi.")); return false; }
	TestTrue(TEXT("Toplu siparis bulk isaretlenmeli"), Del->Orders[0].bBulk);

	// Captured once, because the player walks to a door that does not move. Reading
	// `Orders[0]` again after a delivery attempt would index an empty array on
	// exactly the run where this test is doing its job, and take the whole suite
	// down with an assertion instead of reporting a failure.
	const FVector Door = Del->Orders[0].Pos;

	// One package is not enough for a bulk order, and the near miss is the case
	// worth pinning: a single-package check would happily take it and hand over
	// half an order.
	PutPackageOnShelf(Shop, 1);
	TestTrue(TEXT("Tek paketle varis etkilesimi tuketmeli"), Del->TryDeliverAt(Door));
	TestEqual(TEXT("Tek paket toplu siparisi kapatmamali"), Del->Orders.Num(), 1);
	TestEqual(TEXT("Tek paket raftan alinmamali"), Shop.GM->Orders->Shelf.Num(), 1);

	PutPackageOnShelf(Shop, 1);
	TestTrue(TEXT("Iki paketle toplu teslimat kabul edilmeli"), Del->TryDeliverAt(Door));
	TestEqual(TEXT("Toplu teslimat iki paketi de almali"), Shop.GM->Orders->Shelf.Num(), 0);
	TestEqual(TEXT("Toplu siparis listeden cikmali"), Del->Orders.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDeliveryDayEndTest,
	"Cigkofte.Delivery.TheDayEndingClearsOrdersRatherThanCarryingThemOver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigDeliveryDayEndTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildDeliveryShop(Shop, *this)) { return false; }

	UCigDeliverySystem* Del = Shop.GM->Delivery.Get();
	Del->SpawnOrder();
	Del->SpawnOrder();
	if (Del->Orders.Num() == 0) { AddError(TEXT("Siparis olusmadi.")); return false; }

	// An order left over from yesterday would be undeliverable and would keep its
	// beacon standing in a world that has moved on.
	Del->OnDayEnd(1);
	TestEqual(TEXT("Gun sonunda siparis kalmamali"), Del->Orders.Num(), 0);

	// And the counter is per day rather than cumulative, so it is not reset by
	// OnDayEnd but by the morning.
	Del->OnDayStart(2);
	TestEqual(TEXT("Yeni gunde teslimat sayaci sifirlanmali"), Del->DayDelivered, 0);
	TestEqual(TEXT("Yeni gun siparissiz baslamali"), Del->Orders.Num(), 0);
	return true;
}

#endif
