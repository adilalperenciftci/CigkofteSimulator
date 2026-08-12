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
#include "Economy/CigEconomySystem.h"
#include "Game/CigDaySystem.h"
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

	// The two things standing between the player and an apprentice: a shop that
	// has grown to level 3, and 400 lira. Set them, then hire the way the tablet
	// button does. Nothing here writes to Apprentice - if this returns true, the
	// production hiring path is what put them there.
	bool CigStaffLifeHireSomebody(FCigTestShop& Shop, int32 Money = 5000)
	{
		Shop.GM->Progression->Level = 3;
		Shop.GM->Economy->Money = Money;
		Shop.GM->Staff->Hire();
		return Shop.GM->Staff->Apprentice.bHired;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffHireNeedsLevelTest,
	"Cigkofte.Staff.TheJobIsNotOpenUntilTheShopHasGrown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffHireNeedsLevelTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	// Money enough twice over, so the only thing refusing is the level.
	Shop.GM->Progression->Level = 2;
	Shop.GM->Economy->Money = 5000;

	Staff->Hire();

	TestFalse(TEXT("Seviye 3'un altinda kimse ise alinmamali"), Staff->Apprentice.bHired);
	TestEqual(TEXT("Reddedilen ise alim para harcamamali"), Shop.GM->Economy->Money, 5000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffHireNeedsMoneyTest,
	"Cigkofte.Staff.NobodyIsHiredOnAnEmptyTill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffHireNeedsMoneyTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	// One lira short of the 400 the hire costs, which is the whole difference
	// between this test and the one that succeeds.
	Shop.GM->Progression->Level = 3;
	Shop.GM->Economy->Money = 399;

	Staff->HireAday(0);

	TestFalse(TEXT("Para yetmediginde kimse ise alinmamali"), Staff->Apprentice.bHired);
	TestEqual(TEXT("Basarisiz ise alim kasadan para almamali"), Shop.GM->Economy->Money, 399);

	// And the people are still at the door. Not being able to afford somebody
	// today is not the same as them giving up on you.
	TestTrue(TEXT("Karsilanamayan ise alim adaylari dagitmamali"), Staff->Adaylar.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffOnlyOneJobTest,
	"Cigkofte.Staff.TheJobIsOnlyOpenOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffOnlyOneJobTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ilk ise alim basarisiz; testin geri kalani anlamsiz."));
		return false;
	}

	const FString Ilk = Staff->Apprentice.Name;
	const int32 IlkMaas = Staff->Apprentice.Salary;
	const int32 ParaSonra = Shop.GM->Economy->Money;

	// A second press of the same button. There is one job and somebody has it,
	// so the person who has it must not be replaced or charged for again.
	Staff->Hire();

	TestTrue(TEXT("Zaten calisan biri varken hala ise alinmis olmali"), Staff->Apprentice.bHired);
	TestEqual(TEXT("Ikinci ise alim calisani degistirmemeli"), Staff->Apprentice.Name, Ilk);
	TestEqual(TEXT("Ikinci ise alim maasi degistirmemeli"), Staff->Apprentice.Salary, IlkMaas);
	TestEqual(TEXT("Ikinci ise alim ucret almamali"), Shop.GM->Economy->Money, ParaSonra);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffPoolWalksAwayTest,
	"Cigkofte.Staff.TheRestOfThePoolWalksAwayOnceSomebodyIsHired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffPoolWalksAwayTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	// Choosing one of three has to mean something, so the other two leave. If
	// they lingered the player could hire, sack and rehire the same three faces
	// forever, and the choice would cost nothing.
	TestEqual(TEXT("Ise alim sonrasi aday havuzu bosalmali"), Staff->Adaylar.Num(), 0);
	TestEqual(TEXT("Ise alim 400 lira almali"), Shop.GM->Economy->Money, 4600);
	TestTrue(TEXT("Ise alinan kisinin bir adi olmali"), !Staff->Apprentice.Name.IsEmpty());
	TestEqual(TEXT("Yeni calisan birinci seviyeden baslamali"), Staff->Apprentice.Level, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffWageLeavesTillTest,
	"Cigkofte.Staff.TheWageComesOutOfTheTill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffWageLeavesTillTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	const int32 Maas = Staff->Apprentice.Salary;
	Shop.GM->Economy->Money = 5000;

	// The staff system's own day boundary rather than the whole day's, because
	// what is being measured is one number. A real EndDay also takes the rent
	// and lets every other system touch the till, and then this assertion would
	// be about all of them. That the day boundary reaches staff at all is what
	// TheDayBoundaryIsWhatPaysThem checks.
	Staff->OnDayEnd(1);

	TestEqual(TEXT("Gun sonunda maas kasadan cikmali"), Shop.GM->Economy->Money, 5000 - Maas);
	TestEqual(TEXT("Odenen gun odenmemis sayilmamali"), Staff->Apprentice.OdenmemisGun, 0);
	TestTrue(TEXT("Maasi odenen calisan isten ayrilmamali"), Staff->Apprentice.bHired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffUnpaidDayTest,
	"Cigkofte.Staff.AnUnpaidDayCostsMoraleRatherThanBeingFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffUnpaidDayTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	// One lira short of the wage. The interesting case is not that the payment
	// fails but that failing has a price: labour the shop could not pay for
	// must not quietly turn out to have been free.
	Shop.GM->Economy->Money = Staff->Apprentice.Salary - 1;
	const int32 Once = Shop.GM->Economy->Money;
	Staff->Apprentice.Morale = 70.f;
	Staff->Apprentice.Energy = 100.f;

	Staff->OnDayEnd(1);

	TestEqual(TEXT("Odenemeyen maas kasayi eksiye dusurmemeli"), Shop.GM->Economy->Money, Once);
	TestEqual(TEXT("Odenmeyen gun sayilmali"), Staff->Apprentice.OdenmemisGun, 1);

	// 70 - 18 for the unpaid day, + 4 for a day that did not exhaust them.
	TestEqual(TEXT("Odenmeyen gun moral goturmeli"), Staff->Apprentice.Morale, 56.f, 0.01f);
	TestTrue(TEXT("Tek odenmeyen gun isten ayrilmaya yetmemeli"), Staff->Apprentice.bHired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffPaidByDayBoundaryTest,
	"Cigkofte.Staff.TheDayBoundaryIsWhatPaysThem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffPaidByDayBoundaryTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	// An empty till, then the whole real day boundary: rent takes it below zero
	// and the wage cannot be met. OdenmemisGun is the assertion because nothing
	// else in the game writes it, so it going up proves EndDay reached the staff
	// system rather than proving anything about arithmetic.
	Shop.GM->Economy->Money = 0;
	Staff->Apprentice.Morale = 70.f;
	Staff->Apprentice.Energy = 100.f;

	Shop.GM->Days->EndDay();

	TestEqual(TEXT("Gercek gun sonu maasi denemeli"), Staff->Apprentice.OdenmemisGun, 1);
	TestTrue(TEXT("Odenmeyen gun morali dusurmeli"), Staff->Apprentice.Morale < 70.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffQuitsOnLowMoraleTest,
	"Cigkofte.Staff.AnApprenticeWhoIsMiserableEnoughLeaves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffQuitsOnLowMoraleTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	// Paid on time, rested, and still miserable - so the only thing that can
	// end this is the morale itself. 10 climbs to 14 over the night and 14 is
	// still under the line.
	Shop.GM->Economy->Money = 5000;
	Staff->Apprentice.Morale = 10.f;
	Staff->Apprentice.Energy = 100.f;

	Staff->OnDayEnd(1);

	TestFalse(TEXT("Morali dibe vuran calisan isten ayrilmali"), Staff->Apprentice.bHired);
	TestTrue(TEXT("Ayrilan calisanin adi kalmamali"), Staff->Apprentice.Name.IsEmpty());

	// And the job is open again the same night, not the next time somebody
	// remembers to refresh the pool.
	TestTrue(TEXT("Ayrilistan sonra yeni adaylar gelmeli"), Staff->Adaylar.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffAsksForARaiseTest,
	"Cigkofte.Staff.TheyStartAskingForARaiseAfterFiveDays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffAsksForARaiseTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	// Paid every night and never worked into the ground, so the only thing the
	// clock is measuring is how long they have gone without a raise.
	Shop.GM->Economy->Money = 100000;
	Staff->Apprentice.Morale = 70.f;

	for (int32 Gun = 1; Gun <= 4; ++Gun)
	{
		Staff->Apprentice.Energy = 100.f;
		Staff->OnDayEnd(Gun);
		TestFalse(FString::Printf(TEXT("%d. gunde henuz zam istenmemeli"), Gun),
			Staff->Apprentice.bWantsRaise);
	}

	Staff->Apprentice.Energy = 100.f;
	Staff->OnDayEnd(5);
	TestTrue(TEXT("Besinci gunun sonunda zam istenmeli"), Staff->Apprentice.bWantsRaise);

	// Asking is not free for them either: an unanswered request costs morale
	// every night it stays unanswered, which is what turns it into a decision
	// rather than a notification.
	// +4 for a day that did not exhaust them, -6 for the request nobody answered:
	// a shop that ignores them ends the night two points worse than it started.
	const float Bekleyen = Staff->Apprentice.Morale;
	Staff->Apprentice.Energy = 100.f;
	Staff->OnDayEnd(6);
	TestEqual(TEXT("Cevapsiz zam talebi moral goturmeli"),
		Staff->Apprentice.Morale, Bekleyen - 2.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffRaiseTest,
	"Cigkofte.Staff.ARaiseOnlyLandsWhenItWasAskedFor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffRaiseTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	const int32 Maas = Staff->Apprentice.Salary;
	Staff->Apprentice.Morale = 60.f;

	// Nobody has asked for anything, so the button has nothing to grant. A raise
	// the player can hand out at will is a wage they would never have to defend.
	Staff->GiveRaise();
	TestEqual(TEXT("Istenmeyen zam maasi degistirmemeli"), Staff->Apprentice.Salary, Maas);
	TestEqual(TEXT("Istenmeyen zam moral eklememeli"), Staff->Apprentice.Morale, 60.f, 0.01f);

	Staff->Apprentice.bWantsRaise = true;
	Staff->Apprentice.DaysSinceRaise = 7;

	Staff->GiveRaise();
	TestEqual(TEXT("Kabul edilen zam maasi 50 artirmali"), Staff->Apprentice.Salary, Maas + 50);
	TestEqual(TEXT("Kabul edilen zam moral kazandirmali"), Staff->Apprentice.Morale, 90.f, 0.01f);
	TestFalse(TEXT("Zam verilince talep kapanmali"), Staff->Apprentice.bWantsRaise);
	TestEqual(TEXT("Zam sonrasi sayac sifirlanmali"), Staff->Apprentice.DaysSinceRaise, 0);

	// And the same press again is not a second raise.
	Staff->GiveRaise();
	TestEqual(TEXT("Ikinci basis maasi tekrar artirmamali"), Staff->Apprentice.Salary, Maas + 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffCounterOfferTest,
	"Cigkofte.Staff.MatchingTheRivalsOfferAnswersItExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffCounterOfferTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	// Level 1 so no fresh offer can be rolled at the end of the night; a rival
	// does not approach somebody who has not grown into anything yet. What is
	// being tested is the answer to one offer, not how offers arrive.
	Staff->Apprentice.Level = 1;
	Staff->Apprentice.Morale = 50.f;
	Staff->Apprentice.Energy = 100.f;
	Staff->TransferTeklifi = 250;

	Staff->KarsiTeklifVer();

	TestEqual(TEXT("Karsi teklif maasi rakibin teklifine cikarmali"), Staff->Apprentice.Salary, 250);
	TestEqual(TEXT("Cevaplanan teklif ortadan kalkmali"), Staff->TransferTeklifi, 0);
	TestEqual(TEXT("Kalinmasi moral kazandirmali"), Staff->Apprentice.Morale, 75.f, 0.01f);

	// Pressing it again must not be a second raise. The offer was answered; the
	// button no longer has anything to answer.
	Staff->KarsiTeklifVer();
	TestEqual(TEXT("Ikinci karsi teklif maasi tekrar artirmamali"), Staff->Apprentice.Salary, 250);
	TestEqual(TEXT("Ikinci karsi teklif moral eklememeli"), Staff->Apprentice.Morale, 75.f, 0.01f);

	// And having matched it, the night passes without losing them.
	Shop.GM->Economy->Money = 5000;
	Staff->OnDayEnd(1);

	TestTrue(TEXT("Teklifi karsilanan calisan gitmemeli"), Staff->Apprentice.bHired);
	TestEqual(TEXT("Yeni maas gun sonunda kasadan cikmali"), Shop.GM->Economy->Money, 4750);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffUnansweredOfferTest,
	"Cigkofte.Staff.AnUnansweredOfferTakesThemAway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStaffUnansweredOfferTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigStaffSystem* Staff = Shop.GM->Staff.Get();
	if (!Staff) { AddError(TEXT("Personel sistemi yok.")); return false; }

	if (!CigStaffLifeHireSomebody(Shop))
	{
		AddError(TEXT("Ise alim basarisiz."));
		return false;
	}

	// Paid, rested and content: everything that could otherwise end the job is
	// deliberately healthy, so if they are gone in the morning the standing
	// offer is the only thing that can have taken them.
	Shop.GM->Economy->Money = 5000;
	Staff->Apprentice.Morale = 80.f;
	Staff->Apprentice.Energy = 100.f;
	Staff->TransferTeklifi = 250;

	// Read now, because in a moment there will be no apprentice to read it from.
	const int32 Maas = Staff->Apprentice.Salary;

	Staff->OnDayEnd(1);

	TestFalse(TEXT("Cevapsiz birakilan teklif calisani goturmeli"), Staff->Apprentice.bHired);
	TestEqual(TEXT("Calisan gidince teklif de kalkmali"), Staff->TransferTeklifi, 0);
	TestTrue(TEXT("Bosalan is icin yeni adaylar gelmeli"), Staff->Adaylar.Num() > 0);

	// The wage for the day they did work still left the till - they were not
	// unpaid, they were outbid.
	TestEqual(TEXT("Gittikleri gunun maasi yine de odenmeli"), Shop.GM->Economy->Money, 5000 - Maas);
	return true;
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
