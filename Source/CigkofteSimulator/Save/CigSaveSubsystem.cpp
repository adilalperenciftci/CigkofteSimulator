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
	if (Save.SaveVersion < 7)
	{
		MigrateV6ToV7(Save);
	}
	if (Save.SaveVersion < 8)
	{
		MigrateV7ToV8(Save);
	}
	if (Save.SaveVersion < 9)
	{
		MigrateV8ToV9(Save);
	}
	if (Save.SaveVersion < 10)
	{
		MigrateV9ToV10(Save);
	}
	if (Save.SaveVersion < 11)
	{
		MigrateV10ToV11(Save);
	}
	if (Save.SaveVersion < 12)
	{
		MigrateV11ToV12(Save);
	}
	if (Save.SaveVersion < 13)
	{
		MigrateV12ToV13(Save);
	}
	if (Save.SaveVersion < 14)
	{
		MigrateV13ToV14(Save);
	}
	if (Save.SaveVersion < 15)
	{
		MigrateV14ToV15(Save);
	}
	// In future: if (Save.SaveVersion < 16) { MigrateV15ToV16(Save); } ...

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

void UCigSaveSubsystem::MigrateV6ToV7(UCigSaveGame& Save)
{
	// v7 added the per-product pricing markups. A v6 save was played entirely at
	// list price, so an empty array is the honest conversion: ApplySave fills
	// every product with a markup of 1 and the shop keeps charging what it did.
	Save.UrunCarpanlari.Reset();
}

void UCigSaveSubsystem::MigrateV7ToV8(UCigSaveGame& Save)
{
	// v8 gave staff their own traits. An apprentice hired under v7 had none, so
	// they migrate to a flat 1 across the board - the same neutral worker the
	// old formulas assumed. Archetype 0 is the entry-level one, which is the
	// honest label for someone hired before archetypes existed.
	Save.ApprenticeArketip = 0;
	Save.ApprenticeHiz = 1.f;
	Save.ApprenticeTitizlik = 1.f;
	Save.ApprenticeGulerYuz = 1.f;
	Save.ApprenticeOdenmemisGun = 0;
	Save.StaffTransferTeklifi = 0;
}

void UCigSaveSubsystem::MigrateV8ToV9(UCigSaveGame& Save)
{
	// v9 added the bulk order. A v8 save had none in flight, and the cleared
	// fields are exactly that: no offer, nothing accepted, nothing owed.
	Save.bTopluTeklifVar = false;
	Save.bTopluKabulEdildi = false;
	Save.TopluTeslimGunu = 0;
	Save.TopluIstenenAdet = 0;
	Save.TopluOdul = 0;
	Save.TopluBaslangicServis = 0;
}

void UCigSaveSubsystem::MigrateV9ToV10(UCigSaveGame& Save)
{
	// v10 introduced the licence. A v9 shop was never asked for one, so loading it
	// already expired would be a fine for paperwork that did not exist yesterday:
	// the licence starts valid from the day the save is on.
	Save.RuhsatBitisGunu = Save.Day + 14;
	Save.BasarisizDenetim = 0;
	Save.RusvetSayisi = 0;
	Save.KalanKapaliGun = 0;
}

void UCigSaveSubsystem::MigrateV10ToV11(UCigSaveGame& Save)
{
	// v11 added the follower count. A v10 shop had no online presence at all, so
	// zero is the honest starting point rather than a gift.
	Save.Takipci = 0;
}

void UCigSaveSubsystem::MigrateV11ToV12(UCigSaveGame& Save)
{
	// v11 reviews had no ID. Number them newest-first so the order the list is
	// stored in survives, then park the counter past the highest. The pending
	// reply is dropped rather than guessed: it was an index into a list that has
	// since been reordered, so there is no honest review to point it at.
	for (int32 i = 0; i < Save.Reviews.Num(); ++i)
	{
		Save.Reviews[i].Id = Save.Reviews.Num() - i;
	}
	Save.NextReviewId = Save.Reviews.Num() + 1;
	Save.YanitlanacakYorumId = 0;
}

void UCigSaveSubsystem::MigrateV12ToV13(UCigSaveGame& Save)
{
	// v13 brought the installed shop layout into the save. A v12 file has none,
	// and the honest conversion is to say so rather than to leave an empty array
	// that reads as a shop somebody emptied.
	//
	// Clearing the array is not redundant with the flag. A v12 file cannot contain
	// layout records, but a file whose version was edited backwards can, and the
	// pair has to be consistent: not persisted means nothing to read.
	Save.InstalledLayout.Reset();
	Save.bLayoutPersisted = false;
}

void UCigSaveSubsystem::MigrateV13ToV14(UCigSaveGame& Save)
{
	// v14 gave the shop a name. A v13 shop was never asked for one, and an empty
	// name is exactly that - CigShopIdentity turns it into the default, which is
	// the same board the shop had before this version existed.
	//
	// No flag and no fabricated name: writing the default in here would turn
	// "never named" into "named it that", and a later change to the default would
	// then leave old shops carrying a name nobody chose.
	Save.ShopName.Reset();
}

void UCigSaveSubsystem::MigrateV14ToV15(UCigSaveGame& Save)
{
	// v15 gave the shop a back room. Nobody could take anything off the floor
	// before build mode existed, so a v14 shop has nothing stored - and unlike the
	// layout, an empty back room needs no flag to say so, because "nothing was put
	// away" and "this file predates putting things away" are the same statement
	// and both are true of it.
	Save.StoredLayout.Reset();
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
