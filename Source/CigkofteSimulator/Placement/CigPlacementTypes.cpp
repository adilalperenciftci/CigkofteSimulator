#include "Placement/CigPlacementTypes.h"

namespace
{
	bool IsFinite2D(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	bool StableIdLess(FName A, FName B)
	{
		return A.ToString() < B.ToString();
	}

	bool IsProtectedFailure(ECigPlacementFailure Failure)
	{
		switch (Failure)
		{
		case ECigPlacementFailure::BlocksEntrance:
		case ECigPlacementFailure::BlocksQueue:
		case ECigPlacementFailure::BlocksServiceRoute:
		case ECigPlacementFailure::BlocksStationAccess:
		case ECigPlacementFailure::BlocksPlayerRoute:
			return true;
		default:
			return false;
		}
	}

	FCigPlacementResult Rejected(ECigPlacementFailure Failure, const FTransform& Transform = FTransform::Identity)
	{
		FCigPlacementResult Result;
		Result.Failure = Failure;
		Result.NormalizedTransform = Transform;
		return Result;
	}
}

bool FCigPlacementFootprint::IsValid() const
{
	return IsFinite2D(Size) && IsFinite2D(CenterOffset)
		&& Size.X > 0.f && Size.Y > 0.f
		&& FMath::IsFinite(ClearanceMargin) && ClearanceMargin >= 0.f
		&& FMath::IsFinite(FixedYawDegrees);
}

FCigPlacementFootprint FCigPlacementFootprint::FromOptionalMeshSize(
	const TOptional<FVector2D>& OptionalMeshSize,
	const FVector2D& FallbackSize,
	float InClearanceMargin,
	const FVector2D& InCenterOffset)
{
	FCigPlacementFootprint Result;
	const bool bMeshSizeUsable = OptionalMeshSize.IsSet()
		&& IsFinite2D(OptionalMeshSize.GetValue())
		&& OptionalMeshSize.GetValue().X > 0.f
		&& OptionalMeshSize.GetValue().Y > 0.f;
	Result.Size = bMeshSizeUsable ? OptionalMeshSize.GetValue() : FallbackSize;
	Result.CenterOffset = InCenterOffset;
	Result.ClearanceMargin = InClearanceMargin;
	return Result;
}

void FCigPlacementAuthority::Configure(const FCigPlacementBounds& InBounds, float InPositionSnap,
	float InRotationSnap, float InRotationTolerance)
{
	Bounds = InBounds;
	PositionSnap = FMath::Max(0.f, InPositionSnap);
	RotationSnap = FMath::Max(1.f, InRotationSnap);
	RotationTolerance = FMath::Max(0.f, InRotationTolerance);
	Records.Reset();
	ProtectedZones.Reset();
}

void FCigPlacementAuthority::ResetRecords()
{
	Records.Reset();
}

bool FCigPlacementAuthority::AddProtectedZone(const FCigProtectedZone& Zone)
{
	if (Zone.StableId.IsNone() || !IsFinite2D(Zone.Center) || !IsFinite2D(Zone.HalfExtent)
		|| Zone.HalfExtent.X <= 0.f || Zone.HalfExtent.Y <= 0.f || !IsProtectedFailure(Zone.Failure))
	{
		return false;
	}
	for (const FCigProtectedZone& Existing : ProtectedZones)
	{
		if (Existing.StableId == Zone.StableId)
		{
			return false;
		}
	}
	ProtectedZones.Add(Zone);
	return true;
}

bool FCigPlacementAuthority::RemoveProtectedZone(FName StableId)
{
	return ProtectedZones.RemoveAll([StableId](const FCigProtectedZone& Zone)
	{
		return Zone.StableId == StableId;
	}) == 1;
}

FCigPlacementResult FCigPlacementAuthority::Normalize(const FCigPlacementRequest& Request) const
{
	if (Request.StableId.IsNone() || (!Request.IgnoreStableId.IsNone() && Request.IgnoreStableId != Request.StableId))
	{
		return Rejected(ECigPlacementFailure::InvalidStableId);
	}
	if (Request.Category == ECigPlacementCategory::Unknown)
	{
		return Rejected(ECigPlacementFailure::UnknownCategory);
	}
	if (!Request.Footprint.IsValid())
	{
		return Rejected(ECigPlacementFailure::InvalidFootprint);
	}

	const FVector Location = Request.CandidateTransform.GetLocation();
	const FRotator Rotation = Request.CandidateTransform.Rotator();
	if (!FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) || !FMath::IsFinite(Location.Z)
		|| !FMath::IsFinite(Rotation.Pitch) || !FMath::IsFinite(Rotation.Yaw) || !FMath::IsFinite(Rotation.Roll))
	{
		return Rejected(ECigPlacementFailure::InvalidFootprint);
	}
	if (FMath::Abs(FMath::UnwindDegrees(Rotation.Pitch)) > RotationTolerance
		|| FMath::Abs(FMath::UnwindDegrees(Rotation.Roll)) > RotationTolerance)
	{
		return Rejected(ECigPlacementFailure::UnsupportedRotation);
	}

