// The rivals, on their own terms.
//
// The August audit listed this system as having no test that names it. It is
// exercised indirectly - pricing has five tests and the rivals feed it - but
// nothing here has ever been asked what it does, and the thing it does turns out
// to be worth writing down.
//
// PlayerPullMult divides the rivals' strength by how many are open. That is an
// average, not a total, and averages behave in a way totals do not: closing the
// weakest shop on the street raises the average of the ones left. The player is
// punished for winning. Whether that is the intended reading of "the surviving
// competition is tougher" or an accident of the arithmetic is a design question
// - what is not in doubt is that nobody could see it, because no test said it.

#include "Misc/AutomationTest.h"
#include "Economy/CigRivalSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Tests/CigTestShop.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigRival MakeRival(const TCHAR* Name, float Popularity, bool bOpen = true)
	{
		FCigRival R;
		R.Name = Name;
		R.Popularity = Popularity;
		R.AdPower = 0.f;
		R.bOpen = bOpen;
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRivalPullRangeTest,
	"Cigkofte.Rivals.ThePullStaysInsideItsStatedRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRivalPullRangeTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigRivalSystem* Rivals = Shop.GM->Rivals.Get();
	if (!Rivals) { AddError(TEXT("Rakip sistemi yok.")); return false; }

	// Every rival open and enormous: the player's share of attention goes to
	// nothing, so the pull sits on its floor.
	Rivals->Rivals.Reset();
	Rivals->Rivals.Add(MakeRival(TEXT("A"), 100000.f));
	const float Crushed = Rivals->PlayerPullMult();
	TestTrue(FString::Printf(TEXT("Ezilen oyuncunun cekimi tabanda olmali (%.3f)"), Crushed),
		Crushed >= 0.7f && Crushed < 0.75f);

	// Nobody left to compete: the rival score is zero, so the pull reaches its
	// ceiling. This is the only way to get there - with any rival open the ratio
	// is strictly below one.
	Rivals->Rivals.Reset();
	Rivals->Rivals.Add(MakeRival(TEXT("Kapali"), 60.f, /*bOpen=*/false));
	const float Alone = Rivals->PlayerPullMult();
	TestTrue(FString::Printf(TEXT("Rakipsiz cekim tavanda olmali (%.3f)"), Alone),
		FMath::IsNearlyEqual(Alone, 1.3f, 0.001f));

	TestEqual(TEXT("Kapali rakip acik sayilmamali"), Rivals->OpenRivalCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRivalAverageTest,
	"Cigkofte.Rivals.ClosingTheWeakestRivalMakesThingsWorseForThePlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRivalAverageTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigRivalSystem* Rivals = Shop.GM->Rivals.Get();
	if (!Rivals) { AddError(TEXT("Rakip sistemi yok.")); return false; }

	// Three shops, one clearly failing.
	Rivals->Rivals.Reset();
	Rivals->Rivals.Add(MakeRival(TEXT("Guclu"), 80.f));
	Rivals->Rivals.Add(MakeRival(TEXT("Orta"), 50.f));
	Rivals->Rivals.Add(MakeRival(TEXT("Zayif"), 20.f));
	const float WithThree = Rivals->PlayerPullMult();

	// The weakest one goes under - the outcome the player is working towards.
	Rivals->Rivals[2].bOpen = false;
	const float WithoutWeakest = Rivals->PlayerPullMult();

	TestEqual(TEXT("Iki rakip acik kalmali"), Rivals->OpenRivalCount(), 2);

	// And the player is worse off. PlayerPullMult averages rival strength rather
	// than summing it, so removing the shop that was dragging the average down
	// leaves a tougher street behind. This is pinned rather than fixed: whether
	// it reads as "the survivors are the strong ones" or as punishing the player
	// for winning is a design call, and the point of the test is that the
	// behaviour is now visible to whoever makes it.
	TestTrue(FString::Printf(TEXT("Zayif rakip kapaninca cekim dusmeli (%.4f -> %.4f)"),
		WithThree, WithoutWeakest),
		WithoutWeakest < WithThree);

	// The opposite case, for contrast: losing the strongest helps, which is what
	// makes the rule an average rather than a bug in one direction.
	Rivals->Rivals[2].bOpen = true;
	Rivals->Rivals[0].bOpen = false;
	const float WithoutStrongest = Rivals->PlayerPullMult();
	TestTrue(FString::Printf(TEXT("Guclu rakip kapaninca cekim artmali (%.4f -> %.4f)"),
		WithThree, WithoutStrongest),
		WithoutStrongest > WithThree);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRivalReputationTest,
	"Cigkofte.Rivals.AReputationTheStreetCannotMatchWinsAttentionBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRivalReputationTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigRivalSystem* Rivals = Shop.GM->Rivals.Get();
	UCigProgressionSystem* Prog = Shop.GM->Progression.Get();
	if (!Rivals || !Prog) { AddError(TEXT("Sistemler yok.")); return false; }

	Rivals->Rivals.Reset();
	Rivals->Rivals.Add(MakeRival(TEXT("Rakip"), 60.f));

	Prog->Rep = 10.f;
	const float Poor = Rivals->PlayerPullMult();

	Prog->Rep = 200.f;
	const float Strong = Rivals->PlayerPullMult();

	// The one thing the player controls in this equation. If reputation did not
	// move the pull, the whole rival simulation would be weather rather than a
	// system the player is inside.
	TestTrue(FString::Printf(TEXT("Itibar cekimi artirmali (%.4f -> %.4f)"), Poor, Strong),
		Strong > Poor);
	TestTrue(TEXT("Cekim tavani asmamali"), Strong <= 1.3f);
	TestTrue(TEXT("Cekim tabanin altina inmemeli"), Poor >= 0.7f);
	return true;
}

#endif
