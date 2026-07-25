#pragma once

#include "CoreMinimal.h"
#include "AI/CigDialogueContext.h"

// The dialogue provider interface. Two implementations:
//  - UCigAIServiceSubsystem: an async HTTP request to the Anthropic Messages API
//  - FCigOfflineDialogueProvider: canned lines when there is no network, or it
//    is switched off
// The caller (CustomerSystem) therefore never learns which source was used.
class ICigDialogueProvider
{
public:
	virtual ~ICigDialogueProvider() {}

	// Produces a one-sentence reply from the context. An implementation may be
	// synchronous (offline) or asynchronous (HTTP); either way OnComplete runs.
	virtual void RequestLine(const FCigDialogueContext& Context, FCigDialogueDelegate OnComplete) = 0;
};
