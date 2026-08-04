// Bringing a real shop to match a saved layout.
//
// The transaction itself is checked in CigLayoutLoadTests without a world. What
// needs a shop is the half that was missing until step 1: that the records, the
// meshes and the seats all end up describing the same place. A move that updates
// the record and leaves the table where it was is the failure this whole stage
// exists to prevent, and it is only visible with actors on the floor.

#include "Misc/AutomationTest.h"
#include "Save/CigSavePlacement.h"
#include "Placement/CigPlacementSystem.h"
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

#endif
