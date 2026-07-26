// The rules every sale is priced by.
//
// These used to exist twice: once at the counter and once, differently, in the
// apprentice's hands. The pure parts of the pipeline are checked here so the
// shape of the price - what raises it, what floors it, where it stops - is
// pinned down in one place and cannot drift back apart.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Sale; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Economy/CigSaleSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaleQualityTest,
	"Cigkofte.Sale.QualityAndAccuracyBothMatter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSaleQualityTest::RunTest(const FString& /*Parameters*/)
{
	const float Kusursuz = UCigSaleSystem::KaliteAccuracyCarpani(100.f, 100.f);
	TestEqual(TEXT("Kusursuz dürüm liste fiyatını almalı"), Kusursuz, 1.f, 0.001f);

	// Neither term may be ignored: dropping one has to cost money on its own.
	TestTrue(TEXT("Kalite düşünce fiyat düşmeli"),
		UCigSaleSystem::KaliteAccuracyCarpani(50.f, 100.f) < Kusursuz);
	TestTrue(TEXT("Accuracy düşünce fiyat düşmeli"),
		UCigSaleSystem::KaliteAccuracyCarpani(100.f, 50.f) < Kusursuz);

	// But neither may zero it either. A botched wrap is still sold; the cost of
	// getting it wrong is reputation, not a free meal.
	TestTrue(TEXT("En kötü dürüm bile para etmeli"),
		UCigSaleSystem::KaliteAccuracyCarpani(0.f, 0.f) > 0.f);

	// Corrupt inputs must not invent money.
	TestTrue(TEXT("Aşırı kalite fiyatı uçurmamalı"),
		UCigSaleSystem::KaliteAccuracyCarpani(1000.f, 100.f) <= 1.2f);
	TestTrue(TEXT("Aşırı accuracy fiyatı uçurmamalı"),
		UCigSaleSystem::KaliteAccuracyCarpani(100.f, 1000.f) <= 1.f);
	TestTrue(TEXT("Negatif accuracy fiyatı eksiye düşürmemeli"),
		UCigSaleSystem::KaliteAccuracyCarpani(100.f, -50.f) > 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaleComboTest,
	"Cigkofte.Sale.ComboRewardsStreaksAndStops",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSaleComboTest::RunTest(const FString& /*Parameters*/)
{
	// The first perfect wrap opens the streak rather than paying for it.
	TestEqual(TEXT("Kombo 0 liste fiyatı"), UCigSaleSystem::KomboCarpani(0), 1.f, 0.001f);
	TestEqual(TEXT("Kombo 1 hâlâ liste fiyatı"), UCigSaleSystem::KomboCarpani(1), 1.f, 0.001f);

	TestTrue(TEXT("İkinci kusursuz dürüm ödüllenmeli"), UCigSaleSystem::KomboCarpani(2) > 1.f);
	TestTrue(TEXT("Seri uzadıkça artmalı"),
		UCigSaleSystem::KomboCarpani(4) > UCigSaleSystem::KomboCarpani(3));

	// Uncapped, a long streak would eventually dwarf every other income source.
	const float Tavan = UCigSaleSystem::KomboCarpani(6);
	TestEqual(TEXT("Kombo bir yerde durmalı"), UCigSaleSystem::KomboCarpani(50), Tavan, 0.001f);
	TestTrue(TEXT("Tavan makul olmalı"), Tavan <= 1.5f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
