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

	bool IsKnownCategory(ECigPlacementCategory Category)
	{
		switch (Category)
		{
		case ECigPlacementCategory::Station:
		case ECigPlacementCategory::Seating:
		case ECigPlacementCategory::Storage:
		case ECigPlacementCategory::Decoration:
			return true;
		default:
			return false;
		}
	}

	bool IsKnownLifetime(ECigPlacementLifetime Lifetime)
	{
		return Lifetime == ECigPlacementLifetime::Installed
			|| Lifetime == ECigPlacementLifetime::Transient;
	}

	bool IsKnownContext(ECigPlacementContext Context)
	{
		switch (Context)
		{
		case ECigPlacementContext::BuildMode:
		case ECigPlacementContext::Delivery:
		case ECigPlacementContext::MoveExisting:
		case ECigPlacementContext::WorldRegistration:
			return true;
		default:
			return false;
		}
	}

	bool IsClassificationAllowed(const FCigPlacementRequest& Request)
	{
		if (Request.Lifetime == ECigPlacementLifetime::Transient)
		{
			return Request.Category == ECigPlacementCategory::Storage
				&& Request.Context == ECigPlacementContext::Delivery;
		}
		return Request.Lifetime == ECigPlacementLifetime::Installed
			&& Request.Context != ECigPlacementContext::Delivery;
	}

	FCigPlacementRect RotatedRect(const FTransform& Transform, const FVector2D& Size,
		const FVector2D& CenterOffset, float YawOffsetDegrees, float Margin)
	{
		const float Yaw = FMath::UnwindDegrees(Transform.Rotator().Yaw + YawOffsetDegrees);
		const float Radians = FMath::DegreesToRadians(Yaw);
		const float Cos = FMath::Cos(Radians);
		const float Sin = FMath::Sin(Radians);
		const FVector2D RotatedOffset(
			CenterOffset.X * Cos - CenterOffset.Y * Sin,
			CenterOffset.X * Sin + CenterOffset.Y * Cos);

		const FVector2D RawHalf = Size * 0.5f;
		FCigPlacementRect Result;
		Result.Center = FVector2D(Transform.GetLocation().X, Transform.GetLocation().Y) + RotatedOffset;
		Result.HalfExtent = FVector2D(
			FMath::Abs(Cos) * RawHalf.X + FMath::Abs(Sin) * RawHalf.Y,
			FMath::Abs(Sin) * RawHalf.X + FMath::Abs(Cos) * RawHalf.Y);
		Result.HalfExtent += FVector2D(Margin, Margin);
		return Result;
	}

	bool FootprintsEqual(const FCigPlacementFootprint& A, const FCigPlacementFootprint& B)
	{
		return A.Size.Equals(B.Size)
			&& A.CenterOffset.Equals(B.CenterOffset)
			&& FMath::IsNearlyEqual(A.ClearanceMargin, B.ClearanceMargin)
			&& A.RotationPolicy == B.RotationPolicy
			&& FMath::IsNearlyEqual(A.FixedYawDegrees, B.FixedYawDegrees);
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

bool FCigPlacementUseSpec::IsEmpty() const
{
	return Size.IsNearlyZero() && CenterOffset.IsNearlyZero()
		&& FMath::IsNearlyZero(YawOffsetDegrees) && FunctionalCapacity == 0;
}

bool FCigPlacementUseSpec::IsValid() const
{
	return IsFinite2D(Size) && IsFinite2D(CenterOffset)
		&& Size.X > 0.f && Size.Y > 0.f
		&& FMath::IsFinite(YawOffsetDegrees) && FunctionalCapacity > 0;
}

bool FCigPlacementUseSpec::Equals(const FCigPlacementUseSpec& Other, float Tolerance) const
{
	return Size.Equals(Other.Size, Tolerance)
		&& CenterOffset.Equals(Other.CenterOffset, Tolerance)
		&& FMath::IsNearlyEqual(YawOffsetDegrees, Other.YawOffsetDegrees, Tolerance)
		&& FunctionalCapacity == Other.FunctionalCapacity;
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
	RecordIndices.Reset();
	ProtectedZones.Reset();
}

void FCigPlacementAuthority::ResetRecords()
{
	Records.Reset();
	RecordIndices.Reset();
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
	if (Request.StableId.IsNone())
	{
		return Rejected(ECigPlacementFailure::InvalidStableId);
	}
	if (!IsKnownCategory(Request.Category))
	{
		return Rejected(ECigPlacementFailure::UnknownCategory);
	}
	if (!IsKnownLifetime(Request.Lifetime))
	{
		return Rejected(ECigPlacementFailure::UnknownLifetime);
	}
	if (!IsKnownContext(Request.Context))
	{
		return Rejected(ECigPlacementFailure::UnknownContext);
	}
	const bool bMove = Request.Context == ECigPlacementContext::MoveExisting;
	if ((bMove && Request.IgnoreStableId != Request.StableId)
		|| (!bMove && !Request.IgnoreStableId.IsNone()))
	{
		return Rejected(ECigPlacementFailure::InvalidIgnoreStableId);
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
	// Footprints are explicit centimetre data. Actor scale must not silently
	// resize authoritative placement or consequence geometry.
	Result.NormalizedTransform.SetScale3D(FVector::OneVector);
	return Result;
}

FCigPlacementRect FCigPlacementAuthority::EffectiveRect(const FTransform& Transform,
	const FCigPlacementFootprint& Footprint)
{
	return RotatedRect(Transform, Footprint.Size, Footprint.CenterOffset, 0.f,
		Footprint.ClearanceMargin);
}

FCigPlacementRect FCigPlacementAuthority::EffectiveUseRect(const FTransform& Transform,
	const FCigPlacementUseSpec& UseSpec)
{
	return RotatedRect(Transform, UseSpec.Size, UseSpec.CenterOffset,
		UseSpec.YawOffsetDegrees, 0.f);
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
	FCigPlacementRecord Candidate;
	return EvaluateCandidate(Request, Candidate);
}

FCigPlacementResult FCigPlacementAuthority::EvaluateCandidate(const FCigPlacementRequest& Request,
	FCigPlacementRecord& OutRecord) const
{
	FCigPlacementResult Result = Normalize(Request);
	if (Result.Failure != ECigPlacementFailure::None)
	{
		return Result;
	}

	const FCigPlacementRecord* ExistingSelf = Find(Request.StableId);
	if (Request.Context == ECigPlacementContext::MoveExisting)
	{
		if (!ExistingSelf)
		{
			Result.Failure = ECigPlacementFailure::MissingRecord;
			return Result;
		}
		if (ExistingSelf->Category != Request.Category)
		{
			Result.Failure = ECigPlacementFailure::CategoryMismatch;
			return Result;
		}
		if (ExistingSelf->Lifetime != Request.Lifetime)
		{
			Result.Failure = ECigPlacementFailure::LifetimeMismatch;
			return Result;
		}
		if (!ExistingSelf->UseSpec.Equals(Request.UseSpec))
		{
			Result.Failure = ECigPlacementFailure::ConsequenceMismatch;
			return Result;
		}
	}

	if (!IsClassificationAllowed(Request))
	{
		Result.Failure = ECigPlacementFailure::InvalidClassification;
		return Result;
	}

	FCigPlacementConsequence Consequence;
	const ECigPlacementFailure PolicyFailure = FCigPlacementConsequencePolicy::Derive(
		Request, Result.NormalizedTransform, Consequence);
	if (PolicyFailure != ECigPlacementFailure::None)
	{
		Result.Failure = PolicyFailure;
		return Result;
	}

	if (!IsInsideBounds(Consequence.PhysicalRect))
	{
		Result.Failure = ECigPlacementFailure::OutsideShopBounds;
		return Result;
	}
	if (Consequence.bHasUseArea && !IsInsideBounds(Consequence.UseRect))
	{
		Result.Failure = ECigPlacementFailure::FunctionalAreaOutsideShop;
		return Result;
	}

	if (ExistingSelf && Request.Context != ECigPlacementContext::MoveExisting)
	{
		Result.Failure = ECigPlacementFailure::DuplicateStableId;
		Result.ConflictingStableId = Request.StableId;
		return Result;
	}

	if (Request.Context != ECigPlacementContext::WorldRegistration)
	{
		const FCigProtectedZone* BestZone = nullptr;
		for (const FCigProtectedZone& Zone : ProtectedZones)
		{
			const FCigPlacementRect ZoneRect{ Zone.Center, Zone.HalfExtent };
			const bool bPhysicalBlocks = RectsOverlap(Consequence.PhysicalRect, ZoneRect);
			const bool bUseBlocks = Consequence.bHasUseArea && RectsOverlap(Consequence.UseRect, ZoneRect);
			if (!bPhysicalBlocks && !bUseBlocks)
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
		if (RectsOverlap(Consequence.PhysicalRect, Record.Consequence.PhysicalRect)
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

	const FCigPlacementRecord* BestFunctionalConflict = nullptr;
	ECigPlacementFailure BestFunctionalFailure = ECigPlacementFailure::None;
	for (const FCigPlacementRecord& Record : Records)
	{
		if (Record.StableId == Request.IgnoreStableId)
		{
			continue;
		}
		const bool bBlocksExistingUse = Record.Consequence.bHasUseArea
			&& RectsOverlap(Consequence.PhysicalRect, Record.Consequence.UseRect);
		const bool bUseBlockedByExisting = Consequence.bHasUseArea
			&& RectsOverlap(Consequence.UseRect, Record.Consequence.PhysicalRect);
		// Use areas are approach/work envelopes, not occupied solids. They may
		// share floor (for example two adjacent counters using one aisle); only a
		// physical footprint intruding into an authored use area is rejected.
		if (!bBlocksExistingUse && !bUseBlockedByExisting)
		{
			continue;
		}
		const ECigPlacementFailure Failure =
			(Request.Category == ECigPlacementCategory::Station
				|| Record.Category == ECigPlacementCategory::Station)
			? ECigPlacementFailure::BlocksStationAccess
			: ECigPlacementFailure::BlocksFunctionalClearance;
		if (!BestFunctionalConflict || (uint8)Failure < (uint8)BestFunctionalFailure
			|| (Failure == BestFunctionalFailure
				&& StableIdLess(Record.StableId, BestFunctionalConflict->StableId)))
		{
			BestFunctionalConflict = &Record;
			BestFunctionalFailure = Failure;
		}
	}
	if (BestFunctionalConflict)
	{
		Result.Failure = BestFunctionalFailure;
		Result.ConflictingStableId = BestFunctionalConflict->StableId;
		return Result;
	}

	OutRecord.StableId = Request.StableId;
	OutRecord.Category = Request.Category;
	OutRecord.Lifetime = Request.Lifetime;
	OutRecord.Transform = Result.NormalizedTransform;
	OutRecord.Footprint = Request.Footprint;
	OutRecord.UseSpec = Request.UseSpec;
	OutRecord.Consequence = Consequence;

	Result.bAccepted = true;
	return Result;
}

FCigPlacementResult FCigPlacementAuthority::TryRegister(const FCigPlacementRequest& Request)
{
	FCigPlacementRecord NewRecord;
	FCigPlacementResult Result = EvaluateCandidate(Request, NewRecord);
	if (!Result.bAccepted)
	{
		return Result;
	}

	if (const int32* ExistingIndex = RecordIndices.Find(Request.StableId))
	{
		if (Records.IsValidIndex(*ExistingIndex))
		{
			FCigPlacementRecord& Existing = Records[*ExistingIndex];
			if (!RecordsEqual(Existing, NewRecord))
			{
				Existing = NewRecord;
				Result.bStateChanged = true;
			}
			return Result;
		}
	}
	const int32 NewIndex = Records.Add(NewRecord);
	RecordIndices.Add(NewRecord.StableId, NewIndex);
	Result.bStateChanged = true;
	return Result;
}

bool FCigPlacementAuthority::Remove(FName StableId)
{
	const int32* ExistingIndex = RecordIndices.Find(StableId);
	if (!ExistingIndex || !Records.IsValidIndex(*ExistingIndex))
	{
		return false;
	}
	Records.RemoveAt(*ExistingIndex);
	RebuildRecordIndices();
	return true;
}

const FCigPlacementRecord* FCigPlacementAuthority::Find(FName StableId) const
{
	const int32* Index = RecordIndices.Find(StableId);
	return Index && Records.IsValidIndex(*Index) ? &Records[*Index] : nullptr;
}

bool FCigPlacementAuthority::TryGetConsequence(FName StableId,
	FCigPlacementConsequence& OutConsequence) const
{
	const FCigPlacementRecord* Record = Find(StableId);
	if (!Record)
	{
		return false;
	}
	OutConsequence = Record->Consequence;
	return true;
}

void FCigPlacementAuthority::RebuildRecordIndices()
{
	RecordIndices.Reset();
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		RecordIndices.Add(Records[Index].StableId, Index);
	}
}

bool FCigPlacementAuthority::RecordsEqual(const FCigPlacementRecord& A, const FCigPlacementRecord& B)
{
	return A.StableId == B.StableId
		&& A.Category == B.Category
		&& A.Lifetime == B.Lifetime
		&& A.Transform.Equals(B.Transform)
		&& FootprintsEqual(A.Footprint, B.Footprint)
		&& A.UseSpec.Equals(B.UseSpec);
}

int32 FCigPlacementAuthority::CountByCategory(ECigPlacementCategory Category) const
{
	int32 Count = 0;
	for (const FCigPlacementRecord& Record : Records)
	{
		Count += Record.Category == Category ? 1 : 0;
	}
	return Count;
}

int32 FCigPlacementAuthority::CountByLifetime(ECigPlacementLifetime Lifetime) const
{
	int32 Count = 0;
	for (const FCigPlacementRecord& Record : Records)
	{
		Count += Record.Lifetime == Lifetime ? 1 : 0;
	}
	return Count;
}

int32 FCigPlacementAuthority::CountFunctionalCapacity(ECigPlacementCategory Category) const
{
	int32 Count = 0;
	for (const FCigPlacementRecord& Record : Records)
	{
		if (Record.Category == Category)
		{
			Count += Record.Consequence.FunctionalCapacity;
		}
	}
	return Count;
}

int32 FCigPlacementAuthority::CountInstalledLayoutConsequences() const
{
	int32 Count = 0;
	for (const FCigPlacementRecord& Record : Records)
	{
		Count += Record.Consequence.bInstalledLayout ? 1 : 0;
	}
	return Count;
}

ECigPlacementFailure FCigPlacementConsequencePolicy::Derive(const FCigPlacementRequest& Request,
	const FTransform& NormalizedTransform, FCigPlacementConsequence& OutConsequence)
{
	if (!IsKnownCategory(Request.Category))
	{
		return ECigPlacementFailure::UnknownCategory;
	}
	if (!IsKnownLifetime(Request.Lifetime))
	{
		return ECigPlacementFailure::UnknownLifetime;
	}

	const bool bDecoration = Request.Category == ECigPlacementCategory::Decoration;
	if ((bDecoration && !Request.UseSpec.IsEmpty())
		|| (!bDecoration && !Request.UseSpec.IsValid()))
	{
		return ECigPlacementFailure::InvalidConsequence;
	}
	if ((Request.Category == ECigPlacementCategory::Station
			|| Request.Category == ECigPlacementCategory::Storage)
		&& Request.UseSpec.FunctionalCapacity != 1)
	{
		return ECigPlacementFailure::InvalidConsequence;
	}

	OutConsequence = FCigPlacementConsequence();
	OutConsequence.StableId = Request.StableId;
	OutConsequence.Category = Request.Category;
	OutConsequence.Lifetime = Request.Lifetime;
	OutConsequence.PhysicalRect = FCigPlacementAuthority::EffectiveRect(
		NormalizedTransform, Request.Footprint);
	OutConsequence.FunctionalCapacity = bDecoration ? 0 : Request.UseSpec.FunctionalCapacity;
	OutConsequence.bHasUseArea = !bDecoration;
	OutConsequence.bInstalledLayout = Request.Lifetime == ECigPlacementLifetime::Installed;
	if (OutConsequence.bHasUseArea)
	{
		OutConsequence.UseRect = FCigPlacementAuthority::EffectiveUseRect(
			NormalizedTransform, Request.UseSpec);
	}
	return ECigPlacementFailure::None;
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
