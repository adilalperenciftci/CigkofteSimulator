// Bringing a real shop to match a saved layout.
//
// The transaction itself is checked in CigLayoutLoadTests without a world. What
// needs a shop is the half that was missing until step 1: that the records, the
// meshes and the seats all end up describing the same place. A move that updates
// the record and leaves the table where it was is the failure this whole stage
// exists to prevent, and it is only visible with actors on the floor.

#include "Misc/AutomationTest.h"
#include "Save/CigSavePlacement.h"
#include "Save/CigSaveGame.h"
#include "Placement/CigPlacementSystem.h"
#include "Navigation/CigNavSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const TCHAR* TableId = TEXT("fixture.seating.table.0");

	bool BuildShop(FCigTestShop& Shop, FAutomationTestBase& Test)
	{
		if (!Shop.Build(Test))
		{
			return false;
		}
		if (!Shop.GM->WorldBuilder || !Shop.GM->Placement)
		{
			Test.AddError(TEXT("Dunya kurucusu veya yerlesim sistemi yok."));
			return false;
		}
		Shop.GM->WorldBuilder->BuildWorld();
		return true;
	}

	// The shop's own layout, as a save would hold it.
	TArray<FCigSavePlacement> CaptureLive(const FCigTestShop& Shop)
	{
		return CigSavePlacement::Capture(Shop.GM->Placement->PlacementRecords());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigApplyUnchangedTest,
	"Cigkofte.LayoutApply.ReloadingTheLayoutTheShopAlreadyHasChangesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigApplyUnchangedTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	const TArray<FCigSavePlacement> Saved = CaptureLive(Shop);
	const int32 BeforeCount = Shop.GM->Placement->PlacementCount();
	const FVector BeforeTable = Shop.GM->Placement->FindPlacement(FName(TableId))->Transform.GetLocation();
	const int32 BeforeSeats = Shop.GM->WorldBuilder->Seats.Num();

	FString Diagnostic;
	TestTrue(FString::Printf(TEXT("Kendi yerlesimi kabul edilmeli (%s)"), *Diagnostic),
		Shop.GM->WorldBuilder->ApplyLoadedLayout(Saved, Diagnostic));

	TestEqual(TEXT("Kayit sayisi degismemeli"), Shop.GM->Placement->PlacementCount(), BeforeCount);
	TestEqual(TEXT("Koltuk sayisi degismemeli"), Shop.GM->WorldBuilder->Seats.Num(), BeforeSeats);
	TestTrue(TEXT("Masa yerinde kalmali"),
		Shop.GM->Placement->FindPlacement(FName(TableId))->Transform.GetLocation().Equals(BeforeTable, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigApplyMovedTableTest,
	"Cigkofte.LayoutApply.AMovedTableTakesItsMeshesAndItsSeatsWithIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigApplyMovedTableTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	const FCigPlacementRecord* Before = Shop.GM->Placement->FindPlacement(FName(TableId));
	if (!Before)
	{
		AddError(TEXT("Varsayilan yerlesimde masa yok."));
		return false;
	}
	const FVector Home = Before->Transform.GetLocation();

	// The seat this table owns, before anything moves.
	FVector SeatBefore = FVector::ZeroVector;
	int32 SeatIndex = INDEX_NONE;
	for (int32 i = 0; i < Shop.GM->WorldBuilder->Seats.Num(); ++i)
	{
		if (Shop.GM->WorldBuilder->Seats[i].PlacementId == FName(TableId))
		{
			SeatIndex = i;
			SeatBefore = Shop.GM->WorldBuilder->Seats[i].Pos;
			break;
		}
	}
	if (SeatIndex == INDEX_NONE)
	{
		AddError(TEXT("Masaya ait koltuk bulunamadi."));
		return false;
	}

	// 60 cm along +Y, which is the free direction. The shop is tighter than it
	// looks: -X puts this table on the sofa and -Y puts it on fixture.station.yanurun,
	// and the load refused both. Small, but a move is a move and the test measures
	// the delta rather than a distance.
	TArray<FCigSavePlacement> Saved = CaptureLive(Shop);
	FCigSavePlacement* Moved = Saved.FindByPredicate([](const FCigSavePlacement& S)
	{
		return S.StableId == FName(TableId);
	});
	if (!Moved)
	{
		AddError(TEXT("Yakalanan yerlesimde masa yok."));
		return false;
	}
	const FVector Target = Home + FVector(0.f, 60.f, 0.f);
	Moved->Transform.SetLocation(Target);

	FString Diagnostic;
	if (!Shop.GM->WorldBuilder->ApplyLoadedLayout(Saved, Diagnostic))
	{
		AddError(FString::Printf(TEXT("Tasinmis masa kabul edilmedi: %s"), *Diagnostic));
		return false;
	}

	const FCigPlacementRecord* After = Shop.GM->Placement->FindPlacement(FName(TableId));
	TestTrue(TEXT("Kayit yeni yere tasinmali"),
		After && After->Transform.GetLocation().Equals(Target, 1.f));

	// The point of step 1: the record moving is not enough.
	TestTrue(TEXT("Masanin aktorleri de tasinmali"),
		Shop.GM->WorldBuilder->PlacementVisuals.VisualCount(FName(TableId)) > 1);

	// And the point of this step: a chair belongs to its table.
	const FVector SeatAfter = Shop.GM->WorldBuilder->Seats[SeatIndex].Pos;
	TestFalse(TEXT("Koltuk eski yerinde kalmamali"), SeatAfter.Equals(SeatBefore, 1.f));
	TestTrue(TEXT("Koltuk masayla ayni kadar tasinmali"),
		(SeatAfter - SeatBefore).Equals(Target - Home, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigApplyRemovedTest,
	"Cigkofte.LayoutApply.APlacementTheSaveDoesNotMentionIsRemovedFromTheShop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigApplyRemovedTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	const FName SofaId(TEXT("fixture.seating.sofa"));
	TestTrue(TEXT("Kanepe varsayilan yerlesimde olmali"),
		Shop.GM->Placement->FindPlacement(SofaId) != nullptr);
	TestTrue(TEXT("Kanepenin gorseli olmali"),
		Shop.GM->WorldBuilder->PlacementVisuals.VisualCount(SofaId) > 0);

	TArray<FCigSavePlacement> Saved = CaptureLive(Shop);
	const int32 Removed = Saved.RemoveAll([SofaId](const FCigSavePlacement& S)
	{
		return S.StableId == SofaId;
	});
	TestEqual(TEXT("Yakalanan yerlesimden kanepe cikarilmali"), Removed, 1);

	FString Diagnostic;
	if (!Shop.GM->WorldBuilder->ApplyLoadedLayout(Saved, Diagnostic))
	{
		AddError(FString::Printf(TEXT("Kanepesiz yerlesim kabul edilmedi: %s"), *Diagnostic));
		return false;
	}

	TestTrue(TEXT("Kanepe kaydi silinmeli"), Shop.GM->Placement->FindPlacement(SofaId) == nullptr);
	// Destroyed rather than left standing: a mesh with no record is a thing the
	// player can walk into that nothing knows about.
	TestEqual(TEXT("Kanepenin aktorleri de gitmeli"),
		Shop.GM->WorldBuilder->PlacementVisuals.VisualCount(SofaId), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigApplyRefusedLeavesShopAloneTest,
	"Cigkofte.LayoutApply.ARefusedLayoutLeavesTheAuthoredDefaultStanding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigApplyRefusedLeavesShopAloneTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	const int32 BeforeCount = Shop.GM->Placement->PlacementCount();
	const FVector BeforeTable = Shop.GM->Placement->FindPlacement(FName(TableId))->Transform.GetLocation();
	const int32 BeforeVisuals = Shop.GM->WorldBuilder->PlacementVisuals.VisualCount(FName(TableId));

	// A layout with a broken record in the middle of it. The whole thing must be
	// refused, including the records before the bad one.
	TArray<FCigSavePlacement> Saved = CaptureLive(Shop);
	if (Saved.Num() < 2)
	{
		AddError(TEXT("Yakalanan yerlesim cok kucuk."));
		return false;
	}
	Saved[Saved.Num() / 2].Transform.SetLocation(FVector(NAN, 0.f, 0.f));

	FString Diagnostic;
	TestFalse(TEXT("Bozuk yerlesim reddedilmeli"),
		Shop.GM->WorldBuilder->ApplyLoadedLayout(Saved, Diagnostic));
	TestTrue(TEXT("Reddin gerekcesi bildirilmeli"), !Diagnostic.IsEmpty());

	TestEqual(TEXT("Kayit sayisi degismemeli"), Shop.GM->Placement->PlacementCount(), BeforeCount);
	TestTrue(TEXT("Masa yerinde kalmali"),
		Shop.GM->Placement->FindPlacement(FName(TableId))->Transform.GetLocation().Equals(BeforeTable, 0.01f));
	TestEqual(TEXT("Gorseller de yerinde kalmali"),
		Shop.GM->WorldBuilder->PlacementVisuals.VisualCount(FName(TableId)), BeforeVisuals);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigApplyRoundTripTest,
	"Cigkofte.LayoutApply.CaptureAfterApplyMatchesWhatWasApplied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigApplyRoundTripTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	TArray<FCigSavePlacement> Saved = CaptureLive(Shop);
	FCigSavePlacement* Moved = Saved.FindByPredicate([](const FCigSavePlacement& S)
	{
		return S.StableId == FName(TableId);
	});
	Moved->Transform.SetLocation(Moved->Transform.GetLocation() + FVector(0.f, 60.f, 0.f));

	FString Diagnostic;
	if (!Shop.GM->WorldBuilder->ApplyLoadedLayout(Saved, Diagnostic))
	{
		AddError(FString::Printf(TEXT("Yerlesim kabul edilmedi: %s"), *Diagnostic));
		return false;
	}

	// Saving straight after loading has to produce the same layout, or a shop
	// would drift a little every time the player quit and came back.
	const TArray<FCigSavePlacement> Again = CaptureLive(Shop);
	TestEqual(TEXT("Tekrar yakalama ayni sayida kayit vermeli"), Again.Num(), Saved.Num());
	for (int32 i = 0; i < Again.Num() && i < Saved.Num(); ++i)
	{
		TestEqual(TEXT("Ayni kimlik ayni sirada olmali"), Again[i].StableId, Saved[i].StableId);
		TestTrue(FString::Printf(TEXT("%s ayni yerde olmali"), *Again[i].StableId.ToString()),
			Again[i].Transform.GetLocation().Equals(Saved[i].Transform.GetLocation(), 1.f));
	}
	return true;
}

// ------------------------------------------------------ the bulk load boundary

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigApplyOneEventTest,
	"Cigkofte.LayoutApply.AWholeLayoutArrivesAsOneChangeNotThirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigApplyOneEventTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }
	if (!Shop.GM->Nav)
	{
		AddError(TEXT("Navigasyon sistemi yok."));
		return false;
	}

	// Ask once so the grid is built and not merely dirty, or the assertion below
	// would pass on a rebuild that never happened.
	Shop.GM->Nav->AreRequiredRoutesOpen();
	const int32 RevisionBefore = Shop.GM->Nav->LayoutRevision();
	const int32 RebuildsBefore = Shop.GM->Nav->RebuildCount();

	const TArray<FCigSavePlacement> Saved = CaptureLive(Shop);
	TestTrue(TEXT("Yerlesim yeterince buyuk olmali"), Saved.Num() > 10);

	FString Diagnostic;
	if (!Shop.GM->WorldBuilder->ApplyLoadedLayout(Saved, Diagnostic))
	{
		AddError(FString::Printf(TEXT("Yerlesim kabul edilmedi: %s"), *Diagnostic));
		return false;
	}

	// One notification for the whole layout. Per-record broadcasts would bump this
	// once per placement, and every customer already walking would repath against
	// each intermediate shop on the way.
	TestEqual(TEXT("Yerlesim revizyonu bir kez artmali"),
		Shop.GM->Nav->LayoutRevision(), RevisionBefore + 1);

	// Lazily, too: nothing rebuilds until something asks.
	TestEqual(TEXT("Yukleme kendi basina yeniden kurmamali"),
		Shop.GM->Nav->RebuildCount(), RebuildsBefore);
	TestTrue(TEXT("Yukleme sonrasi grid kirli olmali"), Shop.GM->Nav->IsDirty());

	Shop.GM->Nav->AreRequiredRoutesOpen();
	TestEqual(TEXT("Ilk sorgu tam bir kez yeniden kurmali"),
		Shop.GM->Nav->RebuildCount(), RebuildsBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaveLayoutRoundTripTest,
	"Cigkofte.LayoutApply.SaveThenLoadPutsTheShopBackWhereItWas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSaveLayoutRoundTripTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	// Through the real save object, so the capture and apply the game uses are the
	// ones being measured rather than the helpers underneath them.
	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);

	TestTrue(TEXT("Kaydedilen yerlesim isaretlenmeli"), Save->bLayoutPersisted);
	TestTrue(TEXT("Kaydedilen yerlesim dolu olmali"), Save->InstalledLayout.Num() > 10);

	// Only installed placements are written, and every one of them is.
	int32 LiveInstalled = 0;
	for (const FCigPlacementRecord& Record : Shop.GM->Placement->PlacementRecords())
	{
		if (Record.Lifetime == ECigPlacementLifetime::Installed) { ++LiveInstalled; }
	}
	TestEqual(TEXT("Kurulu kayitlarin hepsi yazilmali"), Save->InstalledLayout.Num(), LiveInstalled);

	// Move a table in the file, then load it back into the same shop.
	FCigSavePlacement* Moved = Save->InstalledLayout.FindByPredicate([](const FCigSavePlacement& S)
	{
		return S.StableId == FName(TableId);
	});
	if (!Moved)
	{
		AddError(TEXT("Kaydedilen yerlesimde masa yok."));
		Save->RemoveFromRoot();
		return false;
	}
	const FVector Target = Moved->Transform.GetLocation() + FVector(0.f, 60.f, 0.f);
	Moved->Transform.SetLocation(Target);

	Shop.GM->ApplySave(*Save);

	const FCigPlacementRecord* After = Shop.GM->Placement->FindPlacement(FName(TableId));
	TestTrue(TEXT("Yuklenen yerlesim uygulanmali"),
		After && After->Transform.GetLocation().Equals(Target, 1.f));

	Save->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaveUnknownLayoutTest,
	"Cigkofte.LayoutApply.ASaveWithNoRecordedLayoutLeavesTheAuthoredDefaultAlone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSaveUnknownLayoutTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	const int32 BeforeCount = Shop.GM->Placement->PlacementCount();

	// A migrated pre-v13 file: empty array, and it means "unknown" rather than
	// "empty". Applying it would clear the shop the world builder just put up.
	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);
	Save->InstalledLayout.Reset();
	Save->bLayoutPersisted = false;

	Shop.GM->ApplySave(*Save);

	TestEqual(TEXT("Bilinmeyen yerlesim dukkani bosaltmamali"),
		Shop.GM->Placement->PlacementCount(), BeforeCount);
	TestTrue(TEXT("Masa hala yerinde olmali"),
		Shop.GM->Placement->FindPlacement(FName(TableId)) != nullptr);

	Save->RemoveFromRoot();
	return true;
}

// ---------------------------------------------------------- transient records

namespace
{
	// A delivery crate, as the inventory system registers one: the same footprint,
	// the same context, and one of the authored delivery spots rather than a
	// coordinate picked by hand. The first hand-picked one was inside a protected
	// zone and the authority refused it, which is the authority working.
	bool AddCrate(FCigTestShop& Shop, FName Id)
	{
		FCigPlacementRequest Request;
		Request.StableId = Id;
		Request.Category = ECigPlacementCategory::Storage;
		Request.Lifetime = ECigPlacementLifetime::Transient;
		Request.Footprint = UCigPlacementSystem::StockCrateFootprint();
		Request.UseSpec = UCigPlacementSystem::StockCrateUseSpec();
		Request.Context = ECigPlacementContext::Delivery;

		const FCigPlacementResult Result = Shop.GM->Placement->FindFirstValidPlacement(
			Request, CigPlacementLayout::DeliverySpots());
		if (!Result.bAccepted)
		{
			return false;
		}
		Request.CandidateTransform = Result.NormalizedTransform;
		return Shop.GM->Placement->RegisterPlacement(Request).bAccepted;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCrateNotSavedTest,
	"Cigkofte.LayoutApply.Transient.ACrateIsNotWrittenIntoTheLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCrateNotSavedTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	const FName CrateId(TEXT("crate.delivery.0"));
	if (!AddCrate(Shop, CrateId))
	{
		AddError(TEXT("Test kasasi kurulamadi."));
		return false;
	}

	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);

	// Written here it would come back twice on load: once from the layout and once
	// from the delivery state that already persists.
	const bool bFound = Save->InstalledLayout.ContainsByPredicate([CrateId](const FCigSavePlacement& S)
	{
		return S.StableId == CrateId;
	});
	TestFalse(TEXT("Kasa yerlesim dosyasina yazilmamali"), bFound);
	TestTrue(TEXT("Kasa dukkanda duruyor olmali"),
		Shop.GM->Placement->FindPlacement(CrateId) != nullptr);

	Save->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCrateSurvivesLoadTest,
	"Cigkofte.LayoutApply.Transient.ACrateStandingInTheShopSurvivesALoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCrateSurvivesLoadTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildShop(Shop, *this)) { return false; }

	const TArray<FCigSavePlacement> Saved = CaptureLive(Shop);

	const FName CrateId(TEXT("crate.delivery.0"));
	if (!AddCrate(Shop, CrateId))
	{
		AddError(TEXT("Test kasasi kurulamadi."));
		return false;
	}
	const FVector CrateWhere = Shop.GM->Placement->FindPlacement(CrateId)->Transform.GetLocation();

	// The layout swap replaces the whole record set. Without carrying transients
	// across it, the crate would vanish from the floor while the delivery system
	// went on believing it had one.
	FString Diagnostic;
	if (!Shop.GM->WorldBuilder->ApplyLoadedLayout(Saved, Diagnostic))
	{
		AddError(FString::Printf(TEXT("Yerlesim kabul edilmedi: %s"), *Diagnostic));
		return false;
	}

	const FCigPlacementRecord* After = Shop.GM->Placement->FindPlacement(CrateId);
	TestTrue(TEXT("Kasa yuklemeden sonra da durmali"), After != nullptr);
	if (After)
	{
		TestTrue(TEXT("Kasa yerinden oynamamali"), After->Transform.GetLocation().Equals(CrateWhere, 0.01f));
		TestTrue(TEXT("Kasa gecici kalmali"), After->Lifetime == ECigPlacementLifetime::Transient);
	}

	// And exactly one: the layout must not have produced a second.
	int32 Crates = 0;
	for (const FCigPlacementRecord& Record : Shop.GM->Placement->PlacementRecords())
	{
		if (Record.Lifetime == ECigPlacementLifetime::Transient) { ++Crates; }
	}
	TestEqual(TEXT("Tam bir kasa olmali"), Crates, 1);
	return true;
}

#endif
