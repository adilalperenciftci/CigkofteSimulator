// Where street pedestrians are allowed to be, and that they stay there.
//
// The bug these were written against was not subtle once the numbers were put
// side by side: the wander rectangle handed to main-street pedestrians was
// X in [-2250, -1450] and the carriageway is X in [-2150, -1450]. They spent the
// game walking in both lanes of moving traffic. They also walked through every
// tree and lamp post, because street furniture is spawned with collision
// disabled and the sweep in StepTowards only asks about WorldStatic blockers.
//
// So the Region group checks the geometry claim directly - no lane may overlap
// the road, and both lanes must sit on a pavement - and the Containment group
// checks the property that makes it hold at runtime: a position is clamped into
// its lane, so no step length can put a pedestrian in the road.

#include "Misc/AutomationTest.h"
#include "Navigation/CigPedRegion.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// A lane running north-south, like a pavement.
	FCigPedRegion Strip(float MinX = -100.f, float MaxX = 100.f, float MinY = -1000.f, float MaxY = 1000.f)
	{
		return FCigPedRegion::SingleLane(FVector2D(MinX, MinY), FVector2D(MaxX, MaxY));
	}

	bool OverlapsRoad(const FCigPedLane& Lane)
	{
		return Lane.Max.X > CigStreet::RoadMinX && Lane.Min.X < CigStreet::RoadMaxX;
	}

	bool WithinPavement(const FCigPedLane& Lane, float PavementMinX, float PavementMaxX)
	{
		return Lane.Min.X >= PavementMinX && Lane.Max.X <= PavementMaxX;
	}
}

// --------------------------------------------------------------------- region

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedStreetLanesTest,
	"Cigkofte.Pedestrian.Region.MainStreetHasALaneOnEachPavement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedStreetLanesTest::RunTest(const FString&)
{
	const FCigPedRegion Street = FCigPedRegion::MainStreet();

	TestEqual(TEXT("Ana caddede iki kaldırım şeridi olmalı"), Street.Lanes.Num(), 2);
	if (Street.Lanes.Num() != 2)
	{
		return false;
	}

	TestTrue(TEXT("Batı şeridi batı kaldırımının içinde olmalı"),
		WithinPavement(Street.Lanes[0], CigStreet::WestPavementMinX, CigStreet::WestPavementMaxX));
	TestTrue(TEXT("Doğu şeridi doğu kaldırımının içinde olmalı"),
		WithinPavement(Street.Lanes[1], CigStreet::EastPavementMinX, CigStreet::EastPavementMaxX));

	for (const FCigPedLane& Lane : Street.Lanes)
	{
		TestTrue(TEXT("Şerit yürünebilir genişlikte olmalı"), Lane.IsValid());
		TestTrue(TEXT("Şerit caddenin boyunca uzanmalı"), Lane.Extent().Y > Lane.Extent().X);
	}
	return true;
}

