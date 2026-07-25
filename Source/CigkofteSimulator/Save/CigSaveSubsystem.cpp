#include "Save/CigSaveSubsystem.h"
#include "Save/CigSaveGame.h"
#include "Game/CigkofteGameMode.h"
#include "Core/CigLog.h"
#include "Kismet/GameplayStatics.h"

bool UCigSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName(), 0);
}

UCigSaveGame* UCigSaveSubsystem::PeekSave()
{
	if (Cached)
	{
		return Cached;
	}
	if (!HasSave())
	{
		return nullptr;
	}
	Cached = Cast<UCigSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName(), 0));
	if (Cached)
	{
		if (Cached->SaveVersion > CurrentVersion)
		{
			UE_LOG(LogCigSave, Warning, TEXT("Save sürümü (%d) bu oyundan yeni (%d); güvenli alanlar okunacak."), Cached->SaveVersion, CurrentVersion);
		}
		else if (Cached->SaveVersion < CurrentVersion)
		{
			MigrateSave(*Cached);
		}
	}
	return Cached;
}

void UCigSaveSubsystem::MigrateSave(UCigSaveGame& Save)
{
	const int32 From = Save.SaveVersion;

	// A step chain: each conversion moves the save one version forward.
	if (Save.SaveVersion < 2)
	{
		MigrateV1ToV2(Save);
	}
	if (Save.SaveVersion < 3)
	{
		MigrateV2ToV3(Save);
	}
	if (Save.SaveVersion < 4)
	{
		MigrateV3ToV4(Save);
	}
	if (Save.SaveVersion < 5)
	{
		MigrateV4ToV5(Save);
	}
	if (Save.SaveVersion < 6)
	{
		MigrateV5ToV6(Save);
	}
	// In future: if (Save.SaveVersion < 7) { MigrateV6ToV7(Save); } ...

	Save.SaveVersion = CurrentVersion;
	UE_LOG(LogCigSave, Log, TEXT("Kayıt taşındı: sürüm %d → %d"), From, CurrentVersion);
}

void UCigSaveSubsystem::MigrateV1ToV2(UCigSaveGame& Save)
{
	// v2 added FavSide (favourite side) to the regular-customer record. v1 saves
	// have no such field; it reads back as the UPROPERTY default 0
	// (ECigSide::Yok), which is the correct behaviour - there is nothing to
	// convert. The step is deliberately left a no-op as the first link in the chain.
}

void UCigSaveSubsystem::MigrateV2ToV3(UCigSaveGame& Save)
{
	// v3 brought the deterministic RNG stream into the save. v2 saves had no such
	// stream; 0 means "no saved state" and GameMode::ApplySave produces a fresh
	// seed in that case. A player loading an old save carries on that day with a
	// new stream - no progress is lost.
	Save.RngInitialSeed = 0;
	Save.RngStateSeed = 0;
}

void UCigSaveSubsystem::MigrateV3ToV4(UCigSaveGame& Save)
{
	// v4 added the accessibility settings. Old saves have none of these fields;
	// the UPROPERTY defaults (scale 1, flashing on, colour-blind off) reproduce
	// v3 behaviour exactly, so there is nothing to convert.
	//
	// The same version removed the Language field that was never applied. The
	// value sitting in an old save is simply not read; USaveGame skips a missing
	// field quietly and nothing is lost.
	//
	// The one real guard is on HUD scale: a 0 from a corrupt save would make the
	// UI entirely invisible, leaving the player no way to turn it back on.
	Save.Settings.UIScaleMult = FMath::Clamp(Save.Settings.UIScaleMult, 0.7f, 1.6f);
}

void UCigSaveSubsystem::MigrateV4ToV5(UCigSaveGame& Save)
{
	// v5 brought the language choice back. v4 saves have no field; the default 0
	// (Turkish) matches existing behaviour, so there is nothing to convert.
	Save.Settings.Language = 0;
}

void UCigSaveSubsystem::MigrateV5ToV6(UCigSaveGame& Save)
{
	// v6 brought key bindings into the save. v5 saves have no field; an empty
	// array means "default bindings", which is exactly v5 behaviour.
	Save.Settings.KeyBindings.Reset();
}

bool UCigSaveSubsystem::SaveNow(ACigkofteGameMode* GM)
{
	if (!GM)
	{
		return false;
	}
	UCigSaveGame* Save = NewObject<UCigSaveGame>(this);
	Save->SaveVersion = CurrentVersion;
	GM->CaptureSave(*Save);

	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SlotName(), 0);
	if (bOk)
	{
		Cached = Save;
		UE_LOG(LogCigSave, Log, TEXT("Oyun kaydedildi (gün %d)"), Save->Day);
	}
	else
	{
		UE_LOG(LogCigSave, Error, TEXT("Kayıt başarısız!"));
	}
	return bOk;
}

bool UCigSaveSubsystem::LoadInto(ACigkofteGameMode* GM)
{
	if (!GM)
	{
		return false;
	}
	UCigSaveGame* Save = PeekSave();
	if (!Save)
	{
		return false;
	}
	GM->ApplySave(*Save);
	UE_LOG(LogCigSave, Log, TEXT("Kayıt yüklendi (gün %d, sürüm %d)"), Save->Day, Save->SaveVersion);
	return true;
}

void UCigSaveSubsystem::DeleteSave()
{
	if (HasSave())
	{
		UGameplayStatics::DeleteGameInSlot(SlotName(), 0);
	}
	Cached = nullptr;
	UE_LOG(LogCigSave, Log, TEXT("Kayıt silindi"));
}
