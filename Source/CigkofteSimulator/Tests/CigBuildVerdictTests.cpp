// Whether a proposed move would be allowed, and which of two authorities gets to
// say why not.
//
// The placement authority reasons about rectangles and the navigation grid about
// width, and a candidate can fail both at once - a slab across the doorway
// overlaps the entrance zone and closes the entrance route. Only one of those two
// answers tells the player where to move their hands, so the order they are asked
// in is a decision rather than an implementation detail, and it is what most of
// this file is about.

#include "Misc/AutomationTest.h"
#include "Placement/CigBuildVerdict.h"
#include "Placement/CigPlacementSystem.h"
#include "Navigation/CigNavSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigPlacementResult AcceptedResult()
	{
		FCigPlacementResult R;
		R.bAccepted = true;
		R.NormalizedTransform = FTransform(FVector(100.f, 200.f, 0.f));
		return R;
	}

	FCigPlacementResult RefusedResult(ECigPlacementFailure Failure, const TCHAR* Conflict)
	{
		FCigPlacementResult R;
		R.bAccepted = false;
		R.Failure = Failure;
		R.ConflictingStableId = FName(Conflict);
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildVerdictOrderTest,
	"Cigkofte.BuildMode.ARefusedPlacementKeepsItsOwnReasonRatherThanTheRouteOne",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildVerdictOrderTest::RunTest(const FString&)
{
	// The case the ordering exists for: both authorities would refuse. Reporting
	// the route would be true and useless - the player would go looking for a
	// corridor when the actual problem is the sofa they are standing the table on.
	const FCigBuildVerdict Both = CigBuildVerdict::Combine(
		RefusedResult(ECigPlacementFailure::Overlap, TEXT("kanepe.01")),
		true, FName(TEXT("rota.giris")));

	TestFalse(TEXT("Iki otorite de reddederse kabul edilmemeli"), Both.IsAccepted());
	TestTrue(TEXT("Yerlesim reddi rota reddinin onunde gelmeli"),
		Both.Status == ECigBuildVerdictStatus::Refused);
	TestTrue(TEXT("Kendi sebebini korumali"), Both.Failure == ECigPlacementFailure::Overlap);
	TestEqual(TEXT("Cakisan nesneyi adlandirmali"), Both.ConflictingStableId, FName(TEXT("kanepe.01")));
	// And it does not quietly carry the route it was not going to report.
	TestTrue(TEXT("Bildirilmeyen rota tasinmamali"), Both.ClosedRouteId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildVerdictRouteTest,
	"Cigkofte.BuildMode.APlacementTheRectanglesAllowCanStillCloseARoute",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildVerdictRouteTest::RunTest(const FString&)
{
	// The reason the grid is consulted at all: a candidate can pass every
	// rectangle rule and still leave a gap narrower than a customer.
	const FCigBuildVerdict Closed = CigBuildVerdict::Combine(
		AcceptedResult(), true, FName(TEXT("rota.giris")));

	TestFalse(TEXT("Rota kapatan aday kabul edilmemeli"), Closed.IsAccepted());
	TestTrue(TEXT("Durum ClosesRoute olmali"), Closed.Status == ECigBuildVerdictStatus::ClosesRoute);
	// Named, because "somewhere would become unreachable" without saying where is
	// the kind of message that teaches a player to ignore messages.
	TestEqual(TEXT("Kapanan rota adlandirilmali"), Closed.ClosedRouteId, FName(TEXT("rota.giris")));
	TestTrue(TEXT("Yerlesim hatasi uydurulmamali"), Closed.Failure == ECigPlacementFailure::None);

	const FCigBuildVerdict Fine = CigBuildVerdict::Combine(AcceptedResult(), false, NAME_None);
	TestTrue(TEXT("Iki otorite de kabul ederse kabul edilmeli"), Fine.IsAccepted());
	TestTrue(TEXT("Kabul edilen adayda kapanan rota olmamali"), Fine.ClosedRouteId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildVerdictNormalizedTest,
	"Cigkofte.BuildMode.TheGhostIsDrawnWhereCommittingWouldPutIt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildVerdictNormalizedTest::RunTest(const FString&)
{
	// The authority snaps position and rotation. Drawing the ghost at the raw
	// candidate rather than the normalized one would put the preview a few
	// centimetres from where committing lands it, which is the sort of difference
	// nobody notices until a table will not fit and the ghost said it would.
	const FCigPlacementResult Snapped = AcceptedResult();
	const FCigBuildVerdict Verdict = CigBuildVerdict::Combine(Snapped, false, NAME_None);
	TestTrue(TEXT("Karar normalize edilmis donusumu tasimali"),
		Verdict.NormalizedTransform.GetLocation().Equals(Snapped.NormalizedTransform.GetLocation()));

	// A refusal carries it too, so a red ghost still stands where the thing would
	// have gone rather than jumping somewhere else on the frame it turns red.
	const FCigBuildVerdict Refused = CigBuildVerdict::Combine(
		RefusedResult(ECigPlacementFailure::Overlap, TEXT("kanepe.01")), false, NAME_None);
	TestTrue(TEXT("Reddedilen aday da bir yerde durmali"),
		Refused.NormalizedTransform.GetLocation().Equals(FVector::ZeroVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildMoveRequestTest,
	"Cigkofte.BuildMode.AMoveIgnoresExactlyItsOwnRecord",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildMoveRequestTest::RunTest(const FString&)
{
	FCigPlacementRecord Table;
	Table.StableId = TEXT("masa.01");
	Table.Category = ECigPlacementCategory::Seating;
	Table.Lifetime = ECigPlacementLifetime::Installed;
	Table.Footprint.Size = FVector2D(160.f, 280.f);
	Table.UseSpec.Size = FVector2D(160.f, 280.f);
	Table.UseSpec.FunctionalCapacity = 2;

	const FCigPlacementRequest Request =
		CigBuildVerdict::MakeMoveRequest(Table, FTransform(FVector(500.f, 0.f, 0.f)));

	// Without this the table would be refused for overlapping the record of where
	// it currently stands, which is to say no table could ever be moved at all.
	TestEqual(TEXT("Tasima kendi kaydini yok saymali"), Request.IgnoreStableId, Table.StableId);
	// And exactly its own: a request that ignored something else would let a move
	// silently overwrite a neighbour.
	TestEqual(TEXT("Yok sayilan kimlik nesnenin kendisi olmali"), Request.IgnoreStableId, Request.StableId);
	TestTrue(TEXT("Baglam MoveExisting olmali"), Request.Context == ECigPlacementContext::MoveExisting);
	// The geometry is the record's own. A move changes where a thing is, not what
	// it is, and a request that rebuilt the footprint could quietly resize it.
	TestTrue(TEXT("Ayak izi kaydin kendisinden gelmeli"), Request.Footprint.Size.Equals(Table.Footprint.Size));
	TestEqual(TEXT("Islevsel kapasite korunmali"), Request.UseSpec.FunctionalCapacity, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildVerdictRealShopTest,
	"Cigkofte.BuildMode.MovingSomethingToWhereItAlreadyStandsIsAllowed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildVerdictRealShopTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement) { AddError(TEXT("Yerlesim sistemi yok.")); return false; }

	// The authored layout is registered while the world is built, not when the
	// shop object is constructed, so without this there are no records to move.
	Shop.GM->WorldBuilder->BuildWorld();

	// Whichever installed placement the authored shop happens to hold. Naming one
	// would make this a test about that table rather than about moves.
	const FCigPlacementRecord* Existing = nullptr;
	for (const FCigPlacementRecord& R : Shop.GM->Placement->PlacementRecords())
	{
		if (R.Lifetime == ECigPlacementLifetime::Installed)
		{
			Existing = &R;
			break;
		}
	}
	if (!Existing) { AddError(TEXT("Kurulu yerlesim yok.")); return false; }

	const FCigPlacementRecord Record = *Existing; // copied: registering invalidates pointers

	// The identity move. It is the sharpest test of IgnoreStableId there is: every
	// rectangle rule is satisfied by construction, so the only thing that can
	// refuse it is the record of itself.
	const FCigPlacementRequest Same = CigBuildVerdict::MakeMoveRequest(Record, Record.Transform);
	const FCigPlacementResult SameResult = Shop.GM->Placement->ValidatePlacement(Same);
	if (!SameResult.bAccepted)
	{
		AddError(FString::Printf(TEXT("Durdugu yere tasima reddedildi: %s"),
			*UCigPlacementSystem::FailureText(SameResult.Failure)));
		return false;
	}
	const FCigBuildVerdict SameVerdict = CigBuildVerdict::Combine(SameResult, false, NAME_None);
	TestTrue(TEXT("Durdugu yere tasima kabul edilmeli"), SameVerdict.IsAccepted());

	// And a move far outside the shop is refused with a reason, not silently.
	const FCigPlacementRequest FarAway =
		CigBuildVerdict::MakeMoveRequest(Record, FTransform(FVector(50000.f, 50000.f, 0.f)));
	const FCigBuildVerdict FarVerdict =
		CigBuildVerdict::Combine(Shop.GM->Placement->ValidatePlacement(FarAway), false, NAME_None);
	TestFalse(TEXT("Dukkan disina tasima kabul edilmemeli"), FarVerdict.IsAccepted());
	TestTrue(TEXT("Ret bir sebep tasimali"), FarVerdict.Failure != ECigPlacementFailure::None);
	TestFalse(TEXT("Ret kendini aciklamali"), CigBuildVerdict::Describe(FarVerdict).IsEmpty());

	// The hypothetical changed nothing: validation is a question, not a move.
	const FCigPlacementRecord* After = Shop.GM->Placement->FindPlacement(Record.StableId);
	if (!After) { AddError(TEXT("Kayit kayboldu.")); return false; }
	TestTrue(TEXT("Dogrulama kaydi kimildatmamali"),
		After->Transform.GetLocation().Equals(Record.Transform.GetLocation()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildCandidateRecordTest,
	"Cigkofte.BuildMode.TheCandidateHandedToTheGridCarriesDerivedGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildCandidateRecordTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->Nav) { AddError(TEXT("Sistemler yok.")); return false; }

	// The authored layout is registered while the world is built, not when the
	// shop object is constructed, so without this there are no records to move.
	Shop.GM->WorldBuilder->BuildWorld();

	const FCigPlacementRecord* Existing = nullptr;
	for (const FCigPlacementRecord& R : Shop.GM->Placement->PlacementRecords())
	{
		if (R.Lifetime == ECigPlacementLifetime::Installed)
		{
			Existing = &R;
			break;
		}
	}
	if (!Existing) { AddError(TEXT("Kurulu yerlesim yok.")); return false; }
	const FCigPlacementRecord Record = *Existing;

	const FCigPlacementRequest Request = CigBuildVerdict::MakeMoveRequest(Record, Record.Transform);
	const FCigPlacementResult Validation = Shop.GM->Placement->ValidatePlacement(Request);

	FCigPlacementRecord Candidate;
	TestTrue(TEXT("Aday kayit uretilebilmeli"),
		CigBuildVerdict::MakeCandidateRecord(Request, Validation.NormalizedTransform, Candidate));

	// The grid reads the consequence rectangle and nothing else, so a candidate
	// without one would be handed over as a placement occupying no floor - and
	// every move would be reported as leaving every route open.
	TestTrue(TEXT("Aday kaydin fiziksel dikdortgeni olmali"),
		Candidate.Consequence.PhysicalRect.HalfExtent.X > 0.f
			&& Candidate.Consequence.PhysicalRect.HalfExtent.Y > 0.f);
	TestEqual(TEXT("Aday kimligini korumali"), Candidate.StableId, Record.StableId);

	// Where it already stands cannot close a route, because it is already there.
	FName ClosedRoute = NAME_None;
	TestFalse(TEXT("Durdugu yer rota kapatmamali"),
		Shop.GM->Nav->WouldCloseRequiredRoute(Candidate, ClosedRoute));
	return true;
}

#endif
