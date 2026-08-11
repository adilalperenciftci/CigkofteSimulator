// Two full-screen panels, one canvas, no z-order.
//
// The HUD draws everything with Canvas calls in the order DrawHUD makes them.
// There is no compositing and nothing occludes anything: a panel drawn earlier
// is simply painted over by whatever is drawn later. So two full-screen layers
// being "open" at once is not a stack - it is one panel's text running through
// the other's background.
//
// That is what happened on the title screen. Settings is opened from the main
// menu's third entry, DrawHUD draws settings before it draws the title screen,
// and the title screen had no rule saying to stand down - so the menu entries
// and the pulsing title were drawn straight over the settings panel. The pause
// menu already carried exactly this rule (`&& !GM->bSettingsOpen`); the title
// screen was the copy that did not.
//
// Tested as a pure decision rather than by drawing, because what went wrong was
// the decision. A screenshot test would need a renderer and would still only
// prove one resolution's worth of overlap.

#include "Misc/AutomationTest.h"
#include "UI/CigkofteHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTitleYieldsToSettingsTest,
	"Cigkofte.Menu.TheTitleScreenStandsDownForSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTitleYieldsToSettingsTest::RunTest(const FString&)
{
	// The ordinary case: the intro phase, nothing else open, menu on screen.
	TestTrue(TEXT("Girişte ayarlar kapalıyken başlık menüsü çizilmeli"),
		ACigkofteHUD::ShouldDrawTitleScreen(/*bIntroPhase=*/true, /*bSettingsOpen=*/false));

	// The defect. Reached by pressing Enter on the settings entry, which is the
	// only way into settings from the main menu - so this state is the normal
	// path through the menu rather than a corner of it.
	TestFalse(TEXT("Ayarlar açıkken başlık menüsü ayarların üstüne binmemeli"),
		ACigkofteHUD::ShouldDrawTitleScreen(/*bIntroPhase=*/true, /*bSettingsOpen=*/true));

	// And it stays a rule about the intro phase. Settings opened mid-game must
	// not somehow bring the title screen back.
	TestFalse(TEXT("Oyun içinde başlık menüsü çizilmemeli"),
		ACigkofteHUD::ShouldDrawTitleScreen(/*bIntroPhase=*/false, /*bSettingsOpen=*/false));
	TestFalse(TEXT("Oyun içinde ayarlar açıkken de çizilmemeli"),
		ACigkofteHUD::ShouldDrawTitleScreen(/*bIntroPhase=*/false, /*bSettingsOpen=*/true));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
