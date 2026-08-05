#include "Placement/CigBuildSelection.h"

#include "Core/CigText.h"

namespace CigBuildSelection
{
	FCigBuildSelection Resolve(FName StableId, const FCigPlacementRecord* Record)
	{
		FCigBuildSelection Selection;

		// The common case, and deliberately first: most of what a player looks at
		// is not furniture. Walls, the floor, customers and the street all land
		// here, and none of them is a mistake.
		if (StableId.IsNone())
		{
			Selection.Fault = ECigBuildSelectionFault::NotAPlacement;
			return Selection;
		}

		// The registry named a placement the authority does not have. Nothing the
		// player did causes this - the two have gone out of step - so it is kept
		// separate from the refusals rather than reported as one.
		if (!Record)
		{
			Selection.StableId = StableId;
			Selection.Fault = ECigBuildSelectionFault::Orphaned;
			return Selection;
		}

		Selection.StableId = StableId;
		Selection.Category = Record->Category;
		Selection.Lifetime = Record->Lifetime;

		// Transient placements are the day's traffic, not the shop's layout. A
		// delivery crate is put down by the delivery system and taken away by it,
		// and the save deliberately persists only Installed records - so a player
		// who moved a crate would be editing something that will not survive the
		// evening, and build mode would be lying about what it can keep.
		if (Record->Lifetime != ECigPlacementLifetime::Installed)
		{
			Selection.Fault = ECigBuildSelectionFault::NotInstalled;
			return Selection;
		}

		Selection.Fault = ECigBuildSelectionFault::None;
		return Selection;
	}

	FString CategoryText(ECigPlacementCategory Category)
	{
		const TCHAR* Key = TEXT("build.category.unknown");
		switch (Category)
		{
		case ECigPlacementCategory::Station: Key = TEXT("build.category.station"); break;
		case ECigPlacementCategory::Seating: Key = TEXT("build.category.seating"); break;
		case ECigPlacementCategory::Storage: Key = TEXT("build.category.storage"); break;
		case ECigPlacementCategory::Decoration: Key = TEXT("build.category.decoration"); break;
		case ECigPlacementCategory::Unknown:
		default: break;
		}
		return CigText::Get(Key);
	}

	FString FaultText(ECigBuildSelectionFault Fault)
	{
		// Silence is a decision here, not a string, so it does not go through the
		// text table at all. An empty entry would not survive it anyway: CigText
		// treats a blank value as a missing translation, falls back to Turkish,
		// finds that blank too, and then warns and returns something visible - so
		// a "say nothing" key would put both a message on screen and a warning in
		// the log on every frame the player looked at a wall.
		switch (Fault)
		{
		case ECigBuildSelectionFault::Orphaned: return CigText::Get(TEXT("build.select.orphaned"));
		case ECigBuildSelectionFault::NotInstalled: return CigText::Get(TEXT("build.select.transient"));
		case ECigBuildSelectionFault::NotAPlacement:
		case ECigBuildSelectionFault::None:
		default: return FString();
		}
	}

	FString Describe(const FCigBuildSelection& Selection)
	{
		if (!Selection.IsValid())
		{
			return FaultText(Selection.Fault);
		}
		// Category first because it is what the player is thinking in - "the
		// seating" - and the stable id second because it is what the save, the
		// authority and any bug report will call it.
		return CigText::Format(TEXT("build.select.label"),
			*CategoryText(Selection.Category), *Selection.StableId.ToString());
	}
}
