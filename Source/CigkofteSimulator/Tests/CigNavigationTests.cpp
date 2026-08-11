// Measured reachability across the shop floor.
//
// Stage 3.3 stopped at rectangles. The whole point of this file is that a
// rectangle is not a route: the tests that matter here are the ones where every
// placement is individually legal and a body still cannot get through.
//
// Grid tests use the pure value type. Shop tests stand up the real shop. The
// Collision group is the one that stops the grid being a private fiction - it
// asks the engine, with the player's own capsule, whether the floor the grid
// calls walkable is floor a body can actually stand on.

#include "Misc/AutomationTest.h"
#include "Navigation/CigNavGrid.h"
#include "Navigation/CigNavLayout.h"
#include "Navigation/CigNavSystem.h"
#include "Placement/CigPlacementSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "CollisionShape.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigPlacementBounds SquareBounds(float HalfExtent = 500.f)
	{
		FCigPlacementBounds Result;
		Result.Center = FVector2D::ZeroVector;
		Result.HalfExtent = FVector2D(HalfExtent, HalfExtent);
		return Result;
	}

	FCigPlacementRect MakeRect(float CenterX, float CenterY, float HalfX, float HalfY)
	{
		FCigPlacementRect Result;
		Result.Center = FVector2D(CenterX, CenterY);
		Result.HalfExtent = FVector2D(HalfX, HalfY);
		return Result;
	}

	// Obstacles are given as static rects: these tests are about the geometry a
	// body has to get past, not about how a record came to describe it.
	FCigNavGrid Grid(const TArray<FCigPlacementRect>& Obstacles, float AgentRadius = 35.f,
		float HalfExtent = 500.f, float CellSize = 25.f)
	{
		FCigNavGrid Result;
		Result.Build(SquareBounds(HalfExtent), TArray<FCigPlacementRecord>(), Obstacles,
			AgentRadius, CellSize);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavOpenFloorTest,
	"Cigkofte.Navigation.Grid.OpenFloorGivesOneStraightLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavOpenFloorTest::RunTest(const FString&)
{
	const FCigNavGrid Nav = Grid({});
	const FCigPathResult Path = Nav.FindPath(FVector2D(-300.f, 0.f), FVector2D(300.f, 0.f));

	TestTrue(TEXT("Boş zeminde yol bulunmalı"), Path.bSuccess);
	// String pulling has to collapse an empty room to its two endpoints. If this
	// starts reporting a staircase, customers will visibly walk one.
	TestEqual(TEXT("Boş zeminde yol iki noktaya inmeli"), Path.PointCount(), 2);
	TestTrue(TEXT("Yol uzunluğu düz mesafeye eşit olmalı"),
		FMath::IsNearlyEqual(Path.Length, 600.f, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavSealedWallTest,
	"Cigkofte.Navigation.Grid.ASealedWallHasNoRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavSealedWallTest::RunTest(const FString&)
{
	// Spans the full height of the region, so there is nothing to go around.
	const FCigNavGrid Nav = Grid({ MakeRect(0.f, 0.f, 20.f, 500.f) });
	const FCigPathResult Path = Nav.FindPath(FVector2D(-300.f, 0.f), FVector2D(300.f, 0.f));

	TestFalse(TEXT("Kapalı duvarın ardına yol olmamalı"), Path.bSuccess);
	TestEqual(TEXT("Başarısızlık NoRoute olarak adlandırılmalı"), Path.Failure, ECigPathFailure::NoRoute);
	TestEqual(TEXT("Başarısız yol nokta döndürmemeli"), Path.PointCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavNarrowGapTest,
	"Cigkofte.Navigation.Grid.AGapNarrowerThanTheAgentIsNotARoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavNarrowGapTest::RunTest(const FString&)
{
	// This is the test the whole stage exists for.
	//
	// Two obstacles, neither overlapping the other, both legal by every
	// rectangle rule the placement authority has. The floor between them is
	// unoccupied - so a check that asked "do these rectangles overlap?" would
	// call the shop fine. A body 70cm wide still does not fit through 60.
	constexpr float AgentRadius = 35.f;

	auto GapOf = [](float Gap)
	{
		const float HalfGap = Gap * 0.5f;
		return TArray<FCigPlacementRect>{
			MakeRect(0.f,  HalfGap + 200.f, 20.f, 200.f),
			MakeRect(0.f, -HalfGap - 200.f, 20.f, 200.f)
		};
	};

	// Stated rather than assumed: the rectangle rule that placement validation
	// actually applies has no objection to either pair below.
	TestFalse(TEXT("Dar boşluktaki dilimler üst üste binmemeli"),
		FCigPlacementAuthority::RectsOverlap(GapOf(60.f)[0], GapOf(60.f)[1]));

	const FCigNavGrid TooNarrow = Grid(GapOf(60.f), AgentRadius);
	const FCigPathResult Blocked = TooNarrow.FindPath(FVector2D(-300.f, 0.f), FVector2D(300.f, 0.f));
	TestFalse(TEXT("60cm boşluk 70cm gövdeye yol olmamalı"), Blocked.bSuccess);
	TestEqual(TEXT("Dar boşluk NoRoute vermeli"), Blocked.Failure, ECigPathFailure::NoRoute);

	// The rectangles have not started overlapping; only the gap has widened past
	// the body that has to use it. Both directions are asserted because a grid
	// that simply refused everything would pass the half above on its own.
	const FCigNavGrid WideEnough = Grid(GapOf(120.f), AgentRadius);
	const FCigPathResult Open = WideEnough.FindPath(FVector2D(-300.f, 0.f), FVector2D(300.f, 0.f));
	TestTrue(TEXT("120cm boşluk aynı gövdeye yol olmalı"), Open.bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavDiagonalTest,
	"Cigkofte.Navigation.Grid.DiagonalsDoNotSqueezeBetweenCorners",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavDiagonalTest::RunTest(const FString&)
{
	// Two blocks touching at one corner. On an 8-connected grid the diagonal
	// step between them is free unless it is explicitly refused, and a body
	// taking it walks through the join of two solid objects.
	const FCigNavGrid Nav = Grid({
		MakeRect(-100.f,  100.f, 100.f, 100.f),
		MakeRect( 100.f, -100.f, 100.f, 100.f)
	}, 10.f);

	const FCigPathResult Path = Nav.FindPath(FVector2D(-250.f, -250.f), FVector2D(250.f, 250.f));
	if (!TestTrue(TEXT("Köşelerin etrafından yol olmalı"), Path.bSuccess))
	{
		return false;
	}
	// The straight line is ~707. Anything close to it means the corner was cut.
	TestTrue(TEXT("Yol köşeyi kesmeyip dolaşmalı"), Path.Length > 780.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavDeterminismTest,
	"Cigkofte.Navigation.Grid.TheSameQueryGivesTheSamePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavDeterminismTest::RunTest(const FString&)
{
	// A symmetric layout is where a tie-break that is not defined shows up: both
	// ways round the block cost exactly the same, so an unstable open set can
	// return either. A test asserting a route would then be flaky rather than
	// wrong, which is the worse of the two.
	const TArray<FCigPlacementRect> Obstacles = { MakeRect(0.f, 0.f, 60.f, 200.f) };
	const FCigNavGrid First = Grid(Obstacles);
	const FCigNavGrid Second = Grid(Obstacles);

	const FCigPathResult A = First.FindPath(FVector2D(-300.f, 0.f), FVector2D(300.f, 0.f));
	const FCigPathResult B = Second.FindPath(FVector2D(-300.f, 0.f), FVector2D(300.f, 0.f));

	if (!TestTrue(TEXT("Simetrik engelin etrafından yol olmalı"), A.bSuccess && B.bSuccess))
	{
		return false;
	}
	TestEqual(TEXT("Nokta sayısı aynı olmalı"), A.PointCount(), B.PointCount());
	for (int32 Index = 0; Index < A.PointCount() && Index < B.PointCount(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("Nokta %d aynı olmalı"), Index), A.Points[Index].Equals(B.Points[Index]));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavEndpointTest,
	"Cigkofte.Navigation.Grid.BadEndpointsAreNamedNotGuessed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavEndpointTest::RunTest(const FString&)
{
	const FCigNavGrid Nav = Grid({ MakeRect(200.f, 0.f, 60.f, 60.f) });

	// A caller that is told "no route" when the real answer is "you asked for a
	// point inside a table" retries forever. Each cause is separately named.
	TestEqual(TEXT("Sınır dışı başlangıç adlandırılmalı"),
		Nav.FindPath(FVector2D(-5000.f, 0.f), FVector2D(0.f, 0.f)).Failure,
		ECigPathFailure::StartOutsideBounds);
	TestEqual(TEXT("Sınır dışı hedef adlandırılmalı"),
		Nav.FindPath(FVector2D(0.f, 0.f), FVector2D(5000.f, 0.f)).Failure,
		ECigPathFailure::GoalOutsideBounds);
	TestEqual(TEXT("Engel içindeki hedef adlandırılmalı"),
		Nav.FindPath(FVector2D(0.f, 0.f), FVector2D(200.f, 0.f)).Failure,
		ECigPathFailure::GoalBlocked);
	TestEqual(TEXT("Engel içindeki başlangıç adlandırılmalı"),
		Nav.FindPath(FVector2D(200.f, 0.f), FVector2D(0.f, 0.f)).Failure,
		ECigPathFailure::StartBlocked);

	FCigNavGrid Empty;
	TestEqual(TEXT("Kurulmamış grid NotBuilt demeli"),
		Empty.FindPath(FVector2D::ZeroVector, FVector2D::ZeroVector).Failure,
		ECigPathFailure::NotBuilt);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavNearestStandTest,
	"Cigkofte.Navigation.Grid.NearestStandIsTheNearestOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavNearestStandTest::RunTest(const FString&)
{
	// A seat sits inside its own table's footprint, so "walk to the seat" always
	// asks for a blocked point. Recovery has to be the closest standable spot and
	// not merely the first one a scan happened to reach, or a customer sent to a
	// chair walks to the far side of the table for no visible reason.
	const FCigNavGrid Nav = Grid({ MakeRect(0.f, 0.f, 100.f, 100.f) }, 20.f);

	FVector2D Stand = FVector2D::ZeroVector;
	// Off-centre inside the block: the nearest way out is +X, not any other side.
	TestTrue(TEXT("Engel içinden çıkış bulunmalı"), Nav.FindNearestWalkable(FVector2D(80.f, 0.f), Stand));
	TestTrue(TEXT("Bulunan nokta yürünebilir olmalı"), Nav.IsWalkable(Stand));
	TestTrue(TEXT("En yakın çıkış +X yönünde olmalı"), Stand.X > 100.f);
	TestTrue(TEXT("Yanlara sapmamalı"), FMath::Abs(Stand.Y) < 60.f);
	return true;
}

// --- The real shop ---------------------------------------------------------

namespace
{
	// The shop as the game builds it, with navigation attached.
	bool BuildRealShop(FAutomationTestBase& Test, FCigTestShop& Shop)
	{
		if (!Shop.Build(Test)) { return false; }
		if (!Shop.GM->Placement || !Shop.GM->WorldBuilder || !Shop.GM->Nav)
		{
			Test.AddError(TEXT("Navigasyon entegrasyonu için sistemler yok."));
			return false;
		}
		Shop.GM->WorldBuilder->BuildWorld();
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavDefaultLayoutTest,
	"Cigkofte.Navigation.Shop.DefaultLayoutKeepsEveryRequiredRouteOpen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavDefaultLayoutTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildRealShop(*this, Shop)) { return false; }

	const TArray<FCigRouteReport> Reports = Shop.GM->Nav->AuditRequiredRoutes();
	TestTrue(TEXT("Denetlenecek rota olmalı"), Reports.Num() > 0);

	// Every seat the shop built gets its own route, plus the entrance and the
	// service counter. A shipped default layout that cannot seat a customer it
	// offers a chair to is the defect this catches.
	int32 SeatRoutes = 0;
	for (const FCigRouteReport& Report : Reports)
	{
		TestTrue(FString::Printf(TEXT("Rota açık olmalı: %s"), *Report.RouteId.ToString()),
			Report.Result.bSuccess);
		if (Report.RouteId.ToString().StartsWith(TEXT("route.seat.")))
		{
			++SeatRoutes;
		}
	}
	TestEqual(TEXT("Her koltuk için bir rota denetlenmeli"),
		SeatRoutes, Shop.GM->WorldBuilder->Seats.Num());
	TestTrue(TEXT("Varsayılan yerleşimde dükkân dolaşılabilir olmalı"),
		Shop.GM->Nav->AreRequiredRoutesOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavBlockedDoorwayTest,
	"Cigkofte.Navigation.Shop.SealingTheShopfrontClosesTheEntranceRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavBlockedDoorwayTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildRealShop(*this, Shop)) { return false; }
	TestTrue(TEXT("Başlangıçta rotalar açık olmalı"), Shop.GM->Nav->AreRequiredRoutesOpen());

	// A slab across the whole shopfront opening. Deliberately built as a record
	// rather than pushed through validation: the question here is whether the
	// grid notices geometry that closes the shop, not whether validation would
	// have let it in.
	FCigPlacementRecord Barricade;
	Barricade.StableId = TEXT("test.barricade");
	Barricade.Category = ECigPlacementCategory::Decoration;
	Barricade.Lifetime = ECigPlacementLifetime::Installed;
	Barricade.Consequence.PhysicalRect = MakeRect(-680.f, 0.f, 30.f, 600.f);

	FName ClosedRoute = NAME_None;
	TestTrue(TEXT("Cepheyi kapatan yerleşim rota kapattığı için reddedilmeli"),
		Shop.GM->Nav->WouldCloseRequiredRoute(Barricade, ClosedRoute));
	TestNotEqual(TEXT("Kapanan rota adlandırılmalı"), ClosedRoute, FName(NAME_None));

	// And the shop it was asked about is untouched: a hypothetical must not
	// leave the live grid describing a shop nobody built.
	TestTrue(TEXT("Denetim canlı gridi bozmamalı"), Shop.GM->Nav->AreRequiredRoutesOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavInvalidationTest,
	"Cigkofte.Navigation.Invalidation.APlacementChangeRebuildsOnceOnNextQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavInvalidationTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildRealShop(*this, Shop)) { return false; }

	// One query settles the grid.
	Shop.GM->Nav->AreRequiredRoutesOpen();
	const int32 SettledRebuilds = Shop.GM->Nav->RebuildCount();
	TestFalse(TEXT("Sorgu sonrası grid temiz olmalı"), Shop.GM->Nav->IsDirty());

	// Repeated queries must not rebuild. This is the assertion that keeps a
	// per-frame path query from becoming a per-frame rasterisation of the shop.
	for (int32 Repeat = 0; Repeat < 5; ++Repeat)
	{
		Shop.GM->Nav->FindCustomerPath(
			FVector(CigNavLayout::StreetApproach(), 0.f),
			CigPlacementLayout::QueueFront());
	}
	TestEqual(TEXT("Değişiklik yokken yeniden kurulmamalı"),
		Shop.GM->Nav->RebuildCount(), SettledRebuilds);

	// A removal is a layout change and has to be noticed. Removing a table is
	// the honest case: it publishes PlacementChanged and its rectangle stops
	// blocking the floor it stood on.
	const FCigPlacementRecord* Table = Shop.GM->Placement->FindPlacement(TEXT("fixture.seating.table.0"));
	if (!TestNotNull(TEXT("Kaldırılacak masa olmalı"), Table))
	{
		return false;
	}
	const FVector2D WasBlocked = Table->Consequence.PhysicalRect.Center;
	TestFalse(TEXT("Masa dururken üstü yürünebilir olmamalı"),
		Shop.GM->Nav->IsCustomerStandable(FVector(WasBlocked, 0.f)));

	Shop.GM->Placement->RemovePlacement(TEXT("fixture.seating.table.0"));
	TestTrue(TEXT("Yerleşim değişikliği gridi kirletmeli"), Shop.GM->Nav->IsDirty());

	TestTrue(TEXT("Masa kalkınca zemin yürünebilir olmalı"),
		Shop.GM->Nav->IsCustomerStandable(FVector(WasBlocked, 0.f)));
	TestEqual(TEXT("Kirlenme tam olarak bir yeniden kurulum yapmalı"),
		Shop.GM->Nav->RebuildCount(), SettledRebuilds + 1);
	return true;
}

// --- Hardening: what a customer does when the answer is "no" ---------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavStrandedTest,
	"Cigkofte.Navigation.Hardening.ACustomerWithNoRouteStopsAndIsRecovered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavStrandedTest::RunTest(const FString&)
{
	// Stage 3.4 fell back to direct movement when no route existed, on the
	// reasoning that a frozen customer is worse than one clipping a table. The
	// trade was wrong in the one direction that mattered: the fallback fires
	// exactly on the layouts the measured navigation exists to catch, so the case
	// where the answer was needed was the case it was thrown away.
	FCigTestShop Shop;
	if (!BuildRealShop(*this, Shop)) { return false; }

	ACigkofteCustomer* Customer = Shop.World->SpawnActor<ACigkofteCustomer>(
		FVector(CigNavLayout::StreetApproach(), 0.f), FRotator::ZeroRotator);
	Customer->SetNavSystem(Shop.GM->Nav);
	if (!TestNotNull(TEXT("Test musterisi spawn edilmeli"), Customer))
	{
		return false;
	}

	// A target inside the east wall: standable nowhere, reachable by nothing.
	Customer->SetTarget(FVector(1000.f, 0.f, 0.f));

	TestTrue(TEXT("Rotasiz hedef musteriyi mahsur birakmali"), Customer->bNavStranded);
	TestEqual(TEXT("Mahsur musteri yol tasimamali"), Customer->PathPointCountForTest(), 0);

	// The load-bearing assertion: it does not creep toward the wall.
	const FVector Before = Customer->GetActorLocation();
	Customer->Tick(0.5f);
	Customer->Tick(0.5f);
	TestTrue(TEXT("Mahsur musteri hedefe dogru ilerlememeli"),
		Customer->GetActorLocation().Equals(Before, 1.f));

	Customer->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavStalePathTest,
	"Cigkofte.Navigation.Hardening.APlacementChangeInvalidatesAWalkingRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavStalePathTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!BuildRealShop(*this, Shop)) { return false; }

	const int32 Before = Shop.GM->Nav->LayoutRevision();
	TestTrue(TEXT("Yerlesim revizyonu sifirdan buyuk baslamali"), Before > 0);

	// The revision must move with the layout, not with the rebuild. Rebuilds are
	// lazy, so a walker watching the rebuild counter would keep following a route
	// through a table that is already standing in it.
	Shop.GM->Placement->RemovePlacement(TEXT("fixture.seating.table.1"));
	TestTrue(TEXT("Yerlesim degisikligi revizyonu ilerletmeli"),
		Shop.GM->Nav->LayoutRevision() > Before);
	TestEqual(TEXT("Revizyon degisikligi yeniden kurulum tetiklememeli"),
		Shop.GM->Nav->RebuildCount(), 0);

	// And a customer holding a route built before it repaths on the next tick.
	ACigkofteCustomer* Customer = Shop.World->SpawnActor<ACigkofteCustomer>(
		FVector(CigNavLayout::StreetApproach(), 0.f), FRotator::ZeroRotator);
	Customer->SetNavSystem(Shop.GM->Nav);
	if (!TestNotNull(TEXT("Test musterisi spawn edilmeli"), Customer)) { return false; }

	Customer->SetTarget(CigPlacementLayout::QueueFront());
	const int32 Built = Customer->PathRevisionForTest();
	TestTrue(TEXT("Yol bir revizyona bagli kurulmali"), Built > 0);

	Shop.GM->Placement->RemovePlacement(TEXT("fixture.seating.table.2"));
	Customer->Tick(0.016f);
	TestTrue(TEXT("Bayat yol bir sonraki tick'te yenilenmeli"),
		Customer->PathRevisionForTest() > Built);

	Customer->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavPoolResetTest,
	"Cigkofte.Navigation.Hardening.ThePoolReturnsACustomerWithNoNavigationState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavPoolResetTest::RunTest(const FString&)
{
	// A pooled actor keeping the previous visitor's route walks it from a
	// completely different spawn, and a pooled actor keeping their stranded flag
	// is recycled again the moment it arrives.
	FCigTestShop Shop;
	if (!BuildRealShop(*this, Shop)) { return false; }

	ACigkofteCustomer* Customer = Shop.World->SpawnActor<ACigkofteCustomer>(
		FVector(CigNavLayout::StreetApproach(), 0.f), FRotator::ZeroRotator);
	Customer->SetNavSystem(Shop.GM->Nav);
	if (!TestNotNull(TEXT("Test musterisi spawn edilmeli"), Customer)) { return false; }

	Customer->SetTarget(FVector(1000.f, 0.f, 0.f));
	TestTrue(TEXT("Once mahsur kalmali"), Customer->bNavStranded);

	Customer->Reactivate(FVector(CigNavLayout::StreetApproach(), 0.f));
	TestFalse(TEXT("Havuzdan donen musteri mahsur olmamali"), Customer->bNavStranded);
	TestEqual(TEXT("Havuzdan donen musteri yol tasimamali"), Customer->PathPointCountForTest(), 0);
	TestEqual(TEXT("Havuzdan donen musteri revizyon tasimamali"), Customer->PathRevisionForTest(), 0);

	Customer->Destroy();
	return true;
}

// --- Does the engine agree? ------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigNavCollisionAgreementTest,
	"Cigkofte.Navigation.Collision.TheEngineAgreesWithTheGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigNavCollisionAgreementTest::RunTest(const FString&)
{
	// Without this test the grid is a private opinion about a shop it has never
	// touched. Here the actual spawned geometry is asked, with the player's own
	// capsule, whether the floor the grid calls walkable is floor a body fits in.
	FCigTestShop Shop;
	if (!BuildRealShop(*this, Shop)) { return false; }

	UWorld* World = Shop.World;
	const float Radius = CigNavLayout::PlayerAgentRadius();
	// The player capsule is 38 x 88, and where its feet go is not a detail.
	//
	// The shop floor and the street pavement are different slabs at different
	// heights - 4.5 and 6.5 - so a capsule placed by one constant stands on one
	// and sinks half a centimetre into the other. The first version of this test
	// did exactly that and reported the pavement as an obstacle blocking the
	// queue, which would have been a real defect if it had been true.
	constexpr float CapsuleHalfHeight = 88.f;
	constexpr float StandClearance = 12.f;
	const FCollisionShape Body = FCollisionShape::MakeCapsule(Radius, CapsuleHalfHeight);

	// Reports what blocked, not just that something did. A disagreement between
	// the grid and the world is only actionable once the actor standing in the
	// way has a name.
	auto EngineBlocker = [World, &Body](const FVector2D& At, FString& OutWho)
	{
		TArray<FOverlapResult> Hits;
		World->OverlapMultiByChannel(Hits,
			FVector(At.X, At.Y, CapsuleHalfHeight + StandClearance), FQuat::Identity, ECC_Pawn, Body,
			FCollisionQueryParams::DefaultQueryParam);
		bool bBlocked = false;
		TArray<FString> Names;
		for (const FOverlapResult& Hit : Hits)
		{
			AActor* Actor = Hit.GetActor();
			// The grid describes shop geometry, not an ambient pedestrian who
			// happens to be crossing this sample when the query runs.
			if (Actor && Actor->IsA(ACigkofteCustomer::StaticClass()))
			{
				continue;
			}
			if (Actor && Hit.Component.IsValid()
				&& Hit.Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				bBlocked = true;
				Names.AddUnique(Actor->GetName());
			}
		}
		OutWho = FString::Join(Names, TEXT(", "));
		return bBlocked;
	};

	auto EngineBlocks = [&EngineBlocker](const FVector2D& At)
	{
		FString Ignored;
		return EngineBlocker(At, Ignored);
	};

	// Direction one: what the grid opens, a body must fit in. This is the
	// direction that matters - the other way round only costs a wasted cell.
	const FVector QueueFront = CigPlacementLayout::QueueFront();
	const TArray<FVector2D> ShouldBeOpen = {
		CigNavLayout::StreetApproach(),
		FVector2D(QueueFront.X, QueueFront.Y),
		CigPlacementLayout::ServiceRouteZone().Center,
		CigPlacementLayout::PlayerRouteZone().Center
	};
	for (const FVector2D& Point : ShouldBeOpen)
	{
		const bool bGridOpen = Shop.GM->Nav->IsPlayerStandable(FVector(Point, 0.f));
		TestTrue(FString::Printf(TEXT("Grid (%.0f,%.0f) noktasını açık saymalı"), Point.X, Point.Y),
			bGridOpen);
		if (bGridOpen)
		{
			FString Blocker;
			const bool bBlocked = EngineBlocker(Point, Blocker);
			TestFalse(
				FString::Printf(TEXT("Motor (%.0f,%.0f) noktasında gövdeyi engellememeli (engelleyen: %s)"),
					Point.X, Point.Y, Blocker.IsEmpty() ? TEXT("-") : *Blocker),
				bBlocked);
		}
	}

	// Direction two: solid geometry the grid must not be offering as floor.
	// Only points well inside an obstacle are asserted - a point just outside a
	// table is grid-blocked by agent inflation while the engine has nothing
	// there, and that disagreement is the design rather than a defect.
	TArray<FVector2D> ShouldBeSolid;
	for (const CigNavLayout::FCigShellWall& Wall : CigNavLayout::ShellWalls())
	{
		ShouldBeSolid.Add(Wall.Rect.Center);
	}
	if (const FCigPlacementRecord* Table = Shop.GM->Placement->FindPlacement(TEXT("fixture.seating.table.0")))
	{
		ShouldBeSolid.Add(Table->Consequence.PhysicalRect.Center);
	}

	for (const FVector2D& Point : ShouldBeSolid)
	{
		TestFalse(FString::Printf(TEXT("Grid (%.0f,%.0f) noktasını zemin saymamalı"), Point.X, Point.Y),
			Shop.GM->Nav->IsPlayerStandable(FVector(Point, 0.f)));
	}

	// The walls in particular have to be real: they are spawned from the same
	// rectangles the grid rasterises, and if that wiring ever breaks the grid
	// would still refuse them while the world let a body walk out of the shop.
	for (const CigNavLayout::FCigShellWall& Wall : CigNavLayout::ShellWalls())
	{
		TestTrue(
			FString::Printf(TEXT("Motor duvarı (%.0f,%.0f) katı görmeli"),
				Wall.Rect.Center.X, Wall.Rect.Center.Y),
			EngineBlocks(Wall.Rect.Center));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
