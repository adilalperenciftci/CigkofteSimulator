// How an inspection is scored.
//
// DenetimPuani is the rule the fine, the strike and the closure all hang off,
// and it is pure, so it can be checked without a council. The part worth
// pinning down is that the licence is not just another deduction: a spotless
// counter must not be able to carry an unlicensed shop over the pass mark.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Inspection; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Economy/CigInspectionSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigInspectionRangeTest,
	"Cigkofte.Inspection.ScoreSpansTheRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigInspectionRangeTest::RunTest(const FString& /*Parameters*/)
{
	TestEqual(TEXT("Kusursuz dükkân 100 almalı"),
		UCigInspectionSystem::DenetimPuani(100.f, 100.f, true), 100.f, 0.01f);
	TestEqual(TEXT("Her şeyi kötü dükkân 0 almalı"),
		UCigInspectionSystem::DenetimPuani(0.f, 0.f, false), 0.f, 0.01f);

	// Out-of-range inputs must not push the score outside 0-100.
	const float Tasan = UCigInspectionSystem::DenetimPuani(150.f, 150.f, true);
	TestTrue(TEXT("Puan 100'ü aşmamalı"), Tasan <= 100.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigInspectionLicenceTest,
	"Cigkofte.Inspection.LicenceCannotBeOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigInspectionLicenceTest::RunTest(const FString& /*Parameters*/)
{
	const float Ruhsatli = UCigInspectionSystem::DenetimPuani(100.f, 100.f, true);
	const float Ruhsatsiz = UCigInspectionSystem::DenetimPuani(100.f, 100.f, false);

	TestTrue(TEXT("Ruhsatsızlık puanı düşürmeli"), Ruhsatsiz < Ruhsatli);

	// A perfect shop without a licence still has to fail: the whole point of the
	// paperwork is that it cannot be scrubbed away with a cloth.
	TestTrue(TEXT("Ruhsatsız kusursuz dükkân bile geçer notun altında kalmalı"),
		Ruhsatsiz < 60.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigInspectionWeightTest,
	"Cigkofte.Inspection.HygieneWeighsMostOfWhatIsLeft",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigInspectionWeightTest::RunTest(const FString& /*Parameters*/)
{
	// Losing hygiene has to cost more than losing freshness, or the player is
	// being asked to clean for no reason.
	const float KirliAmaTaze = UCigInspectionSystem::DenetimPuani(0.f, 100.f, true);
	const float TemizAmaBayat = UCigInspectionSystem::DenetimPuani(100.f, 0.f, true);
	TestTrue(TEXT("Hijyen kaybı tazelik kaybından daha çok puan götürmeli"),
		KirliAmaTaze < TemizAmaBayat);

	// A dirty shop with an expired licence must be worse than either alone.
	TestTrue(TEXT("İki eksik birden en düşük puanı vermeli"),
		UCigInspectionSystem::DenetimPuani(0.f, 100.f, false) < KirliAmaTaze);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
