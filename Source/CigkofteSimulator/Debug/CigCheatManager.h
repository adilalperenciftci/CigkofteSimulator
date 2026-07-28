#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "CigCheatManager.generated.h"

class ACigkofteGameMode;

// Developer commands from the console (`). e.g. AddMoney 500
UCLASS()
class UCigCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec) void StartDayNow();
	UFUNCTION(Exec) void AddMoney(int32 Amount);
	UFUNCTION(Exec) void SetMoney(int32 Amount);
	UFUNCTION(Exec) void SetDay(int32 Day);
	UFUNCTION(Exec) void SkipDay();
	UFUNCTION(Exec) void SpawnCustomer();
	UFUNCTION(Exec) void SpawnInfluencer();
	UFUNCTION(Exec) void SetReputation(float Rep);
	UFUNCTION(Exec) void SetHygiene(float Value);
	UFUNCTION(Exec) void CompleteQuest();
	UFUNCTION(Exec) void TriggerInspector();
	UFUNCTION(Exec) void TriggerEvent(int32 EventIndex);
	UFUNCTION(Exec) void RefillStock();
	UFUNCTION(Exec) void UnlockRecipe();
	UFUNCTION(Exec) void UnlockAllUpgrades();
	UFUNCTION(Exec) void SetTimeScale(float Scale);
	UFUNCTION(Exec) void StartDelivery();
	UFUNCTION(Exec) void SaveGame();
	UFUNCTION(Exec) void LoadGame();
	UFUNCTION(Exec) void ResetSave();
	UFUNCTION(Exec) void AddXPCheat(int32 Amount);
	UFUNCTION(Exec) void AddSkillPoints(int32 Amount);
	UFUNCTION(Exec) void UpgradeSkill(int32 Index);
	UFUNCTION(Exec) void DoPrestige();
	UFUNCTION(Exec) void GiveSides(int32 Amount);
	UFUNCTION(Exec) void RenameCat(const FString& NewName);

	// Deterministic randomness, for reproducing a bug report exactly.
	// `CigSeed` prints the current seed, `CigSetSeed <n>` restarts from it.
	UFUNCTION(Exec) void CigSeed();
	UFUNCTION(Exec) void CigSetSeed(int32 Seed);

	// Re-reads Config/Balance/*.csv, for balance experiments without a restart.
	UFUNCTION(Exec) void CigReloadBalance();


	// Prints how many rows each tab produces. A tab coming back empty was either
	// never moved to data or is broken; this shows it without opening the screen.
	UFUNCTION(Exec) void CigTabletDump();

#if WITH_EDITOR
	// The dialogue pipeline (see AI/CigDialogueGenerator.cpp). Editor only:
	// these are development tools and do not ship with the game.
	UFUNCTION(Exec) void CigGenerateDialogue();
	UFUNCTION(Exec) void CigDialogueStats();
#endif

	// An automated screenshot tour for the README: it moves the player through a
	// set of viewpoints, grabs frames including the HUD, and quits when done.
	UFUNCTION(Exec) void CigShots();
	// Tours and photographs the new assets: shop, seating, street, square, market, cat.
	UFUNCTION(Exec) void CigTour();

	// A fixed benchmark route, for filling in PERFORMANCE_BUDGET.md.
	//
	// The same five viewpoints every run, held for the same time, on a fixed
	// seed with every district open and the stock full - so two runs differ
	// because the build changed, not because the day went differently. It emits
	// a CSV profiler event at each stop, which is what lets a run be read per
	// viewpoint rather than as one average that hides the expensive one. Quits
	// when the route ends.
	//
	// The route is deliberately the heaviest state the game can be in. A budget
	// measured on an empty first day would be met by a build that stutters as
	// soon as the player earns anything.
	UFUNCTION(Exec) void CigBench(int32 SecondsPerStop = 6);

private:
	ACigkofteGameMode* GM() const;

	void ShotStep();
	void TourStep();
	void BenchStep();

	FTimerHandle ShotTimer;
	int32 ShotIndex = 0;
	FTimerHandle BenchTimer;
	int32 BenchIndex = 0;
};
