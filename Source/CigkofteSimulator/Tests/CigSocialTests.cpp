// The follower curve behind the social system.
//
// TakipciCarpani is what turns an audience into footfall, and it is the one
// place where a runaway number could quietly break the game: followers only
// ever go up, so a linear curve would end with a shop that never has a quiet
// day again. What matters here is that it grows, that it slows, and that it
// stops.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Social; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Economy/CigSocialSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSocialGrowthTest,
	"Cigkofte.Social.FollowersHelpButSlowDown",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSocialGrowthTest::RunTest(const FString& /*Parameters*/)
{
	TestEqual(TEXT("Takipçisiz dükkân çarpan almamalı"),
		UCigSocialSystem::TakipciCarpani(0), 1.f, 0.001f);

	const float Yuz = UCigSocialSystem::TakipciCarpani(100);
	const float Bin = UCigSocialSystem::TakipciCarpani(1000);
	const float OnBin = UCigSocialSystem::TakipciCarpani(10000);

	TestTrue(TEXT("Takipçi arttıkça çarpan artmalı"), Yuz < Bin && Bin < OnBin);

	// Each tenfold jump has to be worth roughly the same, or the first hundred
	// followers stop mattering the moment the count gets large.
	const float IlkAtlama = Bin - Yuz;
	const float IkinciAtlama = OnBin - Bin;
	TestTrue(TEXT("Onar katlık atlamalar birbirine yakın olmalı"),
		FMath::Abs(IlkAtlama - IkinciAtlama) < IlkAtlama * 0.6f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSocialCeilingTest,
	"Cigkofte.Social.CurveHasACeiling",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSocialCeilingTest::RunTest(const FString& /*Parameters*/)
{
	// Followers never go down on their own, so without a ceiling a long game
	// ends in permanent free customers.
	const float Devasa = UCigSocialSystem::TakipciCarpani(50000000);
	TestTrue(TEXT("Çarpan tavanı aşmamalı"), Devasa <= 1.8f);
	TestTrue(TEXT("Çarpan hiçbir zaman 1'in altına düşmemeli"),
		UCigSocialSystem::TakipciCarpani(1) >= 1.f);

	// A nonsensical negative count must not invert the multiplier.
	TestEqual(TEXT("Negatif takipçi nötr çarpan vermeli"),
		UCigSocialSystem::TakipciCarpani(-500), 1.f, 0.001f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
