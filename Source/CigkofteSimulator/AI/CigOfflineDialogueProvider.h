#pragma once

#include "CoreMinimal.h"
#include "AI/CigDialogueContext.h"

// Where a customer's line comes from, and the only place it comes from.
//
// This used to be one of two implementations behind a provider interface; the
// other issued an HTTP request to a hosted model. The shipped game does not talk
// to a network service, so the interface had a single implementation and the
// indirection was removed with it. Lines are authored offline, reviewed, and
// committed as data - see Config/Dialogue and Tools/generate_dialogue.py.
class FCigOfflineDialogueProvider
{
public:
	// Pure, so subcategory selection and the language choice are testable without
	// standing up a world.
	static FString PickLine(const FCigDialogueContext& Context);
};
