#include "Placement/CigBuildVerdict.h"

#include "Core/CigText.h"
#include "Placement/CigPlacementSystem.h"

namespace CigBuildVerdict
{
	FCigPlacementRequest MakeMoveRequest(const FCigPlacementRecord& Existing,
		const FTransform& CandidateTransform)
	{
		FCigPlacementRequest Request;
		Request.StableId = Existing.StableId;
		Request.Category = Existing.Category;
		Request.Lifetime = Existing.Lifetime;
		Request.Footprint = Existing.Footprint;
		Request.UseSpec = Existing.UseSpec;
		Request.CandidateTransform = CandidateTransform;
		Request.Context = ECigPlacementContext::MoveExisting;
		// Itself, and only itself. A move that did not say this would be refused
		// for overlapping the record of where it currently stands.
		Request.IgnoreStableId = Existing.StableId;
		return Request;
	}

	bool MakeCandidateRecord(const FCigPlacementRequest& Request,
		const FTransform& NormalizedTransform, FCigPlacementRecord& OutRecord)
	{
		FCigPlacementConsequence Consequence;
		const ECigPlacementFailure Failure =
			FCigPlacementConsequencePolicy::Derive(Request, NormalizedTransform, Consequence);
		if (Failure != ECigPlacementFailure::None)
		{
			return false;
		}

		OutRecord.StableId = Request.StableId;
		OutRecord.Category = Request.Category;
		OutRecord.Lifetime = Request.Lifetime;
		OutRecord.Transform = NormalizedTransform;
		OutRecord.Footprint = Request.Footprint;
		OutRecord.UseSpec = Request.UseSpec;
		OutRecord.Consequence = Consequence;
		return true;
	}

	FCigBuildVerdict Combine(const FCigPlacementResult& Validation,
		bool bClosesRoute, FName ClosedRouteId)
	{
		FCigBuildVerdict Verdict;
		Verdict.NormalizedTransform = Validation.NormalizedTransform;

		// Validation first, and its reason kept whole. A candidate can fail both
		// tests at once - a slab across the doorway overlaps the entrance zone and
		// closes the entrance route - and of the two answers only one tells the
		// player where to move their hands.
		if (!Validation.bAccepted)
		{
			Verdict.Status = ECigBuildVerdictStatus::Refused;
			Verdict.Failure = Validation.Failure;
			Verdict.ConflictingStableId = Validation.ConflictingStableId;
			return Verdict;
		}

		if (bClosesRoute)
		{
			Verdict.Status = ECigBuildVerdictStatus::ClosesRoute;
			Verdict.ClosedRouteId = ClosedRouteId;
			return Verdict;
		}

		Verdict.Status = ECigBuildVerdictStatus::Accepted;
		return Verdict;
	}

	FString Describe(const FCigBuildVerdict& Verdict)
	{
		switch (Verdict.Status)
		{
		case ECigBuildVerdictStatus::Accepted:
			return CigText::Get(TEXT("build.verdict.ok"));
		case ECigBuildVerdictStatus::ClosesRoute:
			// The route is named. "Somewhere would become unreachable" without
			// saying where is the kind of message that teaches a player to ignore
			// messages.
			return CigText::Format(TEXT("build.verdict.route"), *Verdict.ClosedRouteId.ToString());
		case ECigBuildVerdictStatus::Refused:
		default:
			// The authority already writes these, and it writes them for the same
			// failures the world registration path reports. Reusing them keeps one
			// wording for one rule.
			return UCigPlacementSystem::FailureText(Verdict.Failure);
		}
	}

	FLinearColor Tint(const FCigBuildVerdict& Verdict)
	{
		return Verdict.IsAccepted()
			? FLinearColor(0.35f, 1.f, 0.45f)
			: FLinearColor(1.f, 0.35f, 0.3f);
	}
}
