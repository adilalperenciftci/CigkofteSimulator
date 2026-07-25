#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigQuestSystem.generated.h"

// Tutorial steps (the first-day guide).
enum class ECigTutorialStep : uint8
{
	AddIngredient = 0,
	Knead,
	TakeOrder,     // wait for a customer, read their order
	PrepareWrap,   // flatbread + dough + roll
	Serve,
	OrderStock,
	Clean,
	FinishDay,
	Done
};

// Long-term story goals (advanced in order).
enum class ECigStoryStage : uint8
{
	PayFirstRent = 0,   // pay the first rent
	Serve10,            // satisfy 10 customers
	ThreeStars,         // reach a 3-star shop score
	OpenDelivery,       // deliver 3 packages
	HireApprentice,
	RenovateShop,       // install 3 shop upgrades
	BeatRival,          // overtake a rival on popularity
	UpgradeCar,         // repair the car and fill the tank
	SecondBranch,       // the new-branch upgrade
	OwnBrand,           // 5000 TL saved plus 5 loyal customers
	WinFestival,        // 15 services on festival day
	BestInTown,         // reputation 90+
	Completed
};

// Daily quest, story chain and tutorial tracking.
UCLASS()
class UCigQuestSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;
	virtual void UpdateSystem(float DeltaSeconds) override;
	virtual void OnDayStart(int32 Day) override;
	virtual void OnDayEnd(int32 Day) override;

	// --- Event listeners ---
	// Nothing calls these directly any more; they are bound to the event bus in
	// OnInit (see Game/CigEventBus.h). Publishing systems have no idea the quest
	// system exists.
	void NotifyServe(float Accuracy, float Quality, int32 Portions);
	void NotifyDelivery();
	void NotifyWash();
	void NotifyChop();
	void NotifyClean();
	void NotifyStockOrdered();
	void NotifyUpgrade();
	void NotifyHire();
	void NotifyWrapStarted();
	void NotifyWrapFinished();
	void NotifyDoughPrepared(float Quality);
	void NotifyInspector(float Hygiene);
	void NotifyShopScore(float Stars);
	void NotifyRivalBeaten();
	void NotifyRivalClosed();
	void NotifyIngredientAdded();
	void NotifyCustomerArrived();

	// --- Daily quest ---
	FString QuestDesc;
	int32 QuestType = 0;
	int32 QuestTarget = 0;
	int32 QuestProgress = 0;
	int32 QuestReward = 0;
	bool bQuestDone = true;
	void GenerateQuest();
	void CompleteQuestNow(); // debug

	// --- Story ---
	ECigStoryStage StoryStage = ECigStoryStage::PayFirstRent;
	FString StoryGoalText() const;
	int32 StoryProgress = 0; // counter within a stage (e.g. deliveries made)

	// --- Tutorial ---
	bool bTutorialActive = true;
	ECigTutorialStep TutorialStep = ECigTutorialStep::AddIngredient;
	FString TutorialText() const;
	void ResetTutorial();
	void SkipTutorial();

private:
	float StoryCheckTimer = 0.f;

	void QuestEvent(int32 Type, int32 Amount = 1);
	void AdvanceTutorial(ECigTutorialStep FromStep);
	void AdvanceStory();
	void CheckStoryConditions();

	int32 UpgradesBought = 0;
	int32 DeliveriesDone = 0;
};
