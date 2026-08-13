// The five jobs, doing the work rather than being assigned it.
//
// UCigStaffSystem::DoWork is where the apprentice stops being a number on the
// tablet and starts costing stock, clearing dirt and filling the shelf. It is
// private, and it should stay private - the way in is UpdateSystem, which is
// what the game ticks, and going through it means the work tick is reached the
// same way it is reached in play: a timer that has run out, and a competence
// roll that did not fumble.
//
// The failure this file exists to catch is a job that silently does nothing.
// Every one of these five is a switch case guarded by a pile of conditions, and
// a guard that is subtly wrong does not crash or log - the apprentice simply
// stands there all day while the player wonders what they are paying for.

#include "Misc/AutomationTest.h"
#include "Cooking/CigCookingSystem.h"
#include "Core/CigRandomSubsystem.h"
#include "Economy/CigEconomySystem.h"
#include "Game/CigDaySystem.h"
#include "Game/CigkofteGameMode.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Orders/CigOrderSystem.h"
#include "Staff/CigStaffSystem.h"
#include "Tests/CigTestShop.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// An apprentice already on the job, without going through hiring: what is
	// being measured here is the work, and the hiring path has its own twelve
	// tests in CigStaffLifecycleTests.cpp.
	void CigJobTestsPutThemToWork(UCigStaffSystem& Staff, ECigStaffTask Task)
	{
		FCigApprentice& A = Staff.Apprentice;
		A = FCigApprentice();
		A.bHired = true;
		A.Name = TEXT("Yusuf");
		A.Task = Task;
		A.Energy = 100.f;
		A.Hiz = 1.f;
		A.Titizlik = 1.f;
	}

	// Drives exactly one work tick, and guarantees it is a working tick rather
	// than a fumbled one.
	//
	// DoWork sits behind Rng().Chance(HataOlasiligi(...)), so a test that just
	// ticks gets the mistake branch about one time in eight and is a coin flip
	// dressed as a test. Rather than baking in a magic seed, this probes for one
	// whose first draw clears the roll and then rewinds the stream to just
	// before the probe - the same arrangement CigCustomerGroupTests uses, and it
	// keeps meaning what it says if FRandomStream ever changes.
	//
	// Nothing else draws between the rewind and the tick, because UpdateSystem
	// is called directly rather than through the game mode.
	bool CigJobTestsWorkOnce(FCigTestShop& Shop, UCigStaffSystem& Staff)
	{
		UCigRandomSubsystem* Rng = Shop.GI ? Shop.GI->GetSubsystem<UCigRandomSubsystem>() : nullptr;
		if (!Rng)
		{
			return false;
		}

		const FCigApprentice& A = Staff.Apprentice;
		const float Hata = UCigStaffSystem::HataOlasiligi(A.Titizlik, A.Level, A.Energy);

		for (int32 Aday = 1; Aday <= 4096; ++Aday)
		{
			Rng->SeedWith(Aday);
			if (Rng->FRand() >= Hata)
			{
				Rng->SeedWith(Aday);
				Staff.UpdateSystem(UCigStaffSystem::IsAraligi(A.Hiz, A.Level) + 0.01f);
				return true;
			}
		}
		return false;
	}

	// The same, for the other branch: a seed whose first draw lands inside the
	// mistake probability, so HataYap runs instead of DoWork.
	bool CigJobTestsFumbleOnce(FCigTestShop& Shop, UCigStaffSystem& Staff)
	{
		UCigRandomSubsystem* Rng = Shop.GI ? Shop.GI->GetSubsystem<UCigRandomSubsystem>() : nullptr;
		if (!Rng)
		{
			return false;
		}

		const FCigApprentice& A = Staff.Apprentice;
		const float Hata = UCigStaffSystem::HataOlasiligi(A.Titizlik, A.Level, A.Energy);

		for (int32 Aday = 1; Aday <= 4096; ++Aday)
		{
			Rng->SeedWith(Aday);
			if (Rng->FRand() < Hata)
			{
				Rng->SeedWith(Aday);
				Staff.UpdateSystem(UCigStaffSystem::IsAraligi(A.Hiz, A.Level) + 0.01f);
				return true;
			}
		}
		return false;
	}

	// UpdateSystem does nothing unless the day is open, which is correct - an
	// apprentice does not work a shut shop - and is also the first thing a test
	// of the work forgets.
	//
	// Two calls, not one. StartDay only reaches Opening: the morning starts in
	// preparation, stock arrives and staff turn up, but the door is still shut.
	// OpenShop is what starts service, and IsPlaying is true from there. Getting
	// this wrong is silent - every job simply does nothing, which reads exactly
	// like a broken job.
	void CigJobTestsOpenTheShop(FCigTestShop& Shop)
	{
		Shop.GM->Days->StartDay(false);
		Shop.GM->Days->OpenShop();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobChopTest,
	"Cigkofte.Staff.ChoppingTurnsLettuceIntoGarnish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobChopTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Dograma);

	Inv->Stock[CigStockMarul] = 5;
	Inv->Garnish = 0;

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	// The trade the job is: a lettuce goes in, a portion of garnish comes out.
	// Either half of that missing is a job that looks busy and is not.
	TestEqual(TEXT("Dograma bir marul tuketmeli"), Inv->Stock[CigStockMarul], 4);
	TestEqual(TEXT("Dograma bir garnitur uretmeli"), Inv->Garnish, 1);
	TestEqual(TEXT("Calisan tik enerji goturmeli"), Staff->Apprentice.Energy, 97.f, 0.01f);
	TestEqual(TEXT("Calisan tik XP kazandirmali"), Staff->Apprentice.XP, 3);
	TestEqual(TEXT("Yapilan is sayaca islenmeli"),
		Staff->Apprentice.TaskCounts[(int32)ECigStaffTask::Dograma], 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobChopSpecialistTest,
	"Cigkofte.Staff.AChoppingSpecialistGetsTwoPortionsFromOneLettuce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobChopSpecialistTest::RunTest(const FString&)
{
	// Why the specialism is worth earning, stated as the thing the player gets
	// rather than as a label on the tablet. Same lettuce, twice the garnish.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Dograma);
	Staff->Apprentice.Spec = ECigStaffSpec::DogramaUzmani;

	Inv->Stock[CigStockMarul] = 5;
	Inv->Garnish = 0;

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Uzman da yalniz bir marul tuketmeli"), Inv->Stock[CigStockMarul], 4);
	TestEqual(TEXT("Uzman iki garnitur uretmeli"), Inv->Garnish, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobChopWithoutLettuceTest,
	"Cigkofte.Staff.AJobWithNothingToDoCostsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobChopWithoutLettuceTest::RunTest(const FString&)
{
	// The quiet case. With no lettuce there is nothing to chop, and the right
	// answer is that the tick passes without cost - not that energy drains and
	// XP accrues for standing at an empty board. bWorked is what decides it, and
	// a job whose guard forgets to set it pays the apprentice for nothing.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Dograma);

	Inv->Stock[CigStockMarul] = 0;
	Inv->Garnish = 0;

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Marul yokken garnitur olusmamali"), Inv->Garnish, 0);
	TestEqual(TEXT("Bos tik enerji goturmemeli"), Staff->Apprentice.Energy, 100.f, 0.01f);
	TestEqual(TEXT("Bos tik XP kazandirmamali"), Staff->Apprentice.XP, 0);

	// The attempt is still counted, and that is deliberate: TaskCounts measures
	// where the apprentice spent their day, which decides the specialism, and
	// they did spend it at the chopping board.
	TestEqual(TEXT("Denenen is yine de sayaca islenmeli"),
		Staff->Apprentice.TaskCounts[(int32)ECigStaffTask::Dograma], 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobCleanTest,
	"Cigkofte.Staff.CleaningActuallyTakesDirtOffTheCounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobCleanTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigHygieneSystem* Hyg = Shop.GM->Hygiene.Get();
	if (!Staff || !Hyg) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Temizlik);

	Hyg->CounterDirt = 60.f;
	Hyg->ChopDirt = 40.f;
	Hyg->CatFur = 30.f;

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Tezgah kiri azalmali"), Hyg->CounterDirt, 48.f, 0.01f);
	TestEqual(TEXT("Dograma kiri azalmali"), Hyg->ChopDirt, 28.f, 0.01f);

	// Fur comes off at half rate, because sweeping up after a cat is not the
	// same job as wiping a surface.
	TestEqual(TEXT("Kedi tuyu yarim guçle azalmali"), Hyg->CatFur, 24.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobCleanFloorTest,
	"Cigkofte.Staff.CleaningStopsAtCleanRatherThanGoingNegative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobCleanFloorTest::RunTest(const FString&)
{
	// An already-clean counter is the edge that turns a scrub into a negative
	// number, and a negative dirt reading would quietly bank credit against the
	// next inspection.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigHygieneSystem* Hyg = Shop.GM->Hygiene.Get();
	if (!Staff || !Hyg) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Temizlik);

	Hyg->CounterDirt = 3.f;
	Hyg->ChopDirt = 0.f;
	Hyg->CatFur = 1.f;

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Tezgah kiri sifirin altina inmemeli"), Hyg->CounterDirt, 0.f, 0.01f);
	TestEqual(TEXT("Dograma kiri sifirin altina inmemeli"), Hyg->ChopDirt, 0.f, 0.01f);
	TestEqual(TEXT("Kedi tuyu sifirin altina inmemeli"), Hyg->CatFur, 0.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobPackTest,
	"Cigkofte.Staff.PackingPutsARealWrapOnTheShelf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobPackTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	UCigOrderSystem* Orders = Shop.GM->Orders.Get();
	UCigCookingSystem* Cook = Shop.GM->Cooking.Get();
	if (!Staff || !Inv || !Orders || !Cook) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Paket);

	// A batch worth packing, and a flatbread to wrap it in.
	Cook->Dough.Servings = 4;
	Cook->Dough.Quality = 0.8f;
	Cook->Dough.Spice = ECigSpice::CokAci;
	Inv->Stock[CigStockLavas] = 3;
	Orders->Shelf.Reset();

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	if (Orders->Shelf.Num() != 1)
	{
		AddError(FString::Printf(TEXT("Rafta 1 paket bekleniyordu, %d var."), Orders->Shelf.Num()));
		return false;
	}

	TestEqual(TEXT("Paketleme bir lavas tuketmeli"), Inv->Stock[CigStockLavas], 2);
	TestEqual(TEXT("Paketleme bir porsiyon hamur tuketmeli"), Cook->Dough.Servings, 3);

	// What is on the shelf has to be a wrap somebody could actually be sold,
	// not an empty record that only counts.
	const FCigPackagedWrap& P = Orders->Shelf[0];
	TestTrue(TEXT("Raftaki paket gecerli olmali"), P.Build.bActive);
	TestTrue(TEXT("Raftaki paket sarilmis olmali"), P.Build.bWrapped);
	TestTrue(TEXT("Raftaki paket paketlenmis olmali"), P.Build.bPacked);
	TestEqual(TEXT("Raftaki paket tek porsiyon olmali"), P.Build.Portions, 1);

	// The acilik is read before the serving is drawn, and that ordering is the
	// whole reason PaketHazirla takes it as an argument: the last serving of a
	// batch would otherwise package as medium.
	TestTrue(TEXT("Paket partinin aciligini tasimali"), P.Build.Spice == ECigSpice::CokAci);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobPackFullShelfTest,
	"Cigkofte.Staff.PackingStopsWhenTheShelfIsFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobPackFullShelfTest::RunTest(const FString&)
{
	// A shelf that overflows would be stock destroyed with nothing to show for
	// it: the lavash and the serving are consumed before the Add.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	UCigOrderSystem* Orders = Shop.GM->Orders.Get();
	UCigCookingSystem* Cook = Shop.GM->Cooking.Get();
	if (!Staff || !Inv || !Orders || !Cook) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Paket);

	Cook->Dough.Servings = 4;
	Cook->Dough.Quality = 0.8f;
	Inv->Stock[CigStockLavas] = 3;

	Orders->Shelf.Reset();
	for (int32 i = 0; i < UCigOrderSystem::MaxShelf; ++i)
	{
		Orders->Shelf.Add(UCigStaffSystem::PaketHazirla(ECigSpice::Orta, 0.7f, false));
	}

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Dolu raf buyumemeli"), Orders->Shelf.Num(), UCigOrderSystem::MaxShelf);
	TestEqual(TEXT("Dolu rafta lavas harcanmamali"), Inv->Stock[CigStockLavas], 3);
	TestEqual(TEXT("Dolu rafta hamur harcanmamali"), Cook->Dough.Servings, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobRestockTest,
	"Cigkofte.Staff.RestockingPicksTheItemThatRanOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobRestockTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv || !Shop.GM->Economy) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Stok);

	// Everything comfortable except one item, so there is exactly one right
	// answer and picking any other would show. Comfortable rather than full:
	// OrderStock checks room before money, so a shelf stacked to the ceiling
	// refuses the delivery for a reason that has nothing to do with the job.
	for (int32 i = 0; i < CigStockCount; ++i)
	{
		Inv->Stock[i] = 5;
	}
	Inv->Stock[CigStockDomates] = 1;
	Shop.GM->Economy->Money = 10000;

	const int32 Once = Inv->Stock[CigStockDomates];

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	// OrderStock is a delivery rather than an instant top-up, so what is proved
	// here is that the order went out and that it went out against the item that
	// actually ran low - the till moving is what says an order was placed.
	TestTrue(TEXT("Stok siparisi kasadan para goturmeli"), Shop.GM->Economy->Money < 10000);
	TestEqual(TEXT("Siparis aninda stoga yazilmamali"), Inv->Stock[CigStockDomates], Once);
	TestEqual(TEXT("Calisan tik XP kazandirmali"), Staff->Apprentice.XP, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobRestockNoRoomTest,
	"Cigkofte.Staff.RestockingDoesNotBuyWhatThereIsNowhereToPut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobRestockNoRoomTest::RunTest(const FString&)
{
	// Found by getting a test wrong: a shelf filled to the ceiling refuses the
	// delivery, and it refuses it before it looks at the money. That is the right
	// way round - paying for a crate with nowhere to go is worse than not
	// ordering - and it is worth holding, because the apprentice orders without
	// the player watching and could otherwise burn the till on stock that
	// evaporates on arrival.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv || !Shop.GM->Economy) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Stok);

	// Every shelf stacked, one item still technically low enough to qualify.
	for (int32 i = 0; i < CigStockCount; ++i)
	{
		Inv->Stock[i] = 20;
	}
	Inv->Stock[CigStockDomates] = 1;
	Shop.GM->Economy->Money = 10000;

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Yer yokken kasadan para cikmamali"), Shop.GM->Economy->Money, 10000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobRestockCashFloorTest,
	"Cigkofte.Staff.RestockingLeavesTheShopSomethingToTradeWith",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobRestockCashFloorTest::RunTest(const FString&)
{
	// An apprentice who spends the last of the till on tomatoes has ended the
	// run, so the job keeps a 200 lira floor. This is the guard, and a guard
	// nobody tests is a guard that quietly turns into an off-by-one.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv || !Shop.GM->Economy) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Stok);

	for (int32 i = 0; i < CigStockCount; ++i)
	{
		Inv->Stock[i] = 20;
	}
	Inv->Stock[CigStockDomates] = 1;

	// One lira short of the cost plus the floor.
	Shop.GM->Economy->Money = Inv->OrderCost(CigStockDomates) + 199;
	const int32 Once = Shop.GM->Economy->Money;

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Nakit tabani altinda siparis verilmemeli"), Shop.GM->Economy->Money, Once);
	TestEqual(TEXT("Verilmeyen siparis XP kazandirmamali"), Staff->Apprentice.XP, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobChopMistakeTest,
	"Cigkofte.Staff.AFumbleAtTheBoardCostsGarnish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobChopMistakeTest::RunTest(const FString&)
{
	// The other branch of the same tick. Which mistake happens follows the job,
	// and that is what makes tidiness worth paying for rather than a stat.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Dograma);

	Inv->Stock[CigStockMarul] = 5;
	Inv->Garnish = 4;

	if (!CigJobTestsFumbleOnce(Shop, *Staff))
	{
		AddError(TEXT("Hata tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Hata bir garnituru mahvetmeli"), Inv->Garnish, 3);
	TestEqual(TEXT("Hata enerji goturmeli"), Staff->Apprentice.Energy, 97.f, 0.01f);

	// A wasted tick is wasted: no lettuce chopped, and no experience for it.
	TestEqual(TEXT("Hatali tik marul tuketmemeli"), Inv->Stock[CigStockMarul], 5);
	TestEqual(TEXT("Hatali tik XP kazandirmamali"), Staff->Apprentice.XP, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobCleanMistakeTest,
	"Cigkofte.Staff.AFumbleWhileCleaningAddsToTheWashingUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobCleanMistakeTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigHygieneSystem* Hyg = Shop.GM->Hygiene.Get();
	if (!Staff || !Hyg) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Temizlik);

	Hyg->CounterDirt = 60.f;
	Hyg->DishPile = 10.f;

	if (!CigJobTestsFumbleOnce(Shop, *Staff))
	{
		AddError(TEXT("Hata tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Dusurulen tabak bulasigi artirmali"), Hyg->DishPile, 18.f, 0.01f);

	// And the counter is no cleaner for it - the tick went on the mistake.
	TestEqual(TEXT("Hatali tik tezgahi temizlememeli"), Hyg->CounterDirt, 60.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobRestOnEmptyTest,
	"Cigkofte.Staff.AnExhaustedApprenticeStopsRatherThanWorkingOn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobRestOnEmptyTest::RunTest(const FString&)
{
	// Below ten energy they are on a break, and the break has to be real: a
	// guard that reads the wrong way would have them working forever at zero.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsOpenTheShop(Shop);
	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Dograma);
	Staff->Apprentice.Energy = 5.f;

	Inv->Stock[CigStockMarul] = 5;
	Inv->Garnish = 0;

	if (!CigJobTestsWorkOnce(Shop, *Staff))
	{
		AddError(TEXT("Calisma tiki tetiklenemedi."));
		return false;
	}

	TestEqual(TEXT("Molada garnitur uretilmemeli"), Inv->Garnish, 0);
	TestEqual(TEXT("Molada marul tuketilmemeli"), Inv->Stock[CigStockMarul], 5);
	TestEqual(TEXT("Molada enerji daha da dusmemeli"), Staff->Apprentice.Energy, 5.f, 0.01f);

	// The early return happens before the counter, so a break is not recorded as
	// time spent at the board.
	TestEqual(TEXT("Mola gorev sayacina islenmemeli"),
		Staff->Apprentice.TaskCounts[(int32)ECigStaffTask::Dograma], 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigJobShutShopTest,
	"Cigkofte.Staff.NobodyWorksAShutShop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigJobShutShopTest::RunTest(const FString&)
{
	// Everything else is set up to succeed, so any work that happens here is work
	// happening outside opening hours.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	if (!Staff || !Inv) { AddError(TEXT("Sistemler yok.")); return false; }

	CigJobTestsPutThemToWork(*Staff, ECigStaffTask::Dograma);
	Inv->Stock[CigStockMarul] = 5;
	Inv->Garnish = 0;

	// Shut: no day has been started at all.
	CigJobTestsWorkOnce(Shop, *Staff);

	TestEqual(TEXT("Kapali dukkanda garnitur uretilmemeli"), Inv->Garnish, 0);
	TestEqual(TEXT("Kapali dukkanda marul tuketilmemeli"), Inv->Stock[CigStockMarul], 5);
	TestEqual(TEXT("Kapali dukkanda XP kazanilmamali"), Staff->Apprentice.XP, 0);

	// And the half-open case, which is the one that is easy to get wrong. The
	// morning starts in preparation: the day's hooks have fired, the apprentice
	// has turned up and their energy is back, but the door is not open yet and
	// there is nothing to work on. Only OpenShop starts service.
	Shop.GM->Days->StartDay(false);
	CigJobTestsWorkOnce(Shop, *Staff);

	TestEqual(TEXT("Hazirlik fazinda garnitur uretilmemeli"), Inv->Garnish, 0);
	TestEqual(TEXT("Hazirlik fazinda marul tuketilmemeli"), Inv->Stock[CigStockMarul], 5);
	TestEqual(TEXT("Hazirlik fazinda gorev sayaci islememeli"),
		Staff->Apprentice.TaskCounts[(int32)ECigStaffTask::Dograma], 0);

	// Open the door and the same tick does the work, which is what says the three
	// assertions above measured the phase rather than a broken setup.
	Shop.GM->Days->OpenShop();
	CigJobTestsWorkOnce(Shop, *Staff);

	TestEqual(TEXT("Dukkan acilinca ayni tik is yapmali"), Inv->Garnish, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
