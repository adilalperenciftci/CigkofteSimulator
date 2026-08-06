// Build mode step 8: the round trip, which is where Stage 3.5 stops being
// theoretical.
//
// Stage 3.5 proved a layout could be written and read back. What it could not
// prove is that the layout a *player* makes survives, because until step 6 no
// player could make one - every test moved a table by calling the authority
// directly. These go through build mode's own operations, so what is being
// checked is the thing the player will actually do.

#include "Misc/AutomationTest.h"
#include "Placement/CigBuildVerdict.h"
#include "Placement/CigBuildRemoval.h"
#include "Placement/CigPlacementSystem.h"
#include "Save/CigSaveGame.h"
#include "Save/CigSaveSubsystem.h"
#include "Save/CigSavePlacement.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FCigPlacementRecord* FirstSeated(const UCigWorldBuilder& WB, const UCigPlacementSystem& P)
	{
		for (const UCigWorldBuilder::FCigSeat& Seat : WB.Seats)
		{
			if (Seat.PlacementId.IsNone()) { continue; }
			if (const FCigPlacementRecord* R = P.FindPlacement(Seat.PlacementId))
			{
				if (R->Lifetime == ECigPlacementLifetime::Installed) { return R; }
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildSaveMovedTest,
	"Cigkofte.BuildMode.ATableMovedInBuildModeIsStillThereAfterALoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildSaveMovedTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->WorldBuilder) { AddError(TEXT("Sistemler yok.")); return false; }
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	UCigPlacementSystem* Placement = Shop.GM->Placement.Get();
	WB->BuildWorld();

	const FCigPlacementRecord* Found = FirstSeated(*WB, *Placement);
	if (!Found) { AddError(TEXT("Sandalyeli yerlesim yok.")); return false; }
	const FCigPlacementRecord Record = *Found;
	const FTransform Before = Record.Transform;

	// --- the player moves it, through build mode ---
	Shop.GM->bBuildMode = true;
	Shop.GM->BuildSelection = CigBuildSelection::Resolve(Record.StableId, &Record);
	if (!Shop.GM->BeginBuildMove()) { AddError(TEXT("Nesne alinamadi.")); return false; }

	FVector Target = Before.GetLocation() + FVector(0.f, 60.f, 0.f);
	Shop.GM->SetBuildCandidateLocation(Target);
	if (!Shop.GM->BuildVerdict.IsAccepted())
	{
		AddError(FString::Printf(TEXT("Aday kabul edilmedi: %s"),
			*CigBuildVerdict::Describe(Shop.GM->BuildVerdict)));
		return false;
	}
	if (!Shop.GM->CommitBuildMove()) { AddError(TEXT("Onay reddedildi.")); return false; }

	const FCigPlacementRecord* Moved = Placement->FindPlacement(Record.StableId);
	if (!Moved) { AddError(TEXT("Kayit kayboldu.")); return false; }
	const FVector AfterMove = Moved->Transform.GetLocation();
	TestFalse(TEXT("Tasima gercekten kimildatmali"), AfterMove.Equals(Before.GetLocation(), 1.f));

	// --- save, then load into a shop built from scratch ---
	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Shop.GM->CaptureSave(*Save);
	TestTrue(TEXT("Kaydedilen yerlesim isaretlenmeli"), Save->bLayoutPersisted);

	FCigTestShop Reloaded;
	if (!Reloaded.Build(*this)) { return false; }
	Reloaded.GM->WorldBuilder->BuildWorld();
	Reloaded.GM->ApplySave(*Save);

	const FCigPlacementRecord* Landed = Reloaded.GM->Placement->FindPlacement(Record.StableId);
	if (!Landed) { AddError(TEXT("Yuklenen dukkanida kayit yok.")); return false; }

	// The point of the whole stage: where the player left it, not where the
	// authored default puts it.
	TestTrue(TEXT("Yuklenen masa birakildigi yerde olmali"),
		Landed->Transform.GetLocation().Equals(AfterMove, 1.f));
	TestFalse(TEXT("Yuklenen masa varsayilan yerinde olmamali"),
		Landed->Transform.GetLocation().Equals(Before.GetLocation(), 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildSaveStoredTest,
	"Cigkofte.BuildMode.SomethingPutAwayIsStillOwnedAfterALoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildSaveStoredTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->WorldBuilder) { AddError(TEXT("Sistemler yok.")); return false; }
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	UCigPlacementSystem* Placement = Shop.GM->Placement.Get();
	WB->BuildWorld();

	// Something the rules allow taking away.
	const FCigPlacementRecord* Removable = nullptr;
	for (const FCigPlacementRecord& R : Placement->PlacementRecords())
	{
		if (R.Lifetime != ECigPlacementLifetime::Installed) { continue; }
		if (CigBuildRemoval::Judge(&R, Placement->FunctionalCapacityByCategory(R.Category)).IsAllowed())
		{
			Removable = &R;
			break;
		}
	}
	if (!Removable) { AddError(TEXT("Kaldirilabilir yerlesim yok.")); return false; }
	const FCigPlacementRecord Record = *Removable;

	Shop.GM->bBuildMode = true;
	Shop.GM->BuildSelection = CigBuildSelection::Resolve(Record.StableId, &Record);
	if (!Shop.GM->RemoveBuildSelection()) { AddError(TEXT("Kaldirilamadi.")); return false; }
	TestEqual(TEXT("Depoda bir nesne olmali"), Shop.GM->BuildStored.Num(), 1);

	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Shop.GM->CaptureSave(*Save);

	// The record it describes is not on the floor, so it must be in the back room
	// rather than nowhere. This is the trap the version bump exists to close: a
	// save that dropped it would destroy a table the player still owns.
	TestEqual(TEXT("Depo kayda yazilmali"), Save->StoredLayout.Num(), 1);
	TestFalse(TEXT("Depolanan nesne zeminde yazilmamali"),
		Save->InstalledLayout.ContainsByPredicate([&Record](const FCigSavePlacement& S)
		{
			return S.StableId == Record.StableId;
		}));

	FCigTestShop Reloaded;
	if (!Reloaded.Build(*this)) { return false; }
	Reloaded.GM->WorldBuilder->BuildWorld();
	Reloaded.GM->ApplySave(*Save);

	TestEqual(TEXT("Depo yuklendikten sonra dolu olmali"), Reloaded.GM->BuildStored.Num(), 1);
	TestTrue(TEXT("Depolanan nesne otoritede olmamali"),
		Reloaded.GM->Placement->FindPlacement(Record.StableId) == nullptr);
	// Hidden rather than destroyed, which is what makes putting it back possible.
	TestTrue(TEXT("Depolanan nesnenin gorselleri saklanmis olmali"),
		Reloaded.GM->WorldBuilder->HasStoredPlacement(Record.StableId));

	// And it can be put back, which is the whole reason for persisting it.
	//
	// Build mode has to be entered first, and that is the rule rather than an
	// inconvenience: a loaded game does not put the player's furniture back by
	// itself, because where it goes is their decision.
	Reloaded.GM->bBuildMode = true;
	if (!Reloaded.GM->RestoreLastStored())
	{
		AddError(TEXT("Yuklendikten sonra geri konamadi."));
		return false;
	}
	TestTrue(TEXT("Geri konan nesne otoriteye donmeli"),
		Reloaded.GM->Placement->FindPlacement(Record.StableId) != nullptr);
	TestEqual(TEXT("Depo bosalmali"), Reloaded.GM->BuildStored.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildSaveMigrationTest,
	"Cigkofte.BuildMode.AnOlderSaveHasAnEmptyBackRoomRatherThanAMissingOne",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildSaveMigrationTest::RunTest(const FString&)
{
	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->SaveVersion = 14;
	// A v14 file cannot have put anything away, because build mode did not exist.
	// Anything sitting in the field would be noise from a different schema.
	Save->StoredLayout.Add(FCigSavePlacement());

	UCigSaveSubsystem::MigrateSave(*Save);

	TestEqual(TEXT("Surum guncellenmeli"), Save->SaveVersion, UCigSaveSubsystem::CurrentVersion);
	TestEqual(TEXT("v14 kaydin deposu bos olmali"), Save->StoredLayout.Num(), 0);
	return true;
}

#endif
