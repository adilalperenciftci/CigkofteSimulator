// The kneading pitch ramp.
//
// It is the one part of the audio layer that is a rule rather than a wiring
// choice: the pitch is what tells the player how far the batch has come, so it
// has to rise with progress and stay inside a range that still sounds like
// dough being worked rather than a tape running fast.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Audio; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Audio/CigAudioSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigAudioKneadPitchTest,
	"Cigkofte.Audio.KneadPitchRisesWithProgress",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigAudioKneadPitchTest::RunTest(const FString& /*Parameters*/)
{
	const float Bas = UCigAudioSubsystem::YogurmaPerdesi(0.f);
	const float Orta = UCigAudioSubsystem::YogurmaPerdesi(0.5f);
	const float Son = UCigAudioSubsystem::YogurmaPerdesi(1.f);

	TestTrue(TEXT("Perde ilerlemeyle yükselmeli"), Bas < Orta && Orta < Son);

	// The ramp has to be audible; a couple of percent would be indistinguishable
	// from the random detune the stations already use.
	TestTrue(TEXT("Baştan sona fark duyulur olmalı"), Son - Bas > 0.3f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigAudioPitchBoundsTest,
	"Cigkofte.Audio.KneadPitchStaysSane",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigAudioPitchBoundsTest::RunTest(const FString& /*Parameters*/)
{
	// Progress arrives as a ratio computed from the knead count, so a stray
	// value outside 0-1 must not detune the sound into a squeak or a growl.
	TestEqual(TEXT("Negatif ilerleme başlangıç perdesini vermeli"),
		UCigAudioSubsystem::YogurmaPerdesi(-3.f), UCigAudioSubsystem::YogurmaPerdesi(0.f), 0.001f);
	TestEqual(TEXT("Aşırı ilerleme bitiş perdesini vermeli"),
		UCigAudioSubsystem::YogurmaPerdesi(9.f), UCigAudioSubsystem::YogurmaPerdesi(1.f), 0.001f);

	// Anything outside roughly half to double speed stops sounding like the same
	// recording at all.
	TestTrue(TEXT("Perde makul aralıkta kalmalı"),
		UCigAudioSubsystem::YogurmaPerdesi(0.f) >= 0.5f && UCigAudioSubsystem::YogurmaPerdesi(1.f) <= 2.f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
