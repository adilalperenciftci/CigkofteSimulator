#pragma once

#include "CoreMinimal.h"

// Placement geometry is deliberately independent from actor collision.
//
// Physical collision answers where a pawn may stand, Visibility collision
// answers what an interaction trace can aim at, and this footprint answers
// whether the shop layout may reserve the floor. One component cannot express
// all three without making a delivery either intangible to placement or capable
// of walling the player out of the shop.
enum class ECigPlacementCategory : uint8
{
	Unknown = 0,
	FixedFixture,
	StockCrate,
	ShopObject
};

enum class ECigPlacementContext : uint8
{
	BuildMode = 0,
	Delivery,
	MoveExisting,
	WorldRegistration
};

enum class ECigPlacementRotationPolicy : uint8
{
	QuarterTurns = 0,
	FixedYaw
};

// Ordered deliberately. Validation uses this as the stable precedence when a
// candidate violates more than one protected zone.
enum class ECigPlacementFailure : uint8
{
	None = 0,
	InvalidStableId,
	UnknownCategory,
	InvalidFootprint,
	UnsupportedRotation,
	InvalidFloor,
	OutsideShopBounds,
	DuplicateStableId,
	MissingRecord,
	BlocksEntrance,
	BlocksQueue,
	BlocksServiceRoute,
	BlocksStationAccess,
	BlocksPlayerRoute,
	Overlap,
	NoDeliverySpotAvailable
};

struct FCigPlacementFootprint
{
	// Full dimensions in centimetres before rotation.
	FVector2D Size = FVector2D(100.f, 100.f);
	// Offset from the actor pivot, rotated with the candidate yaw.
	FVector2D CenterOffset = FVector2D::ZeroVector;
	float ClearanceMargin = 0.f;
	ECigPlacementRotationPolicy RotationPolicy = ECigPlacementRotationPolicy::QuarterTurns;
	float FixedYawDegrees = 0.f;

	bool IsValid() const;

	// Optional imported assets must never decide whether an object occupies the
	// floor. When a mesh is absent, or reports unusable dimensions, the explicit
	// repository-owned fallback remains authoritative.
	static FCigPlacementFootprint FromOptionalMeshSize(
		const TOptional<FVector2D>& OptionalMeshSize,
		const FVector2D& FallbackSize,
		float InClearanceMargin = 0.f,
		const FVector2D& InCenterOffset = FVector2D::ZeroVector);
};

struct FCigPlacementRequest
{
	FName StableId;
	ECigPlacementCategory Category = ECigPlacementCategory::Unknown;
	FTransform CandidateTransform = FTransform::Identity;
	FCigPlacementFootprint Footprint;
	ECigPlacementContext Context = ECigPlacementContext::BuildMode;

	// Moving an existing object ignores exactly its own old record. It is a
	// stable ID, never an actor pointer, and cannot name some other record.
	FName IgnoreStableId;
};

struct FCigPlacementResult
{
	bool bAccepted = false;
	FTransform NormalizedTransform = FTransform::Identity;
	ECigPlacementFailure Failure = ECigPlacementFailure::None;
	FName ConflictingStableId;
	FName ProtectedZoneId;
	FString Diagnostic;
};

struct FCigPlacementRecord
{
	FName StableId;
	ECigPlacementCategory Category = ECigPlacementCategory::Unknown;
	FTransform Transform = FTransform::Identity;
	FCigPlacementFootprint Footprint;
};

struct FCigPlacementBounds
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtent = FVector2D(100.f, 100.f);
	float FloorZ = 0.f;
	float FloorTolerance = 2.f;
};

struct FCigProtectedZone
{
	FName StableId;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtent = FVector2D(50.f, 50.f);
	ECigPlacementFailure Failure = ECigPlacementFailure::BlocksPlayerRoute;
};

struct FCigPlacementRect
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtent = FVector2D::ZeroVector;
};

// Pure deterministic authority. UCigPlacementSystem owns the production
// instance; tests use this value type directly, without a UWorld or rendering.
class FCigPlacementAuthority
{
public:
	void Configure(const FCigPlacementBounds& InBounds, float InPositionSnap = 10.f,
		float InRotationSnap = 90.f, float InRotationTolerance = 1.f);
	void ResetRecords();

	bool AddProtectedZone(const FCigProtectedZone& Zone);
	bool RemoveProtectedZone(FName StableId);

	FCigPlacementResult Validate(const FCigPlacementRequest& Request) const;
	FCigPlacementResult TryRegister(const FCigPlacementRequest& Request);
	bool Remove(FName StableId);
	const FCigPlacementRecord* Find(FName StableId) const;

	FCigPlacementResult FindFirstValid(const FCigPlacementRequest& BaseRequest,
		const TArray<FTransform>& OrderedCandidates) const;

	int32 RecordCount() const { return Records.Num(); }
	int32 ProtectedZoneCount() const { return ProtectedZones.Num(); }
	const TArray<FCigPlacementRecord>& GetRecords() const { return Records; }

	static FCigPlacementRect EffectiveRect(const FTransform& Transform, const FCigPlacementFootprint& Footprint);
	// Exact edge contact is allowed. A positive-area intersection is not.
	static bool RectsOverlap(const FCigPlacementRect& A, const FCigPlacementRect& B);

private:
	FCigPlacementBounds Bounds;
	float PositionSnap = 10.f;
	float RotationSnap = 90.f;
	float RotationTolerance = 1.f;
	TArray<FCigPlacementRecord> Records;
	TArray<FCigProtectedZone> ProtectedZones;

	FCigPlacementResult Normalize(const FCigPlacementRequest& Request) const;
	bool IsInsideBounds(const FCigPlacementRect& Rect) const;
};

// Authored shop geometry shared by placement validation and the systems whose
// paths it protects. These are layout facts, not balance knobs.
namespace CigPlacementLayout
{
	FCigPlacementBounds ShopBounds();
	FCigProtectedZone EntranceZone();
	FCigProtectedZone QueueZone();
	FCigProtectedZone ServiceRouteZone();
	FCigProtectedZone PlayerRouteZone();
	FVector QueueFront();
	float QueueSpacing();
	const TArray<FTransform>& DeliverySpots();
}
