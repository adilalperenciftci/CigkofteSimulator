#include "Customers/CigLoyalty.h"

namespace CigLoyalty
{
	FCigLoyaltyOutcome AfterServe(float Accuracy, float Satisfaction, float Trust,
		float AvgTip, int32 Tip)
	{
		FCigLoyaltyOutcome Out;

		Out.Satisfaction = FMath::Clamp(
			Satisfaction + (Accuracy - SatisfactionPivot) / SatisfactionDivisor, 0.f, 100.f);

		Out.Trust = FMath::Clamp(
			Trust + (Accuracy >= TrustThreshold ? TrustGain : -TrustLoss), 0.f, 100.f);

		Out.AvgTip = (AvgTip + Tip) * 0.5f;

		Out.bPerfectBonus = Accuracy >= PerfectAccuracy;
		Out.bRemembersMistake = Accuracy < MistakeAccuracy;

		return Out;
	}
}
