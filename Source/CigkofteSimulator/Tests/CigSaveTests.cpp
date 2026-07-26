// Whether a save written by an older build still opens.
//
// This is the one piece of the game with no second chance. A wrong balance
// number can be retuned next patch; a migration that drops a field has already
// destroyed somebody's shop by the time anyone notices, and there is nothing to
// restore it from. So the chain is checked step by step rather than trusted.
//
// MigrateSave takes a save and nothing else - no world, no game mode - so all
// of this runs headless in milliseconds.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.SaveMigration; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Save/CigSaveSubsystem.h"
#include "Save/CigSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// A save as an old build would have left it: a shop mid-run, with the
	// fields that existed at every version since v1 already filled in.
	UCigSaveGame* EskiKayit(int32 Surum)
	{
		UCigSaveGame* S = NewObject<UCigSaveGame>();
		S->SaveVersion = Surum;
		S->Day = 9;
		S->Money = 1730;
		S->Level = 4;
		S->XP = 260;
		S->Rep = 71.5f;
		S->TotalServed = 88;
		S->TotalEarned = 9400;
		S->BestDayEarnings = 640;
		return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaveEveryVersionOpensTest,
	"Cigkofte.SaveMigration.EveryVersionReachesCurrent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSaveEveryVersionOpensTest::RunTest(const FString& /*Parameters*/)
{
	// The chain is a row of independent ifs, so a missing link does not fail to
	// compile and does not throw - the save simply arrives half-converted and
	// the version is stamped current anyway. Walking every starting version is
	// the only thing that catches that.
	for (int32 Surum = 1; Surum <= UCigSaveSubsystem::CurrentVersion; ++Surum)
	{
		UCigSaveGame* S = EskiKayit(Surum);
		S->AddToRoot();
		UCigSaveSubsystem::MigrateSave(*S);

		TestEqual(FString::Printf(TEXT("Sürüm %d güncel sürüme taşınmalı"), Surum),
			S->SaveVersion, UCigSaveSubsystem::CurrentVersion);

		// The version stamp on its own proves nothing: it is assigned at the end
		// of MigrateSave whether or not any step ran. The licence date is the
		// one field a conversion writes to a value no default produces, so it is
		// what shows that the chain actually reached the middle from here.
		if (Surum < 10)
		{
			TestEqual(FString::Printf(TEXT("Sürüm %d ruhsat adımından geçmeli"), Surum),
				S->RuhsatBitisGunu, S->Day + 14);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("Sürüm %d ruhsatı yeniden yazılmamalı"), Surum),
				S->RuhsatBitisGunu, 0);
		}

		// Migration converts the schema. It must never spend the player's money
		// or undo their progress.
		TestEqual(FString::Printf(TEXT("Sürüm %d: gün korunmalı"), Surum), S->Day, 9);
		TestEqual(FString::Printf(TEXT("Sürüm %d: para korunmalı"), Surum), S->Money, 1730);
		TestEqual(FString::Printf(TEXT("Sürüm %d: seviye korunmalı"), Surum), S->Level, 4);
		TestEqual(FString::Printf(TEXT("Sürüm %d: servis sayısı korunmalı"), Surum), S->TotalServed, 88);
		TestEqual(FString::Printf(TEXT("Sürüm %d: itibar korunmalı"), Surum), S->Rep, 71.5f, 0.001f);

		S->RemoveFromRoot();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaveIdempotentTest,
	"Cigkofte.SaveMigration.MigratingTwiceChangesNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSaveIdempotentTest::RunTest(const FString& /*Parameters*/)
{
	// A version that was never bumped is a real bug this project has already
	// had: CurrentVersion stayed behind, so every load re-ran the last step on
	// an already-converted save. Running the chain on a current save has to be
	// a no-op, which is what makes that failure visible instead of silent.
	UCigSaveGame* S = EskiKayit(UCigSaveSubsystem::CurrentVersion);
	S->AddToRoot();

	FCigSaveReview R;
	R.Id = 7;
	R.Stars = 5;
	S->Reviews.Add(R);
	S->NextReviewId = 8;
	S->YanitlanacakYorumId = 7;
	S->Takipci = 1200;

	UCigSaveSubsystem::MigrateSave(*S);

	TestEqual(TEXT("Güncel kaydın yorum kimliği değişmemeli"), S->Reviews[0].Id, 7);
	TestEqual(TEXT("Sayaç geri sarılmamalı"), S->NextReviewId, 8);
	TestEqual(TEXT("Bekleyen yanıt düşürülmemeli"), S->YanitlanacakYorumId, 7);
	TestEqual(TEXT("Takipçi sayısı sıfırlanmamalı"), S->Takipci, 1200);

	S->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaveReviewIdsTest,
	"Cigkofte.SaveMigration.OldReviewsGetUsableIds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSaveReviewIdsTest::RunTest(const FString& /*Parameters*/)
{
	UCigSaveGame* S = EskiKayit(11);
	S->AddToRoot();

	// v11 reviews carry no ID at all, so they arrive as a run of zeroes.
	for (int32 i = 0; i < 5; ++i)
	{
		FCigSaveReview R;
		R.Stars = 3;
		S->Reviews.Add(R);
	}

	UCigSaveSubsystem::MigrateSave(*S);

	TSet<int32> Kimlikler;
	for (const FCigSaveReview& R : S->Reviews)
	{
		TestTrue(TEXT("Her yoruma geçerli bir kimlik verilmeli"), R.Id > 0);
		Kimlikler.Add(R.Id);
	}
	TestEqual(TEXT("Kimlikler benzersiz olmalı"), Kimlikler.Num(), S->Reviews.Num());

	// The list is stored newest first, so the newest must hold the highest ID -
	// otherwise a later review would be handed a number an older one already has.
	TestTrue(TEXT("En yeni yorum en büyük kimliği almalı"), S->Reviews[0].Id > S->Reviews.Last().Id);

	// The counter has to sit past everything that exists, or the next review
	// collides with one already on the list.
	for (const FCigSaveReview& R : S->Reviews)
	{
		TestTrue(TEXT("Sayaç mevcut kimliklerin üstünde olmalı"), S->NextReviewId > R.Id);
	}

	// The pending reply was an index into a list that has since been reordered.
	// Dropping it is the honest conversion; pointing it somewhere is a guess.
	TestEqual(TEXT("Eski bekleyen yanıt düşürülmeli"), S->YanitlanacakYorumId, 0);

	S->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaveGuardsTest,
	"Cigkofte.SaveMigration.ConversionsProtectThePlayer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSaveGuardsTest::RunTest(const FString& /*Parameters*/)
{
	// A shop that existed before licences did must not open to a fine for
	// paperwork that did not exist the day before.
	UCigSaveGame* Ruhsatsiz = EskiKayit(9);
	Ruhsatsiz->AddToRoot();
	UCigSaveSubsystem::MigrateSave(*Ruhsatsiz);
	TestTrue(TEXT("Eski kaydın ruhsatı yüklendiği günde geçerli olmalı"),
		Ruhsatsiz->RuhsatBitisGunu > Ruhsatsiz->Day);
	TestEqual(TEXT("Eski kayıt kapalı gün borcuyla açılmamalı"), Ruhsatsiz->KalanKapaliGun, 0);
	TestEqual(TEXT("Eski kayıt başarısız denetimle açılmamalı"), Ruhsatsiz->BasarisizDenetim, 0);
	Ruhsatsiz->RemoveFromRoot();

	// A corrupt UI scale would leave the player with no visible way to fix it,
	// so this clamp is the one guard that has to hold whatever is in the file.
	for (const float Bozuk : { 0.f, -5.f, 99.f })
	{
		UCigSaveGame* S = EskiKayit(3);
		S->AddToRoot();
		S->Settings.UIScaleMult = Bozuk;
		UCigSaveSubsystem::MigrateSave(*S);
		TestTrue(FString::Printf(TEXT("Bozuk arayüz ölçeği (%.1f) okunabilir aralığa çekilmeli"), Bozuk),
			S->Settings.UIScaleMult >= 0.7f && S->Settings.UIScaleMult <= 1.6f);
		S->RemoveFromRoot();
	}

	// A pre-pricing save was played entirely at list price. An empty markup
	// array is what ApplySave reads as "charge list price"; anything else would
	// silently reprice a shop the player never touched.
	UCigSaveGame* Fiyatsiz = EskiKayit(6);
	Fiyatsiz->AddToRoot();
	UCigSaveSubsystem::MigrateSave(*Fiyatsiz);
	TestEqual(TEXT("Fiyatlandırma öncesi kayıt liste fiyatına dönmeli"), Fiyatsiz->UrunCarpanlari.Num(), 0);
	Fiyatsiz->RemoveFromRoot();

	// The v7->v8 step is a documented no-op: every value it writes equals the
	// field default, because a save with no such field loads as the default
	// anyway. The assertion is still worth making - it pins the contract that an
	// apprentice hired before archetypes existed keeps working, which is what
	// would quietly break if those defaults were ever changed to zero.
	UCigSaveGame* Eleman = EskiKayit(7);
	Eleman->AddToRoot();
	UCigSaveSubsystem::MigrateSave(*Eleman);
	TestEqual(TEXT("Eski eleman nötr hızda olmalı"), Eleman->ApprenticeHiz, 1.f, 0.001f);
	TestEqual(TEXT("Eski eleman nötr titizlikte olmalı"), Eleman->ApprenticeTitizlik, 1.f, 0.001f);
	TestEqual(TEXT("Eski eleman nötr güler yüzde olmalı"), Eleman->ApprenticeGulerYuz, 1.f, 0.001f);
	Eleman->RemoveFromRoot();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
