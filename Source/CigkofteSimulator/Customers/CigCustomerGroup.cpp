#include "Customers/CigCustomerGroup.h"

namespace CigCustomerGroup
{
	int32 SizeFromRoll(float Roll01)
	{
		// 55 / 30 / 15 across two, three and four. A pair is what "a group" means
		// most of the time; a four is rare enough that the queue filling up in one
		// arrival is an event rather than the normal way the shop gets busy.
		const float R = FMath::Clamp(Roll01, 0.f, 1.f);
		if (R < 0.55f) { return 2; }
		if (R < 0.85f) { return 3; }
		return 4;
	}

	FCigGroupIntake JudgeIntake(int32 GroupSize, int32 QueueUsed, int32 QueueCapacity)
	{
		FCigGroupIntake Out;

		// A party of nobody is not a refusal, it is a caller mistake. Report it as
		// full rather than admitting zero people, so a bug upstream cannot look
		// like a successful arrival.
		if (GroupSize <= 0 || QueueCapacity <= 0)
		{
			Out.Refusal = ECigGroupRefusal::ShopFull;
			return Out;
		}

		// Checked before the free-space test, so a party too big for the shop gets
		// that answer even when the queue happens to be empty. The two are
		// different problems and only one of them goes away by waiting.
		if (GroupSize > QueueCapacity)
		{
			Out.Refusal = ECigGroupRefusal::LargerThanShop;
			return Out;
		}

		const int32 Free = FMath::Max(0, QueueCapacity - QueueUsed);
		if (Free <= 0)
		{
			Out.Refusal = ECigGroupRefusal::ShopFull;
			return Out;
		}
		if (Free < GroupSize)
		{
			Out.Refusal = ECigGroupRefusal::WouldSplit;
			return Out;
		}

		Out.Admitted = GroupSize;
		Out.Refusal = ECigGroupRefusal::None;
		return Out;
	}

	float WalkoutRepMult(int32 GroupSize)
	{
		if (GroupSize <= 1)
		{
			return 1.f;
		}
		// Square root, then clamped: a pair costs about 1.4x, a four about 2x.
		// Sub-linear on purpose - see the header. The clamp is what guarantees the
		// bound rather than the shape of the curve, so raising MaxSize later
		// cannot quietly make walkouts unsurvivable.
		return FMath::Min(2.f, FMath::Sqrt((float)GroupSize));
	}
}
