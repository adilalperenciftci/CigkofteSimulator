// What goes into the save file for a placement, and what deliberately does not.
//
// All pure: no world, no actors, no save file. The point of capture being a free
// function over records is that the rules can be checked here rather than through
// a round trip that would only fail once a shop had already been written wrong.

#include "Misc/AutomationTest.h"
#include "Save/CigSavePlacement.h"
#include "Placement/CigPlacementTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigPlacementRecord MakeRecord(const TCHAR* Id, ECigPlacementCategory Category,
		ECigPlacementLifetime Lifetime = ECigPlacementLifetime::Installed)
	{
		FCigPlacementRecord Record;
		Record.StableId = FName(Id);
		Record.Category = Category;
		Record.Lifetime = Lifetime;
		Record.Transform = FTransform(FRotator(0.f, 90.f, 0.f), FVector(-300.f, 950.f, 0.f));
		Record.Footprint.Size = FVector2D(160.f, 280.f);
		Record.Footprint.CenterOffset = FVector2D(5.f, -7.f);
		Record.Footprint.ClearanceMargin = 3.f;
		Record.Footprint.RotationPolicy = ECigPlacementRotationPolicy::FixedYaw;
		Record.Footprint.FixedYawDegrees = 90.f;
		Record.UseSpec.Size = FVector2D(160.f, 320.f);
		Record.UseSpec.CenterOffset = FVector2D(100.f, 0.f);
		Record.UseSpec.YawOffsetDegrees = 180.f;
		Record.UseSpec.FunctionalCapacity = 2;

		// Derived, and present on a live record. Capture must not carry it.
		Record.Consequence.StableId = Record.StableId;
		Record.Consequence.FunctionalCapacity = 99;
		Record.Consequence.bInstalledLayout = true;
		return Record;
	}

	FCigSavePlacement MakeSaved()
	{
		return CigSavePlacement::FromRecord(
			MakeRecord(TEXT("fixture.seating.table.0"), ECigPlacementCategory::Seating));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSavePlacementRoundTripTest,
	"Cigkofte.SavePlacement.Capture.AuthoredInputsSurviveTheRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSavePlacementRoundTripTest::RunTest(const FString&)
{
	const FCigPlacementRecord Record = MakeRecord(TEXT("fixture.seating.table.0"), ECigPlacementCategory::Seating);
	const FCigPlacementRequest Back = CigSavePlacement::ToRequest(CigSavePlacement::FromRecord(Record));

	TestEqual(TEXT("Kararli kimlik korunmali"), Back.StableId, Record.StableId);
	TestTrue(TEXT("Kategori korunmali"), Back.Category == Record.Category);
	TestTrue(TEXT("Yasam suresi korunmali"), Back.Lifetime == Record.Lifetime);
	TestTrue(TEXT("Donusum korunmali"), Back.CandidateTransform.Equals(Record.Transform, 0.01f));

	TestTrue(TEXT("Ayak izi boyutu korunmali"), Back.Footprint.Size.Equals(Record.Footprint.Size, 0.01f));
	TestTrue(TEXT("Ayak izi ofseti korunmali"), Back.Footprint.CenterOffset.Equals(Record.Footprint.CenterOffset, 0.01f));
	TestEqual(TEXT("Bosluk payi korunmali"), Back.Footprint.ClearanceMargin, Record.Footprint.ClearanceMargin);
	TestTrue(TEXT("Donus politikasi korunmali"), Back.Footprint.RotationPolicy == Record.Footprint.RotationPolicy);
	TestEqual(TEXT("Sabit yaw korunmali"), Back.Footprint.FixedYawDegrees, Record.Footprint.FixedYawDegrees);

	TestTrue(TEXT("Kullanim alani korunmali"), Back.UseSpec.Equals(Record.UseSpec));
	TestTrue(TEXT("Baglam dunya kaydi olmali"), Back.Context == ECigPlacementContext::WorldRegistration);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSavePlacementNoConsequenceTest,
	"Cigkofte.SavePlacement.Capture.TheDerivedConsequenceIsNotStored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSavePlacementNoConsequenceTest::RunTest(const FString&)
{
	// The record carries a consequence saying capacity 99, which contradicts its
	// own use spec. Nothing produced from the save may agree with it: policy
	// derives the consequence, and a stored copy would be a second authority.
	const FCigPlacementRecord Record = MakeRecord(TEXT("fixture.seating.table.0"), ECigPlacementCategory::Seating);
	const FCigPlacementRequest Back = CigSavePlacement::ToRequest(CigSavePlacement::FromRecord(Record));

	TestEqual(TEXT("Kapasite kullanim alanindan gelmeli, sonuctan degil"),
		Back.UseSpec.FunctionalCapacity, 2);
	TestNotEqual(TEXT("Saklanan sonuc kapasitesi tasinmamali"),
		Back.UseSpec.FunctionalCapacity, Record.Consequence.FunctionalCapacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSavePlacementTransientTest,
	"Cigkofte.SavePlacement.Capture.TransientPlacementsAreNotWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSavePlacementTransientTest::RunTest(const FString&)
{
	TArray<FCigPlacementRecord> Records;
	Records.Add(MakeRecord(TEXT("fixture.seating.table.0"), ECigPlacementCategory::Seating));
	Records.Add(MakeRecord(TEXT("delivery.crate.0"), ECigPlacementCategory::Storage,
		ECigPlacementLifetime::Transient));
	Records.Add(MakeRecord(TEXT("station.yogurma"), ECigPlacementCategory::Station));

	const TArray<FCigSavePlacement> Saved = CigSavePlacement::Capture(Records);

	// A crate written here would come back twice: once from the layout and once
	// from the delivery state that already persists.
	TestEqual(TEXT("Yalniz kurulu kayitlar yazilmali"), Saved.Num(), 2);
	for (const FCigSavePlacement& One : Saved)
	{
		TestNotEqual(TEXT("Teslimat kasasi yazilmamali"), One.StableId, FName(TEXT("delivery.crate.0")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSavePlacementOrderTest,
	"Cigkofte.SavePlacement.Capture.OrderIsStableRegardlessOfRegistrationOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSavePlacementOrderTest::RunTest(const FString&)
{
	// The authority stores records in registration order, which is world-build
	// order. A save file must not depend on it.
	TArray<FCigPlacementRecord> Forward;
	Forward.Add(MakeRecord(TEXT("station.yogurma"), ECigPlacementCategory::Station));
	Forward.Add(MakeRecord(TEXT("fixture.seating.table.0"), ECigPlacementCategory::Seating));
	Forward.Add(MakeRecord(TEXT("fixture.seating.sofa"), ECigPlacementCategory::Decoration));

	TArray<FCigPlacementRecord> Reversed = Forward;
	Algo::Reverse(Reversed);

	const TArray<FCigSavePlacement> A = CigSavePlacement::Capture(Forward);
	const TArray<FCigSavePlacement> B = CigSavePlacement::Capture(Reversed);

	TestEqual(TEXT("Iki yakalama ayni sayida olmali"), A.Num(), B.Num());
	for (int32 i = 0; i < A.Num() && i < B.Num(); ++i)
	{
		TestEqual(TEXT("Kayit sirasindan bagimsiz ayni sira"), A[i].StableId, B[i].StableId);
	}
	TestEqual(TEXT("Sira sozluk sirasi olmali"), A[0].StableId, FName(TEXT("fixture.seating.sofa")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSavePlacementValidTest,
	"Cigkofte.SavePlacement.Validate.AWellFormedRecordPasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSavePlacementValidTest::RunTest(const FString&)
{
	TestTrue(TEXT("Duzgun kayit gecmeli"),
		CigSavePlacement::Validate(MakeSaved()) == ECigSavePlacementFault::None);

	// Decoration has no use area, and that is legal rather than an omission.
	FCigSavePlacement Decoration = MakeSaved();
	Decoration.Category = (uint8)ECigPlacementCategory::Decoration;
	Decoration.UseSize = FVector2D::ZeroVector;
	Decoration.UseOffset = FVector2D::ZeroVector;
	Decoration.UseYawDegrees = 0.f;
	Decoration.FunctionalCapacity = 0;
	TestTrue(TEXT("Kullanim alani olmayan dekorasyon gecmeli"),
		CigSavePlacement::Validate(Decoration) == ECigSavePlacementFault::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSavePlacementFaultsTest,
	"Cigkofte.SavePlacement.Validate.EachFaultIsNamedRatherThanLumped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigSavePlacementFaultsTest::RunTest(const FString&)
{
	{
		FCigSavePlacement S = MakeSaved();
		S.StableId = NAME_None;
		TestTrue(TEXT("Kimliksiz kayit reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::MissingStableId);
	}
	{
		FCigSavePlacement S = MakeSaved();
		S.Category = 200;
		TestTrue(TEXT("Bilinmeyen kategori reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::UnknownCategory);
	}
	{
		FCigSavePlacement S = MakeSaved();
		S.Category = (uint8)ECigPlacementCategory::Unknown;
		TestTrue(TEXT("Unknown kategorisi de reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::UnknownCategory);
	}
	{
		FCigSavePlacement S = MakeSaved();
		S.Lifetime = (uint8)ECigPlacementLifetime::Transient;
		TestTrue(TEXT("Dosyadaki gecici yerlesim reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::NotInstalled);
	}
	{
		FCigSavePlacement S = MakeSaved();
		S.Transform.SetLocation(FVector(NAN, 0.f, 0.f));
		TestTrue(TEXT("NaN konum reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::NonFiniteTransform);
	}
	{
		FCigSavePlacement S = MakeSaved();
		S.Transform.SetLocation(FVector(TNumericLimits<double>::Max() * 2.0, 0.f, 0.f));
		TestTrue(TEXT("Sonsuz konum reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::NonFiniteTransform);
	}
	{
		// A scaled transform would describe a different-sized object than the
		// footprint that gets validated against the floor.
		FCigSavePlacement S = MakeSaved();
		S.Transform.SetScale3D(FVector(2.f, 1.f, 1.f));
		TestTrue(TEXT("Olcekli donusum reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::NonUniformScale);
	}
	{
		FCigSavePlacement S = MakeSaved();
		S.FootprintSize = FVector2D(0.f, 280.f);
		TestTrue(TEXT("Sifir genislikli ayak izi reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::InvalidFootprint);
	}
	{
		FCigSavePlacement S = MakeSaved();
		S.ClearanceMargin = -1.f;
		TestTrue(TEXT("Negatif bosluk payi reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::InvalidFootprint);
	}
	{
		// Half a use area: a capacity with no rectangle to stand in.
		FCigSavePlacement S = MakeSaved();
		S.UseSize = FVector2D::ZeroVector;
		TestTrue(TEXT("Yarim kullanim alani reddedilmeli"),
			CigSavePlacement::Validate(S) == ECigSavePlacementFault::InvalidUseSpec);
	}
	return true;
}

#endif
