// Where the toppings sit on the open flatbread.
//
// This was seven identical spheres in a line, laid out by index arithmetic
// inside the update function. The table replacing it is a design decision that
// will be revised often, so these tests pin the things that must hold however
// the numbers move: the food stays on the bread, no two toppings look the same,
// and adding a piece does not push the existing ones sideways.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.ToppingVisual; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Orders/CigToppingVisual.h"
#include "Core/CigkofteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigToppingsStayOnTheBread,
	"Cigkofte.ToppingVisual.EveryPieceLandsOnTheFlatbread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigToppingsStayOnTheBread::RunTest(const FString&)
{
	// The bread's half-extents in station space. A piece outside these is on the
	// counter, not on the food - which is the failure this catches, and it is
	// invisible in code review because the offsets are just numbers.
	constexpr float HalfX = 6.5f;
	constexpr float HalfY = 17.5f;

	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		const ECigTopping T = (ECigTopping)i;
		const FCigToppingPlacement& P = CigToppingVisual::Placement(T);

		TestTrue(FString::Printf(TEXT("Garnitür %d en az bir parça göstermeli"), i), P.Count >= 1);
		TestTrue(FString::Printf(TEXT("Garnitür %d ekmeğin üstünde durmalı"), i), P.Offset.Z > 0.f);
		TestTrue(FString::Printf(TEXT("Garnitür %d görünür boyutta olmalı"), i), P.Scale > 0.f);

		for (int32 Piece = 0; Piece < P.Count; ++Piece)
		{
			const FVector O = CigToppingVisual::PieceOffset(T, Piece);
			TestTrue(FString::Printf(TEXT("Garnitür %d parça %d ekmeğin eninde kalmalı"), i, Piece),
				FMath::Abs(O.X) <= HalfX);
			TestTrue(FString::Printf(TEXT("Garnitür %d parça %d ekmeğin boyunda kalmalı"), i, Piece),
				FMath::Abs(O.Y) <= HalfY);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigToppingsAreToldApart,
	"Cigkofte.ToppingVisual.NoTwoToppingsLookOrSitTheSame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigToppingsAreToldApart::RunTest(const FString&)
{
	// Colour is the only thing carrying identity at this distance, and position
	// is the only thing carrying "these are two separate things". Two toppings
	// sharing either would read as one, which is exactly what the row of beads
	// did.
	for (int32 a = 0; a < (int32)ECigTopping::COUNT; ++a)
	{
		for (int32 b = a + 1; b < (int32)ECigTopping::COUNT; ++b)
		{
			const FCigToppingPlacement& A = CigToppingVisual::Placement((ECigTopping)a);
			const FCigToppingPlacement& B = CigToppingVisual::Placement((ECigTopping)b);

			TestFalse(FString::Printf(TEXT("%d ve %d aynı renkte olmamalı"), a, b),
				A.Color.Equals(B.Color, 0.02f));
			TestTrue(FString::Printf(TEXT("%d ve %d üst üste oturmamalı"), a, b),
				FVector::Dist(A.Offset, B.Offset) > 2.f);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigToppingPiecesStayCentred,
	"Cigkofte.ToppingVisual.PiecesSpreadAroundTheirOwnOffset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigToppingPiecesStayCentred::RunTest(const FString&)
{
	// Pieces run along the bread centred on the placement, so changing a count
	// does not shift the topping off to one side - the reason the arithmetic is
	// (i - (n-1)/2) rather than plain i.
	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		const ECigTopping T = (ECigTopping)i;
		const FCigToppingPlacement& P = CigToppingVisual::Placement(T);

		FVector Sum = FVector::ZeroVector;
		for (int32 Piece = 0; Piece < P.Count; ++Piece)
		{
			Sum += CigToppingVisual::PieceOffset(T, Piece);
		}
		const FVector Mean = Sum / FMath::Max(P.Count, 1);

		TestTrue(FString::Printf(TEXT("Garnitür %d parçaları kendi noktasında ortalanmalı"), i),
			FVector::Dist(Mean, P.Offset) < 0.01f);
	}

	// Out of range asks are clamped rather than reading past the end of the
	// table - the caller loops over a pool sized to the largest count, so it
	// will ask a one-piece topping for piece three.
	const FVector First = CigToppingVisual::PieceOffset(ECigTopping::Limon, 0);
	TestEqual(TEXT("Aralık dışı parça ilk parçaya kırpılmalı"),
		CigToppingVisual::PieceOffset(ECigTopping::Limon, 99), First);
	TestEqual(TEXT("Negatif parça ilk parçaya kırpılmalı"),
		CigToppingVisual::PieceOffset(ECigTopping::Limon, -3), First);

	TestTrue(TEXT("Havuz en kalabalık garnitürü karşılamalı"),
		CigToppingVisual::MaxPieces() >= 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
