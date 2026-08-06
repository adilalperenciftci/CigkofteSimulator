// Taking things off the floor, and putting them back.
//
// Moving is always reversible - the shop is a different shape and that is all.
// Removing is a different kind of act: it takes capability away, and a player who
// removes their only preparation station has broken the shop from inside a mode
// that exists to improve it. So removal has a rule moving does not need, and the
// rule is about function rather than taste.

#include "Misc/AutomationTest.h"
#include "Placement/CigBuildRemoval.h"
#include "Placement/CigPlacementSystem.h"
#include "Navigation/CigNavSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigPlacementRecord MakeFunctional(const TCHAR* Id, ECigPlacementCategory Category, int32 Capacity)
	{
		FCigPlacementRecord R;
		R.StableId = FName(Id);
		R.Category = Category;
		R.Lifetime = ECigPlacementLifetime::Installed;
		R.Consequence.FunctionalCapacity = Capacity;
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildRemovalLastOfKindTest,
	"Cigkofte.BuildMode.TheLastOfAFunctionCannotBeRemoved",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildRemovalLastOfKindTest::RunTest(const FString&)
{
	const FCigPlacementRecord Station = MakeFunctional(TEXT("tezgah.01"), ECigPlacementCategory::Station, 1);

	// The only one. Removing it would leave the shop unable to do something it
	// must do, from inside a mode that exists to improve the shop.
	const FCigBuildRemovalVerdict Only = CigBuildRemoval::Judge(&Station, 1);
	TestFalse(TEXT("Tek islevsel nesne kaldirilamamali"), Only.IsAllowed());
	TestTrue(TEXT("Sebep LastOfItsFunction olmali"),
		Only.Fault == ECigBuildRemovalFault::LastOfItsFunction);
	// Named, so the player knows what they would be giving up.
	TestFalse(TEXT("Ret kendini aciklamali"), CigBuildRemoval::Describe(Only).IsEmpty());

	// One of two is fine, and this is the line that makes it a rule about
	// capability rather than a ban on removing anything useful.
	const FCigBuildRemovalVerdict OneOfTwo = CigBuildRemoval::Judge(&Station, 2);
	TestTrue(TEXT("Ikiden biri kaldirilabilmeli"), OneOfTwo.IsAllowed());

	// The boundary is the capacity this record carries, not the count of records.
	// A single table seating four is still the last of its function if nothing
	// else seats anybody.
	const FCigPlacementRecord BigTable = MakeFunctional(TEXT("masa.01"), ECigPlacementCategory::Seating, 4);
	TestFalse(TEXT("Dort kisilik tek masa kaldirilamamali"),
		CigBuildRemoval::Judge(&BigTable, 4).IsAllowed());
	TestTrue(TEXT("Baska oturma varsa kaldirilabilmeli"),
		CigBuildRemoval::Judge(&BigTable, 6).IsAllowed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildRemovalDecorationTest,
	"Cigkofte.BuildMode.SomethingThatDoesNothingIsAlwaysRemovable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildRemovalDecorationTest::RunTest(const FString&)
{
	// Decoration carries no capacity, so removing it takes nothing away - even if
	// it is the only decoration in the shop.
	const FCigPlacementRecord Plant = MakeFunctional(TEXT("saksi.01"), ECigPlacementCategory::Decoration, 0);
	TestTrue(TEXT("Tek sus bile kaldirilabilmeli"), CigBuildRemoval::Judge(&Plant, 0).IsAllowed());

	// A delivery crate is not the player's to take away, same as selection.
	FCigPlacementRecord Crate = MakeFunctional(TEXT("kasa.01"), ECigPlacementCategory::Storage, 0);
	Crate.Lifetime = ECigPlacementLifetime::Transient;
	const FCigBuildRemovalVerdict Transient = CigBuildRemoval::Judge(&Crate, 0);
	TestFalse(TEXT("Teslimat kasasi kaldirilamamali"), Transient.IsAllowed());
	TestTrue(TEXT("Sebep NotSelectable olmali"),
		Transient.Fault == ECigBuildRemovalFault::NotSelectable);

	// And nothing at all is its own answer rather than a crash.
	const FCigBuildRemovalVerdict Nothing = CigBuildRemoval::Judge(nullptr, 0);
	TestFalse(TEXT("Kayitsiz nesne kaldirilamamali"), Nothing.IsAllowed());
	TestTrue(TEXT("Sebep NoRecord olmali"), Nothing.Fault == ECigBuildRemovalFault::NoRecord);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildStoreRoundTripTest,
	"Cigkofte.BuildMode.AStoredPlacementComesBackWithItsSeats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildStoreRoundTripTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->WorldBuilder) { AddError(TEXT("Sistemler yok.")); return false; }
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	UCigPlacementSystem* Placement = Shop.GM->Placement.Get();
	WB->BuildWorld();

	// A seated placement, because seats are the part that can be lost.
	const FCigPlacementRecord* Found = nullptr;
	for (const UCigWorldBuilder::FCigSeat& Seat : WB->Seats)
	{
		if (Seat.PlacementId.IsNone()) { continue; }
		if (const FCigPlacementRecord* R = Placement->FindPlacement(Seat.PlacementId))
		{
			if (R->Lifetime == ECigPlacementLifetime::Installed) { Found = R; break; }
		}
	}
	if (!Found) { AddError(TEXT("Sandalyeli yerlesim yok.")); return false; }
	const FCigPlacementRecord Record = *Found;

	const int32 RecordsBefore = Placement->PlacementCount();
	const int32 SeatsBefore = WB->Seats.Num();
	TArray<FVector> SeatPositionsBefore;
	for (const UCigWorldBuilder::FCigSeat& Seat : WB->Seats)
	{
		if (Seat.PlacementId == Record.StableId) { SeatPositionsBefore.Add(Seat.Pos); }
	}
	if (SeatPositionsBefore.Num() == 0) { AddError(TEXT("Sandalye bulunamadi.")); return false; }

	// --- store ---
	TestTrue(TEXT("Kayit silinebilmeli"), Placement->RemovePlacement(Record.StableId));
	WB->StorePlacementVisuals(Record.StableId);

	TestEqual(TEXT("Depolanan kayit otoritede kalmamali"),
		Placement->PlacementCount(), RecordsBefore - 1);
	TestTrue(TEXT("Depolanan yerlesim depoda gorunmeli"), WB->HasStoredPlacement(Record.StableId));
	// The seats are gone from the world rather than flagged, which is what stops
	// a customer walking to a chair that is in the back room.
	TestEqual(TEXT("Depolanan yerlesimin sandalyeleri dunyada kalmamali"),
		WB->Seats.Num(), SeatsBefore - SeatPositionsBefore.Num());
	for (const UCigWorldBuilder::FCigSeat& Seat : WB->Seats)
	{
		TestTrue(TEXT("Hicbir sandalye depolanan yerlesime ait olmamali"),
			Seat.PlacementId != Record.StableId);
	}

	// --- restore ---
	FCigPlacementRequest Back;
	Back.StableId = Record.StableId;
	Back.Category = Record.Category;
	Back.Lifetime = Record.Lifetime;
	Back.Footprint = Record.Footprint;
	Back.UseSpec = Record.UseSpec;
	Back.CandidateTransform = Record.Transform;
	Back.Context = ECigPlacementContext::BuildMode;

	const FCigPlacementResult Result = Placement->RegisterPlacement(Back);
	if (!Result.bAccepted)
	{
		AddError(FString::Printf(TEXT("Geri koyma reddedildi: %s"),
			*UCigPlacementSystem::FailureText(Result.Failure)));
		return false;
	}
	WB->RestorePlacementVisuals(Record.StableId);

	TestEqual(TEXT("Geri konan kayit sayisi eski haline donmeli"),
		Placement->PlacementCount(), RecordsBefore);
	TestEqual(TEXT("Sandalye sayisi eski haline donmeli"), WB->Seats.Num(), SeatsBefore);
	TestFalse(TEXT("Depo bosalmis olmali"), WB->HasStoredPlacement(Record.StableId));

	// The seats came back where they were, not somewhere re-derived that merely
	// resembles it. This is why the stored seats are kept whole.
	TArray<FVector> SeatPositionsAfter;
	for (const UCigWorldBuilder::FCigSeat& Seat : WB->Seats)
	{
		if (Seat.PlacementId == Record.StableId) { SeatPositionsAfter.Add(Seat.Pos); }
	}
	TestEqual(TEXT("Geri konan sandalye sayisi ayni olmali"),
		SeatPositionsAfter.Num(), SeatPositionsBefore.Num());
	for (int32 i = 0; i < SeatPositionsBefore.Num() && i < SeatPositionsAfter.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("Sandalye %d eski yerine donmeli"), i),
			SeatPositionsAfter[i].Equals(SeatPositionsBefore[i], 0.01f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildStoreRoutesTest,
	"Cigkofte.BuildMode.RemovingSomethingCannotCloseARoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildStoreRoutesTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->WorldBuilder || !Shop.GM->Nav)
	{
		AddError(TEXT("Sistemler yok.")); return false;
	}
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	UCigPlacementSystem* Placement = Shop.GM->Placement.Get();
	WB->BuildWorld();

	TestTrue(TEXT("Baslangicta rotalar acik olmali"), Shop.GM->Nav->AreRequiredRoutesOpen());

	// Removal is never asked about routes, and this is the reason: taking an
	// obstacle away only ever opens floor. Asserted rather than assumed, because
	// the claim is what justifies skipping the check.
	int32 Removed = 0;
	for (const FCigPlacementRecord& R : TArray<FCigPlacementRecord>(Placement->PlacementRecords()))
	{
		if (R.Lifetime != ECigPlacementLifetime::Installed) { continue; }
		if (!CigBuildRemoval::Judge(&R, Placement->FunctionalCapacityByCategory(R.Category)).IsAllowed())
		{
			continue;
		}
		if (Placement->RemovePlacement(R.StableId))
		{
			WB->StorePlacementVisuals(R.StableId);
			++Removed;
		}
	}

	if (Removed == 0) { AddError(TEXT("Kaldirilabilir yerlesim yok.")); return false; }
	TestTrue(TEXT("Kaldirilabilir her sey kaldirildiktan sonra rotalar acik kalmali"),
		Shop.GM->Nav->AreRequiredRoutesOpen());
	return true;
}

#endif
