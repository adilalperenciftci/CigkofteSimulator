#include "Game/CigReleaseSelfTest.h"

#include "Game/CigkofteGameMode.h"
#include "Save/CigSaveSubsystem.h"
#include "Save/CigSaveGame.h"
#include "World/CigWorldBuilder.h"
#include "World/CigMeshLibrary.h"
#include "Audio/CigAudioSubsystem.h"
#include "Core/CigText.h"
#include "Core/CigBalance.h"
#include "Core/CigkofteTypes.h"
#include "Core/CigLog.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CheatManager.h"

namespace
{
	// One check: a stable name for the report and what its failure costs a player.
	struct FCheck
	{
		const TCHAR* Name;
		bool bPassed;
		FString Detail;
	};

	// A floor, not an equality.
	//
	// The table has more keys than this and gains more with every string added;
	// asserting the exact count would make writing a line of dialogue fail the
	// release check. What this catches is the failure that actually happened once:
	// Strings.csv not staged at all, so every label on screen was its own key.
	constexpr int32 MinimumTextKeys = 500;

	const TCHAR* const HeaderLine = TEXT("CIGRELEASESELFTEST v1\n");

	// Where the verdict goes.
	//
	// -CigReleaseSelfTestOut=<path> wins, and the harness always passes it.
	// FPaths::ProjectSavedDir() is the fallback and is not a fixed location: it
	// sits beside the executable in Development and redirects to
	// %LOCALAPPDATA%\<Project>\Saved in Shipping. A caller that has to guess which
	// one applies gets it wrong, and reads a passing run as a build that does not
	// support the mode - which is what happened before this argument existed.
	//
	// The argument is honoured only inside this mode - Run() is the only caller and
	// InitGame reaches it only through IsRequested() - and it is vetted rather than
	// trusted. A local user may point it wherever they like; what an ordinary
	// packaging run must never be able to do is write over data the project reads
	// back, so Config, Content and Source are refused, and so is a .sav extension:
	// the report is not a save and nothing that scans for saves should ever find it.
	bool ResolveReportPath(FString& OutPath, FString& OutError)
	{
		FString Raw;
		switch (CigReleaseSelfTest::ParseOutputArgument(FCommandLine::Get(), Raw, OutError))
		{
		case CigReleaseSelfTest::EOutputArgumentState::Invalid:
			return false;
		case CigReleaseSelfTest::EOutputArgumentState::NotProvided:
			Raw = FPaths::ProjectSavedDir() / TEXT("CigReleaseSelfTest.txt");
			break;
		case CigReleaseSelfTest::EOutputArgumentState::Provided:
			break;
		}

		FString Full = FPaths::ConvertRelativePathToFull(Raw);
		FPaths::NormalizeFilename(Full);
		FPaths::CollapseRelativeDirectories(Full);

		if (FPaths::GetCleanFilename(Full).IsEmpty())
		{
			OutError = FString::Printf(TEXT("dosya adi yok: %s"), *Full);
			return false;
		}
		if (FPaths::GetExtension(Full).Equals(TEXT("sav"), ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT(".sav uzantisi reddedildi: %s"), *Full);
			return false;
		}

		FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(ProjectDir);
		static const TCHAR* const Guarded[] = { TEXT("Config"), TEXT("Content"), TEXT("Source") };
		for (const TCHAR* Dir : Guarded)
		{
			FString Forbidden = ProjectDir / Dir;
			FPaths::NormalizeDirectoryName(Forbidden);
			if (Full.Equals(Forbidden, ESearchCase::IgnoreCase)
				|| Full.StartsWith(Forbidden + TEXT("/"), ESearchCase::IgnoreCase))
			{
				OutError = FString::Printf(TEXT("proje verisinin icine yazilamaz: %s"), *Full);
				return false;
			}
		}

		OutPath = Full;
		return true;
	}

