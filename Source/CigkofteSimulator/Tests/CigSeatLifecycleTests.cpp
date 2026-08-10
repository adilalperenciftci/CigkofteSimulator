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
#include "Customers/CigkofteCustomer.h"
#include "Core/CigRandomSubsystem.h"
#include "Game/CigDaySystem.h"
#include "Orders/CigOrderSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Named for this file. Separate .cpp files share a translation unit under
	// unity builds, so a plain `OccupiedSeats` is a collision waiting for the
	// next test file that wants to count chairs.
	int32 CigSeatTestsOccupiedSeats(const UCigWorldBuilder& WB)
	{
		int32 N = 0;
		for (const UCigWorldBuilder::FCigSeat& S : WB.Seats)
		{
			if (S.bOccupied) { ++N; }
		}
		return N;
	}

	// Everything the seating lifecycle needs, resolved once.
	struct FCigSeatTestsRig
	{
		UCigCustomerSystem* Cust = nullptr;
		UCigDaySystem* Days = nullptr;
		UCigOrderSystem* Orders = nullptr;
		UCigWorldBuilder* WB = nullptr;
		UCigRandomSubsystem* Rng = nullptr;
	};

	// Builds the chairs, opens the day and empties whatever the morning walked in.
	//
	// A day start rolls its own events and two of them bring customers before the
	// player has done anything, so a test that assumed an empty counter passed or
	// failed depending on the clock. They leave happy, which costs the shop
	// nothing, so every baseline taken afterwards is still a clean one.
	//
	// The day has to be open rather than merely started: UCigCustomerSystem's
	// seating timer sits behind an IsPlaying() gate, so a test that only called
	// StartDay would watch a customer eat forever and conclude the timer works.
	bool CigSeatTestsOpenShop(FAutomationTestBase& Test, FCigTestShop& Shop, FCigSeatTestsRig& Out)
	{
		Out.Cust = Shop.GM->Customers.Get();
		Out.Days = Shop.GM->Days.Get();
		Out.Orders = Shop.GM->Orders.Get();
		Out.WB = Shop.GM->WorldBuilder.Get();
		Out.Rng = Shop.GI ? Shop.GI->GetSubsystem<UCigRandomSubsystem>() : nullptr;
		if (!Out.Cust || !Out.Days || !Out.Orders || !Out.WB || !Out.Rng)
		{
			Test.AddError(TEXT("Sistemler yok."));
			return false;
		}

		Out.WB->BuildWorld();
		if (Out.WB->Seats.Num() == 0)
		{
			Test.AddError(TEXT("Sandalye yok."));
			return false;
		}

		Out.Days->StartDay(false);
		Out.Days->OpenShop();
		if (!Out.Days->IsPlaying())
		{
			Test.AddError(TEXT("Dukkan acilmadi; oturma zamanlayicisi calismaz."));
			return false;
		}

		while (Out.Cust->Queue.Num() > 0)
		{
			ACigkofteCustomer* C = Out.Cust->Queue[0].Get();
			if (!C)
			{
				Out.Cust->Queue.RemoveAt(0);
				continue;
			}
			Out.Cust->RemoveCustomer(C, /*bAngry=*/false);
		}
		if (Out.Cust->Queue.Num() != 0)
		{
			Test.AddError(TEXT("Acilis kuyrugu bosaltilamadi."));
			return false;
		}
		return true;
	}

	// Pins the order and the wrap so the only draw ServeFront takes from the
	// shared stream is the seating roll itself. Every branch this does not want
	// is guarded by a number: a tip is only rolled above quality 60, a new
	// regular only above accuracy 80, a family chain only with the trait.
	bool CigSeatTestsPrepareServe(FAutomationTestBase& Test, ACigkofteCustomer& C,
		UCigOrderSystem& Orders, bool bPacked)
	{
		C.bArrived = true;
		C.Traits = ECigTrait::None;
		C.bVIP = false;
		C.LoyalId = -1;
		C.Spec = FCigOrderSpec();
		C.Spec.Spice = ECigSpice::CokAci;
		C.Spec.Portion = 1;
		C.Spec.bPacked = bPacked;

		Orders.Wrap = FCigWrapBuild();
		Orders.Wrap.bActive = true;
		Orders.Wrap.bWrapped = true;
		Orders.Wrap.Portions = 1;
		Orders.Wrap.DoughQuality = 50.f;        // under 60: no tip roll
		Orders.Wrap.Spice = ECigSpice::AzAci;   // two steps off: accuracy under 80
		Orders.Wrap.bPacked = bPacked;

		const FCigOrderScore Score = UCigOrderSystem::ScoreWrap(Orders.Wrap, C.Spec, 0.f);
		if (Score.Accuracy >= 80.f)
		{
			Test.AddError(FString::Printf(
				TEXT("Test kurulumu bozuk: dogruluk %.1f, sadakat cekimi engellenmiyor."), Score.Accuracy));
			return false;
		}
		return true;
	}

	// Rewinds the stream to a seed whose next draw lands on the wanted side of
	// the 0.7 dine-in roll. A probed seed rather than a magic constant, so this
	// keeps working if the stream's arithmetic ever changes.
	bool CigSeatTestsSeedSeatingRoll(FAutomationTestBase& Test, UCigRandomSubsystem& Rng, bool bWantSeated)
	{
		for (int32 Candidate = 1; Candidate <= 128; ++Candidate)
		{
			Rng.SeedWith(Candidate);
			const bool bSeats = Rng.FRand() < 0.7f;
			if (bSeats == bWantSeated)
			{
				Rng.SeedWith(Candidate);
				return true;
			}
		}
		Test.AddError(FString::Printf(
			TEXT("Oturma cekimi icin tohum bulunamadi (istenen oturma %d)."), bWantSeated ? 1 : 0));
		return false;
	}

	// Spawns one system-owned customer, prepares the serve and fixes the roll.
	// System-owned matters: a bare SpawnActor is not in UCigCustomerSystem::Live,
	// so recovery and recycling would never look at it and the test would prove
	// nothing about either.
	ACigkofteCustomer* CigSeatTestsServeOne(FAutomationTestBase& Test, FCigSeatTestsRig& Rig,
		bool bPacked, bool bWantSeated)
	{
		ACigkofteCustomer* C = Rig.Cust->SpawnCustomer();
		if (!C) { Test.AddError(TEXT("Musteri gelmedi.")); return nullptr; }
		if (Rig.Cust->FrontCustomer() != C)
		{
			Test.AddError(TEXT("Gelen musteri kuyrugun onunde degil."));
			return nullptr;
		}
		if (!CigSeatTestsPrepareServe(Test, *C, *Rig.Orders, bPacked)) { return nullptr; }
		if (!CigSeatTestsSeedSeatingRoll(Test, *Rig.Rng, bWantSeated)) { return nullptr; }

		Rig.Cust->ServeFront();
		return C;
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
	TestEqual(TEXT("Dukkan bos sandalyelerle acilmali"), CigSeatTestsOccupiedSeats(*WB), 0);

	const int32 First = WB->ReserveSeat();
	if (First < 0) { AddError(TEXT("Sandalye ayrilamadi.")); return false; }
	TestEqual(TEXT("Bir rezervasyon bir sandalye tutmali"), CigSeatTestsOccupiedSeats(*WB), 1);

	// The same chair must not come back twice. ReserveSeat scans for the first
	// free one, so a second call has to find a different index or the shop would
	// seat two people on one chair.
	const int32 Second = WB->ReserveSeat();
	if (Second >= 0)
	{
		TestNotEqual(TEXT("Ikinci rezervasyon ayni sandalye olmamali"), Second, First);
		TestEqual(TEXT("Iki rezervasyon iki sandalye tutmali"), CigSeatTestsOccupiedSeats(*WB), 2);
		WB->ReleaseSeat(Second);
	}

	WB->ReleaseSeat(First);
	TestEqual(TEXT("Birakilan sandalye geri gelmeli"), CigSeatTestsOccupiedSeats(*WB), 0);

	// Releasing twice must not go negative or free somebody else's chair. The
	// exit paths overlap - a guest can be removed and then have the shop close
	// on them - so a double release is a thing that happens.
	WB->ReleaseSeat(First);
	TestEqual(TEXT("Iki kez birakmak zarar vermemeli"), CigSeatTestsOccupiedSeats(*WB), 0);

	// And an index that was never valid is ignored rather than crashing, which
	// matters because SeatIndex is -1 until a guest is actually seated.
	WB->ReleaseSeat(-1);
	WB->ReleaseSeat(WB->Seats.Num() + 10);
	TestEqual(TEXT("Gecersiz indeks bir sey bozmamali"), CigSeatTestsOccupiedSeats(*WB), 0);
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
	TestEqual(TEXT("Doldurulan sandalyeler tutulmus olmali"), CigSeatTestsOccupiedSeats(*WB), Taken);

	// Closing time. This is the path where a leak would be permanent: the guest
	// records are emptied, so nothing is left holding the index that would have
	// released the chair.
	Customers->SendEveryoneHome();

	TestEqual(TEXT("Kapanista hicbir sandalye tutulu kalmamali"), CigSeatTestsOccupiedSeats(*WB), 0);
	TestEqual(TEXT("Kapanista oturan kayit kalmamali"), Customers->Seated.Num(), 0);

	// The shop can seat a full house again tomorrow. A leak would show here as a
	// smaller number, and only here - nothing else in the game reports it.
	int32 TakenAgain = 0;
	while (WB->ReserveSeat() >= 0) { ++TakenAgain; }
	TestEqual(TEXT("Ertesi gun ayni sayida sandalye ayrilabilmeli"), TakenAgain, Taken);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSeatEatingLifecycleTest,
	"Cigkofte.Seating.AGuestEatsOnlyAfterReachingTheChair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSeatEatingLifecycleTest::RunTest(const FString&)
{
	// The whole dine-in arc through the door the player actually uses: serve,
	// walk, sit, eat, get up. The chairs had tests for reserving and releasing
	// them, and the queue had tests for who leaves it, but nothing had ever run
	// one customer from the counter to the table and out again.
	//
	// The part worth failing on is in the middle. The eating clock is meant to
	// start when the guest sits down, not when they are sent to a table - a
	// customer who is still crossing the room is not eating, and a timer that ran
	// anyway would free the chair while they were walking to it, seat somebody
	// else on it, and put two people on one chair for the rest of the day.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigSeatTestsRig Rig;
	if (!CigSeatTestsOpenShop(*this, Shop, Rig)) { return false; }

	ACigkofteCustomer* C = CigSeatTestsServeOne(*this, Rig, /*bPacked=*/false, /*bWantSeated=*/true);
	if (!C) { return false; }

	if (Rig.Cust->Seated.Num() != 1)
	{
		AddError(FString::Printf(TEXT("Musteri oturmaya gonderilmedi (oturan %d)."), Rig.Cust->Seated.Num()));
		return false;
	}
	TestTrue(TEXT("Oturan kayit servis edilen musteri olmali"), Rig.Cust->Seated[0].Customer.Get() == C);
	TestEqual(TEXT("Bir misafir bir sandalye tutmali"), CigSeatTestsOccupiedSeats(*Rig.WB), 1);
	TestFalse(TEXT("Masaya giden musteri kuyrukta kalmamali"), Rig.Cust->Queue.Contains(C));
	TestTrue(TEXT("Musteri masaya dogru yola cikmali"), C->bSeatMode);
	TestFalse(TEXT("Musteri daha oturmus olmamali"), C->IsSeated());

	// A route to the chair is part of the claim: a guest who cannot reach the
	// table is a layout defect, and the recovery sweep below would take the chair
	// back for a reason that has nothing to do with eating.
	if (C->bNavStranded)
	{
		AddError(TEXT("Sandalyeye rota bulunamadi; oturma akisi surulemez."));
		return false;
	}

	const int32 SeatIndex = Rig.Cust->Seated[0].SeatIndex;
	if (!Rig.WB->Seats.IsValidIndex(SeatIndex))
	{
		AddError(TEXT("Ayrilan sandalye indeksi gecersiz."));
		return false;
	}

	// Pinned rather than left on the 7-12 second roll, so the arithmetic below is
	// about the gate and not about which number the stream produced.
	Rig.Cust->Seated[0].EatTimer = 5.f;

	// Still walking. Two seconds of shop time must not touch the clock.
	Rig.Cust->UpdateSystem(2.f);
	if (Rig.Cust->Seated.Num() != 1)
	{
		AddError(TEXT("Yoldaki misafir kaydi dustu."));
		return false;
	}
	TestEqual(TEXT("Sandalyeye varmadan yeme suresi islememeli"), Rig.Cust->Seated[0].EatTimer, 5.f);
	TestEqual(TEXT("Yoldaki misafir sandalyeyi tutmaya devam etmeli"), CigSeatTestsOccupiedSeats(*Rig.WB), 1);

	// Arrive. Placed on the chair and given one short frame, because sitting down
	// is decided in the customer's own tick against the distance to the target.
	C->SetActorLocation(Rig.WB->Seats[SeatIndex].Pos);
	C->Tick(0.01f);
	if (!C->IsSeated())
	{
		AddError(TEXT("Musteri sandalyeye varinca oturmadi."));
		return false;
	}

	Rig.Cust->UpdateSystem(2.f);
	if (Rig.Cust->Seated.Num() != 1)
	{
		AddError(TEXT("Oturan misafir yemek bitmeden kalkti."));
		return false;
	}
	TestEqual(TEXT("Oturduktan sonra yeme suresi islemeli"), Rig.Cust->Seated[0].EatTimer, 3.f);

	// Finish the meal.
	Rig.Cust->UpdateSystem(4.f);
	TestEqual(TEXT("Yemek bitince oturan kayit kalmamali"), Rig.Cust->Seated.Num(), 0);
	TestEqual(TEXT("Yemek bitince sandalye geri gelmeli"), CigSeatTestsOccupiedSeats(*Rig.WB), 0);
	TestTrue(TEXT("Yemegini bitiren musteri cikisa yonelmeli"), C->bLeaving);
	TestTrue(TEXT("Yemegini bitiren musteri mutlu ayrilmali"), C->bHappy);
	TestFalse(TEXT("Ayrilan musteri hala oturuyor sayilmamali"), C->IsSeated());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSeatTakeawayTest,
	"Cigkofte.Seating.ATakeawayCustomerNeverTakesAChair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSeatTakeawayTest::RunTest(const FString&)
{
	// Three ways a served customer walks out with the food instead of sitting
	// down, and the thing they have in common is that none of them may leave a
	// chair marked taken. Each runs in its own shop: a chair wrongly held in one
	// scenario would otherwise change what the next one is able to reserve, and
	// the failure would be reported against the wrong branch.

	// 1. A packed order. The seating roll is seeded to succeed on purpose - a
	//    packed order must not even reach it, and a seed that would seat is the
	//    only way to tell "packed short-circuits" from "the roll happened to
	//    fail".
	{
		FCigTestShop Shop;
		if (!Shop.Build(*this)) { return false; }
		FCigSeatTestsRig Rig;
		if (!CigSeatTestsOpenShop(*this, Shop, Rig)) { return false; }

		ACigkofteCustomer* C = CigSeatTestsServeOne(*this, Rig, /*bPacked=*/true, /*bWantSeated=*/true);
		if (!C) { return false; }

		TestEqual(TEXT("Paketli siparis oturan kayit birakmamali"), Rig.Cust->Seated.Num(), 0);
		TestEqual(TEXT("Paketli siparis sandalye tutmamali"), CigSeatTestsOccupiedSeats(*Rig.WB), 0);
		TestFalse(TEXT("Paketli musteri kuyrukta kalmamali"), Rig.Cust->Queue.Contains(C));
		TestFalse(TEXT("Paketli musteri masaya yonelmemeli"), C->bSeatMode);
		TestTrue(TEXT("Paketli musteri mutlu ayrilmali"), C->bLeaving && C->bHappy);
	}

	// 2. Dine-in, but the roll says they are taking it with them anyway.
	{
		FCigTestShop Shop;
		if (!Shop.Build(*this)) { return false; }
		FCigSeatTestsRig Rig;
		if (!CigSeatTestsOpenShop(*this, Shop, Rig)) { return false; }

		ACigkofteCustomer* C = CigSeatTestsServeOne(*this, Rig, /*bPacked=*/false, /*bWantSeated=*/false);
		if (!C) { return false; }

		TestEqual(TEXT("Basarisiz oturma cekimi kayit birakmamali"), Rig.Cust->Seated.Num(), 0);
		TestEqual(TEXT("Basarisiz oturma cekimi sandalye tutmamali"), CigSeatTestsOccupiedSeats(*Rig.WB), 0);
		TestTrue(TEXT("Cekimi kaybeden musteri mutlu ayrilmali"), C->bLeaving && C->bHappy);
	}

	// 3. Dine-in, the roll succeeds, and the room is full. This is the branch
	//    that used to be easiest to get wrong: ReserveSeat returning -1 has to
	//    fall through to a happy departure rather than seating somebody on a
	//    chair index that does not exist.
	{
		FCigTestShop Shop;
		if (!Shop.Build(*this)) { return false; }
		FCigSeatTestsRig Rig;
		if (!CigSeatTestsOpenShop(*this, Shop, Rig)) { return false; }

		int32 Full = 0;
		while (Rig.WB->ReserveSeat() >= 0) { ++Full; }
		if (Full == 0) { AddError(TEXT("Salon doldurulamadi.")); return false; }

		ACigkofteCustomer* C = CigSeatTestsServeOne(*this, Rig, /*bPacked=*/false, /*bWantSeated=*/true);
		if (!C) { return false; }

		TestEqual(TEXT("Dolu salonda oturan kayit olusmamali"), Rig.Cust->Seated.Num(), 0);
		TestEqual(TEXT("Dolu salonda mevcut sandalyeler degismemeli"), CigSeatTestsOccupiedSeats(*Rig.WB), Full);
		TestFalse(TEXT("Yer bulamayan musteri masaya yonelmemeli"), C->bSeatMode);
		TestTrue(TEXT("Yer bulamayan musteri mutlu ayrilmali"), C->bLeaving && C->bHappy);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSeatStrandedGuestTest,
	"Cigkofte.Seating.AGuestWithNoRouteGivesTheChairBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSeatStrandedGuestTest::RunTest(const FString&)
{
	// A guest sent to a table who then cannot get there - the player drops a
	// crate across the room while they are walking - is taken back by the
	// customer system. The chair has to come with them: a reservation held by
	// somebody who is being recycled is a chair the shop never sees again.
	//
	// Driven through a system-owned customer on purpose. The navigation suite
	// already pins the stranding itself, but it does so on a bare actor that is
	// not in UCigCustomerSystem::Live, so nothing there ever asked what happens
	// to what the customer was holding.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigSeatTestsRig Rig;
	if (!CigSeatTestsOpenShop(*this, Shop, Rig)) { return false; }

	ACigkofteCustomer* C = CigSeatTestsServeOne(*this, Rig, /*bPacked=*/false, /*bWantSeated=*/true);
	if (!C) { return false; }
	if (Rig.Cust->Seated.Num() != 1 || CigSeatTestsOccupiedSeats(*Rig.WB) != 1)
	{
		AddError(TEXT("Misafir sandalyeye yerlesmedi; senaryo surulemez."));
		return false;
	}

	C->bNavStranded = true;
	Rig.Cust->UpdateSystem(0.f);

	TestEqual(TEXT("Rotasiz misafir oturan kayit birakmamali"), Rig.Cust->Seated.Num(), 0);
	TestEqual(TEXT("Rotasiz misafirin sandalyesi geri gelmeli"), CigSeatTestsOccupiedSeats(*Rig.WB), 0);
	TestFalse(TEXT("Rotasiz misafir kuyruga geri dusmemeli"), Rig.Cust->Queue.Contains(C));
	TestTrue(TEXT("Rotasiz misafir sayilmali"),
		Rig.Cust->StrandedRecovered + Rig.Cust->StrandedRecycled >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSeatRecycledGuestTest,
	"Cigkofte.Seating.ARecycledGuestGivesTheChairBackAndReturnsClean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSeatRecycledGuestTest::RunTest(const FString&)
{
	// Customers are pooled, so the body that walks in this afternoon is one that
	// already sat at a table this morning. Two things have to be true and they
	// are separate: the chair has to be released when the body is recycled, and
	// the body must not come back still believing it is sitting on one.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigSeatTestsRig Rig;
	if (!CigSeatTestsOpenShop(*this, Shop, Rig)) { return false; }

	ACigkofteCustomer* C = CigSeatTestsServeOne(*this, Rig, /*bPacked=*/false, /*bWantSeated=*/true);
	if (!C) { return false; }
	if (Rig.Cust->Seated.Num() != 1 || CigSeatTestsOccupiedSeats(*Rig.WB) != 1)
	{
		AddError(TEXT("Misafir sandalyeye yerlesmedi; senaryo surulemez."));
		return false;
	}

	// However they got there, the body is now finished with and waiting for the
	// pool. Dropping the record alone would leave the chair marked taken.
	C->Deactivate();
	Rig.Cust->UpdateSystem(0.f);

	TestEqual(TEXT("Havuza donen misafir oturan kayit birakmamali"), Rig.Cust->Seated.Num(), 0);
	TestEqual(TEXT("Havuza donen misafirin sandalyesi geri gelmeli"), CigSeatTestsOccupiedSeats(*Rig.WB), 0);

	ACigkofteCustomer* Reused = Rig.Cust->SpawnCustomer();
	if (!Reused) { AddError(TEXT("Havuzdan musteri alinamadi.")); return false; }
	TestTrue(TEXT("Yeni gelen ayni beden olmali"), Reused == C);
	TestFalse(TEXT("Havuzdan gelen musteri masaya yonelmis olmamali"), Reused->bSeatMode);
	TestFalse(TEXT("Havuzdan gelen musteri oturuyor sayilmamali"), Reused->IsSeated());
	TestFalse(TEXT("Havuzdan gelen musteri hala rotasiz sayilmamali"), Reused->bNavStranded);
	TestFalse(TEXT("Havuzdan gelen musteri geri donusu beklememeli"), Reused->bAwaitingRecycle);
	TestEqual(TEXT("Havuzdan gelen musteri sandalye tutmamali"), CigSeatTestsOccupiedSeats(*Rig.WB), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSeatDroppedRecordTest,
	"Cigkofte.Seating.ADroppedGuestRecordDoesNotKeepTheChair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSeatDroppedRecordTest::RunTest(const FString&)
{
	// The guest record holds a weak pointer, so a customer destroyed by anything
	// outside the customer system - a level teardown, a stray Destroy - leaves a
	// record pointing at nothing while the chair is still reserved. Nothing else
	// in the game would ever notice: no error, no message, just a shop that seats
	// fewer people every time it happens.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigSeatTestsRig Rig;
	if (!CigSeatTestsOpenShop(*this, Shop, Rig)) { return false; }

	const int32 SeatIndex = Rig.WB->ReserveSeat();
	if (SeatIndex < 0) { AddError(TEXT("Sandalye ayrilamadi.")); return false; }

	UCigCustomerSystem::FSeatedGuest G;
	G.SeatIndex = SeatIndex;
	G.EatTimer = 5.f;   // still eating, so the record is dropped for being empty
	Rig.Cust->Seated.Add(G);
	TestEqual(TEXT("Sahipsiz kayit once sandalyeyi tutuyor olmali"), CigSeatTestsOccupiedSeats(*Rig.WB), 1);

	Rig.Cust->UpdateSystem(0.016f);

	TestEqual(TEXT("Sahipsiz kayit temizlenmeli"), Rig.Cust->Seated.Num(), 0);
	TestEqual(TEXT("Sahipsiz kaydin sandalyesi geri gelmeli"), CigSeatTestsOccupiedSeats(*Rig.WB), 0);
	return true;
}

#endif
