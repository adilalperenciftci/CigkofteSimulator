// Can the player walk through a tree?
//
// Street furniture was spawned with collision off - the default of SpawnProp,
// taken by both call sites - so trees and lamp posts were scenery you passed
// through. KNOWN_LIMITATIONS.md recorded it and named the better fix: give them
// collision, which is a separate change because it affects the player too.
//
// This asks the question the way the player does: put their capsule where the
// furniture is and see whether anything blocks it. That is deliberately not a
// check on a flag - a flag set on the wrong actor, or on a component whose
// response to ECC_Pawn is Overlap, would pass a flag test and still let somebody
// walk through a lamp post.

#include "Misc/AutomationTest.h"
#include "Navigation/CigPedRegion.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"
#include "Customers/CigkofteCustomer.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// The player's own capsule, matching the figures CigNavigationTests uses.
	constexpr float PlayerRadius = 34.f;
	constexpr float PlayerHalfHeight = 88.f;

	// What blocked, and where it stands. The name alone is not enough: a lane
	// closed by a building that always had collision is a different finding from
	// one closed by a tree this change just gave collision to, and telling them
	// apart needs the blocker's own position rather than the sample's.
	struct FCigBlocker
	{
		FString Name;
		FVector At = FVector::ZeroVector;
		float SampleY = 0.f;

		FString Describe() const
		{
			return FString::Printf(TEXT("%s @(%.0f,%.0f) ornek Y=%.0f"), *Name, At.X, At.Y, SampleY);
		}
	};

	// Walks the line of furniture and counts how many places along it a body
	// cannot stand. Sampling rather than aiming at one prop: trees carry a random
	// Y jitter, so an exact position is not knowable from outside.
	int32 BlockedSamplesAlong(UWorld* World, float X, float YFrom, float YTo, float Step,
		TArray<FCigBlocker>* OutWho = nullptr)
	{
		const FCollisionShape Body = FCollisionShape::MakeCapsule(PlayerRadius, PlayerHalfHeight);
		int32 Blocked = 0;
		for (float Y = YFrom; Y <= YTo; Y += Step)
		{
			TArray<FOverlapResult> Hits;
			World->OverlapMultiByChannel(Hits,
				FVector(X, Y, PlayerHalfHeight + 12.f), FQuat::Identity, ECC_Pawn, Body,
				FCollisionQueryParams::DefaultQueryParam);
			for (const FOverlapResult& Hit : Hits)
			{
				// People are not furniture. Ambient pedestrians walk these lanes,
				// so a sweep that counted them would report a lane as closed
				// because somebody happened to be standing in it - and the count
				// would change between runs, which is how this was found.
				if (Hit.GetActor() && Hit.GetActor()->IsA(ACigkofteCustomer::StaticClass()))
				{
					continue;
				}
				if (Hit.GetActor() && Hit.Component.IsValid()
					&& Hit.Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
				{
					++Blocked;
					if (OutWho)
					{
						FCigBlocker B;
						B.Name = Hit.GetActor()->GetName();
						B.At = Hit.GetActor()->GetActorLocation();
						B.SampleY = Y;
						OutWho->Add(B);
					}
					break;
				}
			}
		}
		return Blocked;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStreetFurnitureBlocksTest,
	"Cigkofte.World.TheStreetFurnitureIsSomethingYouWalkInto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStreetFurnitureBlocksTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->WorldBuilder) { AddError(TEXT("Dunya kurucu yok.")); return false; }
	Shop.GM->WorldBuilder->BuildWorld();

	UWorld* World = Shop.GM->GetWorld();
	if (!World) { AddError(TEXT("Dunya yok.")); return false; }

	// Trees run every 1500 along Y from -7000 to 7000, jittered by up to 150.
	// Sampling every 100 crosses each trunk somewhere.
	//
	// Only the west line is asserted, and that is a deliberate narrowing rather
	// than an oversight. Three assertions were written here; a mutation - putting
	// the collision flag back to false, one prop type at a time - showed that only
	// this one measured the change:
	//
	//   west trees   collision off -> 0 blocked   MEASURES THE FIX
	//   east trees   collision off -> still >0    EastTreeX is EastPavementMaxX,
	//                                             and the buildings behind it block
	//   lamp posts   collision off -> still >0    WestLampX is RoadMinX, the road
	//                                             edge already has geometry
	//
	// The two that pass either way were removed. An assertion that holds whether
	// or not the code is right is worse than no assertion: it reports success and
	// teaches whoever reads the file that something is covered when it is not.
	const int32 WestTrees = BlockedSamplesAlong(World, CigStreet::WestTreeX, -7000.f, 7000.f, 100.f);
	TestTrue(FString::Printf(TEXT("Bati agac hattinda gecilemez nokta olmali (bulunan: %d)"), WestTrees),
		WestTrees > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStreetFurnitureLeavesTheLaneOpenTest,
	"Cigkofte.World.GivingFurnitureCollisionDoesNotCloseThePavement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStreetFurnitureLeavesTheLaneOpenTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->WorldBuilder) { AddError(TEXT("Dunya kurucu yok.")); return false; }
	Shop.GM->WorldBuilder->BuildWorld();

	UWorld* World = Shop.GM->GetWorld();
	if (!World) { AddError(TEXT("Dunya yok.")); return false; }

	// The reason this change is safe: pedestrian lanes are inset from the
	// furniture lines, so nothing that walks a lane meets a trunk.
	//
	// Only furniture is counted, and that narrowing was earned. The first version
	// asked whether the lane centre was clear of *anything*, and it failed on
	// roughly one run in three - not because of this change but because a
	// building's collision reaches the east lane from an actor origin at about
	// x=-578, some 770 units away. That is a real finding and it is recorded in
	// KNOWN_LIMITATIONS.md, but it is not this test's business: an assertion that
	// fails intermittently for a reason its own name does not mention is a test
	// people learn to re-run rather than read.
	const FCigPedRegion Region = FCigPedRegion::MainStreet();
	if (Region.Lanes.Num() == 0) { AddError(TEXT("Yaya seridi yok.")); return false; }

	// The lines this change put blockers on. Anything standing further from them
	// than a trunk is wide belongs to somebody else.
	const float FurnitureLines[] = {
		CigStreet::WestTreeX, CigStreet::WestLampX,
		CigStreet::EastTreeX, CigStreet::EastLampX
	};
	constexpr float TrunkReach = 60.f;

	for (int32 i = 0; i < Region.Lanes.Num(); ++i)
	{
		const FCigPedLane& Lane = Region.Lanes[i];
		const float LaneX = Lane.Center().X;

		TArray<FCigBlocker> Who;
		BlockedSamplesAlong(World, LaneX, Lane.Min.Y, Lane.Max.Y, 250.f, &Who);

		TArray<FString> Furniture;
		for (const FCigBlocker& B : Who)
		{
			for (float LineX : FurnitureLines)
			{
				if (FMath::Abs(B.At.X - LineX) <= TrunkReach)
				{
					Furniture.AddUnique(B.Describe());
					break;
				}
			}
		}

		TestEqual(FString::Printf(TEXT("Serit %d mobilyayla kapanmamali - mobilya engeli: %s"),
			i, Furniture.Num() > 0 ? *FString::Join(Furniture, TEXT(", ")) : TEXT("(yok)")),
			Furniture.Num(), 0);
	}
	return true;
}

#endif
