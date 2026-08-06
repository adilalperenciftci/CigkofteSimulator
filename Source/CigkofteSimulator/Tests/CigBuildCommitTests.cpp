// Committing a move, and everything that has to move with it.
//
// The record is only half of a placement. A table also has actors nobody can find
// from its id without the visual registry, and seats that customers walk to - and
// the failure mode when those are forgotten is not a crash but a shop where people
// walk to where a table used to be. The saved-layout path found that first; a
// player moving a table asks exactly the same question, which is why both go
// through one function.

#include "Misc/AutomationTest.h"
#include "Placement/CigBuildVerdict.h"
#include "Placement/CigPlacementSystem.h"
#include "Navigation/CigNavSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// A seated placement, because seats are the thing that can be left behind.
	const FCigPlacementRecord* FindSeatedPlacement(const UCigWorldBuilder& WB,
		const UCigPlacementSystem& Placement)
	{
		for (const UCigWorldBuilder::FCigSeat& Seat : WB.Seats)
		{
			if (Seat.PlacementId.IsNone())
			{
				continue;
			}
			if (const FCigPlacementRecord* R = Placement.FindPlacement(Seat.PlacementId))
			{
				if (R->Lifetime == ECigPlacementLifetime::Installed)
				{
					return R;
				}
			}
		}
		return nullptr;
	}

	int32 CountSeatsOf(const UCigWorldBuilder& WB, FName PlacementId)
	{
		int32 N = 0;
		for (const UCigWorldBuilder::FCigSeat& Seat : WB.Seats)
		{
			if (Seat.PlacementId == PlacementId) { ++N; }
		}
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildCommitSeatsTest,
	"Cigkofte.BuildMode.MovingATableTakesItsChairsWithIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildCommitSeatsTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->WorldBuilder) { AddError(TEXT("Sistemler yok.")); return false; }
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	UCigPlacementSystem* Placement = Shop.GM->Placement.Get();
	WB->BuildWorld();

	const FCigPlacementRecord* Found = FindSeatedPlacement(*WB, *Placement);
	if (!Found) { AddError(TEXT("Sandalyeli yerlesim yok.")); return false; }
	const FCigPlacementRecord Record = *Found; // copied: registering invalidates pointers

	const int32 SeatCount = CountSeatsOf(*WB, Record.StableId);
	if (SeatCount == 0) { AddError(TEXT("Sandalye sayilmadi.")); return false; }

	// Where the seats stood before, in the order the world holds them.
	TArray<FVector> SeatsBefore;
	for (const UCigWorldBuilder::FCigSeat& Seat : WB->Seats)
	{
		if (Seat.PlacementId == Record.StableId) { SeatsBefore.Add(Seat.Pos); }
	}

	// A small move, chosen small on purpose: a large one risks being refused for a
	// reason that has nothing to do with what is being tested here.
	const FTransform Previous = Record.Transform;
	FTransform Target = Previous;
	Target.SetLocation(Previous.GetLocation() + FVector(0.f, 60.f, 0.f));

	const FCigPlacementRequest Request = CigBuildVerdict::MakeMoveRequest(Record, Target);
	const FCigPlacementResult Result = Placement->RegisterPlacement(Request);
	if (!Result.bAccepted)
	{
		AddError(FString::Printf(TEXT("Tasima reddedildi: %s"),
			*UCigPlacementSystem::FailureText(Result.Failure)));
		return false;
	}

	const int32 SeatsMoved = WB->FollowPlacement(Record.StableId, Previous);
	TestEqual(TEXT("Masanin butun sandalyeleri tasinmali"), SeatsMoved, SeatCount);

	// Each seat moved by the same delta the table did, which is what keeps a chair
	// beside its table rather than merely somewhere else.
	const FCigPlacementRecord* After = Placement->FindPlacement(Record.StableId);
	if (!After) { AddError(TEXT("Kayit kayboldu.")); return false; }
	const FVector Delta = After->Transform.GetLocation() - Previous.GetLocation();

	int32 Index = 0;
	for (const UCigWorldBuilder::FCigSeat& Seat : WB->Seats)
	{
		if (Seat.PlacementId != Record.StableId) { continue; }
		if (!SeatsBefore.IsValidIndex(Index)) { break; }
		const FVector Expected = SeatsBefore[Index] + Delta;
		TestTrue(FString::Printf(TEXT("Sandalye %d masayla ayni kadar tasinmali"), Index),
			Seat.Pos.Equals(Expected, 1.f));
		++Index;
	}

	// And the seats did not stay where they were, which is the defect this exists
	// to catch: a passing test on an unmoved table would prove nothing.
	TestFalse(TEXT("Sandalyeler yerinde kalmamali"), Delta.IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildCommitRoutesTest,
	"Cigkofte.BuildMode.TheShopStillWorksAfterAMoveIsCommitted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildCommitRoutesTest::RunTest(const FString&)
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

	const FCigPlacementRecord* Found = FindSeatedPlacement(*WB, *Placement);
	if (!Found) { AddError(TEXT("Sandalyeli yerlesim yok.")); return false; }
	const FCigPlacementRecord Record = *Found;

	const int32 RecordsBefore = Placement->PlacementCount();
	const int32 CapacityBefore = Placement->FunctionalCapacityByCategory(Record.Category);

	const FTransform Previous = Record.Transform;
	FTransform Target = Previous;
	Target.SetLocation(Previous.GetLocation() + FVector(0.f, 60.f, 0.f));

	const FCigPlacementRequest Request = CigBuildVerdict::MakeMoveRequest(Record, Target);
	const FCigPlacementResult Validation = Placement->ValidatePlacement(Request);
	if (!Validation.bAccepted) { AddError(TEXT("Aday dogrulanamadi.")); return false; }

	// The verdict said yes to this before it was committed, so the shop must still
	// work afterwards. If it does not, the preview was answering a question the
	// commit did not ask.
	FCigPlacementRecord Candidate;
	if (!CigBuildVerdict::MakeCandidateRecord(Request, Validation.NormalizedTransform, Candidate))
	{
		AddError(TEXT("Aday kayit uretilemedi.")); return false;
	}
	FName ClosedRoute = NAME_None;
	const bool bWouldClose = Shop.GM->Nav->WouldCloseRequiredRoute(Candidate, ClosedRoute);
	const FCigBuildVerdict Verdict = CigBuildVerdict::Combine(Validation, bWouldClose, ClosedRoute);
	if (!Verdict.IsAccepted()) { AddError(TEXT("Karar kabul degil; test bu adayi secmemeli.")); return false; }

	if (!Placement->RegisterPlacement(Request).bAccepted)
	{
		AddError(TEXT("Onay reddedildi.")); return false;
	}
	WB->FollowPlacement(Record.StableId, Previous);

	TestTrue(TEXT("Onaydan sonra rotalar acik kalmali"), Shop.GM->Nav->AreRequiredRoutesOpen());

	// Counted before the move, not against itself. A move is a move rather than a
	// remove and an add: getting that wrong would leave the old footprint standing
	// in the authority while the visible table stood somewhere else, and the shop
	// would slowly fill with the ghosts of every table anybody had ever shifted.
	TestEqual(TEXT("Tasima kayit sayisini degistirmemeli"),
		Placement->PlacementCount(), RecordsBefore);
	// And the seats it offers are the same seats. A move that quietly re-derived
	// capacity would change how many customers the shop can hold.
	TestEqual(TEXT("Tasima islevsel kapasiteyi degistirmemeli"),
		Placement->FunctionalCapacityByCategory(Record.Category), CapacityBefore);
	const FCigPlacementRecord* After = Placement->FindPlacement(Record.StableId);
	if (!After) { AddError(TEXT("Kayit kayboldu.")); return false; }
	TestFalse(TEXT("Kayit gercekten tasinmis olmali"),
		After->Transform.GetLocation().Equals(Previous.GetLocation(), 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildCommitIdentityTest,
	"Cigkofte.BuildMode.CommittingWithoutMovingLeavesEverythingWhereItWas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildCommitIdentityTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->WorldBuilder) { AddError(TEXT("Sistemler yok.")); return false; }
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	UCigPlacementSystem* Placement = Shop.GM->Placement.Get();
	WB->BuildWorld();

	const FCigPlacementRecord* Found = FindSeatedPlacement(*WB, *Placement);
	if (!Found) { AddError(TEXT("Sandalyeli yerlesim yok.")); return false; }
	const FCigPlacementRecord Record = *Found;

	TArray<FVector> SeatsBefore;
	for (const UCigWorldBuilder::FCigSeat& Seat : WB->Seats)
	{
		if (Seat.PlacementId == Record.StableId) { SeatsBefore.Add(Seat.Pos); }
	}

	// Picking a table up and putting it straight back down. FollowPlacement must
	// notice nothing changed rather than reapplying a zero delta - which would be
	// harmless here, and would not be if the relative capture ever drifted.
	const FTransform Previous = Record.Transform;
	if (!Placement->RegisterPlacement(CigBuildVerdict::MakeMoveRequest(Record, Previous)).bAccepted)
	{
		AddError(TEXT("Durdugu yere onay reddedildi.")); return false;
	}
	TestEqual(TEXT("Kimildatmayan onay sandalye tasimamali"),
		WB->FollowPlacement(Record.StableId, Previous), 0);

	int32 Index = 0;
	for (const UCigWorldBuilder::FCigSeat& Seat : WB->Seats)
	{
		if (Seat.PlacementId != Record.StableId) { continue; }
		if (!SeatsBefore.IsValidIndex(Index)) { break; }
		TestTrue(FString::Printf(TEXT("Sandalye %d yerinde kalmali"), Index),
			Seat.Pos.Equals(SeatsBefore[Index], 0.01f));
		++Index;
	}
	return true;
}

#endif
