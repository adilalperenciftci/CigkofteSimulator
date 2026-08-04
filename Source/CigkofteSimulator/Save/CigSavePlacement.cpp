#include "Save/CigSavePlacement.h"

namespace
{
	bool IsFiniteSaved2D(const FVector2D& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
	}

	bool IsKnownCategory(uint8 Value)
	{
		return Value > (uint8)ECigPlacementCategory::Unknown
			&& Value <= (uint8)ECigPlacementCategory::Decoration;
	}
}

FCigSavePlacement CigSavePlacement::FromRecord(const FCigPlacementRecord& Record)
{
	FCigSavePlacement Saved;
	Saved.StableId = Record.StableId;
	Saved.Category = (uint8)Record.Category;
	Saved.Lifetime = (uint8)Record.Lifetime;
	Saved.Transform = Record.Transform;

	Saved.FootprintSize = Record.Footprint.Size;
	Saved.FootprintOffset = Record.Footprint.CenterOffset;
	Saved.ClearanceMargin = Record.Footprint.ClearanceMargin;
	Saved.RotationPolicy = (uint8)Record.Footprint.RotationPolicy;
	Saved.FixedYawDegrees = Record.Footprint.FixedYawDegrees;

	Saved.UseSize = Record.UseSpec.Size;
	Saved.UseOffset = Record.UseSpec.CenterOffset;
	Saved.UseYawDegrees = Record.UseSpec.YawOffsetDegrees;
	Saved.FunctionalCapacity = Record.UseSpec.FunctionalCapacity;

	// Consequence is deliberately not copied. See the header.
	return Saved;
}

TArray<FCigSavePlacement> CigSavePlacement::Capture(const TArray<FCigPlacementRecord>& Records)
{
	TArray<FCigSavePlacement> Saved;
	Saved.Reserve(Records.Num());
	for (const FCigPlacementRecord& Record : Records)
	{
		// Transient placements belong to the system that created them. A delivery
		// crate written here would be restored twice on load - once by the layout
		// and once by the delivery state that already persists.
		if (Record.Lifetime != ECigPlacementLifetime::Installed)
		{
			continue;
		}
		Saved.Add(FromRecord(Record));
	}

	// Lexical rather than by FName index: FName indices depend on the order names
	// were first seen in the process, so sorting by them would put the same shop
	// in a different order on a different run.
	Saved.Sort([](const FCigSavePlacement& A, const FCigSavePlacement& B)
	{
		return A.StableId.ToString() < B.StableId.ToString();
	});
	return Saved;
}

FCigPlacementRequest CigSavePlacement::ToRequest(const FCigSavePlacement& Saved)
{
	FCigPlacementRequest Request;
	Request.StableId = Saved.StableId;
	Request.Category = (ECigPlacementCategory)Saved.Category;
	Request.Lifetime = (ECigPlacementLifetime)Saved.Lifetime;
	Request.CandidateTransform = Saved.Transform;

	Request.Footprint.Size = Saved.FootprintSize;
	Request.Footprint.CenterOffset = Saved.FootprintOffset;
	Request.Footprint.ClearanceMargin = Saved.ClearanceMargin;
	Request.Footprint.RotationPolicy = (ECigPlacementRotationPolicy)Saved.RotationPolicy;
	Request.Footprint.FixedYawDegrees = Saved.FixedYawDegrees;

	Request.UseSpec.Size = Saved.UseSize;
	Request.UseSpec.CenterOffset = Saved.UseOffset;
	Request.UseSpec.YawOffsetDegrees = Saved.UseYawDegrees;
	Request.UseSpec.FunctionalCapacity = Saved.FunctionalCapacity;

	Request.Context = ECigPlacementContext::WorldRegistration;
	return Request;
}

ECigSavePlacementFault CigSavePlacement::Validate(const FCigSavePlacement& Saved)
{
	if (Saved.StableId.IsNone())
	{
		return ECigSavePlacementFault::MissingStableId;
	}
	if (!IsKnownCategory(Saved.Category))
	{
		return ECigSavePlacementFault::UnknownCategory;
	}
	if (Saved.Lifetime != (uint8)ECigPlacementLifetime::Installed)
	{
		return ECigSavePlacementFault::NotInstalled;
	}

	const FVector Location = Saved.Transform.GetLocation();
	const FRotator Rotation = Saved.Transform.Rotator();
	const FVector Scale = Saved.Transform.GetScale3D();
	if (Location.ContainsNaN() || !FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y)
		|| !FMath::IsFinite(Location.Z) || Rotation.ContainsNaN() || Scale.ContainsNaN())
	{
		return ECigSavePlacementFault::NonFiniteTransform;
	}
	// The footprint is authored in centimetres and the authority never scales it.
	// A scaled transform in the file would silently describe a different-sized
	// object than the footprint that gets validated against the floor.
	if (!Scale.Equals(FVector::OneVector, 0.001f))
	{
		return ECigSavePlacementFault::NonUniformScale;
	}

	FCigPlacementFootprint Footprint;
	Footprint.Size = Saved.FootprintSize;
	Footprint.CenterOffset = Saved.FootprintOffset;
	Footprint.ClearanceMargin = Saved.ClearanceMargin;
	Footprint.FixedYawDegrees = Saved.FixedYawDegrees;
	if (!IsFiniteSaved2D(Saved.FootprintSize) || !Footprint.IsValid())
	{
		return ECigSavePlacementFault::InvalidFootprint;
	}

	FCigPlacementUseSpec UseSpec;
	UseSpec.Size = Saved.UseSize;
	UseSpec.CenterOffset = Saved.UseOffset;
	UseSpec.YawOffsetDegrees = Saved.UseYawDegrees;
	UseSpec.FunctionalCapacity = Saved.FunctionalCapacity;
	// Empty is legal - decoration has no use area - but a half-filled one is not:
	// it would register a station whose approach side is nowhere.
	if (!UseSpec.IsEmpty() && !UseSpec.IsValid())
	{
		return ECigSavePlacementFault::InvalidUseSpec;
	}

	return ECigSavePlacementFault::None;
}

const TCHAR* CigSavePlacement::FaultText(ECigSavePlacementFault Fault)
{
	switch (Fault)
	{
	case ECigSavePlacementFault::None:               return TEXT("sorun yok");
	case ECigSavePlacementFault::MissingStableId:    return TEXT("kararli kimlik yok");
	case ECigSavePlacementFault::UnknownCategory:    return TEXT("bilinmeyen kategori");
	case ECigSavePlacementFault::NotInstalled:       return TEXT("kurulu olmayan yerlesim");
	case ECigSavePlacementFault::NonFiniteTransform: return TEXT("sonlu olmayan donusum");
	case ECigSavePlacementFault::NonUniformScale:    return TEXT("olcekli donusum");
	case ECigSavePlacementFault::InvalidFootprint:   return TEXT("gecersiz ayak izi");
	case ECigSavePlacementFault::InvalidUseSpec:     return TEXT("gecersiz kullanim alani");
	}
	return TEXT("bilinmeyen");
}
