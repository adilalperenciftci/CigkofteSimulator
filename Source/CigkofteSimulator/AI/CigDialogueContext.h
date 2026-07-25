#pragma once

#include "CoreMinimal.h"
#include "Core/CigkofteTypes.h"

// The structured context for one customer service, as sent to the AI.
// GameMode/CustomerSystem fills this in and the provider turns it into a
// one-sentence customer reaction. No field carries private or sensitive data.
struct FCigDialogueContext
{
	// Customer identity / personality
	uint16 Traits = 0;              // ECigTrait bit mask
	bool bVIP = false;
	bool bRegular = false;          // is this a regular
	FString CustomerName;           // their name if a regular, otherwise empty
	int32 PastVisits = 0;           // history as a regular
	int32 RememberedMistakes = 0;   // past mistakes

	// Outcome of this service
	float Accuracy = 0.f;           // 0-100 order accuracy
	float Quality = 0.f;            // 0-100 wrap quality
	float Hygiene = 100.f;          // 0-100 counter hygiene
	float PatienceFrac = 1.f;       // 0-1 (1 = served immediately)
	ECigSpice ServedSpice = ECigSpice::Orta;
	bool bWantedAyran = false;
	bool bGotAyran = false;
	int32 FinalPrice = 0;
	bool bTipped = false;

	// The broad category the offline side returns when the AI request fails or
	// is off. The cache key for the same context is derived from it too.
	enum class EMood : uint8 { Delighted, Satisfied, Mixed, Unhappy, Angry };

	EMood Mood() const
	{
		if (Accuracy >= 90.f && Quality >= 80.f) { return EMood::Delighted; }
		if (Accuracy >= 75.f && Quality >= 60.f) { return EMood::Satisfied; }
		if (Accuracy < 45.f || Quality < 35.f)   { return EMood::Angry; }
		if (Accuracy < 65.f || Quality < 55.f)   { return EMood::Unhappy; }
		return EMood::Mixed;
	}

	// Human-readable trait list (feeds the AI prompt and the offline choice).
	FString TraitSummary() const
	{
		TArray<FString> Names;
		for (int32 i = 0; i < CigTraitCount; ++i)
		{
			const ECigTrait T = (ECigTrait)(1 << i);
			if ((Traits & (uint16)T) != 0)
			{
				Names.Add(CigTraitName(T));
			}
		}
		return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : TEXT("sıradan");
	}

	// A customer can carry several traits, but exactly one "identity" should
	// drive the line. Priority follows narrative distinctiveness: being an
	// undercover critic says more than being a student.
	//
	// This reduction keeps the bucket count finite: the full bit mask would mean
	// 2^14 buckets, whereas a dominant trait gives 15 (14 traits plus none).
	static int32 DominantTraitIndex(uint16 TraitMask)
	{
		static const ECigTrait Priority[] = {
			ECigTrait::SecretCritic, ECigTrait::Influencer, ECigTrait::Regular,
			ECigTrait::QualityFocused, ECigTrait::HygieneSensitive, ECigTrait::PriceSensitive,
			ECigTrait::Generous, ECigTrait::Impatient, ECigTrait::Patient,
			ECigTrait::SpicyFoodLover, ECigTrait::Tourist, ECigTrait::Student,
			ECigTrait::Family, ECigTrait::Indecisive
		};
		for (const ECigTrait T : Priority)
		{
			if ((TraitMask & (uint16)T) != 0)
			{
				// Bit position = row order in the balance table.
				return FMath::FloorLog2((uint32)T);
			}
		}
		return NoTraitIndex;
	}

	// The "no traits" bucket. 0-13 are the real traits.
	static constexpr int32 NoTraitIndex = 14;
	// Total buckets: 5 moods x 15 traits x 2^5 booleans (VIP, regular, ayran,
	// hygiene, patience) = 2400.
	static constexpr int32 BucketCount = 5 * 15 * 32;

	int32 DominantTraitIndex() const { return DominantTraitIndex(Traits); }

	// The bucket key. Both the runtime AI cache and the pre-generated line table
	// are addressed by it, so the two share one space. Constantly varying fields
	// such as price are deliberately left out.
	static FString BucketKey(int32 MoodIdx, int32 TraitIdx, bool bVIP, bool bRegular,
		bool bAyranOk, bool bHygieneLow, bool bPatienceLow)
	{
		return FString::Printf(TEXT("m%d_t%d_v%d_r%d_a%d_h%d_p%d"),
			MoodIdx, TraitIdx,
			bVIP ? 1 : 0, bRegular ? 1 : 0,
			bAyranOk ? 1 : 0, bHygieneLow ? 1 : 0, bPatienceLow ? 1 : 0);
	}

	FString CacheKey() const
	{
		return BucketKey((int32)Mood(), DominantTraitIndex(), bVIP, bRegular,
			bWantedAyran == bGotAyran, Hygiene < 50.f, PatienceFrac < 0.25f);
	}
};

// The one-sentence reply from the AI or offline provider.
struct FCigDialogueResult
{
	FString Line;
	bool bFromAI = false;   // true: came from the provider, false: offline fallback
	bool bCached = false;   // true: served from the cache
};

DECLARE_DELEGATE_OneParam(FCigDialogueDelegate, const FCigDialogueResult&);
