// The cat, which the audit called decorative.
//
// It is not. Petting grants reputation, feeding spends money, and a happy cat
// hands the player two points of reputation every morning. The system touches
// the two currencies the whole game is scored in, and until now nothing tested
// it - which is exactly the shape of thing where an exploit lives quietly.
//
// The cooldown is the piece that matters. Pet() adds a point of reputation and
// returns early when PetCooldown is running; delete that guard and reputation
// becomes free and unbounded for anyone willing to hold a key down. Nothing else
// in the game caps it.

#include "Misc/AutomationTest.h"
#include "Cat/CigCatSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Tests/CigTestShop.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCatPetCooldownTest,
	"Cigkofte.Cat.PettingOnCooldownIsNotFreeReputation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCatPetCooldownTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCatSystem* Cat = Shop.GM->CatSys.Get();
	UCigProgressionSystem* Prog = Shop.GM->Progression.Get();
	if (!Cat || !Prog) { AddError(TEXT("Sistemler yok.")); return false; }

	Cat->PetCooldown = 0.f;
	Cat->Attention = 10.f;
	const float RepBefore = Prog->Rep;

	Cat->Pet();
	TestTrue(TEXT("Ilk oksama itibar vermeli"), Prog->Rep > RepBefore);
	TestTrue(TEXT("Ilk oksama ilgi artirmali"), Cat->Attention > 10.f);
	TestTrue(TEXT("Oksama bekleme suresi baslatmali"), Cat->PetCooldown > 0.f);

	// The guard. Without it a player holding the key gets unlimited reputation,
	// and nothing else in the game caps it.
	const float RepAfterFirst = Prog->Rep;
	const float AttentionAfterFirst = Cat->Attention;
	for (int32 i = 0; i < 20; ++i)
	{
		Cat->Pet();
	}
	TestEqual(TEXT("Bekleme suresinde oksamak itibar vermemeli"), Prog->Rep, RepAfterFirst);
	TestEqual(TEXT("Bekleme suresinde oksamak ilgi vermemeli"), Cat->Attention, AttentionAfterFirst);

	// And it comes back when the wait is over, so the cooldown is a delay rather
	// than a one-off.
	Cat->PetCooldown = 0.f;
	Cat->Pet();
	TestTrue(TEXT("Bekleme bitince oksama yeniden itibar vermeli"), Prog->Rep > RepAfterFirst);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCatFeedingTest,
	"Cigkofte.Cat.FeedingNeverTakesMoneyWithoutFeeding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCatFeedingTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCatSystem* Cat = Shop.GM->CatSys.Get();
	UCigEconomySystem* Eco = Shop.GM->Economy.Get();
	if (!Cat || !Eco) { AddError(TEXT("Sistemler yok.")); return false; }

	// A full cat refuses, and the refusal is free. Charging for a meal the cat
	// will not eat is the kind of small theft nobody reports and everybody
	// notices.
	Eco->Money = 1000;
	Cat->Food = 95.f;
	Cat->Feed();
	TestEqual(TEXT("Tok kediyi beslemek para almamali"), Eco->Money, 1000);
	TestEqual(TEXT("Tok kedinin doygunlugu degismemeli"), Cat->Food, 95.f);

	// A hungry cat with money: the transaction happens once and fills it.
	Cat->Food = 10.f;
	Cat->Feed();
	TestEqual(TEXT("Ac kediyi beslemek 20 TL almali"), Eco->Money, 980);
	TestEqual(TEXT("Beslenen kedi doymalidir"), Cat->Food, 100.f);

	// And with no money nothing happens at all - not a free meal, not a debt.
	Eco->Money = 5;
	Cat->Food = 10.f;
	Cat->Feed();
	TestEqual(TEXT("Parasizken besleme parayi eksiye dusurmemeli"), Eco->Money, 5);
	TestEqual(TEXT("Parasizken kedi beslenmemeli"), Cat->Food, 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCatMorningBonusTest,
	"Cigkofte.Cat.TheMorningBonusHasAThresholdAndBothSidesOfIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCatMorningBonusTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCatSystem* Cat = Shop.GM->CatSys.Get();
	UCigProgressionSystem* Prog = Shop.GM->Progression.Get();
	if (!Cat || !Prog) { AddError(TEXT("Sistemler yok.")); return false; }

	// Happiness is the average of the two needs, so either one alone can carry
	// the cat over the line - a fed but ignored cat scores the same as a petted
	// but starving one.
	Cat->Food = 100.f;
	Cat->Attention = 40.f;
	TestEqual(TEXT("Mutluluk iki ihtiyacin ortalamasi olmali"), Cat->Happiness(), 70.f);

	// Exactly on the threshold pays, because the check is >= and a cat kept
	// precisely at the line should not be a coin toss.
	float Rep = Prog->Rep;
	Cat->OnDayStart(2);
	TestTrue(TEXT("Esikteki kedi sabah bonusu vermeli"), Prog->Rep > Rep);

	// Just under does not.
	Cat->Attention = 39.f;
	Rep = Prog->Rep;
	Cat->OnDayStart(3);
	TestEqual(TEXT("Esigin altindaki kedi bonus vermemeli"), Prog->Rep, Rep);

	// A miserable cat costs nothing either - it complains, it does not fine you.
	Cat->Food = 0.f;
	Cat->Attention = 0.f;
	Rep = Prog->Rep;
	Cat->OnDayStart(4);
	TestEqual(TEXT("Mutsuz kedi itibar goturmemeli"), Prog->Rep, Rep);
	return true;
}

#endif
