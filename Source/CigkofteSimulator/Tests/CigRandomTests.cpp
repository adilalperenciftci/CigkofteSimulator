// The two promises of the deterministic random stream:
//   1. The same seed yields the same sequence (bug reports are reproducible).
//   2. Restoring from a save continues the stream rather than rewinding it
//      (reloading and replaying a day does not reroll it).
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Random; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/CigRandomSubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// The subsystem can be used on its own without standing up a game: all of
	// its state is in the FRandomStream inside it, with no outside dependency.
	// Even so, UGameInstanceSubsystem has ClassWithin = UGameInstance, so the
	// outer must be a real GameInstance or NewObject ensures. Both are held
	// together to keep them from the GC.
	struct FCigRngFixture
	{
		TStrongObjectPtr<UGameInstance> Owner;
		TStrongObjectPtr<UCigRandomSubsystem> Rng;

		UCigRandomSubsystem& operator*() const { return *Rng; }
		UCigRandomSubsystem* operator->() const { return Rng.Get(); }
	};

	FCigRngFixture MakeRng(int32 Seed)
	{
		FCigRngFixture F;
		F.Owner = TStrongObjectPtr<UGameInstance>(NewObject<UGameInstance>(GetTransientPackage()));
		F.Rng = TStrongObjectPtr<UCigRandomSubsystem>(NewObject<UCigRandomSubsystem>(F.Owner.Get()));
		F.Rng->SeedWith(Seed);
		return F;
	}

	TArray<float> Draw(UCigRandomSubsystem& Rng, int32 Count)
	{
		TArray<float> Out;
		Out.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			Out.Add(Rng.FRand());
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRandomSameSeedSameSequenceTest,
	"Cigkofte.Random.SameSeedProducesSameSequence",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigRandomSameSeedSameSequenceTest::RunTest(const FString& /*Parameters*/)
{
	FCigRngFixture A = MakeRng(4242);
	FCigRngFixture B = MakeRng(4242);

	TestEqual(TEXT("Aynı seed aynı diziyi vermeli"), Draw(*A, 32), Draw(*B, 32));

	FCigRngFixture C = MakeRng(4243);
	TestNotEqual(TEXT("Farklı seed farklı dizi vermeli"), Draw(*C, 32), Draw(*B, 32));

	TestEqual(TEXT("Başlangıç seed'i saklanmalı"), A->InitialSeed(), 4242);
	TestEqual(TEXT("Çekim sayısı sayılmalı"), (int32)A->DrawCount(), 32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRandomRestoreContinuesStreamTest,
	"Cigkofte.Random.RestoreContinuesInsteadOfRewinding",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigRandomRestoreContinuesStreamTest::RunTest(const FString& /*Parameters*/)
{
	// Play a day, then "save": take the stream's current position.
	FCigRngFixture Play = MakeRng(777);
	Draw(*Play, 20);
	const int32 SavedInitial = Play->InitialSeed();
	const int32 SavedState = Play->StateSeed();

	// Play on past the save: this is the expected continuation.
	const TArray<float> Expected = Draw(*Play, 16);

	// "Load": a fresh subsystem restored from the two numbers in the save. The
	// starting seed is irrelevant - RestoreState overwrites the stream state.
	FCigRngFixture Loaded = MakeRng(1);
	Loaded->RestoreState(SavedInitial, SavedState);

	TestEqual(TEXT("Yükleme akışı kaldığı yerden sürdürmeli"), Draw(*Loaded, 16), Expected);
	TestEqual(TEXT("Başlangıç seed'i sunum için korunmalı"), Loaded->InitialSeed(), SavedInitial);

	// Reseeding from scratch must not reproduce the same numbers, or save-scumming
	// would replay the same day and the save/load determinism would mean nothing.
	FCigRngFixture Rewound = MakeRng(SavedInitial);
	TestNotEqual(TEXT("Başa sarma devam dizisini vermemeli"), Draw(*Rewound, 16), Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRandomHelpersStayInRangeTest,
	"Cigkofte.Random.HelpersStayInRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigRandomHelpersStayInRangeTest::RunTest(const FString& /*Parameters*/)
{
	FCigRngFixture Rng = MakeRng(99);

	for (int32 i = 0; i < 500; ++i)
	{
		const float F = Rng->FRand();
		if (F < 0.f || F >= 1.f)
		{
			AddError(FString::Printf(TEXT("FRand [0,1) dışına çıktı: %f"), F));
			break;
		}
		const int32 R = Rng->RandRange(3, 7);
		if (R < 3 || R > 7)
		{
			AddError(FString::Printf(TEXT("RandRange aralık dışına çıktı: %d"), R));
			break;
		}
		const float FR = Rng->FRandRange(-2.f, 5.f);
		if (FR < -2.f || FR > 5.f)
		{
			AddError(FString::Printf(TEXT("FRandRange aralık dışına çıktı: %f"), FR));
			break;
		}
		if (Rng->Rand() < 0)
		{
			AddError(TEXT("Rand negatif değer döndürdü"));
			break;
		}
	}

	// Picking from an empty array returns -1 and must not take a draw; otherwise
	// a state-dependent branch like "is the list empty" would shift the stream.
	const int64 Before = Rng->DrawCount();
	TestEqual(TEXT("Boş dizide PickIndex -1 dönmeli"), Rng->PickIndex(0), -1);
	TestEqual(TEXT("Boş dizi seçimi çekim tüketmemeli"), (int32)(Rng->DrawCount() - Before), 0);

	TestTrue(TEXT("Tek elemanlı dizide PickIndex 0 dönmeli"), Rng->PickIndex(1) == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
