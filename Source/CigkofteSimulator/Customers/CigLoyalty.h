#pragma once

#include "CoreMinimal.h"

// What one serve does to a regular.
//
// The arithmetic lived inside a private method on UCigCustomerSystem, which
// meant it could only be reached by serving an actual customer in an actual
// shop. Nothing tested it, and it turns out to hold two decisions worth being
// able to see.
//
// Trust is asymmetric: a good serve adds four, a bad one takes six. It costs
// three good visits to undo two bad ones, which is a claim about how forgiving
// this shop's regulars are and not an accident of arithmetic.
//
// And satisfaction pivots at 60 while trust turns at 80. Between those numbers a
// serve pleases somebody and disappoints them at the same time - accuracy 70
// raises satisfaction by two and drops trust by six. That is either a nice piece
// of characterisation or a bug, and it could not be argued about while it was
// buried in a method nobody could call.

struct FCigLoyaltyOutcome
{
	float Satisfaction = 0.f;
	float Trust = 0.f;
	float AvgTip = 0.f;

	// The order was made better than the regular hoped: they leave something extra.
	bool bPerfectBonus = false;
	// Bad enough that they will bring it up next time.
	bool bRemembersMistake = false;
};

namespace CigLoyalty
{
	// Where satisfaction stops rising and starts falling.
	inline constexpr float SatisfactionPivot = 60.f;
	// How much of the accuracy gap one visit moves satisfaction by.
	inline constexpr float SatisfactionDivisor = 5.f;

	// Trust does not pivot where satisfaction does. A serve can sit between the
	// two and move them in opposite directions.
	inline constexpr float TrustThreshold = 80.f;
	inline constexpr float TrustGain = 4.f;
	inline constexpr float TrustLoss = 6.f;

	inline constexpr float PerfectAccuracy = 90.f;
	inline constexpr float MistakeAccuracy = 50.f;

	// AvgTip is a name that overstates it: the new tip is averaged with the
	// running value, so the last visit is worth as much as everything before it
	// put together. Kept as it was - this is a description, not a change.
	FCigLoyaltyOutcome AfterServe(float Accuracy, float Satisfaction, float Trust,
		float AvgTip, int32 Tip);
}
