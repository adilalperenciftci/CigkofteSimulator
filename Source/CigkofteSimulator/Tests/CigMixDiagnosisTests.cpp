// Telling the player the mix is wrong while they can still fix it.
//
// The bowl was judged silently: QualityFromBowl added up the ratio errors and
// the answer reached the player at the till, minutes and one customer later, as
// a smaller payment and a worse review. These tests pin the two things that make
// the warning worth having - that it names the worst problem rather than a
// problem, and that it distinguishes a mistake from a loss.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.MixDiagnosis; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Cooking/CigMixDiagnosis.h"
#include "Cooking/CigCookingSystem.h"
#include "Core/CigkofteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// The Klasik recipe's numbers, so the tests read against something real
	// rather than against invented ratios that no recipe uses.
	FCigMixDiagnosis Bak(int32 Bulgur, int32 Su, int32 Salca, int32 Baharat, int32 Isot,
		int32 Capacity = UCigCookingSystem::BowlCapacity)
	{
		const FCigRecipe& R = UCigCookingSystem::Recipe(0);
		int32 Kase[(int32)ECigIngredient::COUNT] = {};
		Kase[(int32)ECigIngredient::Bulgur] = Bulgur;
		Kase[(int32)ECigIngredient::Su] = Su;
		Kase[(int32)ECigIngredient::Salca] = Salca;
		Kase[(int32)ECigIngredient::Baharat] = Baharat;
		Kase[(int32)ECigIngredient::Isot] = Isot;
		const int32 Total = Bulgur + Su + Salca + Baharat + Isot;
		return CigMix::Diagnose(Kase, R.RatioSu, R.RatioSalca, R.RatioBaharat,
			R.IsotMinFrac, R.IsotMaxFrac, Total, Capacity);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigMixOnTargetSaysNothing,
	"Cigkofte.MixDiagnosis.AGoodMixIsNotWarnedAbout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigMixOnTargetSaysNothing::RunTest(const FString&)
{
	// The counts the HUD asks for, which land a few percent off the exact ratios.
	// Warning about that would teach the player to ignore the warning, which is
	// the failure mode that makes a warning worse than none.
	const FCigRecipe& R = UCigCookingSystem::Recipe(0);
	int32 Hedef[(int32)ECigIngredient::COUNT];
	UCigCookingSystem::TargetCounts(R, Hedef);

	const FCigMixDiagnosis D = Bak(
		Hedef[(int32)ECigIngredient::Bulgur], Hedef[(int32)ECigIngredient::Su],
		Hedef[(int32)ECigIngredient::Salca], Hedef[(int32)ECigIngredient::Baharat],
		Hedef[(int32)ECigIngredient::Isot]);

	TestFalse(TEXT("Hedefi tutturan karışım uyarı almamalı"), D.IsProblem());

	// An empty bowl is not a fault. It is where every batch starts.
	TestFalse(TEXT("Boş kase sorun sayılmamalı"), Bak(0, 0, 0, 0, 0).IsProblem());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigMixNamesTheWorstProblem,
	"Cigkofte.MixDiagnosis.TheLoudestFaultIsTheOneNamed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigMixNamesTheWorstProblem::RunTest(const FString&)
{
	// Water missing entirely, everything else on target.
	const FCigMixDiagnosis Susuz = Bak(5, 0, 2, 1, 1);
	TestTrue(TEXT("Susuz karışım uyarı vermeli"), Susuz.IsProblem());
	TestEqual(TEXT("Eksik su adıyla bildirilmeli"), Susuz.Problem, ECigMixProblem::TooLittleSu);

	// Paste at four times the target, water fine.
	const FCigMixDiagnosis Salcali = Bak(5, 3, 8, 1, 1);
	TestEqual(TEXT("Fazla salça adıyla bildirilmeli"), Salcali.Problem, ECigMixProblem::TooMuchSalca);

	// Isot is measured against the whole bowl rather than against bulgur,
	// because heat is a property of the batch. A bowl half isot is the loudest
	// thing in it whatever the other ratios do.
	const FCigMixDiagnosis Acili = Bak(5, 3, 2, 1, 9);
	TestEqual(TEXT("Fazla isot adıyla bildirilmeli"), Acili.Problem, ECigMixProblem::TooMuchIsot);

	// Something in the bowl and no bulgur to measure it against is its own
	// problem, and a different one from a wrong ratio.
	const FCigMixDiagnosis Bulgursuz = Bak(0, 3, 2, 1, 1);
	TestEqual(TEXT("Bulgursuz kase ayrı bildirilmeli"), Bulgursuz.Problem, ECigMixProblem::NoBulgur);

	// Severity rises with the error, so a caller can decide how loudly to say it.
	TestTrue(TEXT("Daha kötü karışım daha yüksek şiddet vermeli"),
		Bak(5, 0, 2, 1, 1).Severity > Bak(5, 2, 2, 1, 1).Severity);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigMixKnowsWhenItIsTooLate,
	"Cigkofte.MixDiagnosis.AFullBowlCannotBeDilutedBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigMixKnowsWhenItIsTooLate::RunTest(const FString&)
{
	// The distinction the warning exists for. Too much isot in a bowl with room
	// is one handful of bulgur away from correct; the same bowl full has nowhere
	// to dilute into, and the honest advice is to dump it.
	const FCigMixDiagnosis Kurtarilir = Bak(5, 3, 2, 1, 7, /*Capacity=*/30);
	TestTrue(TEXT("Yeri olan kase sorunlu olmalı"), Kurtarilir.IsProblem());
	TestTrue(TEXT("Yeri olan kase düzeltilebilir olmalı"), Kurtarilir.bFixableByAdding);

	const FCigMixDiagnosis Dolu = Bak(5, 3, 2, 1, 7, /*Capacity=*/18);
	TestTrue(TEXT("Dolu kase de sorunlu olmalı"), Dolu.IsProblem());
	TestFalse(TEXT("Dolu kase eklemeyle düzeltilemez"), Dolu.bFixableByAdding);

	// Same bowl, same fault, different advice - which is the whole point.
	TestEqual(TEXT("İki durumda da sorun aynı olmalı"), Kurtarilir.Problem, Dolu.Problem);

	// Every problem has wording. A warning with no text is worse than none: it
	// takes up a HUD row and says nothing.
	for (int32 i = 1; i < (int32)ECigMixProblem::COUNT; ++i)
	{
		const TCHAR* Key = CigMix::TextKey((ECigMixProblem)i);
		TestTrue(FString::Printf(TEXT("Sorun %d metin anahtarı taşımalı"), i),
			Key != nullptr && FCString::Strlen(Key) > 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
