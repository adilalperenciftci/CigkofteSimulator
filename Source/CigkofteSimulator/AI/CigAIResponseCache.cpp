#include "AI/CigAIResponseCache.h"

bool FCigAIResponseCache::TryGet(const FString& Key, FString& OutLine) const
{
	if (const FString* Found = Map.Find(Key))
	{
		OutLine = *Found;
		return true;
	}
	return false;
}

void FCigAIResponseCache::Put(const FString& Key, const FString& Line)
{
	if (Map.Contains(Key))
	{
		Map[Key] = Line;
		return;
	}

	while (Order.Num() >= MaxEntries && Order.Num() > 0)
	{
		const FString Oldest = Order[0];
		Order.RemoveAt(0);
		Map.Remove(Oldest);
	}

	Map.Add(Key, Line);
	Order.Add(Key);
}
