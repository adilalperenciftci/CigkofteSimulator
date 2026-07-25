#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CigSaveSubsystem.generated.h"

class UCigSaveGame;
class ACigkofteGameMode;

// Save file I/O: slot management and version control.
// Moving the data into the systems is GameMode's Capture/Apply functions.
UCLASS()
class UCigSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static const TCHAR* SlotName() { return TEXT("CigSave"); }
	// Version 2: FavSide (favourite side) added to the regular-customer record.
	// Version 3: the deterministic RNG seed and stream position entered the save.
	// Version 4: accessibility settings added, the dead Language field dropped.
	// Version 5: UI text became translatable and Language returned, working.
	// Version 6: key bindings entered the save.
	// Version 7: per-product pricing markups entered the save.
	// Version 8: staff traits and the standing transfer offer entered the save.
	static constexpr int32 CurrentVersion = 8;

	bool HasSave() const;
	bool SaveNow(ACigkofteGameMode* GM);
	bool LoadInto(ACigkofteGameMode* GM);
	void DeleteSave();

	// So settings can be read at startup, before the world is built.
	UCigSaveGame* PeekSave();

private:
	UPROPERTY() TObjectPtr<UCigSaveGame> Cached;

	// Migrates old saves to the current schema. It reads SaveVersion and runs
	// the step conversions in order (v1->v2->...), then sets the version to current.
	static void MigrateSave(UCigSaveGame& Save);
	static void MigrateV1ToV2(UCigSaveGame& Save);
	static void MigrateV2ToV3(UCigSaveGame& Save);
	static void MigrateV3ToV4(UCigSaveGame& Save);
	static void MigrateV4ToV5(UCigSaveGame& Save);
	static void MigrateV5ToV6(UCigSaveGame& Save);
	static void MigrateV6ToV7(UCigSaveGame& Save);
	static void MigrateV7ToV8(UCigSaveGame& Save);
	static void MigrateV8ToV9(UCigSaveGame& Save);
};
