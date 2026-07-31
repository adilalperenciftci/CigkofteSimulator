#include "AI/CigOfflineDialogueProvider.h"
#include "AI/CigDialogueTable.h"
#include "Core/CigText.h"

namespace
{
	// Which column of the line table to read.
	//
	// This used to ask UE's culture (`-culture=en` or the system language),
	// which is not what the game's language menu writes to. On an English
	// Windows the shop spoke Turkish and the customers answered in English. The
	// menu, the HUD and the customers now all read one setting.
	bool bWantsEnglish()
	{
		return CigText::GetLanguage() == 1; // 0 Turkish, 1 English
	}

	// Canned lines per mood, keyed into Config/Text/Strings.csv. With the AI off
	// and no generated bucket, this is what the player hears, so it is player-
	// facing text and belongs in the table like the rest of it - it was written
	// straight into TEXT() here, in Turkish only, which is half of why an
	// English game answered in Turkish.
	const TArray<const TCHAR*>& MoodKeys(FCigDialogueContext::EMood Mood)
	{
		static const TArray<const TCHAR*> Delighted = {
			TEXT("dlg.mood.delighted.0"), TEXT("dlg.mood.delighted.1"),
			TEXT("dlg.mood.delighted.2"), TEXT("dlg.mood.delighted.3")
		};
		static const TArray<const TCHAR*> Satisfied = {
			TEXT("dlg.mood.satisfied.0"), TEXT("dlg.mood.satisfied.1"),
			TEXT("dlg.mood.satisfied.2")
		};
		static const TArray<const TCHAR*> Mixed = {
			TEXT("dlg.mood.mixed.0"), TEXT("dlg.mood.mixed.1"), TEXT("dlg.mood.mixed.2")
		};
		static const TArray<const TCHAR*> Unhappy = {
			TEXT("dlg.mood.unhappy.0"), TEXT("dlg.mood.unhappy.1"), TEXT("dlg.mood.unhappy.2")
		};
		static const TArray<const TCHAR*> Angry = {
			TEXT("dlg.mood.angry.0"), TEXT("dlg.mood.angry.1"), TEXT("dlg.mood.angry.2")
		};

		switch (Mood)
		{
		case FCigDialogueContext::EMood::Delighted: return Delighted;
		case FCigDialogueContext::EMood::Satisfied: return Satisfied;
		case FCigDialogueContext::EMood::Mixed:     return Mixed;
		case FCigDialogueContext::EMood::Unhappy:   return Unhappy;
		default:                                    return Angry;
		}
	}
}

FString FCigOfflineDialogueProvider::PickLine(const FCigDialogueContext& Context)
{
	// Picking a line changes no game state, so it is left on the global RNG to
	// keep it out of the deterministic stream (see Core/CigRandomSubsystem.h).

	// The pre-generated table first: far more context-aware, and bilingual.
	const TArray<FCigDialogueRow>& Rows = CigDialogue::LinesFor(Context.CacheKey());
	if (Rows.Num() > 0)
	{
		const FCigDialogueRow& Picked = Rows[FMath::RandRange(0, Rows.Num() - 1)];
		const FString& Line = bWantsEnglish() && !Picked.EN.IsEmpty() ? Picked.EN : Picked.TR;
		if (!Line.IsEmpty())
		{
			return Line;
		}
	}

	// No table, or this bucket not generated yet: fall back to the mood pool.
	// The special-case rules below still apply on either path.
	const TArray<const TCHAR*>& Keys = MoodKeys(Context.Mood());
	FString Base = Keys.Num() > 0
		? CigText::Get(Keys[FMath::RandRange(0, Keys.Num() - 1)])
		: TEXT("...");

	// A small personal touch for certain traits.
	if (Context.bRegular && !Context.CustomerName.IsEmpty() && Context.Mood() >= FCigDialogueContext::EMood::Mixed)
	{
		// an unhappy regular may bring up the past
		if (Context.RememberedMistakes > 0 && Context.Mood() >= FCigDialogueContext::EMood::Unhappy)
		{
			return CigText::Get(TEXT("dlg.regular.repeatmistake"));
		}
	}
	if ((Context.Traits & (uint16)ECigTrait::HygieneSensitive) != 0 && Context.Hygiene < 50.f)
	{
		return CigText::Get(TEXT("dlg.trait.hygiene"));
	}
	if ((Context.Traits & (uint16)ECigTrait::PriceSensitive) != 0 && Context.Mood() <= FCigDialogueContext::EMood::Mixed)
	{
		return CigText::Get(TEXT("dlg.trait.price"));
	}
	return Base;
}