	float NormalizedYaw = 0.f;
	if (Request.Footprint.RotationPolicy == ECigPlacementRotationPolicy::FixedYaw)
	{
		NormalizedYaw = FMath::UnwindDegrees(Request.Footprint.FixedYawDegrees);
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(Rotation.Yaw, NormalizedYaw)) > RotationTolerance)
		{
			return Rejected(ECigPlacementFailure::UnsupportedRotation);
		}
	}
	else
	{
		NormalizedYaw = FMath::GridSnap(FMath::UnwindDegrees(Rotation.Yaw), RotationSnap);
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(Rotation.Yaw, NormalizedYaw)) > RotationTolerance)
		{
			return Rejected(ECigPlacementFailure::UnsupportedRotation);
		}
	}

	if (FMath::Abs(Location.Z - Bounds.FloorZ) > Bounds.FloorTolerance)
	{
		return Rejected(ECigPlacementFailure::InvalidFloor);
	}

	FVector Snapped = Location;
	if (PositionSnap > 0.f)
	{
		Snapped.X = FMath::GridSnap(Snapped.X, PositionSnap);
		Snapped.Y = FMath::GridSnap(Snapped.Y, PositionSnap);
	}
	Snapped.Z = Bounds.FloorZ;

	FCigPlacementResult Result;
	Result.NormalizedTransform = Request.CandidateTransform;
	Result.NormalizedTransform.SetLocation(Snapped);
	Result.NormalizedTransform.SetRotation(FRotator(0.f, NormalizedYaw, 0.f).Quaternion());
	return Result;
}

FCigPlacementRect FCigPlacementAuthority::EffectiveRect(const FTransform& Transform,
	const FCigPlacementFootprint& Footprint)
{
	const float Yaw = FMath::UnwindDegrees(Transform.Rotator().Yaw);
	const float Radians = FMath::DegreesToRadians(Yaw);
	const float Cos = FMath::Cos(Radians);
	const float Sin = FMath::Sin(Radians);
	const FVector2D RotatedOffset(
		Footprint.CenterOffset.X * Cos - Footprint.CenterOffset.Y * Sin,
		Footprint.CenterOffset.X * Sin + Footprint.CenterOffset.Y * Cos);

	const FVector2D RawHalf = Footprint.Size * 0.5f;
	FCigPlacementRect Result;
	Result.Center = FVector2D(Transform.GetLocation().X, Transform.GetLocation().Y) + RotatedOffset;
	// Axis-aligned extent of the rotated rectangle. Production accepts quarter
	// turns only, while this formula stays exact for those turns and useful in
	// isolated geometry tests.
	Result.HalfExtent = FVector2D(
		FMath::Abs(Cos) * RawHalf.X + FMath::Abs(Sin) * RawHalf.Y,
		FMath::Abs(Sin) * RawHalf.X + FMath::Abs(Cos) * RawHalf.Y);
	Result.HalfExtent += FVector2D(Footprint.ClearanceMargin, Footprint.ClearanceMargin);
	return Result;
}

