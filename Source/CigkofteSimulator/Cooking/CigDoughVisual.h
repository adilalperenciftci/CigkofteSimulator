#pragma once

#include "CoreMinimal.h"

// What the dough looks like, derived from what it is.
//
// The station used to be told two numbers - how full the bowl is and how far
// the kneading has got - and it lerped between two hardcoded colours. Everything
// else the player had done to that batch was invisible: a bowl loaded with isot
// looked the same as one with none, a batch left out until it was nearly dead
// looked exactly like a fresh one, and a careless mix looked like a perfect one.
// Since the wrap's price, the customer's reaction and the review all read those
// values, the one thing that could not see them was the thing on the counter.
//
// Pure and free of Unreal state on purpose: it takes numbers and returns a
// colour, so the derivation can be tested without a world, a station or a mesh
// (see Tests/CigDoughVisualTests.cpp). The actor keeps the job of pushing the
// result into a material.
struct FCigDoughVisual
{
	// How much is in the bowl or the ball, 0-1.
	float Fill01 = 0.f;
	// Kneading progress, 0-1. A finished batch sits at 1.
	float Knead01 = 0.f;
	// Isot as a fraction of the batch, normalised against the hottest recipe's
	// upper band rather than against 1: a bowl is never a third isot, so scaling
	// to the real range is what makes the difference visible at all.
	float Spice01 = 0.f;
	// Mix quality, 0-1, against the recipe's ceiling.
	float Quality01 = 1.f;
	// Freshness, 0-1. Falls while the batch sits out.
	float Freshness01 = 1.f;

	// Nothing to show until something is in the bowl.
	bool IsVisible() const { return Fill01 > 0.f; }

	// The ball's radius as a fraction of the station's, so an empty bowl and a
	// full one are told apart at a glance across the room.
	float Scale() const { return 0.25f + 0.55f * FMath::Clamp(Fill01, 0.f, 1.f); }

	// How well the batch holds together. Kneading is what does it, so this is
	// Knead01 by another name - but it is named for what the player sees rather
	// than for the counter that drives it, because the two stop agreeing the
	// moment anything else affects cohesion.
	float Cohesion01() const { return FMath::Clamp(Knead01, 0.f, 1.f); }

	// Not a uniform ball. Wet bulgur that has just been mixed slumps: it spreads
	// across the bowl and sits low. Worked dough holds its shape and stands up.
	// The volume is roughly preserved between the two, so the batch does not
	// appear to grow or shrink as it comes together - only to gather.
	//
	// This is the whole visible difference between a bowl the player has stirred
	// twice and one that is ready, and until now there was none: the ball was a
	// sphere at every stage and only its colour moved.
	FVector Scale3D() const
	{
		const float S = Scale();
		const float Loose = 1.f - Cohesion01();
		return FVector(S * (1.f + 0.30f * Loose), S * (1.f + 0.30f * Loose), S * (1.f - 0.40f * Loose));
	}

	// Bulgur soaked in paste and isot: pale and dry at the start, dark and red
	// once it has been worked. The order matters - spice reads through kneading,
	// because isot stirred into a loose bowl has not coloured anything yet.
	FLinearColor Color() const
	{
		const FLinearColor Raw(0.76f, 0.60f, 0.42f);      // dry bulgur
		const FLinearColor Kneaded(0.45f, 0.12f, 0.06f);  // worked, plain
		const FLinearColor Hot(0.62f, 0.08f, 0.04f);      // worked, heavy isot

		const float K = FMath::Clamp(Knead01, 0.f, 1.f);
		const float S = FMath::Clamp(Spice01, 0.f, 1.f);
		const float Q = FMath::Clamp(Quality01, 0.f, 1.f);
		const float F = FMath::Clamp(Freshness01, 0.f, 1.f);

		FLinearColor C = FMath::Lerp(Raw, FMath::Lerp(Kneaded, Hot, S), K);

		// A poor mix is duller: too much water or too little paste both read as
		// washed out rather than as a different colour.
		const float Doygunluk = 0.75f + 0.25f * Q;
		const float Gri = (C.R + C.G + C.B) / 3.f;
		C = FMath::Lerp(FLinearColor(Gri, Gri, Gri, 1.f), C, Doygunluk);

		// Staling darkens and greys it. Half strength, because a batch at zero
		// freshness is still sellable and should not look like ash.
		const float Bayat = (1.f - F) * 0.5f;
		C = FMath::Lerp(C, FLinearColor(0.32f, 0.29f, 0.26f, 1.f), Bayat);

		C.A = 1.f;
		return C;
	}

	// Two visuals are the same if nothing a player could see has changed. The
	// station uses this to skip material writes: the kneading station ticks and
	// pushes its state every stroke, and a SetVectorParameterValue that writes
	// the value already there still costs a render-thread command.
	bool NearlyEqual(const FCigDoughVisual& Other, float Tolerance = 0.005f) const
	{
		return FMath::IsNearlyEqual(Fill01, Other.Fill01, Tolerance)
			&& FMath::IsNearlyEqual(Knead01, Other.Knead01, Tolerance)
			&& FMath::IsNearlyEqual(Spice01, Other.Spice01, Tolerance)
			&& FMath::IsNearlyEqual(Quality01, Other.Quality01, Tolerance)
			&& FMath::IsNearlyEqual(Freshness01, Other.Freshness01, Tolerance);
	}
};
