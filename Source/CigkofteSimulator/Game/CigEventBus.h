#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigEventBus.generated.h"

// Event broadcasting between systems.
//
// The problem: on an event like "wrap rolled", OrderSystem used to call
// GM->Quests->NotifyWrapStarted() directly. That forced the order system to
// know about the quest system - the include list grew, build times went up,
// and testing OrderSystem on its own became impossible.
//
// The fix: broadcast the event, never learn who listens. A publisher knows
// only the bus. Today the quest system listens; if achievements start
// listening tomorrow, not one line changes on the publishing side.
//
// Lifetime: this is a UCigSystem, so it is built and torn down with the
// GameMode. Living on the GameInstance would leave dead subscriptions behind
// on a level reload, so world lifetime is deliberate.
UCLASS()
class UCigEventBus : public UCigSystem
{
	GENERATED_BODY()

public:
	// Events with no payload
	DECLARE_MULTICAST_DELEGATE(FCigEvent);
	// Events carrying a single number (quality, hygiene, score)
	DECLARE_MULTICAST_DELEGATE_OneParam(FCigValueEvent, float);
	// Serving: accuracy, quality, portions
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FCigServeEvent, float, float, int32);

	// --- Kitchen ---
	FCigEvent IngredientAdded;
	FCigValueEvent DoughPrepared;    // dough quality
	FCigEvent Chopped;
	FCigEvent WrapStarted;
	FCigEvent WrapFinished;

	// --- Customers & serving ---
	FCigEvent CustomerArrived;
	FCigServeEvent Served;           // accuracy, quality, portions
	FCigValueEvent InspectorVisited; // hygiene at the time

	// --- Business ---
	FCigEvent Delivered;
	FCigEvent StockOrdered;
	FCigEvent UpgradeBought;
	FCigEvent StaffHired;
	FCigValueEvent ShopScoreChanged; // stars

	// --- Hygiene ---
	FCigEvent Washed;
	FCigEvent Cleaned;

	// --- Rivals ---
	FCigEvent RivalBeaten;
	FCigEvent RivalClosed;
};
