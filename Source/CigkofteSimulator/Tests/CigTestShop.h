#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/CigkofteGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

// A whole shop, standing up in a throwaway world, for tests.
//
// Most of this project's rules are pure functions and can be checked directly.
// The ones that are not are exactly the ones that have gone wrong: a sale is
// only correct in terms of what it does to eight other systems, and a price is
// only wrong once demand, reviews and reputation disagree about it. Those
// defects lived between systems, where no pure function could see them.
//
// The world here is empty on purpose. ACigkofteGameMode::CreateSystems builds
// and initialises the systems without touching meshes, actors or the save file,
// so a test gets the rules without waiting for a map to load.
//
// Usage:
//   FCigTestShop Shop;
//   if (!Shop.Build(*this)) { return false; }   // reports its own failure
//   ...
//   Shop.Destroy();                             // or let the destructor do it
struct FCigTestShop
{
	UWorld* World = nullptr;
	UGameInstance* GI = nullptr;
	ACigkofteGameMode* GM = nullptr;

	~FCigTestShop() { Destroy(); }

	bool Build(FAutomationTestBase& Test)
	{
		if (!GEngine)
		{
			Test.AddError(TEXT("GEngine yok; test dükkânı kurulamaz."));
			return false;
		}

		// A game instance has to exist first: UCigSystem::InitSystem asserts on
		// the deterministic RNG subsystem, which lives on the instance.
		GI = NewObject<UGameInstance>(GEngine);
		GI->AddToRoot();
		GI->InitializeStandalone();

		World = GI->GetWorld();
		if (!World)
		{
			Test.AddError(TEXT("Test dünyası oluşmadı."));
			Destroy();
			return false;
		}

		GM = World->SpawnActor<ACigkofteGameMode>();
		if (!GM)
		{
			Test.AddError(TEXT("GameMode spawn edilemedi."));
			Destroy();
			return false;
		}

		// Deliberately not InitGame: that would build the world, spawn the car
		// and load the player's real save file, which a test must never touch.
		GM->CreateSystems();

		if (!GM->Sales || !GM->Pricing || !GM->Progression || !GM->Economy)
		{
			Test.AddError(TEXT("Sistemler kurulmadı."));
			Destroy();
			return false;
		}

		return true;
	}

	void Destroy()
	{
		GM = nullptr;
		World = nullptr;
		if (GI)
		{
			GI->Shutdown();
			GI->RemoveFromRoot();
			GI = nullptr;
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
