// Does the queue treat the person at the back the same as the person at the front?
//
// The queue is a plain TArray and the promise it makes is first-come-first-served:
// FrontCustomer() is Queue[0], ServeFront() serves them, and every tick pushes
// each customer at the slot for their index. That is a promise about fairness
// made by an array index, and nothing checked it.
//
// It is worth checking because three separate things reorder the queue and each
// of them uses a different mechanism. Serving takes from the front. Running out
// of patience takes from wherever the customer happens to stand - RemoveCustomer
// is a Remove(C), not a pop. And a customer who cannot be reached at all is
// pulled out by ReleaseCustomerOwnership. If any one of them left a hole, or
// shuffled the order, the visible symptom would be a customer standing at the
// counter watching somebody behind them get served - and no error anywhere.
//
// The other half of fairness is the physical queue. Slots are handed out by
// index, so a customer leaving from the middle must make everybody behind them
// step forward. Get that wrong and the queue grows a permanent gap that the
// player reads as "that person is being ignored".

#include "Misc/AutomationTest.h"
#include "Customers/CigCustomerSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "Placement/CigPlacementTypes.h"
#include "Progression/CigProgressionSystem.h"
#include "Tests/CigTestShop.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Puts N customers in the queue and marks them all as standing at the
	// counter. Walking is not what these tests are about: SpawnCustomer produces
	// a real customer with real traits and a real order, and bArrived is the one
	// thing that would otherwise need a world with floors in it.
	bool FillQueue(FAutomationTestBase& Test, UCigCustomerSystem& Cust, int32 N)
	{
		for (int32 i = 0; i < N; ++i)
		{
			Cust.SpawnCustomer();
		}
		if (Cust.Queue.Num() != N)
		{
			Test.AddError(FString::Printf(TEXT("Kuyruga %d musteri girmedi (%d var)."),
				N, Cust.Queue.Num()));
			return false;
		}
		for (TObjectPtr<ACigkofteCustomer>& C : Cust.Queue)
		{
			if (!C) { Test.AddError(TEXT("Kuyrukta bos kayit var.")); return false; }
			C->bArrived = true;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigQueueOrderTest,
	"Cigkofte.Queue.NobodyIsOvertakenInTheQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigQueueOrderTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	if (!Cust) { AddError(TEXT("Musteri sistemi yok.")); return false; }

	if (!FillQueue(*this, *Cust, 4)) { return false; }

	// Remember the order that formed, so the claim below is about these four
	// people rather than about whatever the array happens to hold.
	TArray<ACigkofteCustomer*> Arrival;
	for (const TObjectPtr<ACigkofteCustomer>& C : Cust->Queue) { Arrival.Add(C.Get()); }

	TestEqual(TEXT("Ilk gelen kuyrugun basinda olmali"),
		(ACigkofteCustomer*)Cust->FrontCustomer(), Arrival[0]);

	// Taking the front one out must promote the second, not the third or the
	// last. This is the whole first-come-first-served promise in one line.
	Cust->RemoveCustomer(Arrival[0], /*bAngry=*/false);
	TestEqual(TEXT("Bastaki cikinca ikinci one gecmeli"),
		(ACigkofteCustomer*)Cust->FrontCustomer(), Arrival[1]);
	TestEqual(TEXT("Kuyruk bir kisalmali"), Cust->Queue.Num(), 3);

	// Somebody in the middle giving up must not disturb who is at the front, and
	// must not lose the person behind them. RemoveCustomer is a Remove(C) rather
	// than a pop, so this is the case where an index-based implementation would
	// quietly drop the wrong customer.
	Cust->RemoveCustomer(Arrival[2], /*bAngry=*/true);
	TestEqual(TEXT("Ortadaki cikinca bastaki degismemeli"),
		(ACigkofteCustomer*)Cust->FrontCustomer(), Arrival[1]);
	TestEqual(TEXT("Ortadaki cikinca arkadaki kalmali"), Cust->Queue.Num(), 2);
	TestTrue(TEXT("Sondaki hala kuyrukta olmali"), Cust->Queue.Contains(Arrival[3]));
	TestFalse(TEXT("Cikan musteri kuyrukta kalmamali"), Cust->Queue.Contains(Arrival[2]));

	// And the survivors keep their relative order: the person who arrived second
	// is still ahead of the person who arrived fourth.
	if (Cust->Queue.Num() == 2)
	{
		TestEqual(TEXT("Kalanlarin sirasi bozulmamali"),
			(ACigkofteCustomer*)Cust->Queue[1].Get(), Arrival[3]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigQueueSlotsTest,
	"Cigkofte.Queue.LeavingFromTheMiddleClosesTheGap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigQueueSlotsTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	if (!Cust || !Days) { AddError(TEXT("Sistemler yok.")); return false; }

	// The queue only advances while the shop is serving. UpdateSystem returns
	// early otherwise, which is correct and is also why this has to open.
	Days->StartDay(false);
	Days->OpenShop();
	if (!FillQueue(*this, *Cust, 4)) { return false; }

	TArray<ACigkofteCustomer*> Arrival;
	for (const TObjectPtr<ACigkofteCustomer>& C : Cust->Queue) { Arrival.Add(C.Get()); }

	// Slots are a straight line back from the counter. What matters is not the
	// numbers but that index i gets slot i and the spacing never collapses - two
	// customers sharing a slot would stand inside one another.
	const FVector Front = CigPlacementLayout::QueueFront();
	const float Spacing = CigPlacementLayout::QueueSpacing();
	TestTrue(TEXT("Kuyruk araligi pozitif olmali"), Spacing > 0.f);

	Cust->UpdateSystem(0.016f);
	for (int32 i = 0; i < Cust->Queue.Num(); ++i)
	{
		const FVector Want = Front - FVector(Spacing * i, 0.f, 0.f);
		TestTrue(*FString::Printf(TEXT("%d. sira dogru yere gonderilmeli"), i),
			Cust->Queue[i]->GetTargetForTest().Equals(Want, 1.f));
	}

	// The second customer gives up. Everybody behind them has to step forward, or
	// the queue keeps a hole at slot 1 that reads on screen as a person being
	// skipped over.
	Cust->RemoveCustomer(Arrival[1], /*bAngry=*/true);
	Cust->UpdateSystem(0.016f);

	TestEqual(TEXT("Bir kisi ciktiktan sonra ucu kalmali"), Cust->Queue.Num(), 3);
	for (int32 i = 0; i < Cust->Queue.Num(); ++i)
	{
		const FVector Want = Front - FVector(Spacing * i, 0.f, 0.f);
		TestTrue(*FString::Printf(TEXT("Bosluk kapandiktan sonra %d. sira dogru olmali"), i),
			Cust->Queue[i]->GetTargetForTest().Equals(Want, 1.f));
	}

	// Said as a property rather than as three coordinates: no two people in the
	// queue are sent to the same place.
	for (int32 i = 0; i < Cust->Queue.Num(); ++i)
	{
		for (int32 j = i + 1; j < Cust->Queue.Num(); ++j)
		{
			TestFalse(*FString::Printf(TEXT("%d ve %d ayni yere gonderilmemeli"), i, j),
				Cust->Queue[i]->GetTargetForTest().Equals(Cust->Queue[j]->GetTargetForTest(), 1.f));
		}
	}

	// A customer who left is no longer steered by the queue: their target is the
	// exit. Otherwise the queue would keep pulling somebody who is walking out.
	TestFalse(TEXT("Cikan musteri kuyruk yerine cekilmemeli"),
		Arrival[1]->GetTargetForTest().Equals(Front - FVector(Spacing, 0.f, 0.f), 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigQueueCapacityTest,
	"Cigkofte.Queue.TheQueueNeverGrowsPastItsOwnLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigQueueCapacityTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	if (!Cust || !Days) { AddError(TEXT("Sistemler yok.")); return false; }

	const int32 Cap = Cust->MaxQueue();
	TestTrue(TEXT("Kuyruk siniri pozitif olmali"), Cap > 0);

	Days->StartDay(false);
	Days->OpenShop();

	// Two full days' worth of ticks at one second each. The spawn interval is a
	// few seconds, so this is far more chances to spawn than there are slots -
	// the limit has to be what stops it, not the clock running out.
	for (int32 i = 0; i < 400; ++i)
	{
		Cust->UpdateSystem(1.f);
		if (Cust->Queue.Num() > Cap)
		{
			AddError(FString::Printf(TEXT("Kuyruk sinirini asti: %d > %d (tik %d)."),
				Cust->Queue.Num(), Cap, i));
			return false;
		}
	}

	// And it actually filled up - a limit that holds because nobody ever arrived
	// would pass the check above against any bug at all.
	TestEqual(TEXT("Kuyruk tam siniri kadar dolmali"), Cust->Queue.Num(), Cap);

	// Slots exist for everyone standing in it. A queue that can hold more people
	// than it has positions would stack the extras on the last slot.
	Cust->UpdateSystem(0.016f);
	TSet<FString> Places;
	for (const TObjectPtr<ACigkofteCustomer>& C : Cust->Queue)
	{
		if (C) { Places.Add(C->GetTargetForTest().ToString()); }
	}
	TestEqual(TEXT("Herkesin kendi yeri olmali"), Places.Num(), Cust->Queue.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigQueuePatienceTest,
	"Cigkofte.Queue.OnlyPeopleWhoHaveArrivedRunOutOfPatience",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigQueuePatienceTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	if (!Cust || !Days) { AddError(TEXT("Sistemler yok.")); return false; }

	Days->StartDay(false);
	Days->OpenShop();

	Cust->SpawnCustomer();
	Cust->SpawnCustomer();
	if (Cust->Queue.Num() != 2) { AddError(TEXT("Iki musteri kuyruga girmedi.")); return false; }

	ACigkofteCustomer* Walking = Cust->Queue[0].Get();
	ACigkofteCustomer* Standing = Cust->Queue[1].Get();
	if (!Walking || !Standing) { AddError(TEXT("Musteri kaydi bos.")); return false; }

	// One is still crossing the pavement, the other is at the counter. The clock
	// runs for the second and not the first: charging somebody for the time it
	// took them to walk in would punish the player for a long shop.
	Walking->bArrived = false;
	Standing->bArrived = true;

	// Each customer's own starting value: patience is rolled per customer from
	// traits, upgrades and the day, so the two do not begin equal and comparing
	// one against the other's start would prove nothing.
	const float WalkingStart = Walking->Patience;
	const float StandingStart = Standing->Patience;

	Cust->UpdateSystem(2.f);

	TestEqual(TEXT("Yurumekte olanin sabri islememeli"), Walking->Patience, WalkingStart);
	TestTrue(TEXT("Tezgahtakinin sabri islemeli"), Standing->Patience < StandingStart);

	// Patience running out takes the customer out of the queue, and it does it
	// for the person it belongs to rather than for whoever stands at the front.
	Standing->Patience = 0.5f;
	Cust->UpdateSystem(1.f);
	TestFalse(TEXT("Sabri biten kuyruktan cikmali"), Cust->Queue.Contains(Standing));
	TestTrue(TEXT("Sabri islemeyen kuyrukta kalmali"), Cust->Queue.Contains(Walking));
	return true;
}

#endif
