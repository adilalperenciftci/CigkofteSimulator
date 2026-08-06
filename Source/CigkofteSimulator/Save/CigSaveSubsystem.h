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
	// Version 9: the pending bulk order entered the save.
	// Version 10: the licence, inspection record and bribe count entered the save.
	// Version 11: the follower count entered the save.
	// Version 12: reviews gained stable IDs and the pending reply is held by ID.
	// Version 13: the installed shop layout entered the save.
	//
	// Version 13 exists for a reason that is not "a field was added". An absent
	// array reads back empty and would have been harmless; what is not harmless is
	// that empty would then mean two different things - a save written before
	// layout was persisted, whose layout is unknown and must fall back to the
	// authored default, and a v13 save of a shop the player has genuinely emptied.
	// Load cannot tell those apart from the array, so bLayoutPersisted says which,
	// and MigrateV12ToV13 is what sets it for every older file.
	// Version 14: the shop has a name of its own.
	//
	// No companion flag this time, and the difference is worth stating: an empty
	// layout array is a state a player can reach, so it needed one. An empty shop
	// name is not - CigShopIdentity refuses it - so empty means "never named" and
	// nothing else.
	// Version 15: the back room - placements the player took off the floor.
	//
	// Also without a flag, and for the same reason as the name rather than the
	// layout: an empty back room means nothing is stored, a save written before
	// this field reads back empty, and both of those are the same true statement.
	// The layout needed a flag because "empty" there could also mean "unknown".
	static constexpr int32 CurrentVersion = 15;

	bool HasSave() const;
	bool SaveNow(ACigkofteGameMode* GM);
	bool LoadInto(ACigkofteGameMode* GM);
	void DeleteSave();

	// So settings can be read at startup, before the world is built.
	UCigSaveGame* PeekSave();

	// Migrates old saves to the current schema. It reads SaveVersion and runs
	// the step conversions in order (v1->v2->...), then sets the version to current.
	//
	// Public because it is the whole of the schema contract and needs to be
	// checked directly. It takes a save and nothing else - no world, no game
	// mode - so the one thing that decides whether a year-old file still opens
	// can be tested without standing up a shop (see Tests/CigSaveTests.cpp).
	static void MigrateSave(UCigSaveGame& Save);

private:
	UPROPERTY() TObjectPtr<UCigSaveGame> Cached;

	static void MigrateV1ToV2(UCigSaveGame& Save);
	static void MigrateV2ToV3(UCigSaveGame& Save);
	static void MigrateV3ToV4(UCigSaveGame& Save);
	static void MigrateV4ToV5(UCigSaveGame& Save);
	static void MigrateV5ToV6(UCigSaveGame& Save);
	static void MigrateV6ToV7(UCigSaveGame& Save);
	static void MigrateV7ToV8(UCigSaveGame& Save);
	static void MigrateV8ToV9(UCigSaveGame& Save);
	static void MigrateV9ToV10(UCigSaveGame& Save);
	static void MigrateV10ToV11(UCigSaveGame& Save);
	static void MigrateV11ToV12(UCigSaveGame& Save);
	static void MigrateV12ToV13(UCigSaveGame& Save);
	static void MigrateV13ToV14(UCigSaveGame& Save);
	static void MigrateV14ToV15(UCigSaveGame& Save);
};
