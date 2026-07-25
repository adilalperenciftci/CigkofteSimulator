#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CigDialogueTable.generated.h"

// Pre-generated customer lines.
//
// Why there is no API call at runtime: firing a request for every customer of
// every player is not a sustainable product - cost scales with the player
// count, latency lands in the gameplay, it fails without a connection, and it
// carries moderation risk. Instead the lines are generated in bulk during
// development and shipped as data: zero runtime cost, zero latency, works offline.
//
// The bucket key is the same one as FCigDialogueContext::BucketKey() (2400
// buckets). Generation flow:
//   1. `CigGenerateDialogue` in the editor -> Saved/Dialogue/prompts.jsonl
//   2. python Tools/generate_dialogue.py  (needs an API key)
//   3. The output is reviewed and committed as Config/Dialogue/Lines.csv
USTRUCT(BlueprintType)
struct FCigDialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	// Bucket key, matching FCigDialogueContext::CacheKey().
	UPROPERTY(EditAnywhere, Category = "Diyalog") FString Key;

	// Which variant within a bucket, so the same situation is not always the same line.
	UPROPERTY(EditAnywhere, Category = "Diyalog") int32 Variant = 0;

	UPROPERTY(EditAnywhere, Category = "Diyalog") FString TR;
	UPROPERTY(EditAnywhere, Category = "Diyalog") FString EN;
};

namespace CigDialogue
{
	// Lines matching a bucket. Returns an empty array when the table is missing
	// or the bucket is empty, at which point the caller falls back to canned lines.
	const TArray<FCigDialogueRow>& LinesFor(const FString& BucketKey);

	// Total number of lines in the table (debugging / verification).
	int32 TotalLines();

	// Re-reads Config/Dialogue/Lines.csv.
	void Reload();
}
