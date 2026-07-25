#pragma once

#include "CoreMinimal.h"
#include "UI/CigTabletWidget.h"
#include "Game/CigkofteGameMode.h"

// Produces the content of the tablet tabs as DATA.
//
// Why separate: the tabs used to draw straight to Canvas, which tangled content
// and layout inside one function. Like that it could neither move to UMG nor be
// tested. A tab now answers "which rows" and someone else does the drawing.
//
// A side benefit: row generation is a pure function - GameMode state in, a list
// of rows out.
namespace CigTablet
{
	// The tab's title (from Config/Text/Strings.csv).
	FString TabName(ECigTabletTab Tab);

	// The tab's rows. An unknown tab returns an empty list.
	TArray<FCigTabletRow> BuildRows(ACigkofteGameMode* GM, ECigTabletTab Tab);
}