// The whole point. This is the assertion the old rectangle failed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedNoRoadOverlapTest,
	"Cigkofte.Pedestrian.Region.NoLaneOverlapsTheCarriageway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedNoRoadOverlapTest::RunTest(const FString&)
{
	const FCigPedRegion Street = FCigPedRegion::MainStreet();
	for (const FCigPedLane& Lane : Street.Lanes)
	{
		TestFalse(TEXT("Hiçbir yaya şeridi yola taşmamalı"), OverlapsRoad(Lane));
	}

	// Named separately from the road box: a lane can clear the asphalt and still
	// sit where a car drives if the lanes are ever widened.
	for (const float CarLane : { CigStreet::CarLaneWest, CigStreet::CarLaneEast })
	{
		for (const FCigPedLane& Lane : Street.Lanes)
		{
			TestFalse(TEXT("Yaya şeridi araç şeridini kapsamamalı"),
				CarLane >= Lane.Min.X && CarLane <= Lane.Max.X);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedFurnitureClearanceTest,
	"Cigkofte.Pedestrian.Region.LanesClearTheStreetFurniture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedFurnitureClearanceTest::RunTest(const FString&)
{
	const FCigPedRegion Street = FCigPedRegion::MainStreet();

	// Trees and lamps have no collision, so nothing at runtime would stop a
	// pedestrian walking through one. Clearance is the only thing that does.
	for (const FCigPedLane& Lane : Street.Lanes)
	{
		for (const float TreeX : { CigStreet::WestTreeX, CigStreet::EastTreeX })
		{
			const float Gap = FMath::Min(FMath::Abs(Lane.Min.X - TreeX), FMath::Abs(Lane.Max.X - TreeX));
			TestTrue(TEXT("Şerit ağaç hattından gövde yarıçapı kadar uzak olmalı"),
				(TreeX < Lane.Min.X || TreeX > Lane.Max.X) && Gap >= CigStreet::PedRadius);
		}
		for (const float LampX : { CigStreet::WestLampX, CigStreet::EastLampX })
		{
			const float Gap = FMath::Min(FMath::Abs(Lane.Min.X - LampX), FMath::Abs(Lane.Max.X - LampX));
			TestTrue(TEXT("Şerit lamba hattından gövde yarıçapı kadar uzak olmalı"),
				(LampX < Lane.Min.X || LampX > Lane.Max.X) && Gap >= CigStreet::PedRadius);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedDegenerateLaneTest,
	"Cigkofte.Pedestrian.Region.ADegenerateLaneIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedDegenerateLaneTest::RunTest(const FString&)
{
	FCigPedRegion Region;
	TestEqual(TEXT("Sıfır genişlikli şerit reddedilmeli"),
		Region.AddLane(FVector2D(0.f, 0.f), FVector2D(0.f, 100.f)), (int32)INDEX_NONE);
	TestTrue(TEXT("Reddedilen şerit saklanmamalı"), Region.IsEmpty());

	// Given backwards, which is a caller mistake rather than a degenerate lane.
	TestEqual(TEXT("Ters verilen şerit düzeltilmeli"),
		Region.AddLane(FVector2D(100.f, 100.f), FVector2D(-100.f, -100.f)), 0);
	TestTrue(TEXT("Düzeltilen şerit merkezi içermeli"), Region.Lanes[0].Contains(FVector2D::ZeroVector));
	return true;
}

// ----------------------------------------------------------------- containment

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedClampContainsTest,
	"Cigkofte.Pedestrian.Containment.AnyPointClampsIntoTheLane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedClampContainsTest::RunTest(const FString&)
{
	const FCigPedRegion Street = FCigPedRegion::MainStreet();

	// Walk the whole carriageway and both pavements. Every one of these is a
	// position the old code could produce and none may survive the clamp.
	for (float X = CigStreet::WestPavementMinX; X <= CigStreet::EastPavementMaxX; X += 10.f)
	{
		const FVector2D Point(X, 0.f);
		const FVector2D Clamped = Street.ClampToLane(0, Point, CigStreet::PedRadius);
		TestTrue(TEXT("Kelepçelenen nokta şeridin içinde olmalı"), Street.Lanes[0].Contains(Clamped));
		TestFalse(TEXT("Kelepçelenen nokta yolda olmamalı"),
			Clamped.X > CigStreet::RoadMinX && Clamped.X < CigStreet::RoadMaxX);
	}
	return true;
}

// A long frame is the case the rectangle never survived: one step can be wider
// than the lane, and the collision sweep knows nothing about pavements.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedNoTunnellingTest,
	"Cigkofte.Pedestrian.Containment.ALongStepCannotCrossTheRoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedNoTunnellingTest::RunTest(const FString&)
{
	const FCigPedRegion Street = FCigPedRegion::MainStreet();
	const FCigPedLane& West = Street.Lanes[0];

	// 220 cm/s for two whole seconds, straight at the far pavement.
	FVector2D Pos = West.Center();
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Pos.X += 440.f;
		Pos = Street.ClampToLane(0, Pos, CigStreet::PedRadius);
		TestTrue(TEXT("Uzun adım sonrası da şeritte kalmalı"), West.Contains(Pos));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedObstacleTest,
	"Cigkofte.Pedestrian.Containment.AnObstacleIsWalkedRoundNotThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedObstacleTest::RunTest(const FString&)
{
	FCigPedRegion Region = Strip();
	Region.AddObstacle(FVector2D(0.f, 0.f), 60.f);

	TestFalse(TEXT("Engelin merkezi yürünebilir sayılmamalı"),
		Region.IsWalkable(0, FVector2D::ZeroVector, 30.f));

	const FVector2D Pushed = Region.ClampToLane(0, FVector2D::ZeroVector, 30.f);
	TestTrue(TEXT("Engelden çıkarılan nokta şeritte kalmalı"), Region.Lanes[0].Contains(Pushed));
	TestTrue(TEXT("Engelden çıkarılan nokta engelin dışında olmalı"),
		Region.IsWalkable(0, Pushed, 30.f));

	// Out along the lane rather than across it: the lane is 200 wide and the disc
	// is 180 across, so radially out is out of the lane.
	TestTrue(TEXT("Engelden çıkış şeridin uzun ekseninde olmalı"),
		FMath::Abs(Pushed.Y) > FMath::Abs(Pushed.X));

	// Leaving by the nearer end, so a pedestrian steps round rather than walking
	// the length of the street.
	const FVector2D FromSouth = Region.ClampToLane(0, FVector2D(0.f, -20.f), 30.f);
	TestTrue(TEXT("Güneyden gelen güneye çıkmalı"), FromSouth.Y < 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedObstacleClusterTest,
	"Cigkofte.Pedestrian.Containment.ACoveredLaneStillReturnsALanePoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedObstacleClusterTest::RunTest(const FString&)
{
	// Obstacles closer together than an agent is wide: there is no clear point,
	// and the answer must still be inside the lane rather than a hang or a point
	// in the road.
	FCigPedRegion Region = Strip(-100.f, 100.f, -200.f, 200.f);
	for (float Y = -200.f; Y <= 200.f; Y += 40.f)
	{
		Region.AddObstacle(FVector2D(0.f, Y), 90.f);
	}

	const FVector2D Result = Region.ClampToLane(0, FVector2D::ZeroVector, 30.f);
	TestTrue(TEXT("Kapalı şeritte bile sonuç şeridin içinde olmalı"), Region.Lanes[0].Contains(Result));
	return true;
}

// ---------------------------------------------------------------------- routes

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedDeterministicTest,
	"Cigkofte.Pedestrian.Route.TheSameSeedWalksTheSameRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedDeterministicTest::RunTest(const FString&)
{
	const FCigPedRegion Street = FCigPedRegion::MainStreet();

	auto Walk = [&Street](int32 Seed)
	{
		FRandomStream Stream(Seed);
		TArray<FVector2D> Points;
		for (int32 i = 0; i < 12; ++i)
		{
			Points.Add(Street.PickTarget(0, Stream, CigStreet::PedRadius));
		}
		return Points;
	};

	const TArray<FVector2D> First = Walk(1234);
	const TArray<FVector2D> Again = Walk(1234);
	const TArray<FVector2D> Other = Walk(5678);

	TestEqual(TEXT("Aynı tohum aynı hedef dizisini vermeli"), First, Again);
	TestNotEqual(TEXT("Farklı tohum farklı yürüyüş vermeli"), First, Other);

	for (const FVector2D& Point : First)
	{
		TestTrue(TEXT("Seçilen her hedef yürünebilir olmalı"),
			Street.IsWalkable(0, Point, CigStreet::PedRadius));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedTargetAvoidsObstaclesTest,
	"Cigkofte.Pedestrian.Route.TargetsAreNotChosenInsideProps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedTargetAvoidsObstaclesTest::RunTest(const FString&)
{
	FCigPedRegion Region = Strip();
	Region.AddObstacle(FVector2D(0.f, 0.f), 150.f);
	Region.AddObstacle(FVector2D(0.f, 600.f), 150.f);

	FRandomStream Stream(99);
	for (int32 i = 0; i < 50; ++i)
	{
		const FVector2D Target = Region.PickTarget(0, Stream, 30.f);
		TestTrue(TEXT("Hedef şeridin içinde olmalı"), Region.Lanes[0].Contains(Target));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedNearestLaneTest,
	"Cigkofte.Pedestrian.Route.APedestrianInTheRoadIsPutOnAPavement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedNearestLaneTest::RunTest(const FString&)
{
	const FCigPedRegion Street = FCigPedRegion::MainStreet();

	// Standing in the middle of the road: no lane contains it, and it still has to
	// be given one.
	const FVector2D InRoad((CigStreet::RoadMinX + CigStreet::RoadMaxX) * 0.5f, 0.f);
	TestEqual(TEXT("Yolun ortası hiçbir şeridin içinde olmamalı"),
		Street.FindLane(InRoad), (int32)INDEX_NONE);

	const int32 Lane = Street.NearestLane(InRoad);
	TestTrue(TEXT("Yoldaki yayaya bir şerit verilmeli"), Street.Lanes.IsValidIndex(Lane));

	const FVector2D Placed = Street.ClampToLane(Lane, InRoad, CigStreet::PedRadius);
	TestFalse(TEXT("Yerleştirilen yaya yolda kalmamalı"),
		Placed.X > CigStreet::RoadMinX && Placed.X < CigStreet::RoadMaxX);

	// A pedestrian already on a pavement keeps that pavement.
	for (int32 Index = 0; Index < Street.Lanes.Num(); ++Index)
	{
		TestEqual(TEXT("Kaldırımdaki yaya kendi şeridinde kalmalı"),
			Street.NearestLane(Street.Lanes[Index].Center()), Index);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPedDistrictRectangleTest,
	"Cigkofte.Pedestrian.Route.ADistrictIsOneHonestRectangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPedDistrictRectangleTest::RunTest(const FString&)
{
	// Districts are not surveyed and the type must not pretend otherwise: one
	// lane, no obstacles, containment but no claim of a route.
	const FCigPedRegion District = FCigPedRegion::SingleLane(
		FVector2D(-3500.f, -13100.f), FVector2D(-100.f, -9700.f));

	TestEqual(TEXT("İlçe tek dikdörtgen olmalı"), District.Lanes.Num(), 1);
	TestEqual(TEXT("İlçede engel listesi olmamalı"), District.Obstacles.Num(), 0);

	FRandomStream Stream(7);
	for (int32 i = 0; i < 20; ++i)
	{
		TestTrue(TEXT("İlçe hedefi dikdörtgenin içinde olmalı"),
			District.Lanes[0].Contains(District.PickTarget(0, Stream, 30.f)));
	}
	return true;
}

#endif
