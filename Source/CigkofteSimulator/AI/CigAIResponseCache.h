#pragma once

#include "CoreMinimal.h"

// A simple cache so the AI is not called again for the same class of context.
// Key: FCigDialogueContext::CacheKey(). At capacity the oldest is evicted (FIFO).
class FCigAIResponseCache
{
public:
	explicit FCigAIResponseCache(int32 InMaxEntries = 128)
		: MaxEntries(FMath::Max(1, InMaxEntries)) {}

	bool TryGet(const FString& Key, FString& OutLine) const;
	void Put(const FString& Key, const FString& Line);

	int32 Num() const { return Map.Num(); }
	void Empty() { Map.Empty(); Order.Empty(); }

private:
	int32 MaxEntries;
	TMap<FString, FString> Map;
	TArray<FString> Order; // insertion order (for FIFO eviction)
};
