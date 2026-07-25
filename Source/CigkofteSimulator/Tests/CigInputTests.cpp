// The key binding layer.
//
// The critical behaviour: one key must never stay bound to two actions. If it
// does, pressing it makes two things happen at once and the player has no way
// to work out why.

#include "Misc/AutomationTest.h"
#include "Core/CigInput.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigInputDefaultsTest,
	"Cigkofte.Input.DefaultsAreDistinct",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigInputDefaultsTest::RunTest(const FString& /*Parameters*/)
{
	CigInput::ResetToDefaults();

	// The defaults must all differ - ship a clashing default and the player meets
	// a broken control scheme the first time they launch.
	TSet<FName> Seen;
	for (int32 i = 0; i < (int32)ECigAction::COUNT; ++i)
	{
		const FKey K = CigInput::Key((ECigAction)i);
		TestTrue(FString::Printf(TEXT("Eylem %d varsayılan tuşu geçerli olmalı"), i), K.IsValid());
		bool bAlready = false;
		Seen.Add(K.GetFName(), &bAlready);
		TestFalse(FString::Printf(TEXT("Eylem %d varsayılanı başka bir eylemle çakışmamalı"), i), bAlready);

		TestFalse(TEXT("Sıfırlama sonrası hiçbir eylem yeniden atanmış görünmemeli"),
			CigInput::IsRemapped((ECigAction)i));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigInputRebindStealsConflictTest,
	"Cigkofte.Input.RebindClearsConflictingAction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigInputRebindStealsConflictTest::RunTest(const FString& /*Parameters*/)
{
	CigInput::ResetToDefaults();

	// Bind jump to the forward key and forward must be left unbound.
	const FKey Forward = CigInput::Key(ECigAction::MoveForward);
	CigInput::SetKey(ECigAction::Jump, Forward);

	TestEqual(TEXT("Zıplama yeni tuşu almalı"), CigInput::Key(ECigAction::Jump), Forward);
	TestFalse(TEXT("İleri gitme artık atanmamış olmalı"), CigInput::Key(ECigAction::MoveForward).IsValid());
	TestTrue(TEXT("Zıplama yeniden atanmış sayılmalı"), CigInput::IsRemapped(ECigAction::Jump));

	CigInput::ResetToDefaults();
	TestEqual(TEXT("Sıfırlama ileri gitmeyi geri vermeli"), CigInput::Key(ECigAction::MoveForward), Forward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigInputSaveRoundTripTest,
	"Cigkofte.Input.SaveRoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigInputSaveRoundTripTest::RunTest(const FString& /*Parameters*/)
{
	CigInput::ResetToDefaults();
	CigInput::SetKey(ECigAction::Interact, EKeys::Q);
	const TArray<FString> Saved = CigInput::SaveBindings();

	CigInput::ResetToDefaults();
	TestNotEqual(TEXT("Sıfırlama sonrası atama varsayılana dönmeli"),
		CigInput::Key(ECigAction::Interact), FKey(EKeys::Q));

	CigInput::LoadBindings(Saved);
	TestEqual(TEXT("Yükleme atamayı geri getirmeli"), CigInput::Key(ECigAction::Interact), FKey(EKeys::Q));

	// An old or short save: missing entries stay on the default and must not crash.
	CigInput::ResetToDefaults();
	CigInput::LoadBindings(TArray<FString>{ TEXT("W") });
	TestTrue(TEXT("Kısa kayıt sonrası diğer eylemler geçerli kalmalı"),
		CigInput::Key(ECigAction::Tablet).IsValid());

	// An unrecognised key name falls back to the default, so no action is left dead.
	CigInput::ResetToDefaults();
	TArray<FString> Bogus;
	Bogus.Init(TEXT("BoyleBirTusYok"), (int32)ECigAction::COUNT);
	CigInput::LoadBindings(Bogus);
	TestTrue(TEXT("Tanınmayan tuş adı varsayılana dönmeli"),
		CigInput::Key(ECigAction::MoveForward).IsValid());

	CigInput::ResetToDefaults();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