bool FCigPlacementAuthority::RectsOverlap(const FCigPlacementRect& A, const FCigPlacementRect& B)
{
	const FVector2D Delta = (A.Center - B.Center).GetAbs();
	return Delta.X < A.HalfExtent.X + B.HalfExtent.X - KINDA_SMALL_NUMBER
		&& Delta.Y < A.HalfExtent.Y + B.HalfExtent.Y - KINDA_SMALL_NUMBER;
}

bool FCigPlacementAuthority::IsInsideBounds(const FCigPlacementRect& Rect) const
{
	const FVector2D Delta = (Rect.Center - Bounds.Center).GetAbs();
	return Delta.X + Rect.HalfExtent.X <= Bounds.HalfExtent.X + KINDA_SMALL_NUMBER
		&& Delta.Y + Rect.HalfExtent.Y <= Bounds.HalfExtent.Y + KINDA_SMALL_NUMBER;
}

FCigPlacementResult FCigPlacementAuthority::Validate(const FCigPlacementRequest& Request) const
{
	FCigPlacementResult Result = Normalize(Request);
	if (Result.Failure != ECigPlacementFailure::None)
	{
		return Result;
	}

	const FCigPlacementRect Candidate = EffectiveRect(Result.NormalizedTransform, Request.Footprint);
	if (!IsInsideBounds(Candidate))
	{
		Result.Failure = ECigPlacementFailure::OutsideShopBounds;
		return Result;
	}

	const FCigPlacementRecord* ExistingSelf = Find(Request.StableId);
	if (ExistingSelf && Request.IgnoreStableId != Request.StableId)
	{
		Result.Failure = ECigPlacementFailure::DuplicateStableId;
		Result.ConflictingStableId = Request.StableId;
		return Result;
	}
	if (Request.Context == ECigPlacementContext::MoveExisting && !ExistingSelf)
	{
		Result.Failure = ECigPlacementFailure::MissingRecord;
		return Result;
	}

	if (Request.Context != ECigPlacementContext::WorldRegistration)
	{
		const FCigProtectedZone* BestZone = nullptr;
		for (const FCigProtectedZone& Zone : ProtectedZones)
		{
			const FCigPlacementRect ZoneRect{ Zone.Center, Zone.HalfExtent };
			if (!RectsOverlap(Candidate, ZoneRect))
			{
				continue;
			}
			if (!BestZone || (uint8)Zone.Failure < (uint8)BestZone->Failure
				|| (Zone.Failure == BestZone->Failure && StableIdLess(Zone.StableId, BestZone->StableId)))
			{
				BestZone = &Zone;
			}
		}
		if (BestZone)
		{
			Result.Failure = BestZone->Failure;
			Result.ProtectedZoneId = BestZone->StableId;
			return Result;
		}
	}

	const FCigPlacementRecord* BestConflict = nullptr;
	for (const FCigPlacementRecord& Record : Records)
	{
		if (Record.StableId == Request.IgnoreStableId)
		{
			continue;
		}
		if (RectsOverlap(Candidate, EffectiveRect(Record.Transform, Record.Footprint))
			&& (!BestConflict || StableIdLess(Record.StableId, BestConflict->StableId)))
		{
			BestConflict = &Record;
		}
	}
	if (BestConflict)
	{
		Result.Failure = ECigPlacementFailure::Overlap;
		Result.ConflictingStableId = BestConflict->StableId;
		return Result;
	}

	Result.bAccepted = true;
	return Result;
}

FCigPlacementResult FCigPlacementAuthority::TryRegister(const FCigPlacementRequest& Request)
{
	FCigPlacementResult Result = Validate(Request);
	if (!Result.bAccepted)
	{
		return Result;
	}

	FCigPlacementRecord NewRecord;
	NewRecord.StableId = Request.StableId;
	NewRecord.Category = Request.Category;
	NewRecord.Transform = Result.NormalizedTransform;
	NewRecord.Footprint = Request.Footprint;

	for (FCigPlacementRecord& Existing : Records)
	{
		if (Existing.StableId == Request.StableId)
		{
			Existing = NewRecord;
			return Result;
		}
	}
	Records.Add(NewRecord);
	return Result;
}

