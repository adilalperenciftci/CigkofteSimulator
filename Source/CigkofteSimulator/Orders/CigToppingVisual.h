#pragma once

#include "CoreMinimal.h"
#include "Core/CigkofteTypes.h"

// Where each topping sits on the open flatbread, and what it looks like there.
//
// This was seven identical spheres in a straight line, one colour apart, laid
// out by an index arithmetic expression inside the update function. It read as a
// row of beads rather than as food, and there was no way to move one without
// moving all of them.
//
// Pure data with a pure lookup, for the same reason FCigDoughVisual is pure:
// where the pieces go is a design decision that will be revised often, and it
// should be revisable without a world, a wrap or a running game. See
// Tests/CigToppingVisualTests.cpp.
struct FCigToppingPlacement
{
	// Colour rather than a model. At the distance the player builds a wrap from,
	// a piece of lettuce is read by its colour and nothing else, and the shop has
	// no lettuce model to load: the vegetable packs are photoscanned produce
	// meant for the market stalls outside.
	FLinearColor Color = FLinearColor::White;

	// Offset from the wrap origin, in centimetres. X is across the bread's short
	// axis, Y along its length, Z above the surface.
	FVector Offset = FVector::ZeroVector;

	// Piece size. A tomato slice is not a parsley fleck, and having them the same
	// size was most of why the row read as beads.
	float Scale = 0.045f;

	// How many pieces of this topping to show. Herbs are scattered; a slice is
	// one thing. Pieces after the first are offset along the bread by Spread.
	int32 Count = 1;
	float Spread = 3.0f;
};

// The table. Order must match ECigTopping.
//
// The bread is about 40cm along Y and 16 across X in station space, so nothing
// here reaches beyond 16 on Y or 6 on X: a topping outside that is sitting on
// the counter rather than on the food.
namespace CigToppingVisual
{
	inline const FCigToppingPlacement& Placement(ECigTopping T)
	{
		static const FCigToppingPlacement Table[(int32)ECigTopping::COUNT] = {
			// Marul - shredded, so several small pieces spread the length of it.
			{ FLinearColor(0.35f, 0.68f, 0.24f), FVector(-2.0f, -11.0f, 6.0f), 0.038f, 3, 3.4f },
			// Maydanoz - finer and darker than the lettuce, scattered wider.
			{ FLinearColor(0.20f, 0.52f, 0.18f), FVector( 2.5f,  -6.0f, 6.2f), 0.028f, 4, 2.6f },
			// Domates - slices, the largest single pieces on the bread.
			{ FLinearColor(0.82f, 0.18f, 0.14f), FVector(-1.0f,   0.5f, 6.4f), 0.062f, 2, 5.0f },
			// Tursu - cut smaller than the tomato and a duller green.
			{ FLinearColor(0.55f, 0.62f, 0.24f), FVector( 3.0f,   5.5f, 6.2f), 0.042f, 2, 3.6f },
			// Sogan - rings, pale, sitting on top of the rest.
			{ FLinearColor(0.93f, 0.90f, 0.84f), FVector(-2.5f,   9.5f, 6.8f), 0.046f, 2, 4.0f },
			// Limon - one wedge, at the end where a hand would squeeze it.
			{ FLinearColor(0.95f, 0.85f, 0.25f), FVector( 1.5f,  13.5f, 6.4f), 0.050f, 1, 0.f },
			// Nar eksisi - a drizzle rather than a piece: small, dark, and run
			// down the length of the bread so it reads as poured on.
			//
			// Centred on the bread, not offset like the others. Six pieces at
			// 4.8 apart is a 24cm run, and starting that at -13 put half of it
			// on the counter - which the placement test caught before anybody
			// looked at a wrap. A drizzle is the one topping that spans the
			// whole thing, so it has nowhere else to start from.
			{ FLinearColor(0.45f, 0.12f, 0.16f), FVector( 4.5f,   0.0f, 6.6f), 0.022f, 6, 4.8f },
		};
		static_assert(UE_ARRAY_COUNT(Table) == (int32)ECigTopping::COUNT,
			"Garnitür yerleşim tablosu ECigTopping ile aynı uzunlukta olmalı");

		const int32 i = FMath::Clamp((int32)T, 0, (int32)ECigTopping::COUNT - 1);
		return Table[i];
	}

	// Where piece N of a topping goes. Pieces run along the bread from the
	// placement's own offset, centred on it, so adding a piece does not shift
	// the ones already there off to one side.
	inline FVector PieceOffset(ECigTopping T, int32 Piece)
	{
		const FCigToppingPlacement& P = Placement(T);
		const int32 Count = FMath::Max(P.Count, 1);
		const int32 Clamped = FMath::Clamp(Piece, 0, Count - 1);
		const float Along = (Clamped - (Count - 1) * 0.5f) * P.Spread;
		return P.Offset + FVector(0.f, Along, 0.f);
	}

	// The most pieces any one topping asks for, so the caller can size a pool
	// once instead of growing an array while the player is building a wrap.
	inline int32 MaxPieces()
	{
		int32 Most = 1;
		for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
		{
			Most = FMath::Max(Most, Placement((ECigTopping)i).Count);
		}
		return Most;
	}
}
