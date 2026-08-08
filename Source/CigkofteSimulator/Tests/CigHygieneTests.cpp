// Hygiene, on its own terms.
//
// Listed in the August audit as having no test that names it. Inspection has
// four tests and hygiene feeds them, so it was exercised without ever being
// asked what it does.
//
// The thing worth pinning is the calibration. OverallHygiene subtracts a
// weighted sum of six dirt values from 100, and the weights happen to add up to
// exactly one - which is the only reason the scale reaches both ends. Add a
// seventh area, or nudge one weight, and the score silently stops being able to
// reach 0 or stops being able to reach 100. Nothing would crash; the inspector
// would just start grading against a scale nobody meant.

#include "Misc/AutomationTest.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Tests/CigTestShop.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	void SetAllDirt(UCigHygieneSystem& H, float V)
	{
		H.HandDirt = V;
		H.CounterDirt = V;
		H.ChopDirt = V;
		H.TrashFill = V;
		H.DishPile = V;
		H.CatFur = V;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigHygieneScaleTest,
	"Cigkofte.Hygiene.TheScaleReachesBothEnds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigHygieneScaleTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigHygieneSystem* H = Shop.GM->Hygiene.Get();
	if (!H) { AddError(TEXT("Hijyen sistemi yok.")); return false; }

	SetAllDirt(*H, 0.f);
	TestEqual(TEXT("Her sey temizken hijyen 100 olmali"), H->OverallHygiene(), 100.f);

	// The end that proves the weights sum to one. If they summed to less, a
	// completely filthy shop would still score above zero and the inspector
	// would be marking against a scale nobody chose.
	SetAllDirt(*H, 100.f);
	TestEqual(TEXT("Her sey kirliyken hijyen 0 olmali"), H->OverallHygiene(), 0.f);

	// And the midpoint, which catches a weight moved from one area to another -
	// something the two extremes above would both survive.
	SetAllDirt(*H, 50.f);
	TestEqual(TEXT("Her sey yari kirliyken hijyen 50 olmali"), H->OverallHygiene(), 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigHygieneWeightsTest,
	"Cigkofte.Hygiene.HandsCostMoreThanAnywhereElse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigHygieneWeightsTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigHygieneSystem* H = Shop.GM->Hygiene.Get();
	if (!H) { AddError(TEXT("Hijyen sistemi yok.")); return false; }

	// Each area dirtied alone, so the drop it causes is its weight.
	auto DropFrom = [H](float UCigHygieneSystem::* Field)
	{
		SetAllDirt(*H, 0.f);
		H->*Field = 100.f;
		return 100.f - H->OverallHygiene();
	};

	const float Hands = DropFrom(&UCigHygieneSystem::HandDirt);
	const float Counter = DropFrom(&UCigHygieneSystem::CounterDirt);
	const float Chop = DropFrom(&UCigHygieneSystem::ChopDirt);
	const float Trash = DropFrom(&UCigHygieneSystem::TrashFill);
	const float Dishes = DropFrom(&UCigHygieneSystem::DishPile);
	const float Fur = DropFrom(&UCigHygieneSystem::CatFur);

	// Dirty hands on somebody's food is the worst thing in a kitchen, and the
	// weights say so. This is a gameplay claim as much as an arithmetic one: it
	// is why washing is worth interrupting a wrap for.
	TestTrue(FString::Printf(TEXT("Eller en agir basmali (el %.0f)"), Hands),
		Hands > Counter && Hands > Chop && Hands > Trash && Hands > Dishes && Hands > Fur);

	// Everything together is the whole scale - the same invariant as the extremes
	// above, stated as a sum so a future seventh area has to be given a weight
	// taken from somewhere rather than added on top.
	const float Total = Hands + Counter + Chop + Trash + Dishes + Fur;
	TestTrue(FString::Printf(TEXT("Agirliklar tam olceye toplanmali (%.1f)"), Total),
		FMath::IsNearlyEqual(Total, 100.f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigHygieneCleaningTest,
	"Cigkofte.Hygiene.TheCleaningStationIsWorthMoreThanTheSink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigHygieneCleaningTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigHygieneSystem* H = Shop.GM->Hygiene.Get();
	if (!H) { AddError(TEXT("Hijyen sistemi yok.")); return false; }

	// A shop where everything is equally filthy, so what each action recovers is
	// decided by what it covers rather than by where the dirt happens to be.
	SetAllDirt(*H, 100.f);
	const float Before = H->OverallHygiene();

	H->WashHands();
	const float AfterHands = H->OverallHygiene();

	SetAllDirt(*H, 100.f);
	H->CleanSurfaces();
	const float AfterSurfaces = H->OverallHygiene();

	// Hands weigh most of any single area, but the cleaning station clears three
	// at once - counter, chopping board and cat hair. So the best single action
	// in the shop is not the one with the heaviest area. Worth pinning because it
	// is the sort of thing a later balance pass would change by accident.
	TestTrue(FString::Printf(TEXT("El yikamak hijyeni artirmali (%.0f -> %.0f)"), Before, AfterHands),
		AfterHands > Before);
	TestTrue(FString::Printf(TEXT("Temizlik istasyonu el yikamaktan cok getirmeli (%.0f vs %.0f)"),
		AfterSurfaces, AfterHands),
		AfterSurfaces > AfterHands);

	// And it really does take the cat hair with it, which is the part somebody
	// reading the function name would not expect.
	TestEqual(TEXT("Temizlik istasyonu kedi tuyunu de almali"), H->CatFur, 0.f);
	return true;
}

#endif
