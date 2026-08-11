// Can a customer actually walk the route the grid gave them?
//
// Every navigation test in this project so far asks one of two questions: does
// the grid return a path, and does the engine agree the grid's floor is floor.
// Neither of them moves anybody. The customer is steered in Tick by
// StepTowards, which is a sweep against ECC_WorldStatic - a completely separate
// authority from the grid, run against the actual spawned world rather than
// against placement rectangles.
//
// So there is a gap exactly the width of the thing the player complains about:
// a route that exists, a floor the engine calls open, and a body that does not
// move along it. Nothing here spawns a customer into the real shop and drives
// Tick until they arrive, which is the only way that gap shows up.
//
// These tests do that. They assert progress rather than a final position where
// they can, because progress is what "yürüyemiyor" means and a position test
// would pass on a customer that teleports.

#include "Misc/AutomationTest.h"
#include "Customers/CigCustomerSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Game/CigDaySystem.h"
#include "Game/CigkofteGameMode.h"
#include "Navigation/CigNavLayout.h"
#include "Navigation/CigNavSystem.h"
#include "Placement/CigPlacementTypes.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "CollisionShape.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// The real shop, built the way the game builds it. Not the empty harness
	// world: the whole question here is whether the spawned geometry lets a body
	// through, so the geometry has to be there.
	bool CigWalkTestsBuildShop(FAutomationTestBase& Test, FCigTestShop& Shop)
	{
		if (!Shop.Build(Test)) { return false; }
		if (!Shop.GM->WorldBuilder || !Shop.GM->Nav || !Shop.GM->Customers)
		{
			Test.AddError(TEXT("Yurume testi icin sistemler yok."));
			return false;
		}
		Shop.GM->WorldBuilder->BuildWorld();
		return true;
	}

	// Drives Tick at a fixed step and reports how far the customer got toward
	// their target. A fixed step rather than the world's delta because the step
	// length is what the collision sweep sees, and a variable one would make a
	// failure depend on the machine.
	float CigWalkTestsRun(ACigkofteCustomer& C, int32 Steps, float Step = 0.05f)
	{
		const FVector Start = C.GetActorLocation();
		for (int32 i = 0; i < Steps; ++i)
		{
			C.Tick(Step);
		}
		return FVector::Dist2D(Start, C.GetActorLocation());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCustomerWalksToQueueTest,
	"Cigkofte.Walking.ACustomerActuallyReachesTheQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCustomerWalksToQueueTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!CigWalkTestsBuildShop(*this, Shop)) { return false; }

	// Spawned where the customer system spawns them, and pointed where the queue
	// points them. Both numbers come from production rather than from this test,
	// so a change to either is a change this test sees.
	ACigkofteCustomer* C = Shop.GM->Customers->SpawnCustomer();
	if (!C) { AddError(TEXT("Musteri spawn edilemedi.")); return false; }
	C->SetNavSystem(Shop.GM->Nav);

	const FVector Front = CigPlacementLayout::QueueFront();
	C->SetTarget(Front);

	// The route has to exist before movement means anything. A stranded customer
	// failing to move is a different defect with its own test, and conflating the
	// two would let either one hide behind the other.
	if (C->bNavStranded)
	{
		AddError(FString::Printf(
			TEXT("Musteri kuyruga rota bulamadi (sebep %d); yurume olcumu anlamsiz."),
			(int32)C->NavFailure));
		return false;
	}

	const FVector Start = C->GetActorLocation();
	const float StartDist = FVector::Dist2D(Start, Front);
	TestTrue(TEXT("Baslangic hedefe uzak olmali"), StartDist > 200.f);

	// Twenty seconds at 300 units a second is over five thousand units of travel
	// for a route well under two thousand. If they have not arrived in that, they
	// are not walking - they are stuck against something.
	const float Moved = CigWalkTestsRun(*C, 400);
	const float EndDist = FVector::Dist2D(C->GetActorLocation(), Front);

	const FVector Stopped = C->GetActorLocation();
	AddInfo(FString::Printf(TEXT("Baslangic mesafe %.0f, kalan %.0f, alinan yol %.0f, durdugu yer (%.0f,%.0f)"),
		StartDist, EndDist, Moved, Stopped.X, Stopped.Y));

	// If they stopped short, name what is standing there. A distance alone says
	// the walk failed; the actor's name says which authority was wrong, which is
	// the difference between a fixable report and a shrug.
	if (EndDist >= 60.f)
	{
		TArray<FOverlapResult> Hits;
		Shop.World->OverlapMultiByChannel(Hits, Stopped + FVector(0.f, 0.f, 90.f),
			FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(60.f),
			FCollisionQueryParams::DefaultQueryParam);
		TArray<FString> Names;
		for (const FOverlapResult& Hit : Hits)
		{
			if (Hit.GetActor()) { Names.AddUnique(Hit.GetActor()->GetName()); }
		}
		AddInfo(FString::Printf(TEXT("Durdugu noktada duran: %s"),
			Names.Num() ? *FString::Join(Names, TEXT(", ")) : TEXT("-")));
		AddInfo(FString::Printf(TEXT("Grid burayi yurunebilir sayiyor mu: %d"),
			Shop.GM->Nav->IsCustomerStandable(Stopped) ? 1 : 0));
	}

	TestTrue(FString::Printf(TEXT("Musteri yerinden kimildamali (alinan yol %.0f)"), Moved),
		Moved > 50.f);
	TestTrue(FString::Printf(TEXT("Musteri kuyruga varmali (kalan %.0f)"), EndDist),
		EndDist < 60.f);
	TestFalse(TEXT("Yol boyunca mahsur kalmamali"), C->bNavStranded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCustomerWalkNotBlockedTest,
	"Cigkofte.Walking.NothingInTheShopStopsTheFirstStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCustomerWalkNotBlockedTest::RunTest(const FString&)
{
	// The narrower question, asked separately because it fails differently. The
	// test above measures the whole walk; this one measures the first step, which
	// is where a body spawned inside geometry shows up. A customer whose very
	// first sweep is blocked never gets a second one worth having.
	FCigTestShop Shop;
	if (!CigWalkTestsBuildShop(*this, Shop)) { return false; }

	ACigkofteCustomer* C = Shop.GM->Customers->SpawnCustomer();
	if (!C) { AddError(TEXT("Musteri spawn edilemedi.")); return false; }
	C->SetNavSystem(Shop.GM->Nav);
	C->SetTarget(CigPlacementLayout::QueueFront());

	if (C->bNavStranded)
	{
		AddError(TEXT("Musteri rotasiz; ilk adim olculemez."));
		return false;
	}

	// One step of a twentieth of a second at 300 a second is fifteen units. A
	// customer who moves less than a third of that on open ground is being held
	// by something.
	const float Moved = CigWalkTestsRun(*C, 1);
	TestTrue(FString::Printf(TEXT("Ilk adim ilerlemeli (alinan %.1f)"), Moved), Moved > 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigWalkSweepAgreesWithGridTest,
	"Cigkofte.Walking.TheWalkSweepAgreesWithTheGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigWalkSweepAgreesWithGridTest::RunTest(const FString&)
{
	// The navigation suite already asks whether the engine agrees with the grid -
	// but it asks with the player's capsule on ECC_Pawn, and that is not the
	// question a walking customer asks. StepTowards sweeps a 30-unit sphere at
	// chest height against ECC_WorldStatic. Two different shapes on two different
	// channels, so a thing that blocks one and not the other is invisible to both
	// the grid and the existing agreement test.
	//
	// This asks the customer's own question at the customer's own points.
	FCigTestShop Shop;
	if (!CigWalkTestsBuildShop(*this, Shop)) { return false; }

	UWorld* World = Shop.World;

	// The exact shape and channel StepTowards uses. Copied deliberately rather
	// than shared: if production changes them, this test should stop matching and
	// be updated, not silently follow.
	constexpr float BodyRadius = 30.f;
	constexpr float ChestHeight = 90.f;
	const FCollisionShape Sweep = FCollisionShape::MakeSphere(BodyRadius);

	auto WorldStaticBlocker = [World, &Sweep](const FVector& At, FString& OutWho)
	{
		TArray<FOverlapResult> Hits;
		World->OverlapMultiByChannel(Hits, At + FVector(0.f, 0.f, ChestHeight),
			FQuat::Identity, ECC_WorldStatic, Sweep, FCollisionQueryParams::DefaultQueryParam);
		TArray<FString> Names;
		for (const FOverlapResult& Hit : Hits)
		{
			AActor* Actor = Hit.GetActor();
			// Other customers are not geometry. The sweep is on WorldStatic
			// precisely so a queue does not deadlock on itself, and a pedestrian
			// crossing the sample would otherwise be reported as a wall.
			if (!Actor || Actor->IsA(ACigkofteCustomer::StaticClass())) { continue; }
			if (Hit.Component.IsValid()
				&& Hit.Component->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block)
			{
				Names.AddUnique(Actor->GetName());
			}
		}
		OutWho = FString::Join(Names, TEXT(", "));
		return Names.Num() > 0;
	};

	// Walk the real route the customer system uses: spawn point to the queue.
	// Sampled along the grid's own answer, so every point tested is one the grid
	// has already declared walkable.
	const FVector Spawn(-1750.f, 1200.f, 0.f);
	const FVector Front = CigPlacementLayout::QueueFront();
	const FCigPathResult Route = Shop.GM->Nav->FindCustomerPath(Spawn, Front);
	if (!Route.bSuccess)
	{
		AddError(TEXT("Grid spawn noktasindan kuyruga rota vermedi."));
		return false;
	}

	// Every corner, and the line between corners at the step length a walking
	// customer actually covers. A blocker between two waypoints stops the body
	// just as completely as one standing on a waypoint.
	int32 Disagreements = 0;
	for (int32 i = 0; i + 1 < Route.Points.Num(); ++i)
	{
		const FVector2D A = Route.Points[i];
		const FVector2D B = Route.Points[i + 1];
		const float Span = FVector2D::Distance(A, B);
		const int32 Samples = FMath::Max(1, FMath::CeilToInt(Span / 25.f));
		for (int32 S = 0; S <= Samples; ++S)
		{
			const FVector2D P = FMath::Lerp(A, B, (float)S / (float)Samples);
			const FVector At(P.X, P.Y, 0.f);
			FString Who;
			if (WorldStaticBlocker(At, Who))
			{
				++Disagreements;
				AddError(FString::Printf(
					TEXT("Grid (%.0f,%.0f) noktasini yol sayiyor ama yurume supurmesi orada takiliyor (engelleyen: %s)"),
					P.X, P.Y, *Who));
			}
		}
	}

	TestEqual(TEXT("Rota uzerinde gridin bilmedigi engel olmamali"), Disagreements, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCustomerWalkPastPersonTest,
	"Cigkofte.Walking.APersonStandingInTheWayIsNotAWall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCustomerWalkPastPersonTest::RunTest(const FString&)
{
	// StepTowards sweeps against ECC_WorldStatic and its comment says why: "so a
	// queue does not deadlock on itself". People are supposed to be invisible to
	// that sweep. This asks whether they actually are.
	//
	// It matters because the route between the street and the counter crosses the
	// east pavement, and eight ambient pedestrians walk that pavement at random Y.
	// If a person blocks the sweep, a customer walking in stops dead behind
	// whoever happens to be standing there - which is intermittent by
	// construction, and looks exactly like "customers cannot walk".
	FCigTestShop Shop;
	if (!CigWalkTestsBuildShop(*this, Shop)) { return false; }

	ACigkofteCustomer* C = Shop.GM->Customers->SpawnCustomer();
	if (!C) { AddError(TEXT("Musteri spawn edilemedi.")); return false; }
	C->SetNavSystem(Shop.GM->Nav);

	// Open pavement, well clear of the shop and of any furniture, so the only
	// thing this measures is the other person.
	const FVector From(-1350.f, 600.f, 0.f);
	const FVector To(-1350.f, 0.f, 0.f);
	C->SetActorLocation(From);
	C->SetTarget(To);
	if (C->bNavStranded)
	{
		// Outside the navigable region there is no route, and that is by design;
		// the walk is direct there. Not a failure of this test's subject.
		AddInfo(TEXT("Bu nokta grid disinda; yurume dogrudan."));
	}

	// A control run with nobody in the way, so the comparison below is against
	// this shop rather than against a number chosen here.
	const float Clear = CigWalkTestsRun(*C, 60);

	// Now the same walk with a person standing halfway along it.
	C->SetActorLocation(From);
	C->SetTarget(To);
	ACigkofteCustomer* Bystander = Shop.World->SpawnActor<ACigkofteCustomer>(
		FVector(-1350.f, 300.f, 0.f), FRotator::ZeroRotator);
	if (!Bystander) { AddError(TEXT("Yoldaki kisi spawn edilemedi.")); return false; }
	Bystander->InitVisuals();

	const float Blocked = CigWalkTestsRun(*C, 60);

	AddInfo(FString::Printf(TEXT("Bos yolda %.0f, insan varken %.0f"), Clear, Blocked));

	// People are not walls. A body may slow slightly while rounding somebody, but
	// a person must not stop a customer the way a counter does.
	TestTrue(FString::Printf(
		TEXT("Yoldaki insan musteriyi durdurmamali (bos %.0f, insanli %.0f)"), Clear, Blocked),
		Blocked > Clear * 0.8f);

	Bystander->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCustomerWalkThroughDoorTest,
	"Cigkofte.Walking.AGuestCanCrossTheShopToASeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigCustomerWalkThroughDoorTest::RunTest(const FString&)
{
	// The queue is on the pavement; a seated guest is the only customer who
	// crosses the shopfront and rounds the counter. That is the longest route the
	// game asks for and the one with real geometry either side of it, so it is
	// where a body that fits the grid but not the world would be caught.
	FCigTestShop Shop;
	if (!CigWalkTestsBuildShop(*this, Shop)) { return false; }

	if (Shop.GM->WorldBuilder->Seats.Num() == 0)
	{
		AddError(TEXT("Dukkanda sandalye yok; oturma rotasi surulemez."));
		return false;
	}
	const FVector Seat = Shop.GM->WorldBuilder->Seats[0].Pos;

	ACigkofteCustomer* C = Shop.GM->Customers->SpawnCustomer();
	if (!C) { AddError(TEXT("Musteri spawn edilemedi.")); return false; }
	C->SetNavSystem(Shop.GM->Nav);

	// Put them at the counter first, which is where a served guest starts from.
	C->SetActorLocation(CigPlacementLayout::QueueFront());
	C->SetTarget(Seat);

	if (C->bNavStranded)
	{
		AddError(FString::Printf(TEXT("Koltuga rota yok (sebep %d)."), (int32)C->NavFailure));
		return false;
	}

	const float StartDist = FVector::Dist2D(C->GetActorLocation(), Seat);
	CigWalkTestsRun(*C, 600);
	const float EndDist = FVector::Dist2D(C->GetActorLocation(), Seat);

	AddInfo(FString::Printf(TEXT("Koltuga baslangic %.0f, kalan %.0f"), StartDist, EndDist));

	// Not asserted as arrival at the chair: the seat rectangle is solid by
	// construction, so the grid stops the body beside it rather than on it. What
	// has to be true is that the distance closed substantially.
	TestTrue(FString::Printf(TEXT("Misafir koltuga yaklasmali (baslangic %.0f, kalan %.0f)"),
		StartDist, EndDist), EndDist < StartDist * 0.35f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