bool FCigPlacementAuthority::Remove(FName StableId)
{
	return Records.RemoveAll([StableId](const FCigPlacementRecord& Record)
	{
		return Record.StableId == StableId;
	}) == 1;
}

const FCigPlacementRecord* FCigPlacementAuthority::Find(FName StableId) const
{
	for (const FCigPlacementRecord& Record : Records)
	{
		if (Record.StableId == StableId)
		{
			return &Record;
		}
	}
	return nullptr;
}

FCigPlacementResult FCigPlacementAuthority::FindFirstValid(const FCigPlacementRequest& BaseRequest,
	const TArray<FTransform>& OrderedCandidates) const
{
	for (const FTransform& Candidate : OrderedCandidates)
	{
		FCigPlacementRequest Request = BaseRequest;
		Request.CandidateTransform = Candidate;
		FCigPlacementResult Result = Validate(Request);
		if (Result.bAccepted)
		{
			return Result;
		}
	}
	FCigPlacementResult Result = Rejected(ECigPlacementFailure::NoDeliverySpotAvailable);
	return Result;
}

namespace CigPlacementLayout
{
	FCigPlacementBounds ShopBounds()
	{
		FCigPlacementBounds Result;
		// Matches the code-built floor: centre (150,0), 1750 x 2600.
		// Forty centimetres are kept inside the back wall; the open front edge is
		// available to the delivery row.
		Result.Center = FVector2D(150.f, 0.f);
		Result.HalfExtent = FVector2D(850.f, 1300.f);
		Result.FloorZ = 0.f;
		Result.FloorTolerance = 2.f;
		return Result;
	}

	FCigProtectedZone EntranceZone()
	{
		return { TEXT("zone.entrance"), FVector2D(-650.f, 0.f), FVector2D(100.f, 160.f),
			ECigPlacementFailure::BlocksEntrance };
	}

	FCigProtectedZone QueueZone()
	{
		// The queue continues out along -X. The interior portion is what a shop
		// object could obstruct; CustomerSystem reads the same authored axis.
		return { TEXT("zone.queue"), FVector2D(-800.f, 0.f), FVector2D(300.f, 100.f),
			ECigPlacementFailure::BlocksQueue };
	}

	FCigProtectedZone ServiceRouteZone()
	{
		return { TEXT("zone.service-route"), FVector2D(-430.f, 0.f), FVector2D(170.f, 90.f),
			ECigPlacementFailure::BlocksServiceRoute };
	}

	FCigProtectedZone PlayerRouteZone()
	{
		return { TEXT("zone.player-route"), FVector2D(100.f, 0.f), FVector2D(250.f, 80.f),
			ECigPlacementFailure::BlocksPlayerRoute };
	}

	FVector QueueFront()
	{
		return FVector(-850.f, 0.f, 0.f);
	}

	float QueueSpacing()
	{
		return 160.f;
	}

	const TArray<FTransform>& DeliverySpots()
	{
		// Declared order is the tie-breaker. There is no random fallback and no
		// modulo wrap: once all six are occupied, the delivery remains pending.
		static const TArray<FTransform> Spots = {
			FTransform(FRotator::ZeroRotator, FVector(-620.f, -330.f, 0.f)),
			FTransform(FRotator::ZeroRotator, FVector(-620.f,  330.f, 0.f)),
			FTransform(FRotator::ZeroRotator, FVector(-650.f, -480.f, 0.f)),
			FTransform(FRotator::ZeroRotator, FVector(-650.f,  480.f, 0.f)),
			FTransform(FRotator::ZeroRotator, FVector(-650.f, -650.f, 0.f)),
			FTransform(FRotator::ZeroRotator, FVector(-650.f,  650.f, 0.f))
		};
		return Spots;
	}
}
