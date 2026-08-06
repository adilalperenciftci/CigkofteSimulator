#include "Placement/CigBuildRemoval.h"

#include "Core/CigText.h"
#include "Placement/CigBuildSelection.h"

namespace CigBuildRemoval
{
	FCigBuildRemovalVerdict Judge(const FCigPlacementRecord* Record, int32 CategoryCapacityTotal)
	{
		FCigBuildRemovalVerdict Verdict;
		if (!Record)
		{
			Verdict.Fault = ECigBuildRemovalFault::NoRecord;
			return Verdict;
		}

		Verdict.StableId = Record->StableId;
		Verdict.Category = Record->Category;

		// The same rule selection applies, restated here rather than assumed:
		// nothing that arrives and leaves on the day's own schedule is the
		// player's to take away.
		if (Record->Lifetime != ECigPlacementLifetime::Installed)
		{
			Verdict.Fault = ECigBuildRemovalFault::NotSelectable;
			return Verdict;
		}

		const int32 ThisCapacity = Record->Consequence.FunctionalCapacity;

		// Carries no function, so removing it takes nothing away. Decoration lands
		// here, and so does anything else the consequence policy gave no capacity.
		if (ThisCapacity <= 0)
		{
			Verdict.Fault = ECigBuildRemovalFault::None;
			return Verdict;
		}

		// What the shop would have left. The last of a function is refused; the
		// second-to-last is not, which is the line that makes this a rule about
		// capability rather than a blanket ban on removing useful things.
		if (CategoryCapacityTotal - ThisCapacity <= 0)
		{
			Verdict.Fault = ECigBuildRemovalFault::LastOfItsFunction;
			return Verdict;
		}

		Verdict.Fault = ECigBuildRemovalFault::None;
		return Verdict;
	}

	FString Describe(const FCigBuildRemovalVerdict& Verdict)
	{
		switch (Verdict.Fault)
		{
		case ECigBuildRemovalFault::None:
			return CigText::Get(TEXT("build.remove.ok"));
		case ECigBuildRemovalFault::LastOfItsFunction:
			// Named, so the player knows what they would be giving up rather than
			// only that they may not.
			return CigText::Format(TEXT("build.remove.lastofkind"),
				*CigBuildSelection::CategoryText(Verdict.Category));
		case ECigBuildRemovalFault::NoRecord:
			return CigText::Get(TEXT("build.select.orphaned"));
		case ECigBuildRemovalFault::NotSelectable:
		default:
			return CigText::Get(TEXT("build.select.transient"));
		}
	}
}
