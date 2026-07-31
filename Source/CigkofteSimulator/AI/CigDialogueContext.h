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

	// What was asked for against what was handed over. Both halves are needed:
	// a customer can only complain about the wrong wrap if the context knows the
	// wrap was wrong. These used to be filled from the order alone, with the
	// delivered side copied from the requested side, so every service looked
	// like a service that went right.
	ECigSpice RequestedSpice = ECigSpice::Orta;
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

	// Was the order actually fulfilled as asked. Kept as one predicate because
	// the bucket key has room for one bit, and because a customer who got the
	// wrong wrap reacts to that fact rather than to which detail was wrong.
	bool OrderMatched() const
	{
		return bWantedAyran == bGotAyran && RequestedSpice == ServedSpice;
	}

	// The specific mistakes, in words, for the AI prompt. Empty when the order
	// was right. The prompt carries scores otherwise, and a percentage does not
	// tell the customer what to complain about.
	FString MistakeSummary() const
	{
		TArray<FString> Notlar;
		if (RequestedSpice != ServedSpice)
		{
			Notlar.Add(FString::Printf(TEXT("%s istedi, %s verildi"),
				*CigSpiceName(RequestedSpice), *CigSpiceName(ServedSpice)));
		}
		if (bWantedAyran && !bGotAyran)
		{
			Notlar.Add(TEXT("ayran istedi, gelmedi"));
		}
		else if (!bWantedAyran && bGotAyran)
		{
			Notlar.Add(TEXT("istemediği hâlde ayran geldi"));
		}
		return FString::Join(Notlar, TEXT("; "));
	}

	// The bucket key. Both the runtime AI cache and the pre-generated line table
	// are addressed by it, so the two share one space. Constantly varying fields
	// such as price are deliberately left out.
	//
	// bOrderOk is the a-flag. Half the table is written for the case where it is
	// false, so it has to be able to be false: the caller used to copy the
	// delivered ayran from the requested one, which pinned it to true and left
	// 1200 of the 2400 buckets unreachable.
	static FString BucketKey(int32 MoodIdx, int32 TraitIdx, bool bVIP, bool bRegular,
		bool bOrderOk, bool bHygieneLow, bool bPatienceLow)
	{
		return FString::Printf(TEXT("m%d_t%d_v%d_r%d_a%d_h%d_p%d"),
			MoodIdx, TraitIdx,
			bVIP ? 1 : 0, bRegular ? 1 : 0,
			bOrderOk ? 1 : 0, bHygieneLow ? 1 : 0, bPatienceLow ? 1 : 0);
	}

	FString CacheKey() const
	{
		return BucketKey((int32)Mood(), DominantTraitIndex(), bVIP, bRegular,
			OrderMatched(), Hygiene < 50.f, PatienceFrac < 0.25f);
	}
};
