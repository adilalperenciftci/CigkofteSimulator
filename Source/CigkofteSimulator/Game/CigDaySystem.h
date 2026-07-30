#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigDaySystem.generated.h"

enum class ECigPhase : uint8
{
	Intro,
	// Before the door opens. The shop exists, the stations work, and nobody is
	// waiting - so the batch that gets served at nine was made at half eight.
	//
	// The clock does not run here and neither does the queue. What limits it is
	// energy: kneading and chopping cost the same as they do during service, so
	// a morning spent preparing is a shift spent tired. That is the trade, and
	// it is the reason this phase is not simply free time.
	Opening,
	Playing,
	// After the last customer, before the books. A short window with the door
	// shut and the stations still working, for the mess the day left.
	//
	// Deliberately short rather than open-ended. Cleaning already competes with
	// serving during a shift, which is the same problem preparation had; giving
	// it unlimited time afterwards would not fix that, it would make hygiene
	// free. A window forces the choice of which mess is worth the minute.
	Closing,
	Summary,
	GameOver
};

// The day loop: phase handling, timing, end-of-day rent and the summary.
UCLASS()
class UCigDaySystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;

	void StartDay(bool bAdvanceDay);
	void EndDay();

	void RegisterSale(int32 Amount) { DayEarnings += Amount; DayServed++; }
	void RegisterMissed() { DayMissed++; }

	// The shop is open: the clock runs, customers arrive, deliveries are due.
	bool IsPlaying() const { return Phase == ECigPhase::Playing; }

	// The player may use the stations. True through preparation as well as
	// service, and the distinction matters everywhere the two were the same
	// question before: dough ages while it is being prepared, hands get dirty
	// kneading at half eight, and none of it summons a customer.
	bool CanWork() const
	{
		return Phase == ECigPhase::Opening || Phase == ECigPhase::Playing || Phase == ECigPhase::Closing;
	}

	// Ends preparation and lets the queue in. Nothing forces it - a shop with an
	// empty counter can open, and that is the player's call to make.
	void OpenShop();

	// Ends the closing window early and goes to the books. The window runs out
	// on its own if the player would rather keep scrubbing.
	void FinishClosing();

	// How long the closing window lasts. Enough for two or three jobs out of the
	// six kinds of mess the shop accumulates, which is the point.
	static constexpr float ClosingLength = 45.f;

	// Seconds left in the closing window, zero at any other time. A reader
	// rather than opening PhaseTimer up: that field also runs the summary, and
	// the HUD has no business knowing which phase it is counting.
	float ClosingLeft() const
	{
		return Phase == ECigPhase::Closing ? FMath::Max(0.f, PhaseTimer) : 0.f;
	}
	float DayProgress() const { return 1.f - TimeLeft / FMath::Max(DayLength, 1.f); }

	ECigPhase Phase = ECigPhase::Intro;
	int32 Day = 1;
	float DayLength = 180.f;
	float TimeLeft = 180.f;

	int32 DayEarnings = 0;
	int32 DayServed = 0;
	int32 DayMissed = 0;
	int32 LastRent = 0;

private:
	float PhaseTimer = 0.f;
};
