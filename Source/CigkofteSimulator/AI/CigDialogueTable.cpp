#include "AI/CigDialogueTable.h"
#include "Core/CigLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"

namespace
{
	struct FCigDialogueStore
	{
		// Bucket key -> the lines in that bucket. Lookup has to be constant time:
		// it runs on every service.
		TMap<FString, TArray<FCigDialogueRow>> ByKey;
		int32 Total = 0;
		bool bLoaded = false;

		void Load()
		{
			ByKey.Reset();
			Total = 0;
			bLoaded = true;

			const FString Path = FPaths::ProjectDir() / TEXT("Config/Dialogue/Lines.csv");
			if (!FPaths::FileExists(Path))
			{
				// The table may not have been generated yet; the game runs on canned lines.
				UE_LOG(LogCig, Log, TEXT("Diyalog tablosu yok; hazır cümleler kullanılacak."));
				return;
			}

			FString Raw;
			if (!FFileHelper::LoadFileToString(Raw, *Path))
			{
				UE_LOG(LogCig, Warning, TEXT("Lines.csv okunamadı; hazır cümleler kullanılacak."));
				return;
			}

			const FCsvParser Parser(MoveTemp(Raw));
			const FCsvParser::FRows& Rows = Parser.GetRows();
			if (Rows.Num() < 2)
			{
				UE_LOG(LogCig, Warning, TEXT("Lines.csv boş."));
				return;
			}

			TMap<FString, int32> Cols;
			for (int32 i = 0; i < Rows[0].Num(); ++i)
			{
				if (Rows[0][i] && *Rows[0][i])
				{
					Cols.Add(FString(Rows[0][i]).TrimStartAndEnd(), i);
				}
			}

			auto Cell = [&Cols](const TArray<const TCHAR*>& R, const TCHAR* Name) -> FString
			{
				const int32* Idx = Cols.Find(Name);
				if (!Idx || !R.IsValidIndex(*Idx) || !R[*Idx])
				{
					return FString();
				}
				return FString(R[*Idx]).TrimStartAndEnd();
			};

			for (int32 r = 1; r < Rows.Num(); ++r)
			{
				FCigDialogueRow Row;
				Row.Key = Cell(Rows[r], TEXT("Key"));
				Row.TR = Cell(Rows[r], TEXT("TR"));
				Row.EN = Cell(Rows[r], TEXT("EN"));
				Row.Variant = FCString::Atoi(*Cell(Rows[r], TEXT("Variant")));

				// A row with no key or no Turkish text is useless; count it rather than
				// skipping silently, so a gap in the generated output is noticed.
				if (Row.Key.IsEmpty() || Row.TR.IsEmpty())
				{
					continue;
				}
				ByKey.FindOrAdd(Row.Key).Add(MoveTemp(Row));
				++Total;
			}

			UE_LOG(LogCig, Log, TEXT("Diyalog tablosu yüklendi: %d replik, %d kova."), Total, ByKey.Num());
		}
	};

	FCigDialogueStore& CigDialogueStore()
	{
		static FCigDialogueStore Data;
		if (!Data.bLoaded)
		{
			Data.Load();
		}
		return Data;
	}

	const TArray<FCigDialogueRow> GEmpty;
}

namespace CigDialogue
{
	const TArray<FCigDialogueRow>& LinesFor(const FString& BucketKey)
	{
		if (const TArray<FCigDialogueRow>* Found = CigDialogueStore().ByKey.Find(BucketKey))
		{
			return *Found;
		}
		return GEmpty;
	}

	int32 TotalLines()
	{
		return CigDialogueStore().Total;
	}

	void Reload()
	{
		CigDialogueStore().Load();
	}
}
