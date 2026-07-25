#include "AI/CigOfflineDialogueProvider.h"
#include "AI/CigDialogueTable.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

namespace
{
	// Is the game currently in English? Read UE's culture setting (`-culture=en`
	// or the system language). The line table carries both languages, so
	// translation needs no extra work.
	bool bWantsEnglish()
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
		return Culture.Equals(TEXT("en"), ESearchCase::IgnoreCase);
	}

	// Canned lines per mood. With the AI off the player still gets a sensible reaction.
	const TArray<FString>& MoodLines(FCigDialogueContext::EMood Mood)
	{
		static const TArray<FString> Delighted = {
			TEXT("Eline sağlık, tam kıvamında olmuş!"),
			TEXT("Vallahi mahallenin en iyisi, helal olsun."),
			TEXT("İşte bunu arıyordum, üstü kalsın."),
			TEXT("Bu acı, bu kıvam... on numara!")
		};
		static const TArray<FString> Satisfied = {
			TEXT("Güzeldi, teşekkürler."),
			TEXT("İdare eder, yine bekleriz."),
			TEXT("Fena değil, sağ ol usta.")
		};
		static const TArray<FString> Mixed = {
			TEXT("Eh işte, biraz daha özenseydin."),
			TEXT("Olmuş ama harika değil."),
			TEXT("Ortalama, bir dahakine daha iyi olsun.")
		};
		static const TArray<FString> Unhappy = {
			TEXT("Bu sefer pek olmamış ya."),
			TEXT("Beklediğim gibi değildi açıkçası."),
			TEXT("Hmm, keyfim kaçtı biraz.")
		};
		static const TArray<FString> Angry = {
			TEXT("Bu ne böyle?! Param geri!"),
			TEXT("Bir daha ayak basmam buraya!"),
			TEXT("Rezalet, hiç beğenmedim.")
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

	// No table, or this bucket not generated yet: fall back to the in-code mood
	// pool. The special-case rules below still apply on either path.
	const TArray<FString>& Lines = MoodLines(Context.Mood());
	FString Base = Lines.Num() > 0 ? Lines[FMath::RandRange(0, Lines.Num() - 1)] : TEXT("...");

	// A small personal touch for certain traits.
	if (Context.bRegular && !Context.CustomerName.IsEmpty() && Context.Mood() >= FCigDialogueContext::EMood::Mixed)
	{
		// an unhappy regular may bring up the past
		if (Context.RememberedMistakes > 0 && Context.Mood() >= FCigDialogueContext::EMood::Unhappy)
		{
			return TEXT("Yine mi? Geçen sefer de böyle olmuştu...");
		}
	}
	if ((Context.Traits & (uint16)ECigTrait::HygieneSensitive) != 0 && Context.Hygiene < 50.f)
	{
		return TEXT("Şu tezgahın hali ne böyle, iğrenç!");
	}
	if ((Context.Traits & (uint16)ECigTrait::PriceSensitive) != 0 && Context.Mood() <= FCigDialogueContext::EMood::Mixed)
	{
		return TEXT("Bu fiyata bu mu? Pahalı geldi.");
	}
	return Base;
}

void FCigOfflineDialogueProvider::RequestLine(const FCigDialogueContext& Context, FCigDialogueDelegate OnComplete)
{
	FCigDialogueResult Result;
	Result.Line = PickLine(Context);
	Result.bFromAI = false;
	OnComplete.ExecuteIfBound(Result);
}
