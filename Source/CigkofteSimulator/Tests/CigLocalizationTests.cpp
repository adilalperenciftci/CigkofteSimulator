// Does the language setting actually reach the strings the player reads?
//
// It did not. The setting was honoured by everything routed through CigText and
// silently ignored by 46 strings written with LOCTEXT - a mechanism this project
// deliberately does not use, and for which no .po or .locres data has ever
// existed, so those strings were permanently Turkish in both languages. Another
// group never reached the text system at all: the reputation title under the money
// in the corner of the screen, every recipe and supplier name, and each "SEVIYE N"
// sign in the world were hardcoded literals.
//
// Cigkofte.Text already checks that the table loads, that switching languages
// changes text and that quoted fields survive parsing. What was missing is the
// check that could have caught the above: take the strings the player actually
// reads, in both languages, and insist the language chose them.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Localization; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/CigText.h"
#include "Cooking/CigCookingSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Core/CigkofteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// A key that resolves to itself is a key with no row in the table.
	void PinsResolve(FAutomationTestBase& Test, const TArray<FString>& Keys)
	{
		const int32 Original = CigText::GetLanguage();
		for (int32 Lang = 0; Lang < CigText::LanguageCount(); ++Lang)
		{
			CigText::SetLanguage(Lang);
			for (const FString& Key : Keys)
			{
				const FString Value = CigText::Get(*Key);
				Test.TestNotEqual(FString::Printf(TEXT("dil %d: '%s' tabloda olmalı"), Lang, *Key),
					Value, Key);
				Test.TestFalse(FString::Printf(TEXT("dil %d: '%s' boş olmamalı"), Lang, *Key),
					Value.IsEmpty());
			}
		}
		CigText::SetLanguage(Original);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLocKeysExist,
	"Cigkofte.Localization.EveryStringThatWasHardcodedNowHasARow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLocKeysExist::RunTest(const FString&)
{
	TArray<FString> Keys = {
		// Always on screen.
		TEXT("rep.title.legend"), TEXT("rep.title.city"), TEXT("rep.title.pride"),
		TEXT("rep.title.trader"), TEXT("rep.title.rookie"), TEXT("rep.title.unknown"),
		// The world's locked-station signs and the level-up feedback.
		TEXT("level.locked"), TEXT("level.up"), TEXT("level.float"),
		TEXT("level.unlock.more"),
		// The tablet rows that were LOCTEXT.
		TEXT("tablet.licence"), TEXT("tablet.licence.valid"), TEXT("tablet.licence.expired"),
		TEXT("tablet.bribe"), TEXT("tablet.bribe.amount"), TEXT("tablet.shopclosed"),
		TEXT("tablet.price.rivalavg"), TEXT("tablet.price.income"), TEXT("tablet.price.row"),
		TEXT("tablet.price.hint"), TEXT("tablet.levelshort"),
		TEXT("tablet.social.header"), TEXT("tablet.social.promo"), TEXT("tablet.social.campaign"),
		TEXT("tablet.social.defend"), TEXT("tablet.social.apologise"), TEXT("tablet.social.ignore"),
		TEXT("tablet.bulk.header"), TEXT("tablet.bulk.offer"), TEXT("tablet.bulk.accept"),
		TEXT("tablet.bulk.reject"), TEXT("tablet.bulk.progress"),
		TEXT("tablet.staff.skills"), TEXT("tablet.staff.unpaid"), TEXT("tablet.staff.transfer"),
		TEXT("tablet.candidate.wage"), TEXT("tablet.candidate.skills"),
		TEXT("tablet.pricechanged"),
		// Reviews.
		TEXT("review.dirty"), TEXT("review.queue"), TEXT("review.critic.author"),
	};
	for (int32 i = 1; i <= 4; ++i)
	{
		Keys.Add(FString::Printf(TEXT("review.good.%d"), i));
		Keys.Add(FString::Printf(TEXT("review.mid.%d"), i));
		Keys.Add(FString::Printf(TEXT("review.bad.%d"), i));
	}
	for (int32 i = 1; i <= 3; ++i)
	{
		Keys.Add(FString::Printf(TEXT("review.expensive.%d"), i));
		Keys.Add(FString::Printf(TEXT("review.cheap.%d"), i));
	}
	for (int32 i = 2; i <= 7; ++i)
	{
		Keys.Add(FString::Printf(TEXT("level.unlock.%d"), i));
	}

	PinsResolve(*this, Keys);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLocRecipesAndSuppliersFollowLanguage,
	"Cigkofte.Localization.RecipeAndSupplierNamesFollowTheSetting",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLocRecipesAndSuppliersFollowLanguage::RunTest(const FString&)
{
	const int32 Original = CigText::GetLanguage();

	// Every recipe and supplier must have both a name and a blurb in both
	// languages, and none of them may come back as a raw key. The accessors fall
	// back to the table's Turkish literal when a row is missing, so a forgotten
	// translation shows up here as "the English name equals the Turkish one".
	for (int32 i = 0; i < CigRecipeCount; ++i)
	{
		CigText::SetLanguage(0);
		const FString TrName = UCigCookingSystem::RecipeName(i);
		const FString TrDesc = UCigCookingSystem::RecipeDesc(i);
		CigText::SetLanguage(1);
		const FString EnName = UCigCookingSystem::RecipeName(i);
		const FString EnDesc = UCigCookingSystem::RecipeDesc(i);

		TestFalse(FString::Printf(TEXT("Tarif %d adı boş olmamalı"), i), TrName.IsEmpty() || EnName.IsEmpty());
		TestFalse(FString::Printf(TEXT("Tarif %d açıklaması boş olmamalı"), i), TrDesc.IsEmpty() || EnDesc.IsEmpty());
		TestFalse(FString::Printf(TEXT("Tarif %d adı ham anahtar olmamalı"), i), EnName.StartsWith(TEXT("recipe.")));
		TestNotEqual(FString::Printf(TEXT("Tarif %d açıklaması çevrilmiş olmalı"), i), TrDesc, EnDesc);
	}

	for (int32 i = 0; i < CigSupplierCount; ++i)
	{
		CigText::SetLanguage(0);
		const FString TrName = UCigEconomySystem::SupplierName(i);
		CigText::SetLanguage(1);
		const FString EnName = UCigEconomySystem::SupplierName(i);

		TestFalse(FString::Printf(TEXT("Tedarikçi %d adı boş olmamalı"), i), TrName.IsEmpty() || EnName.IsEmpty());
		TestFalse(FString::Printf(TEXT("Tedarikçi %d adı ham anahtar olmamalı"), i), EnName.StartsWith(TEXT("supplier.")));
		TestNotEqual(FString::Printf(TEXT("Tedarikçi %d adı çevrilmiş olmalı"), i), TrName, EnName);
	}

	// "Premium" is the same word in both languages, and that is the correct answer
	// rather than a missing translation - so the name comparison above is left to
	// the suppliers, where every one of the five does differ. Recipes are checked
	// on their blurbs for the same reason.
	CigText::SetLanguage(1);
	TestEqual(TEXT("Ekonomik tarifi İngilizcede Budget olmalı"),
		UCigCookingSystem::RecipeName(1), FString(TEXT("Budget")));

	CigText::SetLanguage(Original);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLocOrderedArgumentsCanReorder,
	"Cigkofte.Localization.ATemplateMayPutItsArgumentsInADifferentOrder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLocOrderedArgumentsCanReorder::RunTest(const FString&)
{
	const int32 Original = CigText::GetLanguage();

	// The bulk order row is the reason CigText::Format takes ordered placeholders
	// instead of printf specifiers: Turkish leads with the day and English with the
	// count. A %d template cannot express that, and would have silently produced
	// "6 wraps by day 10" from the same call.
	CigText::SetLanguage(0);
	const FString Tr = CigText::Format(TEXT("tablet.bulk.offer"), 6, 10);
	CigText::SetLanguage(1);
	const FString En = CigText::Format(TEXT("tablet.bulk.offer"), 6, 10);

	TestTrue(TEXT("Türkçe gün ile başlamalı"), Tr.StartsWith(TEXT("6.")));
	TestTrue(TEXT("İngilizce adet ile başlamalı"), En.StartsWith(TEXT("10 ")));
	TestTrue(TEXT("İngilizce 6. günü söylemeli"), En.Contains(TEXT("day 6")));
	TestFalse(TEXT("Çözülmemiş yer tutucu kalmamalı"), Tr.Contains(TEXT("{")) || En.Contains(TEXT("{")));

	CigText::SetLanguage(Original);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLocNoLoctextLeftBehind,
	"Cigkofte.Localization.NothingClaimsToBeLocalisedWithoutBeing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigLocNoLoctextLeftBehind::RunTest(const FString&)
{
	// The failure this whole slice came from: a string that looks localised, is
	// wrapped in the engine's localisation macro, and has no translation data
	// anywhere. There is no runtime way to enumerate those, so the check lives in
	// Tools/check_sources.py, which fails the build on a LOCTEXT in Source. What is
	// verifiable here is that the strings that used to be LOCTEXT now answer to the
	// language setting - which is what the two tests above do - and that the table
	// grew rather than the strings simply being deleted.
	TestTrue(FString::Printf(TEXT("Metin tablosu yeterince büyük olmalı (%d)"), CigText::KeyCount()),
		CigText::KeyCount() >= 800);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
