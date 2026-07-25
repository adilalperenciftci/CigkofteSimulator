#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AI/CigAIProvider.h"
#include "AI/CigAIResponseCache.h"
#include "AI/CigOfflineDialogueProvider.h"
#include "CigAIServiceSubsystem.generated.h"

// The customer dialogue service.
//  1) If the context is cached, return that.
//  2) With no API key, or once the daily budget is spent, fall back to offline.
//  3) Otherwise send an async request to the Anthropic Messages API; on failure
//     fall back to offline as well. The game never waits (non-blocking).
//
// The API key is read ONLY from the ANTHROPIC_API_KEY environment variable; it
// is never written into the code or into a save file.
UCLASS()
class UCigAIServiceSubsystem : public UGameInstanceSubsystem, public ICigDialogueProvider
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ICigDialogueProvider
	virtual void RequestLine(const FCigDialogueContext& Context, FCigDialogueDelegate OnComplete) override;

	// Whether the AI service is usable (key present, budget not exhausted).
	bool IsAIEnabled() const;

	// Called by GameMode at the start of a day: resets the daily request counter.
	void ResetDailyBudget();

	// Testing / tuning: how many live AI requests are allowed per day.
	int32 MaxRequestsPerDay = 40;

private:
	FString ApiKey;                       // from the environment; empty means AI off
	int32 RequestsToday = 0;
	FCigOfflineDialogueProvider Offline;
	FCigAIResponseCache Cache;

	// Builds the Anthropic request body.
	FString BuildRequestBody(const FCigDialogueContext& Context) const;
	// Pulls the first text block out of the Anthropic response JSON; false on failure.
	static bool ParseResponse(const FString& Json, FString& OutLine);

	// The reply is collapsed to a single line and trimmed (for the in-game feed).
	static FString Sanitize(const FString& Raw);
};
