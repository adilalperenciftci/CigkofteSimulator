// Does every way out of a chair give the chair back?
//
// A seat is claimed by ReserveSeat setting bOccupied and given back by
// ReleaseSeat clearing it. There are four places that release, because there are
// four ways a guest stops sitting: they finish eating, the shop closes, they are
// removed from the world, or their record is dropped. Miss one and the failure
// is silent and cumulative - the shop's seating capacity shrinks over a day, no
// error is logged, and the symptom is customers queueing while empty chairs sit
// there marked as taken.
//
// UCigCustomerSystem::ReleaseCustomerOwnership carries a comment saying that
// dropping the record alone leaks the chair, so somebody already met this. What
// was missing is anything that would catch it coming back.

#include "Misc/AutomationTest.h"
#include "Customers/CigCustomerSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 OccupiedSeats(const UCigWorldBuilder& WB)
	{
		int32 N = 0;
		for (const UCigWorldBuilder::FCigSeat& S : WB.Seats)
		{
			if (S.bOccupied) { ++N; }
		}
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSeatReserveReleaseTest,
	"Cigkofte.Seating.AReservedSeatIsGivenBackExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSeatReserveReleaseTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	if (!WB) { AddError(TEXT("Dunya kurucu yok.")); return false; }
	WB->BuildWorld();

	if (WB->Seats.Num() == 0) { AddError(TEXT("Sandalye yok.")); return false; }
	TestEqual(TEXT("Dukkan bos sandalyelerle acilmali"), OccupiedSeats(*WB), 0);

	const int32 First = WB->ReserveSeat();
	if (First < 0) { AddError(TEXT("Sandalye ayrilamadi.")); return false; }
	TestEqual(TEXT("Bir rezervasyon bir sandalye tutmali"), OccupiedSeats(*WB), 1);

	// The same chair must not come back twice. ReserveSeat scans for the first
	// free one, so a second call has to find a different index or the shop would
	// seat two people on one chair.
	const int32 Second = WB->ReserveSeat();
	if (Second >= 0)
	{
		TestNotEqual(TEXT("Ikinci rezervasyon ayni sandalye olmamali"), Second, First);
		TestEqual(TEXT("Iki rezervasyon iki sandalye tutmali"), OccupiedSeats(*WB), 2);
		WB->ReleaseSeat(Second);
	}

	WB->ReleaseSeat(First);
	TestEqual(TEXT("Birakilan sandalye geri gelmeli"), OccupiedSeats(*WB), 0);

	// Releasing twice must not go negative or free somebody else's chair. The
	// exit paths overlap - a guest can be removed and then have the shop close
	// on them - so a double release is a thing that happens.
	WB->ReleaseSeat(First);
	TestEqual(TEXT("Iki kez birakmak zarar vermemeli"), OccupiedSeats(*WB), 0);

	// And an index that was never valid is ignored rather than crashing, which
	// matters because SeatIndex is -1 until a guest is actually seated.
	WB->ReleaseSeat(-1);
	WB->ReleaseSeat(WB->Seats.Num() + 10);
	TestEqual(TEXT("Gecersiz indeks bir sey bozmamali"), OccupiedSeats(*WB), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSeatClosingTimeTest,
	"Cigkofte.Seating.ClosingTheShopLeavesNoChairMarkedTaken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSeatClosingTimeTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	UCigCustomerSystem* Customers = Shop.GM->Customers.Get();
	if (!WB || !Customers) { AddError(TEXT("Sistemler yok.")); return false; }
	WB->BuildWorld();
	if (WB->Seats.Num() == 0) { AddError(TEXT("Sandalye yok.")); return false; }

	// Fill every chair the shop has, the way a busy evening would.
	int32 Taken = 0;
	while (true)
	{
		const int32 Index = WB->ReserveSeat();
		if (Index < 0) { break; }
		UCigCustomerSystem::FSeatedGuest G;
		G.SeatIndex = Index;
		Customers->Seated.Add(G);
		++Taken;
	}
	if (Taken == 0) { AddError(TEXT("Hic sandalye ayrilamadi.")); return false; }
	TestEqual(TEXT("Doldurulan sandalyeler tutulmus olmali"), OccupiedSeats(*WB), Taken);

	// Closing time. This is the path where a leak would be permanent: the guest
	// records are emptied, so nothing is left holding the index that would have
	// released the chair.
	Customers->SendEveryoneHome();

	TestEqual(TEXT("Kapanista hicbir sandalye tutulu kalmamali"), OccupiedSeats(*WB), 0);
	TestEqual(TEXT("Kapanista oturan kayit kalmamali"), Customers->Seated.Num(), 0);

	// The shop can seat a full house again tomorrow. A leak would show here as a
	// smaller number, and only here - nothing else in the game reports it.
	int32 TakenAgain = 0;
	while (WB->ReserveSeat() >= 0) { ++TakenAgain; }
	TestEqual(TEXT("Ertesi gun ayni sayida sandalye ayrilabilmeli"), TakenAgain, Taken);
	return true;
}

#endif
