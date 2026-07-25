// The tutorial step table.
//
// The first day is the one part of the game a player cannot come back to, so a
// broken row here is a broken first impression: a step with no wording shows an
// empty hint, and a step pointing at a station that does not exist highlights
// nothing. Neither would crash, which is exactly why they need catching here.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Tutorial; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/CigBalance.h"
#include "Core/CigBalanceTypes.h"
#include "Core/CigText.h"
#include "Core/CigkofteTypes.h"
#include "Quests/CigQuestSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTutorialCoversStepsTest,
	"Cigkofte.Tutorial.TableCoversEveryStep",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTutorialCoversStepsTest::RunTest(const FString& /*Parameters*/)
{
	// Done is a terminal marker rather than a step, so the table stops before it.
	const int32 AdimSayisi = (int32)ECigTutorialStep::Done;
	TestEqual(TEXT("Tablo her öğretici adımını kapsamalı"), CigBalance::TutorialCount(), AdimSayisi);

	for (int32 i = 0; i < AdimSayisi; ++i)
	{
		const FCigTutorialRow& R = CigBalance::Tutorial(i);
		TestFalse(FString::Printf(TEXT("Adım %d anahtarsız olmamalı"), i), R.Key.IsEmpty());
		TestFalse(FString::Printf(TEXT("Adım %d metin anahtarsız olmamalı"), i), R.MetinAnahtari.IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTutorialTextResolvesTest,
	"Cigkofte.Tutorial.EveryStepHasWording",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTutorialTextResolvesTest::RunTest(const FString& /*Parameters*/)
{
	// CigText returns the key itself when it cannot find one, so a step whose
	// key is wrong shows the player something like "tutorial.4" on their first
	// day. Comparing against the key is how that gets caught.
	for (int32 i = 0; i < CigBalance::TutorialCount(); ++i)
	{
		const FCigTutorialRow& R = CigBalance::Tutorial(i);
		const FString Metin = CigText::Get(*R.MetinAnahtari);
		TestNotEqual(FString::Printf(TEXT("Adım %d metni tabloda bulunmalı"), i), Metin, R.MetinAnahtari);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTutorialStationsValidTest,
	"Cigkofte.Tutorial.HighlightedStationsExist",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTutorialStationsValidTest::RunTest(const FString& /*Parameters*/)
{
	const int32 IstasyonSayisi = (int32)ECigStation::YanUrun + 1;
	int32 VurguluAdim = 0;

	for (int32 i = 0; i < CigBalance::TutorialCount(); ++i)
	{
		const int32 Istasyon = CigBalance::Tutorial(i).VurguIstasyon;
		if (Istasyon < 0)
		{
			continue; // deliberately not about a station
		}
		VurguluAdim++;
		TestTrue(FString::Printf(TEXT("Adım %d geçerli bir istasyonu göstermeli"), i),
			Istasyon < IstasyonSayisi);
	}

	// If nothing is highlighted the feature is not doing anything, which would
	// pass every check above while helping nobody.
	TestTrue(TEXT("En az bir adım bir istasyonu vurgulamalı"), VurguluAdim > 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
