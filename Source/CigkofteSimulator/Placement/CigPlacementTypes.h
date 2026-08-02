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
	Station,
	Seating,
	Storage,
	Decoration
};

// Category describes function; lifetime describes how a placement enters and
// leaves the authority. Keeping these axes separate prevents a delivery crate
// from becoming a special-purpose furniture category.
enum class ECigPlacementLifetime : uint8
{
	Unknown = 0,
	Installed,
	Transient
};

enum class ECigPlacementContext : uint8
{
	Unknown = 0,
	BuildMode,
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
	UnknownLifetime,
	UnknownContext,
	InvalidIgnoreStableId,
	InvalidClassification,
	CategoryMismatch,
	LifetimeMismatch,
	ConsequenceMismatch,
	InvalidFootprint,
	InvalidConsequence,
	UnsupportedRotation,
	InvalidFloor,
	OutsideShopBounds,
	FunctionalAreaOutsideShop,
	DuplicateStableId,
	MissingRecord,
	BlocksEntrance,
	BlocksQueue,
	BlocksServiceRoute,
	BlocksStationAccess,
	BlocksFunctionalClearance,
	BlocksPlayerRoute,
	Overlap,
	NoDeliverySpotAvailable
};

enum class ECigPlacementMutation : uint8
{
	Added = 0,
	Moved,
	Removed
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

// Gameplay geometry authored independently from the occupied footprint. The
// offset and yaw define one local use-area frame: placement yaw and yaw offset
// rotate both the center offset and the rectangle axes. Decoration deliberately
// leaves this empty; functional categories must provide it.
struct FCigPlacementUseSpec
{
	FVector2D Size = FVector2D::ZeroVector;
	FVector2D CenterOffset = FVector2D::ZeroVector;
	float YawOffsetDegrees = 0.f;
	int32 FunctionalCapacity = 0;

	bool IsEmpty() const;
	bool IsValid() const;
	bool Equals(const FCigPlacementUseSpec& Other, float Tolerance = KINDA_SMALL_NUMBER) const;
};

struct FCigPlacementRequest
{
	FName StableId;
	ECigPlacementCategory Category = ECigPlacementCategory::Unknown;
	ECigPlacementLifetime Lifetime = ECigPlacementLifetime::Unknown;
	FTransform CandidateTransform = FTransform::Identity;
	FCigPlacementFootprint Footprint;
	FCigPlacementUseSpec UseSpec;
	ECigPlacementContext Context = ECigPlacementContext::Unknown;

	// Moving an existing object ignores exactly its own old record. It is a
	// stable ID, never an actor pointer, and cannot name some other record.
	FName IgnoreStableId;
};

struct FCigPlacementResult
{
	bool bAccepted = false;
	// Validate/FindFirstValid never change state. TryRegister sets this only when
	// a record was added or its normalized value actually changed.
	bool bStateChanged = false;
	FTransform NormalizedTransform = FTransform::Identity;
	ECigPlacementFailure Failure = ECigPlacementFailure::None;
	FName ConflictingStableId;
	FName ProtectedZoneId;
	FString Diagnostic;
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

// Derived by pure policy and stored inside the authoritative record. The
// embedded representation makes record/consequence lifetime atomic: no second
// container can retain stale state after a move or remove.
struct FCigPlacementConsequence
{
	FName StableId;
	ECigPlacementCategory Category = ECigPlacementCategory::Unknown;
	ECigPlacementLifetime Lifetime = ECigPlacementLifetime::Unknown;
	FCigPlacementRect PhysicalRect;
	FCigPlacementRect UseRect;
	int32 FunctionalCapacity = 0;
	bool bHasUseArea = false;
	bool bInstalledLayout = false;
};

struct FCigPlacementRecord
{
	FName StableId;
	ECigPlacementCategory Category = ECigPlacementCategory::Unknown;
	ECigPlacementLifetime Lifetime = ECigPlacementLifetime::Unknown;
	FTransform Transform = FTransform::Identity;
	FCigPlacementFootprint Footprint;
	FCigPlacementUseSpec UseSpec;
	FCigPlacementConsequence Consequence;
};

struct FCigPlacementChange
{
	FName StableId;
	ECigPlacementMutation Mutation = ECigPlacementMutation::Added;
	ECigPlacementCategory Category = ECigPlacementCategory::Unknown;
	ECigPlacementLifetime Lifetime = ECigPlacementLifetime::Unknown;
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
	// Consequences are copied out so callers cannot retain a pointer invalidated
	// by a later register/remove. The stable-ID index keeps this query O(1).
	bool TryGetConsequence(FName StableId, FCigPlacementConsequence& OutConsequence) const;

	FCigPlacementResult FindFirstValid(const FCigPlacementRequest& BaseRequest,
		const TArray<FTransform>& OrderedCandidates) const;

	int32 RecordCount() const { return Records.Num(); }
	int32 ConsequenceCount() const { return Records.Num(); }
	int32 CountByCategory(ECigPlacementCategory Category) const;
	int32 CountByLifetime(ECigPlacementLifetime Lifetime) const;
	int32 CountFunctionalCapacity(ECigPlacementCategory Category) const;
	int32 CountInstalledLayoutConsequences() const;
	int32 ProtectedZoneCount() const { return ProtectedZones.Num(); }
	const TArray<FCigPlacementRecord>& GetRecords() const { return Records; }

	static FCigPlacementRect EffectiveRect(const FTransform& Transform, const FCigPlacementFootprint& Footprint);
	static FCigPlacementRect EffectiveUseRect(const FTransform& Transform, const FCigPlacementUseSpec& UseSpec);
	// Exact edge contact is allowed. A positive-area intersection is not.
	static bool RectsOverlap(const FCigPlacementRect& A, const FCigPlacementRect& B);

private:
	FCigPlacementBounds Bounds;
	float PositionSnap = 10.f;
	float RotationSnap = 90.f;
	float RotationTolerance = 1.f;
	TArray<FCigPlacementRecord> Records;
	// Validation deliberately scans the small record set, but gameplay queries
	// (including per-frame focus and seat reservation) must not rediscover an ID
	// with a global scan. Mutations maintain this index synchronously.
	TMap<FName, int32> RecordIndices;
	TArray<FCigProtectedZone> ProtectedZones;

	FCigPlacementResult Normalize(const FCigPlacementRequest& Request) const;
	FCigPlacementResult EvaluateCandidate(const FCigPlacementRequest& Request,
		FCigPlacementRecord& OutRecord) const;
	void RebuildRecordIndices();
	bool IsInsideBounds(const FCigPlacementRect& Rect) const;
	static bool RecordsEqual(const FCigPlacementRecord& A, const FCigPlacementRecord& B);
};

// Category/lifetime semantics and geometry derivation without UWorld, actors or
// mutable state. Authority uses this same function that pure tests exercise.
class FCigPlacementConsequencePolicy
{
public:
	static ECigPlacementFailure Derive(const FCigPlacementRequest& Request,
		const FTransform& NormalizedTransform, FCigPlacementConsequence& OutConsequence);
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
