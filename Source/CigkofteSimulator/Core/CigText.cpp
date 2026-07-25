#include "Core/CigText.h"
#include "Core/CigLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"

namespace
{
	const TCHAR* GLanguageNames[] = { TEXT("Türkçe"), TEXT("English") };
	constexpr int32 GLanguageCount = UE_ARRAY_COUNT(GLanguageNames);

	struct FCigTextStore
	{
		// Key -> one string per language. Every lookup hits the TMap; the HUD
		// redraws each frame, but the table is small and measured fine.
		TMap<FString, TArray<FString>> ByKey;
		TSet<FString> Warned;
		int32 Language = 0;
		bool bLoaded = false;

		void Load()
		{
			ByKey.Reset();
			Warned.Reset();
			bLoaded = true;

			const FString Path = FPaths::ProjectDir() / TEXT("Config/Text/Strings.csv");
			FString Raw;
			if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Raw, *Path))
			{
				UE_LOG(LogCig, Warning, TEXT("Strings.csv okunamadı; arayüzde anahtarlar görünecek."));
				return;
			}

			const FCsvParser Parser(MoveTemp(Raw));
			const FCsvParser::FRows& Rows = Parser.GetRows();
			if (Rows.Num() < 2)
			{
				UE_LOG(LogCig, Warning, TEXT("Strings.csv boş."));
				return;
			}

			// Header is Key,TR,EN - column order must match GLanguageNames.
			int32 KeyCol = -1;
			TArray<int32> LangCols;
			LangCols.Init(-1, GLanguageCount);
			static const TCHAR* ColNames[] = { TEXT("TR"), TEXT("EN") };
			for (int32 i = 0; i < Rows[0].Num(); ++i)
			{
				const FString H = FString(Rows[0][i] ? Rows[0][i] : TEXT("")).TrimStartAndEnd();
				if (H == TEXT("Key")) { KeyCol = i; }
				for (int32 L = 0; L < GLanguageCount; ++L)
				{
					if (H == ColNames[L]) { LangCols[L] = i; }
				}
			}
			if (KeyCol < 0)
			{
				UE_LOG(LogCig, Warning, TEXT("Strings.csv'de 'Key' sütunu yok."));
				return;
			}

			for (int32 r = 1; r < Rows.Num(); ++r)
			{
				const TArray<const TCHAR*>& R = Rows[r];
				if (!R.IsValidIndex(KeyCol) || !R[KeyCol] || !*R[KeyCol])
				{
					continue;
				}
				const FString Key = FString(R[KeyCol]).TrimStartAndEnd();

				TArray<FString> Values;
				Values.Reserve(GLanguageCount);
				for (int32 L = 0; L < GLanguageCount; ++L)
				{
					const int32 Col = LangCols[L];
					Values.Add(R.IsValidIndex(Col) && R[Col] ? FString(R[Col]) : FString());
				}
				ByKey.Add(Key, MoveTemp(Values));
			}

			UE_LOG(LogCig, Log, TEXT("Metin tablosu yüklendi: %d anahtar."), ByKey.Num());
		}
	};

	FCigTextStore& CigTextStore()
	{
		static FCigTextStore Data;
		if (!Data.bLoaded)
		{
			Data.Load();
		}
		return Data;
	}
}

namespace CigText
{
	FString Get(const TCHAR* Key)
	{
		FCigTextStore& S = CigTextStore();
		if (const TArray<FString>* Values = S.ByKey.Find(Key))
		{
			// Fall back to Turkish when the selected language has nothing, so a
			// missing translation shows readable text instead of a blank.
			if (Values->IsValidIndex(S.Language) && !(*Values)[S.Language].IsEmpty())
			{
				return (*Values)[S.Language];
			}
			if (Values->Num() > 0 && !(*Values)[0].IsEmpty())
			{
				return (*Values)[0];
			}
		}

		if (!S.Warned.Contains(Key))
		{
			S.Warned.Add(Key);
			UE_LOG(LogCig, Warning, TEXT("Metin anahtarı bulunamadı: %s"), Key);
		}
		return FString(Key);
	}

	int32 GetLanguage() { return CigTextStore().Language; }

	void SetLanguage(int32 Language)
	{
		CigTextStore().Language = FMath::Clamp(Language, 0, GLanguageCount - 1);
	}

	int32 LanguageCount() { return GLanguageCount; }

	const TCHAR* LanguageName(int32 Language)
	{
		return GLanguageNames[FMath::Clamp(Language, 0, GLanguageCount - 1)];
	}

	void Reload() { CigTextStore().Load(); }

	int32 KeyCount() { return CigTextStore().ByKey.Num(); }
}
