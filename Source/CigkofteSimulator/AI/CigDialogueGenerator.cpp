// The first step of the dialogue pipeline: walk every bucket, build a prompt
// for each, and write them out as JSONL. This command does NOT call the API.
//
// The split is deliberate: counting buckets and building prompts is
// deterministic and free, whereas calling the API costs money, can take hours
// and can die halfway. Separated, the output can be reviewed, a run can be
// stopped and resumed, and generation never locks up the editor session.
//
// Flow:
//   1. From the editor console:  CigGenerateDialogue
//        -> Saved/Dialogue/prompts.jsonl
//   2. python Tools/generate_dialogue.py   (needs ANTHROPIC_API_KEY)
//        -> Config/Dialogue/Lines.csv
//   3. The CSV is reviewed and committed; the game reads it (AI/CigDialogueTable.h).

#include "Debug/CigCheatManager.h"
#include "AI/CigDialogueContext.h"
#include "AI/CigDialogueTable.h"
#include "Core/CigkofteTypes.h"
#include "Core/CigLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR

namespace
{
	const TCHAR* MoodName(int32 Mood)
	{
		switch (Mood)
		{
		case 0:  return TEXT("çok memnun");
		case 1:  return TEXT("memnun");
		case 2:  return TEXT("kararsız/orta");
		case 3:  return TEXT("memnuniyetsiz");
		default: return TEXT("çok kızgın");
		}
	}

	FString TraitName(int32 TraitIdx)
	{
		if (TraitIdx == FCigDialogueContext::NoTraitIndex)
		{
			return TEXT("belirgin bir özelliği olmayan sıradan");
		}
		return CigTraitName((ECigTrait)(1 << TraitIdx));
	}

	// A human-readable description of the bucket, so the model can ground the line.
	FString DescribeBucket(int32 Mood, int32 TraitIdx, bool bVIP, bool bRegular,
		bool bAyranOk, bool bHygieneLow, bool bPatienceLow)
	{
		TArray<FString> Parts;
		Parts.Add(FString::Printf(TEXT("Müşteri tipi: %s"), *TraitName(TraitIdx)));
		Parts.Add(FString::Printf(TEXT("Ruh hali: %s"), MoodName(Mood)));
		if (bVIP)         { Parts.Add(TEXT("Ünlü/VIP bir müşteri")); }
		if (bRegular)     { Parts.Add(TEXT("Dükkânın müdavimi, seni tanıyor")); }
		if (!bAyranOk)    { Parts.Add(TEXT("İstediği ayran verilmedi (ya da istemediği hâlde verildi)")); }
		if (bHygieneLow)  { Parts.Add(TEXT("Tezgâh gözle görülür kirli")); }
		if (bPatienceLow) { Parts.Add(TEXT("Çok uzun bekletildi, sabrı taşmak üzereydi")); }
		return FString::Join(Parts, TEXT(". "));
	}

	FString BuildPrompt(const FString& Bucket, int32 Mood, int32 TraitIdx, bool bVIP,
		bool bRegular, bool bAyranOk, bool bHygieneLow, bool bPatienceLow, int32 VariantCount)
	{
		return FString::Printf(
			TEXT("Bir Türk mahalle çiğköftecisinde geçen oyun için müşteri repliği yazıyorsun.\n")
			TEXT("Durum: %s.\n\n")
			TEXT("Bu duruma uygun, birbirinden farklı %d adet TEK CÜMLELİK müşteri repliği yaz. ")
			TEXT("Replikler günlük konuşma Türkçesiyle, samimi ve mahalle ağzına yakın olsun; ")
			TEXT("küfür, hakaret ve marka adı geçmesin. Her replik en fazla 12 kelime olsun.\n")
			TEXT("Her replik için bir de doğal İngilizce karşılığını ver (birebir çeviri değil, ")
			TEXT("aynı tonu taşıyan karşılık).\n\n")
			TEXT("Yanıtı yalnızca şu JSON biçiminde ver, başka hiçbir şey yazma:\n")
			TEXT("{\"lines\": [{\"tr\": \"...\", \"en\": \"...\"}, ...]}"),
			*DescribeBucket(Mood, TraitIdx, bVIP, bRegular, bAyranOk, bHygieneLow, bPatienceLow),
			VariantCount);
	}
}

void UCigCheatManager::CigGenerateDialogue()
{
	// Several lines per request: one request per bucket with 4 variants rather
	// than 2400 x 1, so roughly 2400 requests and 9,600 lines.
	constexpr int32 VariantsPerBucket = 4;

	FString Out;
	int32 Buckets = 0;

	for (int32 Mood = 0; Mood < 5; ++Mood)
	{
		for (int32 TraitIdx = 0; TraitIdx <= FCigDialogueContext::NoTraitIndex; ++TraitIdx)
		{
			// Five booleans: VIP, regular, ayran correct, hygiene low, patience nearly out.
			for (int32 Flags = 0; Flags < 32; ++Flags)
			{
				const bool bVIP         = (Flags & 1) != 0;
				const bool bRegular     = (Flags & 2) != 0;
				const bool bAyranOk     = (Flags & 4) != 0;
				const bool bHygieneLow  = (Flags & 8) != 0;
				const bool bPatienceLow = (Flags & 16) != 0;

				const FString Key = FCigDialogueContext::BucketKey(
					Mood, TraitIdx, bVIP, bRegular, bAyranOk, bHygieneLow, bPatienceLow);

				const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetStringField(TEXT("key"), Key);
				Obj->SetNumberField(TEXT("variants"), VariantsPerBucket);
				Obj->SetStringField(TEXT("prompt"), BuildPrompt(
					Key, Mood, TraitIdx, bVIP, bRegular, bAyranOk, bHygieneLow, bPatienceLow, VariantsPerBucket));

				FString Line;
				const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
					TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
				FJsonSerializer::Serialize(Obj, Writer);
				Out += Line + LINE_TERMINATOR;
				++Buckets;
			}
		}
	}

	const FString Path = FPaths::ProjectDir() / TEXT("Saved/Dialogue/prompts.jsonl");
	if (!FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogCig, Error, TEXT("prompts.jsonl yazılamadı: %s"), *Path);
		return;
	}

	UE_LOG(LogCig, Log, TEXT("%d kova için istem yazıldı: %s"), Buckets, *Path);
	UE_LOG(LogCig, Log, TEXT("Sonraki adım: python Tools/generate_dialogue.py"));
}

void UCigCheatManager::CigDialogueStats()
{
	UE_LOG(LogCig, Log, TEXT("Diyalog tablosu: %d replik yüklü (kova uzayı %d)."),
		CigDialogue::TotalLines(), FCigDialogueContext::BucketCount);
}

#endif // WITH_EDITOR
