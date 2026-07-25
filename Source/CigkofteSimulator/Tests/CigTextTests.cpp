// The UI text table: does every key have both languages, and does the text
// actually change when the language does.
//
// The real job here is catching a key whose translation was forgotten and which
// silently stays Turkish. Someone playing in English must not see a mixed screen.

#include "Misc/AutomationTest.h"
#include "Core/CigText.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTextTableLoadsTest,
	"Cigkofte.Text.TableLoads",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTextTableLoadsTest::RunTest(const FString& /*Parameters*/)
{
	if (CigText::KeyCount() == 0)
	{
		AddError(TEXT("Metin tablosu boş — Config/Text/Strings.csv okunamadı."));
		return false;
	}
	TestEqual(TEXT("İki dil desteklenmeli"), CigText::LanguageCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTextSwitchesLanguageTest,
	"Cigkofte.Text.SwitchingLanguageChangesText",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTextSwitchesLanguageTest::RunTest(const FString& /*Parameters*/)
{
	const int32 Original = CigText::GetLanguage();

	CigText::SetLanguage(0);
	const FString Tr = CigText::Get(TEXT("settings.title"));
	CigText::SetLanguage(1);
	const FString En = CigText::Get(TEXT("settings.title"));

	TestEqual(TEXT("Türkçe başlık"), Tr, FString(TEXT("AYARLAR")));
	TestEqual(TEXT("İngilizce başlık"), En, FString(TEXT("SETTINGS")));
	TestNotEqual(TEXT("Dil değişince metin de değişmeli"), Tr, En);

	CigText::SetLanguage(Original);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTextMissingKeyIsVisibleTest,
	"Cigkofte.Text.MissingKeyIsVisible",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTextMissingKeyIsVisibleTest::RunTest(const FString& /*Parameters*/)
{
	// If a missing key returned an empty string nothing would appear on screen
	// and the mistake would go unnoticed; returning the key makes it visible.
	AddExpectedError(TEXT("Metin anahtarı bulunamadı"), EAutomationExpectedErrorFlags::Contains, 1);
	const FString Missing = CigText::Get(TEXT("boyle.bir.anahtar.yok"));
	TestEqual(TEXT("Eksik anahtar kendini döndürmeli"), Missing, FString(TEXT("boyle.bir.anahtar.yok")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTextQuotedFieldTest,
	"Cigkofte.Text.QuotedFieldsSurviveParsing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTextQuotedFieldTest::RunTest(const FString& /*Parameters*/)
{
	// An unquoted comma inside a CSV field shifts the columns: half the sentence
	// moves into the next column and that language loses its real translation.
	// Both columns still look populated, so the "missing translation" check never
	// fires. Tools/check_sources.py catches it in the table; the job here is to
	// prove UE's FCsvParser actually honours the quotes - Python doing so is no
	// evidence that it does.
	const int32 Original = CigText::GetLanguage();

	for (int32 Lang = 0; Lang < 2; ++Lang)
	{
		CigText::SetLanguage(Lang);
		const FString S = CigText::Format(TEXT("msg.order.portionadded"), 2, TEXT("acılı"));

		TestTrue(FString::Printf(TEXT("dil %d: virgülden sonrası korunmalı"), Lang),
			S.Contains(TEXT(",")) && S.EndsWith(TEXT(").")));
		TestFalse(FString::Printf(TEXT("dil %d: tırnak metne sızmamalı"), Lang),
			S.Contains(TEXT("\"")));
		TestFalse(FString::Printf(TEXT("dil %d: çözülmemiş yer tutucu kalmamalı"), Lang),
			S.Contains(TEXT("{")));
	}

	CigText::SetLanguage(Original);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
