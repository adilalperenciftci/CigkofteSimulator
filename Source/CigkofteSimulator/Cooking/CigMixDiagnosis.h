#pragma once

#include "CoreMinimal.h"
#include "Core/CigkofteTypes.h"

// What is wrong with the bowl, while it can still be put right.
//
// The mix has always been judged silently. QualityFromBowl adds up the ratio
// errors and hands back a number that reaches the player at the till, three
// minutes and one customer later, as a smaller payment and a worse review. The
// HUD showed target counts - "Su 0 / 3" - which tells a player who is reading
// carefully what to add next and tells nobody at all that the batch they are
// about to knead is already ruined.
//
// This names the single worst thing and says whether it can still be fixed by
// adding, which is the difference between a mistake and a loss: too little water
// is one keypress away from correct, and too much isot is recoverable by
// bulking the bowl out - but only while there is room left in it.
//
// Pure, like FCigDoughVisual, and for the same reason: what counts as "wrong
// enough to warn about" is a balance decision that will move, and moving it
// should not need a running game. See Tests/CigMixDiagnosisTests.cpp.

enum class ECigMixProblem : uint8
{
	None,
	NoBulgur,      // nothing to measure the ratios against yet
	TooLittleSu,
	TooMuchSu,
	TooLittleSalca,
	TooMuchSalca,
	TooLittleBaharat,
	TooMuchBaharat,
	TooLittleIsot,
	TooMuchIsot,
	COUNT
};

struct FCigMixDiagnosis
{
	ECigMixProblem Problem = ECigMixProblem::None;

	// How far off, as a fraction of the target. Carried so the caller can decide
	// how loudly to say it rather than having that decided here.
	float Severity = 0.f;

	// Whether adding to the bowl can still put it right. "Too little" always can,
	// while there is room. "Too much" can only be diluted, and a full bowl has
	// nowhere to dilute into - at which point the honest answer is to dump it.
	bool bFixableByAdding = false;

	bool IsProblem() const { return Problem != ECigMixProblem::None; }
};

namespace CigMix
{
	// Below this the mix is close enough that saying anything would be noise.
	// A player hitting the target counts exactly still lands a few percent out
	// on the ratios, and warning them about that would teach them to ignore it.
	inline constexpr float WarnThreshold = 0.35f;

	// Diagnoses the bowl against a recipe.
	//
	// Ratios are all measured against bulgur, the way QualityFromBowl does, so
	// the two cannot disagree about what is wrong. Isot is the exception and is
	// measured against the whole bowl, because heat is a property of the batch
	// rather than of the bulgur in it.
	inline FCigMixDiagnosis Diagnose(
		const int32 Bowl[(int32)ECigIngredient::COUNT],
		float RatioSu, float RatioSalca, float RatioBaharat,
		float IsotMinFrac, float IsotMaxFrac,
		int32 Total, int32 Capacity)
	{
		FCigMixDiagnosis D;

		const float B = (float)Bowl[(int32)ECigIngredient::Bulgur];
		if (B <= 0.f)
		{
			// Not a fault to report while the bowl is empty; it is only a fault
			// once there is something in it that is not bulgur.
			if (Total > 0)
			{
				D.Problem = ECigMixProblem::NoBulgur;
				D.Severity = 1.f;
				D.bFixableByAdding = Total < Capacity;
			}
			return D;
		}

		const bool bRoom = Total < Capacity;

		// Each candidate as (signed error against target, too-little, too-much).
		// The largest absolute error wins: naming three problems at once is the
		// same as naming none, and the player fixes them one keypress at a time
		// anyway.
		struct FCandidate
		{
			float Error;
			ECigMixProblem Low;
			ECigMixProblem High;
		};

		auto RatioError = [B](float Have, float Target)
		{
			const float Safe = FMath::Max(Target, 0.05f);
			return (Have / B - Target) / Safe;
		};

		const FCandidate Candidates[] = {
			{ RatioError((float)Bowl[(int32)ECigIngredient::Su], RatioSu),
			  ECigMixProblem::TooLittleSu, ECigMixProblem::TooMuchSu },
			{ RatioError((float)Bowl[(int32)ECigIngredient::Salca], RatioSalca),
			  ECigMixProblem::TooLittleSalca, ECigMixProblem::TooMuchSalca },
			{ RatioError((float)Bowl[(int32)ECigIngredient::Baharat], RatioBaharat),
			  ECigMixProblem::TooLittleBaharat, ECigMixProblem::TooMuchBaharat },
		};

		float Worst = 0.f;
		for (const FCandidate& C : Candidates)
		{
			if (FMath::Abs(C.Error) > FMath::Abs(Worst))
			{
				Worst = C.Error;
				D.Problem = C.Error < 0.f ? C.Low : C.High;
			}
		}

		// Isot, against the whole bowl and against a band rather than a point:
		// a recipe accepts a range of heat and only the ends of it are wrong.
		const float IsotFrac = (float)Bowl[(int32)ECigIngredient::Isot] / (float)FMath::Max(Total, 1);
		float IsotError = 0.f;
		if (IsotFrac < IsotMinFrac)
		{
			IsotError = (IsotFrac - IsotMinFrac) / FMath::Max(IsotMinFrac, 0.02f);
		}
		else if (IsotFrac > IsotMaxFrac)
		{
			IsotError = (IsotFrac - IsotMaxFrac) / FMath::Max(IsotMaxFrac, 0.02f);
		}
		if (FMath::Abs(IsotError) > FMath::Abs(Worst))
		{
			Worst = IsotError;
			D.Problem = IsotError < 0.f ? ECigMixProblem::TooLittleIsot : ECigMixProblem::TooMuchIsot;
		}

		D.Severity = FMath::Abs(Worst);
		if (D.Severity < WarnThreshold)
		{
			D.Problem = ECigMixProblem::None;
			D.Severity = 0.f;
			return D;
		}

		// Too little of something is fixed by adding it. Too much is fixed by
		// adding everything else, which is only possible while the bowl has room.
		D.bFixableByAdding = bRoom;
		return D;
	}

	// The text key for a problem. Kept here so the enum and its wording cannot
	// drift apart in two files.
	inline const TCHAR* TextKey(ECigMixProblem P)
	{
		switch (P)
		{
		case ECigMixProblem::NoBulgur:         return TEXT("mix.nobulgur");
		case ECigMixProblem::TooLittleSu:      return TEXT("mix.toolittle.su");
		case ECigMixProblem::TooMuchSu:        return TEXT("mix.toomuch.su");
		case ECigMixProblem::TooLittleSalca:   return TEXT("mix.toolittle.salca");
		case ECigMixProblem::TooMuchSalca:     return TEXT("mix.toomuch.salca");
		case ECigMixProblem::TooLittleBaharat: return TEXT("mix.toolittle.baharat");
		case ECigMixProblem::TooMuchBaharat:   return TEXT("mix.toomuch.baharat");
		case ECigMixProblem::TooLittleIsot:    return TEXT("mix.toolittle.isot");
		case ECigMixProblem::TooMuchIsot:      return TEXT("mix.toomuch.isot");
		default:                               return TEXT("");
		}
	}
}
