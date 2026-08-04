// Loading a layout is one transaction, and this is the failure matrix.
//
// The state worth being unable to reach is a shop that is half authored default
// and half save: it happens the moment records are registered straight into the
// live authority and one of them is refused, because every record after the bad
// one is then validated against a floor that is neither layout.
//
// Pure. Every case below is reachable here without corrupting a real save file,
// which is the only way a failure matrix gets exercised at all.

#include "Misc/AutomationTest.h"
#include "Save/CigLayoutLoad.h"
#include "Save/CigSavePlacement.h"
#include "Placement/CigPlacementTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigSavePlacement Station(const TCHAR* Id, float X, float Y)
	{
		FCigSavePlacement S;
		S.StableId = FName(Id);
		S.Category = (uint8)ECigPlacementCategory::Station;
		S.Lifetime = (uint8)ECigPlacementLifetime::Installed;
		S.Transform = FTransform(FRotator::ZeroRotator, FVector(X, Y, 0.f));
		S.FootprintSize = FVector2D(120.f, 120.f);
		S.RotationPolicy = (uint8)ECigPlacementRotationPolicy::FixedYaw;
		S.UseSize = FVector2D(100.f, 120.f);
		S.UseOffset = FVector2D(100.f, 0.f);
		S.FunctionalCapacity = 1;
		return S;
	}

	FCigLayoutLoadReport Run(const TArray<FCigSavePlacement>& Saved,
		const TArray<FVector2D>& Seats = {})
	{
		FCigPlacementAuthority Candidate;
		return CigLayoutLoad::BuildCandidate(Saved, CigPlacementLayout::ShopBounds(),
			CigLayoutLoad::ShopProtectedZones(), Seats, Candidate);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutEmptyTest,
	"Cigkofte.LayoutLoad.AnEmptyLayoutIsAcceptedAndTheShopStaysWalkable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutEmptyTest::RunTest(const FString&)
{
	// An empty shop is a legal shop. Whether it *means* anything is the loader's
	// question, and version 13 is what answers it - not this function.
	const FCigLayoutLoadReport Report = Run({});
	TestTrue(TEXT("Bos yerlesim kabul edilmeli"), Report.bAccepted);
	TestEqual(TEXT("Bos yerlesimde kayit olmamali"), Report.RegisteredCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutAcceptsGoodTest,
	"Cigkofte.LayoutLoad.AWellFormedLayoutIsAccepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutAcceptsGoodTest::RunTest(const FString&)
{
	TArray<FCigSavePlacement> Saved;
	Saved.Add(Station(TEXT("station.a"), 300.f, -400.f));
	Saved.Add(Station(TEXT("station.b"), 300.f, 400.f));

	FCigPlacementAuthority Candidate;
	const FCigLayoutLoadReport Report = CigLayoutLoad::BuildCandidate(Saved,
		CigPlacementLayout::ShopBounds(), CigLayoutLoad::ShopProtectedZones(), {}, Candidate);

	TestTrue(FString::Printf(TEXT("Duzgun yerlesim kabul edilmeli (%s)"), *Report.Diagnostic),
		Report.bAccepted);
	TestEqual(TEXT("Iki kayit da kaydedilmeli"), Report.RegisteredCount, 2);
	TestEqual(TEXT("Aday yetki iki kayit tutmali"), Candidate.RecordCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutRecordFaultTest,
	"Cigkofte.LayoutLoad.AMalformedRecordFailsAsItselfNotAsAnOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutRecordFaultTest::RunTest(const FString&)
{
	// Every record is checked on its own terms before any of them touch the floor.
	// Without that ordering a NaN transform would be reported as whatever the
	// authority happened to make of it.
	TArray<FCigSavePlacement> Saved;
	Saved.Add(Station(TEXT("station.a"), 300.f, -400.f));
	FCigSavePlacement Broken = Station(TEXT("station.b"), 300.f, 400.f);
	Broken.Transform.SetLocation(FVector(NAN, 0.f, 0.f));
	Saved.Add(Broken);

	const FCigLayoutLoadReport Report = Run(Saved);

	TestFalse(TEXT("Bozuk kayitli yerlesim reddedilmeli"), Report.bAccepted);
	TestTrue(TEXT("Hata kayit hatasi olarak bildirilmeli"),
		Report.Failure == ECigLayoutLoadFailure::RecordFault);
	TestTrue(TEXT("Hata sonlu olmayan donusum olmali"),
		Report.RecordFault == ECigSavePlacementFault::NonFiniteTransform);
	TestEqual(TEXT("Hangi kayit oldugu bildirilmeli"), Report.FailedStableId, FName(TEXT("station.b")));
	// The point of the transaction: the good record before it is not kept either.
	TestEqual(TEXT("Reddedilen islemde hicbir kayit tutulmamali"), Report.RegisteredCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutDuplicateTest,
	"Cigkofte.LayoutLoad.ADuplicateStableIdIsRefusedRatherThanCollapsed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutDuplicateTest::RunTest(const FString&)
{
	// The authority would treat the second registration as a move of the first and
	// quietly keep one record where the file has two. Caught before it can.
	TArray<FCigSavePlacement> Saved;
	Saved.Add(Station(TEXT("station.a"), 300.f, -400.f));
	Saved.Add(Station(TEXT("station.a"), 300.f, 400.f));

	const FCigLayoutLoadReport Report = Run(Saved);

	TestFalse(TEXT("Yinelenen kimlik reddedilmeli"), Report.bAccepted);
	TestTrue(TEXT("Hata yinelenen kimlik olmali"),
		Report.Failure == ECigLayoutLoadFailure::DuplicateStableId);
	TestEqual(TEXT("Yinelenen kimlik bildirilmeli"), Report.FailedStableId, FName(TEXT("station.a")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutOverlapTest,
	"Cigkofte.LayoutLoad.OverlappingRecordsAreRefusedByTheSameRulesAsLivePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutOverlapTest::RunTest(const FString&)
{
	TArray<FCigSavePlacement> Saved;
	Saved.Add(Station(TEXT("station.a"), 300.f, 0.f));
	Saved.Add(Station(TEXT("station.b"), 320.f, 0.f));

	const FCigLayoutLoadReport Report = Run(Saved);

	TestFalse(TEXT("Ust uste binen yerlesim reddedilmeli"), Report.bAccepted);
	TestTrue(TEXT("Hata yerlesim reddi olmali"),
		Report.Failure == ECigLayoutLoadFailure::Rejected);
	TestTrue(TEXT("Yetkinin kendi gerekcesi tasinmali"),
		Report.PlacementFailure != ECigPlacementFailure::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutOutOfBoundsTest,
	"Cigkofte.LayoutLoad.ARecordOutsideTheShopIsRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutOutOfBoundsTest::RunTest(const FString&)
{
	TArray<FCigSavePlacement> Saved;
	Saved.Add(Station(TEXT("station.faraway"), 50000.f, 50000.f));

	const FCigLayoutLoadReport Report = Run(Saved);

	TestFalse(TEXT("Dukkan disindaki kayit reddedilmeli"), Report.bAccepted);
	TestTrue(TEXT("Hata yerlesim reddi olmali"),
		Report.Failure == ECigLayoutLoadFailure::Rejected);
	TestEqual(TEXT("Hangi kayit oldugu bildirilmeli"),
		Report.FailedStableId, FName(TEXT("station.faraway")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutSeatWalledInTest,
	"Cigkofte.LayoutLoad.ASeatWalledInByLegalPlacementsClosesTheLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutSeatWalledInTest::RunTest(const FString&)
{
	// The case rectangles cannot see. Four decorations, none overlapping another -
	// they meet at exact edges, which the authority allows - and together they box
	// a seat in. Every placement is legal and the shop is broken.
	//
	// Not a seat outside the world: FCigNavGrid::FindNearestWalkable clamps its
	// start cell into the grid, so a far-away point resolves to the nearest edge
	// and reports itself reachable. That is existing behaviour the stranded-customer
	// recovery depends on, and testing against it would have measured the clamp
	// rather than the load.
	auto Block = [](const TCHAR* Id, float X, float Y, float SizeX, float SizeY)
	{
		FCigSavePlacement S;
		S.StableId = FName(Id);
		S.Category = (uint8)ECigPlacementCategory::Decoration;
		S.Lifetime = (uint8)ECigPlacementLifetime::Installed;
		S.Transform = FTransform(FRotator::ZeroRotator, FVector(X, Y, 0.f));
		S.FootprintSize = FVector2D(SizeX, SizeY);
		return S;
	};

	TArray<FCigSavePlacement> Saved;
	Saved.Add(Block(TEXT("deco.north"), 400.f, 1050.f, 400.f, 100.f));
	Saved.Add(Block(TEXT("deco.south"), 400.f, 750.f, 400.f, 100.f));
	Saved.Add(Block(TEXT("deco.west"), 250.f, 900.f, 100.f, 200.f));
	Saved.Add(Block(TEXT("deco.east"), 550.f, 900.f, 100.f, 200.f));

	const TArray<FVector2D> Seats = { FVector2D(400.f, 900.f) };
	const FCigLayoutLoadReport Report = Run(Saved, Seats);

	TestFalse(TEXT("Duvarla cevrilen koltuk yuklemeyi kapatmali"), Report.bAccepted);
	TestTrue(FString::Printf(TEXT("Hata rota kapali olmali, gelen: %s (%s)"),
		CigLayoutLoad::FailureText(Report.Failure), *Report.Diagnostic),
		Report.Failure == ECigLayoutLoadFailure::RouteClosed);
	TestEqual(TEXT("Hangi rota oldugu bildirilmeli"), Report.ClosedRouteId, FName(TEXT("route.seat.0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutReachableSeatTest,
	"Cigkofte.LayoutLoad.AReachableSeatDoesNotCloseTheLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutReachableSeatTest::RunTest(const FString&)
{
	// The mirror of the case above: without it the seat check would pass by never
	// having accepted a seat at all.
	const TArray<FVector2D> Seats = { FVector2D(-300.f, 855.f) };
	const FCigLayoutLoadReport Report = Run({}, Seats);

	TestTrue(FString::Printf(TEXT("Ulasilabilir koltuk kabul edilmeli (%s)"), *Report.Diagnostic),
		Report.bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigLayoutCandidateIsolationTest,
	"Cigkofte.LayoutLoad.TheCandidateIsBuiltToOneSideOfAnythingLive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigLayoutCandidateIsolationTest::RunTest(const FString&)
{
	// Reusing a populated authority must not blend the old layout into the new
	// one. The candidate is reset before anything is registered into it.
	FCigPlacementAuthority Candidate;
	Candidate.Configure(CigPlacementLayout::ShopBounds());
	Candidate.TryRegister(CigSavePlacement::ToRequest(Station(TEXT("station.stale"), 300.f, 0.f)));
	TestEqual(TEXT("Onceden dolu yetki kurulmali"), Candidate.RecordCount(), 1);

	TArray<FCigSavePlacement> Saved;
	Saved.Add(Station(TEXT("station.a"), 300.f, -400.f));

	const FCigLayoutLoadReport Report = CigLayoutLoad::BuildCandidate(Saved,
		CigPlacementLayout::ShopBounds(), CigLayoutLoad::ShopProtectedZones(), {}, Candidate);

	TestTrue(TEXT("Yeni aday kabul edilmeli"), Report.bAccepted);
	TestEqual(TEXT("Eski kayit adaya sizmamali"), Candidate.RecordCount(), 1);
	TestTrue(TEXT("Kalan kayit yeni olan olmali"),
		Candidate.Find(FName(TEXT("station.a"))) != nullptr);
	TestTrue(TEXT("Eski kayit gitmis olmali"),
		Candidate.Find(FName(TEXT("station.stale"))) == nullptr);
	return true;
}

#endif
