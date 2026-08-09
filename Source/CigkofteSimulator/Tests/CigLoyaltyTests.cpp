// What one serve does to a regular.
//
// The arithmetic was inside a private method, reachable only by serving an
// actual customer in an actual shop, so nothing tested it. Moving it out did not
// change a number - what it changed is that the two decisions inside are now
// things somebody can disagree with.

#include "Misc/AutomationTest.h"
#include "Customers/CigLoyalty.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLoyaltyAsymmetryTest,
	"Cigkofte.Loyalty.TrustIsHarderToEarnThanToLose",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLoyaltyAsymmetryTest::RunTest(const FString&)
{
	// One good serve and one bad one, from the same starting trust.
	const FCigLoyaltyOutcome Good = CigLoyalty::AfterServe(95.f, 50.f, 50.f, 0.f, 0);
	const FCigLoyaltyOutcome Bad = CigLoyalty::AfterServe(40.f, 50.f, 50.f, 0.f, 0);

	const float Gained = Good.Trust - 50.f;
	const float Lost = 50.f - Bad.Trust;

	TestTrue(FString::Printf(TEXT("Iyi servis guven kazandirmali (+%.1f)"), Gained), Gained > 0.f);
	TestTrue(FString::Printf(TEXT("Kotu servis guven kaybettirmeli (-%.1f)"), Lost), Lost > 0.f);

	// The claim: one mistake costs more than one success earns. This is a
	// statement about how forgiving the regulars are, and it is deliberate
	// enough to be worth failing on if somebody levels the two.
	TestTrue(FString::Printf(TEXT("Kayip kazanctan buyuk olmali (%.1f > %.1f)"), Lost, Gained),
		Lost > Gained);

	// Trust cannot run away in either direction, which is what stops a long good
	// streak making a regular immune to a disaster.
	const FCigLoyaltyOutcome AtCeiling = CigLoyalty::AfterServe(100.f, 50.f, 100.f, 0.f, 0);
	TestEqual(TEXT("Guven tavani asmamali"), AtCeiling.Trust, 100.f);
	const FCigLoyaltyOutcome AtFloor = CigLoyalty::AfterServe(0.f, 50.f, 0.f, 0.f, 0);
	TestEqual(TEXT("Guven tabanin altina inmemeli"), AtFloor.Trust, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLoyaltyOppositeDirectionsTest,
	"Cigkofte.Loyalty.AServeCanPleaseAndDisappointAtOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLoyaltyOppositeDirectionsTest::RunTest(const FString&)
{
	// Satisfaction pivots at 60, trust turns at 80. Anything between them moves
	// the two the opposite way, and 70 is squarely in the gap.
	const float Start = 50.f;
	const FCigLoyaltyOutcome Middling = CigLoyalty::AfterServe(70.f, Start, Start, 0.f, 0);

	TestTrue(FString::Printf(TEXT("Ortalama servis tatmini artirmali (%.1f)"), Middling.Satisfaction),
		Middling.Satisfaction > Start);
	TestTrue(FString::Printf(TEXT("Ayni servis guveni dusurmeli (%.1f)"), Middling.Trust),
		Middling.Trust < Start);

	// Not a rounding artefact: the whole band between the two thresholds does it.
	for (float Accuracy = CigLoyalty::SatisfactionPivot + 1.f;
		Accuracy < CigLoyalty::TrustThreshold; Accuracy += 1.f)
	{
		const FCigLoyaltyOutcome O = CigLoyalty::AfterServe(Accuracy, Start, Start, 0.f, 0);
		TestTrue(FString::Printf(TEXT("%.0f isabet: tatmin artmali"), Accuracy), O.Satisfaction > Start);
		TestTrue(FString::Printf(TEXT("%.0f isabet: guven dusmeli"), Accuracy), O.Trust < Start);
	}

	// Above the trust threshold they finally agree.
	const FCigLoyaltyOutcome Good = CigLoyalty::AfterServe(CigLoyalty::TrustThreshold, Start, Start, 0.f, 0);
	TestTrue(TEXT("Esikte ikisi de artmali"), Good.Satisfaction > Start && Good.Trust > Start);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLoyaltyMemoryTest,
	"Cigkofte.Loyalty.OnlyTheEndsOfTheRangeAreRememberedOrRewarded",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLoyaltyMemoryTest::RunTest(const FString&)
{
	// The two flags are exclusive by construction, and the middle of the range
	// sets neither - a regular does not remember an ordinary visit.
	const FCigLoyaltyOutcome Perfect = CigLoyalty::AfterServe(CigLoyalty::PerfectAccuracy, 50.f, 50.f, 0.f, 0);
	TestTrue(TEXT("Kusursuz servis bonus vermeli"), Perfect.bPerfectBonus);
	TestFalse(TEXT("Kusursuz servis hata hatirlatmamali"), Perfect.bRemembersMistake);

	const FCigLoyaltyOutcome Disaster = CigLoyalty::AfterServe(0.f, 50.f, 50.f, 0.f, 0);
	TestTrue(TEXT("Felaket servis hatirlanmali"), Disaster.bRemembersMistake);
	TestFalse(TEXT("Felaket servis bonus vermemeli"), Disaster.bPerfectBonus);

	const FCigLoyaltyOutcome Ordinary = CigLoyalty::AfterServe(70.f, 50.f, 50.f, 0.f, 0);
	TestFalse(TEXT("Siradan servis bonus vermemeli"), Ordinary.bPerfectBonus);
	TestFalse(TEXT("Siradan servis hatirlanmamali"), Ordinary.bRemembersMistake);

	// Just under each boundary, because both are one-sided comparisons and an
	// off-by-one here would move a threshold nobody would notice moving.
	TestFalse(TEXT("Esigin bir altinda bonus olmamali"),
		CigLoyalty::AfterServe(CigLoyalty::PerfectAccuracy - 0.1f, 50.f, 50.f, 0.f, 0).bPerfectBonus);
	TestFalse(TEXT("Esikte hata hatirlanmamali"),
		CigLoyalty::AfterServe(CigLoyalty::MistakeAccuracy, 50.f, 50.f, 0.f, 0).bRemembersMistake);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLoyaltyTipTest,
	"Cigkofte.Loyalty.TheAverageTipIsMostlyTheLastTip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLoyaltyTipTest::RunTest(const FString&)
{
	// AvgTip is a running half-and-half, not a mean over visits. After one big
	// tip it jumps halfway there, and a long history counts for no more than the
	// most recent visit. The name overstates it; the behaviour is pinned here so
	// anyone reading the field knows what it actually holds.
	const FCigLoyaltyOutcome First = CigLoyalty::AfterServe(85.f, 50.f, 50.f, 0.f, 100);
	TestEqual(TEXT("Ilk bahsis yariya gitmeli"), First.AvgTip, 50.f);

	const FCigLoyaltyOutcome Second = CigLoyalty::AfterServe(85.f, 50.f, 50.f, First.AvgTip, 100);
	TestEqual(TEXT("Ikinci bahsis kalan yariyi kapatmali"), Second.AvgTip, 75.f);

	// And one stingy visit halves it again, whatever came before.
	const FCigLoyaltyOutcome Stingy = CigLoyalty::AfterServe(85.f, 50.f, 50.f, 80.f, 0);
	TestEqual(TEXT("Bahsissiz ziyaret ortalamayi yariya indirmeli"), Stingy.AvgTip, 40.f);
	return true;
}

#endif
