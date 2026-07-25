#pragma once

#include "CoreMinimal.h"
#include "AI/CigAIProvider.h"

// The fallback provider that works without a network or AI. It picks
// non-deterministically from canned lines based on mood and dominant trait.
// Synchronous: OnComplete runs immediately.
class FCigOfflineDialogueProvider : public ICigDialogueProvider
{
public:
	virtual void RequestLine(const FCigDialogueContext& Context, FCigDialogueDelegate OnComplete) override;

	// Kept as a separate pure function so subcategory selection can be tested.
	static FString PickLine(const FCigDialogueContext& Context);
};