	// The report is built in <path>.tmp and renamed over the final name, so the
	// harness either finds a complete verdict or finds nothing. A run that dies
	// half way therefore cannot leave a truncated file that reads as a result, and
	// - the reason this matters most - a previous run's PASS cannot survive a new
	// run that failed to write: the final name is removed before the checks start.
	//
	// Every write is checked. SaveStringToFile returning false was ignored before,
	// which made "the disk is full" and "every check passed" the same observable
	// outcome from outside.
	struct FReportWriter
	{
		FString FinalPath;
		FString TempPath;
		FString Error;

		bool Begin()
		{
			if (!ResolveReportPath(FinalPath, Error))
			{
				return false;
			}
			TempPath = FinalPath + TEXT(".tmp");

			IFileManager& Files = IFileManager::Get();
			const FString Dir = FPaths::GetPath(FinalPath);
			if (!Dir.IsEmpty() && !Files.DirectoryExists(*Dir) && !Files.MakeDirectory(*Dir, true))
			{
				Error = FString::Printf(TEXT("dizin olusturulamadi: %s"), *Dir);
				return false;
			}

			// Order matters: the stale verdict goes before anything can fail. A
			// failed deletion is itself unwritable; continuing would allow an old
			// PASS to survive a run that returns ReportUnwritable.
			auto DeleteStale = [this, &Files](const FString& Path, const TCHAR* What)
			{
				if (Files.FileExists(*Path)
					&& (!Files.Delete(*Path, false, true, true) || Files.FileExists(*Path)))
				{
					Error = FString::Printf(TEXT("eski %s silinemedi: %s"), What, *Path);
					return false;
				}
				return true;
			};
			if (!DeleteStale(FinalPath, TEXT("rapor")) || !DeleteStale(TempPath, TEXT("gecici rapor")))
			{
				return false;
			}

			// The sentinel proves the path is writable before the checks run
			// against it. Writing only at the end would report a full disk as a
			// self-test that never happened.
			if (!FFileHelper::SaveStringToFile(FString(HeaderLine), *TempPath,
					FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Error = FString::Printf(TEXT("baslik yazilamadi: %s"), *TempPath);
				return false;
			}
			return true;
		}

		bool Commit(const FString& Body)
		{
			if (!FFileHelper::SaveStringToFile(Body, *TempPath,
					FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Error = FString::Printf(TEXT("rapor yazilamadi: %s"), *TempPath);
				return false;
			}
			// Move replaces on Windows, which is the only platform this project
			// packages for; no attempt is made to be atomic anywhere else.
			if (!IFileManager::Get().Move(*FinalPath, *TempPath, true, true))
			{
				// A failed move must not expose a partial or stale final verdict.
				IFileManager::Get().Delete(*FinalPath, false, true, true);
				Error = FString::Printf(TEXT("rapor yerine tasinamadi: %s"), *FinalPath);
				return false;
			}
			if (!IFileManager::Get().FileExists(*FinalPath)
				|| IFileManager::Get().FileExists(*TempPath))
			{
				IFileManager::Get().Delete(*FinalPath, false, true, true);
				Error = FString::Printf(TEXT("rapor tasimasi dogrulanamadi: %s"), *FinalPath);
				return false;
			}
			return true;
		}
	};

	FString FormatReport(const TArray<FCheck>& Checks, int32 FailedIndex)
	{
		FString Out = HeaderLine;
		for (const FCheck& C : Checks)
		{
			Out += FString::Printf(TEXT("%s  %s%s\n"),
				C.bPassed ? TEXT("PASS") : TEXT("FAIL"), C.Name,
				C.Detail.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  (%s)"), *C.Detail));
		}
		Out += FString::Printf(TEXT("RESULT %s %d\n"), FailedIndex == 0 ? TEXT("PASS") : TEXT("FAIL"), FailedIndex);
		return Out;
	}

}

CigReleaseSelfTest::EOutputArgumentState CigReleaseSelfTest::ParseOutputArgument(
	const TCHAR* CommandLine, FString& OutValue, FString& OutError)
{
	OutValue.Reset();
	OutError.Reset();

	int32 MatchCount = 0;
	bool bHadAssignment = false;
	FString RawValue;
	// Validate quoting and the overall CLI grammar first. Token() below is then
	// used for this one option because, unlike a value parser, it preserves the
	// boundary between `-Option=` and a following switch.
	const FParse::FGrammarBasedParseResult ParseResult = FParse::GrammarBasedCLIParse(
		CommandLine ? CommandLine : TEXT(""), [](FStringView, FStringView) {});

	if (ParseResult.ErrorCode != FParse::EGrammarBasedParseErrorCode::Succeeded)
	{
		OutError = TEXT("komut satiri bicimi gecersiz");
		return EOutputArgumentState::Invalid;
	}

	const FString Option = TEXT("-CigReleaseSelfTestOut");
	const TCHAR* Cursor = CommandLine ? CommandLine : TEXT("");
	while (*Cursor)
	{
		FString Token;
		if (!FParse::Token(Cursor, Token, false))
		{
			break;
		}

		const bool bBare = Token.Equals(Option, ESearchCase::IgnoreCase);
		const bool bAssigned = Token.Len() > Option.Len()
			&& Token[Option.Len()] == TEXT('=')
			&& Token.Left(Option.Len()).Equals(Option, ESearchCase::IgnoreCase);
		if (!bBare && !bAssigned)
		{
			continue;
		}

		++MatchCount;
		if (MatchCount == 1)
		{
			bHadAssignment = bAssigned;
			RawValue = bAssigned ? Token.Mid(Option.Len() + 1) : FString();
		}
	}
	if (MatchCount == 0)
	{
		return EOutputArgumentState::NotProvided;
	}
	if (MatchCount != 1)
	{
		OutError = TEXT("-CigReleaseSelfTestOut birden fazla verilemez");
		return EOutputArgumentState::Invalid;
	}
	if (!bHadAssignment)
	{
		OutError = TEXT("-CigReleaseSelfTestOut deger atamasi olmadan verildi");
		return EOutputArgumentState::Invalid;
	}

	RawValue.TrimStartAndEndInline();
	RawValue.TrimQuotesInline();
	RawValue.TrimStartAndEndInline();
	if (RawValue.IsEmpty())
	{
		OutError = TEXT("-CigReleaseSelfTestOut verildi ama bos");
		return EOutputArgumentState::Invalid;
	}

	OutValue = MoveTemp(RawValue);
	return EOutputArgumentState::Provided;
}

bool CigReleaseSelfTest::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("CigReleaseSelfTest"));
}

int32 CigReleaseSelfTest::Run(ACigkofteGameMode& GameMode)
{
	// Before the checks, so a path that cannot be written is reported as that
	// rather than as ten checks whose verdict nobody can read. The harness treats
	// an absent report as a failure, and this makes the exit status agree with it.
	FReportWriter Report;
	if (!Report.Begin())
	{
		UE_LOG(LogCig, Error, TEXT("Sürüm öz-testi raporu hazırlanamadı: %s"), *Report.Error);
		return ReportUnwritable;
	}

	TArray<FCheck> Checks;
	auto Add = [&Checks](const TCHAR* Name, bool bPassed, const FString& Detail = FString())
	{
		Checks.Add({ Name, bPassed, Detail });
	};

	// 1. The systems exist. A null one is a rule engine that is simply absent:
	//    orders never price, or the day never ends.
	{
		int32 Count = 0;
		const bool bOk = GameMode.HasAllSystems(Count);
		Add(TEXT("sistemler"), bOk, FString::Printf(TEXT("%d sistem"), Count));
	}

	// 2. The world was built. Without these the player spawns into an empty street.
	{
		const bool bBuilt = GameMode.WorldBuilder
			&& GameMode.WorldBuilder->FindStation(ECigStation::Servis) != nullptr
			&& GameMode.WorldBuilder->FindStation(ECigStation::Yogurma) != nullptr;
		Add(TEXT("dunya"), bBuilt, bBuilt ? TEXT("") : TEXT("servis veya yogurma istasyonu yok"));
	}

	// 3. The text table is staged and parsed.
	{
		const int32 Keys = CigText::KeyCount();
		Add(TEXT("metin-tablosu"), Keys >= MinimumTextKeys, FString::Printf(TEXT("%d anahtar"), Keys));
	}

	// 4/5. Both languages resolve, and resolve differently.
	//
	// The second half carries the check. CigText::Get falls back to Turkish when
	// the English cell is blank, so a missing or column-shifted EN translation
	// does not come back empty - it comes back Turkish. Equality with the Turkish
	// string is exactly how that failure presents.
	{
		const int32 Previous = CigText::GetLanguage();
		CigText::SetLanguage(0);
		const FString Tr = CigText::Get(TEXT("settings.title"));
		CigText::SetLanguage(1);
		const FString En = CigText::Get(TEXT("settings.title"));
		CigText::SetLanguage(Previous);

		const bool bTr = !Tr.IsEmpty() && Tr != TEXT("settings.title");
		Add(TEXT("metin-tr"), bTr, bTr ? TEXT("") : TEXT("settings.title cozulmedi"));

		const bool bEn = !En.IsEmpty() && En != TEXT("settings.title") && En != Tr;
		Add(TEXT("metin-en"), bEn, bEn ? TEXT("") : TEXT("ingilizce turkceye dusuyor"));
	}

	// 6. The balance tables were applied, not merely staged. Without them the game
	//    runs on the pre-tuning C++ defaults and nothing looks broken.
	{
		const int32 Applied = CigBalance::LoadedCsvFileCount();
		TArray<FString> Staged;
		IFileManager::Get().FindFiles(Staged, *(FPaths::ProjectDir() / TEXT("Config/Balance/*.csv")), true, false);
		const bool bOk = Applied > 0 && Applied >= Staged.Num();
		Add(TEXT("denge-verisi"), bOk, FString::Printf(TEXT("%d/%d csv"), Applied, Staged.Num()));
	}

	// 7. Repository-owned audio resolves. Only the sounds this repository owns are
	//    checked; an absent optional pack is a licensing state, not a build fault.
	{
		UCigAudioSubsystem* Audio = GameMode.GetGameInstance()
			? GameMode.GetGameInstance()->GetSubsystem<UCigAudioSubsystem>() : nullptr;
		const bool bOk = Audio
			&& Audio->HasSound(ECigSound::UIClick)
			&& Audio->HasSound(ECigSound::Serve);
		Add(TEXT("ses-varliklari"), bOk, bOk ? TEXT("") : TEXT("/Game/Audio cozulmedi"));
	}

	// 8. Repository-owned meshes resolve.
	{
		const bool bOk = CigMesh::Food(TEXT("bowl")) != nullptr
			&& CigMesh::Furniture(TEXT("chair")) != nullptr;
		Add(TEXT("mesh-varliklari"), bOk, bOk ? TEXT("") : TEXT("/Game/LowPoly cozulmedi"));
	}

	// 9. The migration chain still reaches the current version from the oldest.
	//    MigrateSave is static and takes only a save object - no slot is touched.
	{
		UCigSaveGame* Old = NewObject<UCigSaveGame>(&GameMode);
		const int32 Started = Old ? Old->SaveVersion : -1;
		if (Old)
		{
			UCigSaveSubsystem::MigrateSave(*Old);
		}
		const bool bOk = Old && Old->SaveVersion == UCigSaveSubsystem::CurrentVersion;
		Add(TEXT("kayit-surumu"), bOk,
			FString::Printf(TEXT("%d -> %d, hedef %d"), Started, Old ? Old->SaveVersion : -1,
				UCigSaveSubsystem::CurrentVersion));
	}

	// 10. Capture and apply round-trip, in memory. Runs last because it writes
	//     system state back. No slot is read or written: these are the game mode's
	//     own marshalling functions, and bSaveDisabled is already set.
	{
		UCigSaveGame* A = NewObject<UCigSaveGame>(&GameMode);
		UCigSaveGame* B = NewObject<UCigSaveGame>(&GameMode);
		bool bOk = A && B;
		FString Detail;
		if (bOk)
		{
			GameMode.CaptureSave(*A);
			GameMode.ApplySave(*A);
			GameMode.CaptureSave(*B);
			bOk = A->Day == B->Day && A->Money == B->Money
				&& A->UrunCarpanlari.Num() == B->UrunCarpanlari.Num();
			if (!bOk)
			{
				Detail = TEXT("capture/apply ayni durumu vermedi");
			}
		}

		// The shop layout goes round the same trip. Checked here rather than in a
		// check of its own so the packaged harness contract does not move: the
		// automation suite proves the layout rules, and what a package can add is
		// that they still hold in a build with no editor, no log and no console.
		if (bOk)
		{
			bOk = A->bLayoutPersisted && B->bLayoutPersisted
				&& A->InstalledLayout.Num() == B->InstalledLayout.Num()
				&& A->InstalledLayout.Num() > 0;
			if (!bOk)
			{
				Detail = FString::Printf(TEXT("yerlesim turu bozuk: %d -> %d (isaret %d/%d)"),
					A->InstalledLayout.Num(), B->InstalledLayout.Num(),
					A->bLayoutPersisted ? 1 : 0, B->bLayoutPersisted ? 1 : 0);
			}
		}
		if (bOk)
		{
			// Order and position, not just the count: a layout that came back with
			// the right number of placements in the wrong places would pass a count.
			for (int32 i = 0; i < A->InstalledLayout.Num(); ++i)
			{
				const FCigSavePlacement& Before = A->InstalledLayout[i];
				const FCigSavePlacement& After = B->InstalledLayout[i];
				if (Before.StableId != After.StableId
					|| !Before.Transform.GetLocation().Equals(After.Transform.GetLocation(), 0.01f))
				{
					bOk = false;
					Detail = FString::Printf(TEXT("yerlesim %d degisti: %s -> %s"),
						i, *Before.StableId.ToString(), *After.StableId.ToString());
					break;
				}
			}
		}
		Add(TEXT("kayit-turu"), bOk, Detail);
	}

	// 11. A Shipping controller must have neither an instance nor a class from
	//     which one could be created. Development keeps deterministic capture and
	//     benchmark commands; UE_WITH_CHEAT_MANAGER disables the retail path and
	//     the project controller explicitly clears its class as defence in depth.
	{
#if UE_BUILD_SHIPPING
		const APlayerController* RuntimePC = GameMode.GetWorld()
			? GameMode.GetWorld()->GetFirstPlayerController() : nullptr;
		// InitGame runs before the first controller is guaranteed to exist. The
		// configured class default proves what every future controller starts with;
		// if one already exists, verify the live instance as well.
		const APlayerController* ControllerDefault = GameMode.PlayerControllerClass
			? GameMode.PlayerControllerClass->GetDefaultObject<APlayerController>() : nullptr;
		const bool bDefaultSafe = ControllerDefault
			&& ControllerDefault->CheatManager == nullptr
			&& ControllerDefault->CheatClass == nullptr;
		const bool bRuntimeSafe = !RuntimePC
			|| (RuntimePC->CheatManager == nullptr && RuntimePC->CheatClass == nullptr);
		const bool bOk = bDefaultSafe && bRuntimeSafe;
		Add(TEXT("shipping-hileleri"), bOk,
			bOk ? TEXT("") : FString::Printf(TEXT("default=%s runtime=%s"),
				bDefaultSafe ? TEXT("guvenli") : TEXT("hile-sinifi"),
				bRuntimeSafe ? TEXT("guvenli/yok") : TEXT("hile-manager")));
#else
		Add(TEXT("shipping-hileleri"), true, TEXT("Shipping disi yapida uygulanmaz"));
#endif
	}

	int32 FailedIndex = 0;
	for (int32 i = 0; i < Checks.Num(); ++i)
	{
		if (!Checks[i].bPassed)
		{
			FailedIndex = i + 1;
			break;
		}
	}

	if (!Report.Commit(FormatReport(Checks, FailedIndex)))
	{
		UE_LOG(LogCig, Error, TEXT("Sürüm öz-testi raporu yazılamadı: %s"), *Report.Error);
		return ReportUnwritable;
	}
	return FailedIndex;
}
