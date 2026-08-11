// The apprentice as the player meets them, rather than as two pure functions.
//
// UCigStaffSystem hires from a pool of walk-ins, runs five jobs, levels up,
// earns a specialism, loses morale, asks for a raise, gets poached and is paid
// out of the till every night. Cigkofte.Staff covers none of that: its four
// tests check HataOlasiligi and IsAraligi, which are the competence model and
// deliberately testable on their own, but everything that reaches the shop goes
// through DoWork, OnDayEnd and the save - and none of those had a test.
//
// This file starts with the save, because reading the two sides against each
// other already shows one field that is captured nowhere.

#include "Misc/AutomationTest.h"
#include "Game/CigkofteGameMode.h"
#include "Progression/CigProgressionSystem.h"
#include "Save/CigSaveGame.h"
#include "Staff/CigStaffSystem.h"
#include "Tests/CigTestShop.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// An apprentice with a history: hired, promoted, and with two days of work
	// behind them. Written onto the system directly because what is being
	// measured here is the save, not how they got this way.
	void CigStaffTestsGiveThemAPast(UCigStaffSystem& Staff)
	{
		FCigApprentice& A = Staff.Apprentice;
		A = FCigApprentice();
		A.bHired = true;
		A.Name = TEXT("Berat");
		A.Level = 2;
		A.XP = 37;
		A.Morale = 64.f;
		A.Salary = 135;
		A.Task = ECigStaffTask::Temizlik;
		A.DaysSinceRaise = 3;
		A.Arketip = 1;
		A.Hiz = 1.2f;
		A.Titizlik = 0.9f;
		A.GulerYuz = 1.1f;

		// The history that matters: they have spent their time on the chopping
		// board, so the specialism waiting for them at level 3 is DogramaUzmani.
		A.TaskCounts[(int32)ECigStaffTask::Dograma] = 40;
		A.TaskCounts[(int32)ECigStaffTask::Temizlik] = 6;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffSaveRoundTripTest,
	"Cigkofte.Staff.EverythingAboutTheApprenticeSurvivesASave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffSaveRoundTripTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	CigStaffTestsGiveThemAPast(*Staff);
	Staff->TransferTeklifi = 189;

	const FCigApprentice Before = Staff->Apprentice;

	// Through the real save object, so what is measured is the capture and apply
	// the game actually runs rather than a helper written for the test.
	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);

	// Wiped rather than left standing, so a field that apply never touches shows
	// up as its default instead of as the value that happened to still be there.
	Staff->Apprentice = FCigApprentice();
	Staff->TransferTeklifi = 0;

	Shop.GM->ApplySave(*Save);
	Save->RemoveFromRoot();

	const FCigApprentice& After = Staff->Apprentice;

	TestTrue(TEXT("Ise alinmis olma durumu korunmali"), After.bHired);
	TestEqual(TEXT("Ad korunmali"), After.Name, Before.Name);
	TestEqual(TEXT("Seviye korunmali"), After.Level, Before.Level);
	TestEqual(TEXT("XP korunmali"), After.XP, Before.XP);
	TestEqual(TEXT("Moral korunmali"), After.Morale, Before.Morale, 0.01f);
	TestEqual(TEXT("Maas korunmali"), After.Salary, Before.Salary);
	TestTrue(TEXT("Gorev korunmali"), After.Task == Before.Task);
	TestEqual(TEXT("Zamsiz gun sayisi korunmali"), After.DaysSinceRaise, Before.DaysSinceRaise);
	TestEqual(TEXT("Arketip korunmali"), After.Arketip, Before.Arketip);
	TestEqual(TEXT("Hiz korunmali"), After.Hiz, Before.Hiz, 0.001f);
	TestEqual(TEXT("Titizlik korunmali"), After.Titizlik, Before.Titizlik, 0.001f);
	TestEqual(TEXT("Guler yuz korunmali"), After.GulerYuz, Before.GulerYuz, 0.001f);
	TestEqual(TEXT("Transfer teklifi korunmali"), Staff->TransferTeklifi, 189);

	// The one that is not a formality. TaskCounts is what decides the specialism
	// at level 3, so an apprentice who loses it comes back having forgotten what
	// they spent their days doing - and the specialism they earn is then decided
	// by whatever they happen to do next. Nothing in the game reports this.
	for (int32 i = 0; i < (int32)ECigStaffTask::COUNT; ++i)
	{
		TestEqual(
			FString::Printf(TEXT("%d numarali gorevin sayaci korunmali"), i),
			After.TaskCounts[i], Before.TaskCounts[i]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffSpecialismFollowsHistoryTest,
	"Cigkofte.Staff.TheSpecialismFollowsTheWorkTheyActuallyDid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffSpecialismFollowsHistoryTest::RunTest(const FString&)
{
	// Why the field above is worth keeping, said as behaviour rather than as a
	// round-trip: the specialism is a reward for how the player used this person,
	// so it has to come from the work, and the work has to survive a night.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	CigStaffTestsGiveThemAPast(*Staff);

	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);
	Staff->Apprentice = FCigApprentice();
	Shop.GM->ApplySave(*Save);
	Save->RemoveFromRoot();

	// Whatever survived the save, the chopping board has to still be the job
	// this apprentice has done most - that is the whole claim.
	int32 Best = 0;
	for (int32 i = 1; i < (int32)ECigStaffTask::COUNT; ++i)
	{
		if (Staff->Apprentice.TaskCounts[i] > Staff->Apprentice.TaskCounts[Best]) { Best = i; }
	}
	TestTrue(TEXT("Yuklemeden sonra en cok yapilan is dograma olmali"),
		(ECigStaffTask)Best == ECigStaffTask::Dograma
		&& Staff->Apprentice.TaskCounts[Best] > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
