// Which layer may read the keyboard.
//
// This exists because input in this project is polled, not bound: the character
// asks the controller for raw key state every tick, and that does not stop when a
// widget takes focus. Nothing about a UEditableTextBox on screen prevents the
// game from also seeing the letters, so the gate is the only thing that does, and
// the gate is a decision worth testing without starting a game.

#include "Misc/AutomationTest.h"
#include "Core/CigInput.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigInputScopeTest,
	"Cigkofte.Input.Scope.TextEntryOutranksTheTabletAndTheTabletOutranksTheWorld",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigInputScopeTest::RunTest(const FString&)
{
	TestTrue(TEXT("Hicbir sey acik degilse oynanis"),
		CigInput::Scope(false, false, false) == ECigInputScope::Gameplay);

	TestTrue(TEXT("Tablet acikken tablet"),
		CigInput::Scope(false, true, false) == ECigInputScope::Tablet);

	// The case the whole gate exists for. The field is hosted by the tablet, so
	// if the tablet kept polling while the field had focus, its number-key
	// shortcuts would fire on every digit typed into a shop name.
	TestTrue(TEXT("Metin girisi tabletin onunde gelmeli"),
		CigInput::Scope(true, true, false) == ECigInputScope::TextEntry);

	// And it does not depend on the tablet being open. A field could be hosted
	// anywhere later; the gate must not quietly assume where it is.
	TestTrue(TEXT("Metin girisi tabletsiz de gecerli olmali"),
		CigInput::Scope(true, false, false) == ECigInputScope::TextEntry);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigInputScopeExclusiveTest,
	"Cigkofte.Input.Scope.ExactlyOneLayerIsEverActive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigInputScopeExclusiveTest::RunTest(const FString&)
{
	// Every combination resolves to one scope and never to something in between.
	// The failure this guards against is a future third modal state being added
	// as another boolean and two of them being true at once.
	//
	// That third state arrived: build mode. The loop grew from four combinations
	// to eight rather than the new flag getting a check of its own, because what
	// is being pinned is the property of the whole set - and a flag tested only
	// against the cases somebody thought of is exactly what this was written to
	// prevent.
	for (int32 i = 0; i < 8; ++i)
	{
		const bool bText = (i & 1) != 0;
		const bool bTablet = (i & 2) != 0;
		const bool bBuild = (i & 4) != 0;
		const ECigInputScope S = CigInput::Scope(bText, bTablet, bBuild);

		TestTrue(FString::Printf(TEXT("metin=%d tablet=%d insa=%d tanimli bir kapsam vermeli"), bText, bTablet, bBuild),
			S == ECigInputScope::Gameplay || S == ECigInputScope::Tablet
				|| S == ECigInputScope::BuildMode || S == ECigInputScope::TextEntry);

		// Gameplay is only reachable when nothing is open, which is the property
		// that stops the world taking keys behind an open panel.
		TestEqual(FString::Printf(TEXT("metin=%d tablet=%d insa=%d oynanis yalniz hicbir sey acik degilken"), bText, bTablet, bBuild),
			S == ECigInputScope::Gameplay, !bText && !bTablet && !bBuild);
	}
	return true;
}

#endif
