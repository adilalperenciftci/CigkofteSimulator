// What a shop may be called.
//
// Pure: no world, no widget, no save file. The rules are the whole of this file
// because the interesting cases are the ones a player will actually type - a name
// with a Turkish character in it, a name that is only spaces, a name pasted out
// of a chat window with a newline on the end.

#include "Misc/AutomationTest.h"
#include "Game/CigShopIdentity.h"
#include "Save/CigSaveGame.h"
#include "Save/CigSaveSubsystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameAcceptsRealNamesTest,
	"Cigkofte.ShopIdentity.Name.RealNamesAreAccepted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigShopNameAcceptsRealNamesTest::RunTest(const FString&)
{
	const TCHAR* Names[] = {
		TEXT("Cigkofteci Ali"),
		// The characters this game is written in. If any of these are refused the
		// shop cannot be named in its own language.
		TEXT("ÇİĞKÖFTECİ ŞÜKRÜ"),
		TEXT("Öz Urfa Çiğköfte"),
		TEXT("Ali's Cig Kofte"),
		TEXT("Cig  Kofte"),
		TEXT("A")
	};
	for (const TCHAR* Name : Names)
	{
		TestTrue(FString::Printf(TEXT("'%s' kabul edilmeli"), Name),
			CigShopIdentity::Validate(Name) == ECigShopNameFault::None);
		// Inner spacing is the player's business. "Cig  Kofte" is a name somebody
		// chose and normalising it away would be rewriting it.
		TestEqual(FString::Printf(TEXT("'%s' aynen korunmali"), Name),
			CigShopIdentity::Resolve(Name), FString(Name));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameTrimsEndsTest,
	"Cigkofte.ShopIdentity.Name.SurroundingSpaceIsTrimmedRatherThanRefused",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigShopNameTrimsEndsTest::RunTest(const FString&)
{
	// Typing a space before a name is a slip, not a decision.
	TestEqual(TEXT("Bastaki ve sondaki bosluk kirpilmali"),
		CigShopIdentity::Resolve(TEXT("  Cigkofteci Ali  ")), FString(TEXT("Cigkofteci Ali")));
	TestTrue(TEXT("Kirpilmis ad gecerli olmali"),
		CigShopIdentity::Validate(TEXT("  Cigkofteci Ali  ")) == ECigShopNameFault::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameEmptyTest,
	"Cigkofte.ShopIdentity.Name.AnEmptyOrBlankNameIsRefused",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigShopNameEmptyTest::RunTest(const FString&)
{
	TestTrue(TEXT("Bos ad reddedilmeli"),
		CigShopIdentity::Validate(TEXT("")) == ECigShopNameFault::Empty);
	// A board with nothing on it is a missing sign rather than a minimal one.
	TestTrue(TEXT("Yalniz bosluktan olusan ad reddedilmeli"),
		CigShopIdentity::Validate(TEXT("    ")) == ECigShopNameFault::Empty);

	// And the fallback is the shop's own default, not an empty board.
	TestEqual(TEXT("Bos ad varsayilana dusmeli"),
		CigShopIdentity::Resolve(TEXT("   ")), CigShopIdentity::DefaultName());
	TestFalse(TEXT("Varsayilan ad bos olamaz"), CigShopIdentity::DefaultName().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameLengthTest,
	"Cigkofte.ShopIdentity.Name.TheLengthBoundaryIsExact",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigShopNameLengthTest::RunTest(const FString&)
{
	const FString AtLimit = FString::ChrN(CigShopIdentity::MaxNameLength, TEXT('A'));
	const FString OverLimit = FString::ChrN(CigShopIdentity::MaxNameLength + 1, TEXT('A'));

	// On the boundary rather than near it: an off-by-one here is a name the
	// player typed and the game silently replaced.
	TestTrue(TEXT("Sinirdaki ad kabul edilmeli"),
		CigShopIdentity::Validate(AtLimit) == ECigShopNameFault::None);
	TestTrue(TEXT("Sinirin bir uzerindeki ad reddedilmeli"),
		CigShopIdentity::Validate(OverLimit) == ECigShopNameFault::TooLong);

	// Length is measured after trimming, so trailing spaces cannot push a
	// perfectly good name over the edge.
	TestTrue(TEXT("Sondaki bosluk adi uzun yapmamali"),
		CigShopIdentity::Validate(AtLimit + TEXT("   ")) == ECigShopNameFault::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameControlCharTest,
	"Cigkofte.ShopIdentity.Name.ControlCharactersAndLineBreaksAreRefused",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigShopNameControlCharTest::RunTest(const FString&)
{
	// A newline in a name is a sign with half a word on it and a HUD field that
	// grows a row. Pasted names carry them.
	const TCHAR* Bad[] = {
		TEXT("Cig\nKofte"),
		TEXT("Cig\r\nKofte"),
		TEXT("Cig\tKofte"),
		TEXT("Cig\x01Kofte")
	};
	for (const TCHAR* Name : Bad)
	{
		TestTrue(TEXT("Kontrol karakterli ad reddedilmeli"),
			CigShopIdentity::Validate(Name) == ECigShopNameFault::InvalidCharacter);
		TestEqual(TEXT("Kontrol karakterli ad varsayilana dusmeli"),
			CigShopIdentity::Resolve(Name), CigShopIdentity::DefaultName());
	}

	// A trailing newline is whitespace and is trimmed rather than refused: the
	// name itself is fine and the player did not type the newline on purpose.
	TestTrue(TEXT("Sondaki satir sonu kirpilmali"),
		CigShopIdentity::Validate(TEXT("Cigkofteci Ali\n")) == ECigShopNameFault::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameFaultOrderTest,
	"Cigkofte.ShopIdentity.Name.ANameThatIsBothIsReportedAsTheSpecificFault",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigShopNameFaultOrderTest::RunTest(const FString&)
{
	// Too long and containing a newline. "Too long" would send the player to
	// shorten a name whose real problem is the paste they made; the character
	// fault is the one that tells them something they can act on.
	const FString Both = FString::ChrN(CigShopIdentity::MaxNameLength + 5, TEXT('A')) + TEXT("\x01");
	TestTrue(TEXT("Hem uzun hem gecersiz ad karakter hatasi vermeli"),
		CigShopIdentity::Validate(Both) == ECigShopNameFault::InvalidCharacter);
	return true;
}

// ----------------------------------------------------- in a shop, and in a save

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopRenameTest,
	"Cigkofte.ShopIdentity.Shop.RenamingChangesTheBoardAndARefusalChangesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigShopRenameTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->WorldBuilder)
	{
		AddError(TEXT("Dunya kurucusu yok."));
		return false;
	}
	Shop.GM->WorldBuilder->BuildWorld();

	// An unnamed shop shows the default rather than an empty board.
	TestEqual(TEXT("Adsiz dukkan varsayilani gostermeli"),
		Shop.GM->WorldBuilder->ShopDisplayName(), CigShopIdentity::DefaultName());

	TestTrue(TEXT("Gecerli ad kabul edilmeli"),
		Shop.GM->WorldBuilder->SetShopName(TEXT("ÇİĞKÖFTECİ ŞÜKRÜ")) == ECigShopNameFault::None);
	TestEqual(TEXT("Tabela yeni adi gostermeli"),
		Shop.GM->WorldBuilder->ShopDisplayName(), FString(TEXT("ÇİĞKÖFTECİ ŞÜKRÜ")));

	// A refused rename must not blank a sign that was fine a moment ago.
	TestTrue(TEXT("Bos ad reddedilmeli"),
		Shop.GM->WorldBuilder->SetShopName(TEXT("   ")) == ECigShopNameFault::Empty);
	TestEqual(TEXT("Reddedilen yeniden adlandirma adi degistirmemeli"),
		Shop.GM->WorldBuilder->ShopDisplayName(), FString(TEXT("ÇİĞKÖFTECİ ŞÜKRÜ")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameRoundTripTest,
	"Cigkofte.ShopIdentity.Shop.TheNameSurvivesSaveAndLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigShopNameRoundTripTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	Shop.GM->WorldBuilder->BuildWorld();
	Shop.GM->WorldBuilder->SetShopName(TEXT("Öz Urfa Çiğköfte"));

	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);

	TestEqual(TEXT("Ad kayda yazilmali"), Save->ShopName, FString(TEXT("Öz Urfa Çiğköfte")));

	// Renamed to something else, then the save is applied over it.
	Shop.GM->WorldBuilder->SetShopName(TEXT("Baska Ad"));
	Shop.GM->ApplySave(*Save);
	TestEqual(TEXT("Yuklenen ad tabelaya donmeli"),
		Shop.GM->WorldBuilder->ShopDisplayName(), FString(TEXT("Öz Urfa Çiğköfte")));

	Save->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameUnnamedSaveTest,
	"Cigkofte.ShopIdentity.Shop.AnUnnamedShopIsSavedAsUnnamedNotAsItsDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigShopNameUnnamedSaveTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	Shop.GM->WorldBuilder->BuildWorld();

	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);

	// Writing the default in would turn "never named" into "named it that", and a
	// later change to the default would leave old shops carrying a name nobody
	// chose.
	TestTrue(TEXT("Adsiz dukkanin kaydinda ad bos olmali"), Save->ShopName.IsEmpty());
	TestEqual(TEXT("Ama tabela yine varsayilani gostermeli"),
		Shop.GM->WorldBuilder->ShopDisplayName(), CigShopIdentity::DefaultName());

	Save->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigShopNameMigrationTest,
	"Cigkofte.ShopIdentity.Save.AV13SaveArrivesUnnamedRatherThanNamedByDefault",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigShopNameMigrationTest::RunTest(const FString&)
{
	UCigSaveGame* S = NewObject<UCigSaveGame>();
	S->AddToRoot();
	S->SaveVersion = 13;
	S->ShopName = TEXT("elle yazilmis");

	UCigSaveSubsystem::MigrateSave(*S);

	TestEqual(TEXT("v13 kayit guncel surume tasinmali"),
		S->SaveVersion, UCigSaveSubsystem::CurrentVersion);
	// A v13 file cannot carry a name; one whose version was edited backwards can,
	// and it is cleared for the same reason the layout array is.
	TestTrue(TEXT("Tasinan v13 kaydin adi bos olmali"), S->ShopName.IsEmpty());
	// And an unnamed save still shows a board.
	TestEqual(TEXT("Bos ad varsayilana cozulmeli"),
		CigShopIdentity::Resolve(S->ShopName), CigShopIdentity::DefaultName());

	S->RemoveFromRoot();
	return true;
}

#endif
