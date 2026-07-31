// When a station's name is on screen.
//
// The rule had one threshold, and the check runs on a 0.25 s clock. A player
// moving sideways at roughly that distance - which is what walking the length of
// the shop does to the far row of counters - lands on either side of the line on
// successive samples, and the sign blinks. Standing still is stable, which is why
// a screenshot pass could never catch it.
//
// The property these tests are really about is the middle one: inside the band,
// the answer is whatever it already was. That sentence is the fix.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.LabelVisibility; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// The shipped thresholds, squared, as the caller passes them.
	constexpr float ShowSq = UCigWorldBuilder::LabelShowRange * UCigWorldBuilder::LabelShowRange;
	constexpr float HideSq = UCigWorldBuilder::LabelHideRange * UCigWorldBuilder::LabelHideRange;

	float Sq(float Distance) { return Distance * Distance; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLabelBandHoldsItsAnswer,
	"Cigkofte.LabelVisibility.InsideTheBandNothingChanges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLabelBandHoldsItsAnswer::RunTest(const FString&)
{
	// Halfway between the two thresholds: the sample that used to decide the
	// question on its own now defers to what was already true.
	const float Middle = Sq((UCigWorldBuilder::LabelShowRange + UCigWorldBuilder::LabelHideRange) * 0.5f);

	TestTrue(TEXT("Görünürken bantta görünür kalmalı"),
		UCigWorldBuilder::LabelShouldShow(true, Middle, ShowSq, HideSq));
	TestFalse(TEXT("Gizliyken bantta gizli kalmalı"),
		UCigWorldBuilder::LabelShouldShow(false, Middle, ShowSq, HideSq));

	// The whole band, not just its centre.
	for (float D = UCigWorldBuilder::LabelShowRange + 1.f; D <= UCigWorldBuilder::LabelHideRange; D += 5.f)
	{
		TestTrue(FString::Printf(TEXT("%.0f: görünür kalmalı"), D),
			UCigWorldBuilder::LabelShouldShow(true, Sq(D), ShowSq, HideSq));
		TestFalse(FString::Printf(TEXT("%.0f: gizli kalmalı"), D),
			UCigWorldBuilder::LabelShouldShow(false, Sq(D), ShowSq, HideSq));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLabelSwitchesOnce,
	"Cigkofte.LabelVisibility.ApproachingAndLeavingEachSwitchOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLabelSwitchesOnce::RunTest(const FString&)
{
	// Walking in from well outside. One transition, and it happens at the near
	// threshold - a player who keeps walking towards a counter must not see the
	// name appear, vanish and appear again.
	bool bVisible = false;
	int32 Transitions = 0;
	float TurnedOnAt = 0.f;
	for (float D = 1200.f; D >= 0.f; D -= 1.f)
	{
		const bool bNext = UCigWorldBuilder::LabelShouldShow(bVisible, Sq(D), ShowSq, HideSq);
		if (bNext != bVisible)
		{
			++Transitions;
			TurnedOnAt = D;
		}
		bVisible = bNext;
	}
	TestEqual(TEXT("Yaklaşırken tek geçiş"), Transitions, 1);
	TestTrue(TEXT("Yakın eşikte açılmalı"),
		FMath::IsNearlyEqual(TurnedOnAt, UCigWorldBuilder::LabelShowRange, 2.f));
	TestTrue(TEXT("Tezgâhın başında görünür"), bVisible);

	// And back out. One transition, at the far threshold.
	Transitions = 0;
	float TurnedOffAt = 0.f;
	for (float D = 0.f; D <= 1200.f; D += 1.f)
	{
		const bool bNext = UCigWorldBuilder::LabelShouldShow(bVisible, Sq(D), ShowSq, HideSq);
		if (bNext != bVisible)
		{
			++Transitions;
			TurnedOffAt = D;
		}
		bVisible = bNext;
	}
	TestEqual(TEXT("Uzaklaşırken tek geçiş"), Transitions, 1);
	TestTrue(TEXT("Uzak eşikte kapanmalı"),
		FMath::IsNearlyEqual(TurnedOffAt, UCigWorldBuilder::LabelHideRange, 2.f));
	TestFalse(TEXT("Uzakta gizli"), bVisible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLabelNoBandIsTheOldBehaviour,
	"Cigkofte.LabelVisibility.EqualThresholdsReproduceTheOldFlicker",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLabelNoBandIsTheOldBehaviour::RunTest(const FString&)
{
	// Collapsing the band gives back exactly the rule that flickered, which is
	// what makes this suite evidence that the band is the only difference.
	const float Single = Sq(700.f);

	TestTrue(TEXT("Eşikte açık"), UCigWorldBuilder::LabelShouldShow(false, Sq(699.f), Single, Single));
	TestFalse(TEXT("Eşiğin bir birim ötesinde kapalı"),
		UCigWorldBuilder::LabelShouldShow(true, Sq(701.f), Single, Single));

	// Two samples a metre apart either side of the line - a player moving
	// sideways at that radius - flip the answer with no band and hold it with one.
	TestNotEqual(TEXT("Bantsız: iki örnek farklı cevap verir"),
		UCigWorldBuilder::LabelShouldShow(true, Sq(699.f), Single, Single),
		UCigWorldBuilder::LabelShouldShow(true, Sq(701.f), Single, Single));
	TestEqual(TEXT("Bantlı: aynı iki örnek aynı cevabı verir"),
		UCigWorldBuilder::LabelShouldShow(true, Sq(699.f), ShowSq, HideSq),
		UCigWorldBuilder::LabelShouldShow(true, Sq(701.f), ShowSq, HideSq));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
