#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigDaySystem.generated.h"

enum class ECigPhase : uint8
{
	Intro,
	Playing,
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

	bool IsPlaying() const { return Phase == ECigPhase::Playing; }
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
