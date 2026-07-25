// UCigOrderSystem::ScoreWrap() is a pure function, which makes it ideal to test.
// These cover the perfect order, the wrong-side penalty, the floor on excess
// toppings, and the prep-time thresholds.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Orders; Quit" -unattended -nop4 -nullrhi
//   (mind the unity-build trap: build on its own, against the editor target.)

#include "Misc/AutomationTest.h"
#include "Orders/CigOrderSystem.h"
#include "Core/CigkofteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Builds a flawless wrap matching the spec exactly (no side, no toppings).
	FCigOrderSpec MakePlainSpec()
	{
		FCigOrderSpec S;
		S.Spice = ECigSpice::Orta;
		S.Portion = 1;
		S.ToppingMask = 0;
		S.bWantsAyran = false;
		S.bPacked = false;
		S.Side = ECigSide::Yok;
		return S;
	}

	FCigWrapBuild MakeMatchingBuild(const FCigOrderSpec& S)
	{
		FCigWrapBuild B;
		B.bActive = true;
		B.bWrapped = true;
		B.Portions = S.Portion;
		B.Spice = S.Spice;
		B.ToppingMask = S.ToppingMask;
		B.bAyran = S.bWantsAyran;
		B.bPacked = S.bPacked;
		B.Side = S.Side;
		B.DoughQuality = 100.f;
		return B;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigScorePerfectOrderTest,
	"Cigkofte.Orders.ScoreWrap.PerfectOrderIs100",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigScorePerfectOrderTest::RunTest(const FString& /*Parameters*/)
{
	const FCigOrderSpec Spec = MakePlainSpec();
	const FCigWrapBuild Build = MakeMatchingBuild(Spec);

	const FCigOrderScore Score = UCigOrderSystem::ScoreWrap(Build, Spec, 30.f);

	TestTrue(TEXT("Kusursuz sipariş 100 puan olmalı"), FMath::IsNearlyEqual(Score.Accuracy, 100.f, 0.01f));
	TestEqual(TEXT("Kusursuz siparişte uyarı olmamalı"), Score.Notes.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigScoreForgottenSideTest,
	"Cigkofte.Orders.ScoreWrap.ForgottenSidePenalty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigScoreForgottenSideTest::RunTest(const FString& /*Parameters*/)
{
	FCigOrderSpec Spec = MakePlainSpec();
	Spec.Side = ECigSide::Corba;            // the customer asked for soup
	FCigWrapBuild Build = MakeMatchingBuild(Spec);
	Build.Side = ECigSide::Yok;             // but we forgot it

	const FCigOrderScore Score = UCigOrderSystem::ScoreWrap(Build, Spec, 30.f);

	// Perfect base is 100 (108 with the side, clamped) and a -12 penalty replaces
	// the match. 25+15+30+10+10 + (-12) + 10 = 88.
	TestTrue(TEXT("Unutulan yan ürün cezası 88 vermeli"), FMath::IsNearlyEqual(Score.Accuracy, 88.f, 0.01f));
	TestTrue(TEXT("Uyarı 'unutuldu' içermeli"),
		Score.Notes.ContainsByPredicate([](const FString& N){ return N.Contains(TEXT("unutuldu")); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigScoreExtraToppingsFloorTest,
	"Cigkofte.Orders.ScoreWrap.ExtraToppingsFlooredAtZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigScoreExtraToppingsFloorTest::RunTest(const FString& /*Parameters*/)
{
	FCigOrderSpec Spec = MakePlainSpec();   // wants no toppings at all
	FCigWrapBuild Build = MakeMatchingBuild(Spec);
	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		Build.ToppingMask |= (1 << i);      // 7 excess toppings -> 30 - 7*6 = -12
	}

	const FCigOrderScore Score = UCigOrderSystem::ScoreWrap(Build, Spec, 30.f);

	// The topping component floors at 0 rather than going negative: 25+15+0+10+10+10 = 70.
	TestTrue(TEXT("Fazla garnitür puanı sıfırın altına düşmemeli (toplam 70)"),
		FMath::IsNearlyEqual(Score.Accuracy, 70.f, 0.01f));
	TestTrue(TEXT("Puan 0 ile 100 arasında kalmalı"),
		Score.Accuracy >= 0.f && Score.Accuracy <= 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigScorePrepTimeThresholdsTest,
	"Cigkofte.Orders.ScoreWrap.PrepTimeThresholds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigScorePrepTimeThresholdsTest::RunTest(const FString& /*Parameters*/)
{
	const FCigOrderSpec Spec = MakePlainSpec();
	const FCigWrapBuild Build = MakeMatchingBuild(Spec);

	// Under 45s: full time score -> 100.
	const FCigOrderScore Fast = UCigOrderSystem::ScoreWrap(Build, Spec, 45.f);
	TestTrue(TEXT("45 sn tam puan (100)"), FMath::IsNearlyEqual(Fast.Accuracy, 100.f, 0.01f));

	// At 90s: the time score falls to 0 -> 90.
	const FCigOrderScore Slow = UCigOrderSystem::ScoreWrap(Build, Spec, 90.f);
	TestTrue(TEXT("90 sn süre puanı sıfırlanır (90)"), FMath::IsNearlyEqual(Slow.Accuracy, 90.f, 0.01f));

	// At 91s: a 'too slow' note and no time score -> 90.
	const FCigOrderScore VerySlow = UCigOrderSystem::ScoreWrap(Build, Spec, 91.f);
	TestTrue(TEXT("91 sn süre puanı yok (90)"), FMath::IsNearlyEqual(VerySlow.Accuracy, 90.f, 0.01f));
	TestTrue(TEXT("91 sn 'çok yavaş' uyarısı vermeli"),
		VerySlow.Notes.ContainsByPredicate([](const FString& N){ return N.Contains(TEXT("çok yavaş")); }));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
