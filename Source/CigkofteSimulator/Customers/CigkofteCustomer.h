#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/CigkofteTypes.h"
// For ECigPathFailure. The customer stores why navigation gave up, and a
// forward declaration is not enough for a default-initialised member.
#include "Navigation/CigNavGrid.h"
// Ambient pedestrians carry the region they are allowed to walk in, by value.
#include "Navigation/CigPedRegion.h"
#include "CigkofteCustomer.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UCLASS()
class ACigkofteCustomer : public AActor
{
	GENERATED_BODY()

public:
	ACigkofteCustomer();

	virtual void Tick(float DeltaSeconds) override;

	// Builds height, colour and accessories from Seed (same seed = same look, for regulars).
	void InitVisuals(int32 Seed = -1);

	// Sets the overhead text and icons from the traits and the order.
	void ApplyOrderVisuals();

	// Sets them up as a street pedestrian who never orders.
	//
	// The rectangle overload is what a district gets: one lane, no obstacles, and
	// no claim that the district interior is modelled. The region overload is for
	// the main street, where the pavements are known and the road must not be
	// walked in. Seed makes the wander deterministic per pedestrian without
	// putting decorative movement into the shared gameplay stream.
	void InitAmbient(const FVector2D& WanderMin, const FVector2D& WanderMax, int32 Seed = 0);
	void InitAmbient(const FCigPedRegion& Region, int32 LaneIndex, int32 Seed);

	// Where this pedestrian may walk, for tests and for the world builder. An
	// empty region means it is not an ambient pedestrian.
	const FCigPedRegion& AmbientRegion() const { return WanderRegion; }
	int32 AmbientLane() const { return WanderLane; }
	void SetTarget(const FVector& InTarget);

	// Where the queue is currently sending this customer. Read-only, and only
	// worth having because queue fairness is a claim about where people are
	// standing: that slots go out in order, that nobody shares one, and that a
	// customer who left is no longer being pulled back into the line.
	const FVector& GetTargetForTest() const { return Target; }

	void Leave(bool bAngry, const FVector& ExitPos);
	void SetPatienceColor(float Frac01);

	// Puts the resolved pose on the body: a lean and a fidget whose amplitude is
	// the readout's urgency.
	//
	// A second channel for urgency, deliberately not a colour. The label tint
	// this sits beside is small text and one hue - unreadable from where the
	// player actually stands, and invisible to anyone who cannot separate red
	// from green. A silhouette that leans and shifts survives both.
	//
	// The decision of *which* pose lives in CigCustomerReadout, where it can be
	// tested without a world. This only draws it.
	void ApplyPose(float DeltaSeconds);

	// After serving, sends them to a table; on arrival they sit and start eating.
	void GoToSeat(const FVector& SeatPos, float SeatYaw);
	bool IsSeated() const { return bSeated; }

	// --- Pooling ---
	// A customer reaching the exit does not destroy itself; it hides and raises
	// this flag. UCigCustomerSystem sweeps them into the pool each frame, so a
	// busy day reuses the same actors instead of doing several SpawnActor and
	// Destroy calls per second.
	bool bAwaitingRecycle = false;

	// No route to the current target, and the customer has stopped rather than
	// walked through whatever was in the way.
	//
	// Stage 3.4 shipped a direct fallback here on the argument that a frozen
	// customer is worse than one that clips a table. Both are wrong: the fallback
	// meant the measured navigation could be bypassed by the exact layouts it
	// existed to catch. Stopping is only half an answer, so the customer system
	// sweeps this flag and gets them out of the shop through a real transition.
	bool bNavStranded = false;
	ECigPathFailure NavFailure = ECigPathFailure::None;
	// Set once the system has already tried to send a stranded customer home, so
	// a second failure recycles instead of looping.
	bool bStrandRecoveryAttempted = false;

	// The navigation this customer paths against. Handed over by whoever spawned
	// or reused them rather than looked up.
	//
	// The first version resolved it through GetWorld()->GetAuthGameMode(), which
	// is null in the test harness - FCigTestShop spawns a GameMode rather than
	// installing one - so every customer in every test silently had no navigation
	// and behaved exactly as if the shop were empty. An explicit dependency
	// cannot be quietly absent.
	void SetNavSystem(class UCigNavSystem* InNav);

	// Route state, for tests. The path itself stays private: nothing in gameplay
	// has any business reading a customer's waypoints, and exposing them would
	// invite a second thing to start steering from them.
	int32 PathPointCountForTest() const { return Path.Num(); }
	int32 PathRevisionForTest() const { return PathRevision; }

	// Called on entering the pool: hide, and switch off tick and collision.
	void Deactivate();
	// Called on leaving the pool: position, show, and reset gameplay state.
	void Reactivate(const FVector& SpawnPos);

	FString OrderString() const;

	// --- Order & personality ---
	FCigOrderSpec Spec;
	ECigTrait Traits = ECigTrait::None;
	int32 LoyalId = -1;
	FString LoyalName;
	int32 VisualSeed = 0;

	float Patience = 45.f;
	float MaxPatience = 45.f;
	bool bArrived = false;
	bool bArrivalNotified = false;
	bool bLeaving = false;
	bool bAmbient = false;
	bool bHappy = false;
	bool bVIP = false;
	bool bSeatMode = false;  // heading to a table / seated
	bool bSeated = false;    // reached the table and sat down

	// A real body, when the character pack is installed. The primitives below
	// stay as the fallback and are hidden when this takes over - the same rule
	// the cat and the stations follow, and it matters because Content/Characters
	// is not in the repository.
	UPROPERTY() TObjectPtr<class USkeletalMeshComponent> SkelBody;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftArm;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RightArm;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Hat;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Bag;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> GlassL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> GlassR;
	UPROPERTY() TObjectPtr<UTextRenderComponent> OrderText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> TraitText;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeadMID;

	// Pose state. The phase is per-customer and seeded from the visual seed, so a
	// queue of impatient people does not fidget in lockstep - which would read as
	// a machine rather than as a row of annoyed customers.
	float PosePhase = 0.f;
	// Eased rather than snapped, so a body settles into a pose instead of
	// jumping between bands on the frame patience crosses a threshold.
	float PoseUrgencyEased = 0.f;

	// Which working animation the apprentice should be playing, or none to fall
	// back to idle and walking. Set by the staff system from the job it has been
	// given; nothing else on this actor decides it.
	enum class EWorkAnim : uint8 { None, Work, Reach, Serve, Wrap };
	void SetWorkAnim(EWorkAnim InWork);

private:
	// Where the customer is going. Arrival is always measured against this, never
	// against a waypoint - otherwise a customer would sit down, or start queueing,
	// at the first corner it rounded.
	FVector Target = FVector::ZeroVector;

	// The route to it. Empty means walk straight there, which is correct for
	// street pedestrians: the pavement outside is not modelled, so a path across
	// it would be a guess dressed up as a measurement.
	//
	// Before this existed a customer interpolated straight at its target and went
	// through counters, tables and the shopfront wall on the way.
	TArray<FVector> Path;
	int32 PathCursor = 0;
	// The layout revision this route was built against. A placement that moves
	// invalidates every route built before it, and comparing one integer per tick
	// is what makes that cheap enough to check rather than hope about.
	int32 PathRevision = 0;

	// Recomputes Path for the current Target. Called when the target changes and
	// never per frame: the shop does not move between frames, and the nav system
	// rebuilds only when a placement actually changes.
	void RepathToTarget();

	FCigPedRegion WanderRegion;
	int32 WanderLane = INDEX_NONE;
	// Its own stream, seeded per pedestrian. Decorative wandering stays out of the
	// deterministic gameplay stream - that was a deliberate decision and this
	// keeps it - but a test still needs the same seed to produce the same walk.
	FRandomStream WanderStream;
	// Consecutive blocked steps. Ambient pedestrians do not repath, so this is
	// what stops one pressing into a wall forever: after a few, it gives up on
	// the target and picks another.
	int32 WanderBlockedSteps = 0;
	float WalkPhase = 0.f;
	float HopTime = 0.f;
	float IdleSeed = 0.f;
	float BodyBaseZ = 80.f;
	float HeightScale = 1.f;
	float SeatYaw = 0.f;
	float EatPhase = 0.f;

	void PickWanderTarget();
	void ApplyWalkAnim(bool bWalking, float DeltaSeconds);

	// The navigation system this customer paths against, or null outside a real
	// shop. Cached because Tick compares a layout revision every frame and
	// resolving the GameMode for that would be a lookup per customer per frame.
	class UCigNavSystem* ResolveNav() const;
	mutable TWeakObjectPtr<class UCigNavSystem> CachedNav;

	// Moves toward Steer without passing through world geometry, and reports
	// whether something blocked. The grid answers where a body may walk; this
	// answers what actually happened when it tried, which is not the same
	// question once a crate is standing somewhere the grid has not seen yet.
	bool StepTowards(const FVector& Steer, float DeltaSeconds, float Speed, FVector& OutLocation) const;

	// True once a skeletal body is standing in for the primitives.
	bool bSkeletal = false;
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimIdle;
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimWalk;
	// From MC_Sample, which is rigged to the same UE5 mannequin skeleton, so
	// these load and play with no retargeting. Null without the pack.
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimSit;
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimHappy;
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimAngry;
	// Queueing with patience running out. The last customer state that had no
	// clip: until now a customer about to walk out looked exactly like one who
	// had just arrived, and the patience bar over their head was the only thing
	// saying otherwise.
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimImpatient;

	// Working at a station. This class doubles as the apprentice NPC, which is
	// the only body in the shop the player ever sees do the job - so these are
	// the animations for the staff jobs rather than for anything a customer does.
	//
	// Found in MC_Sample, which was already installed and licensed for this
	// project, rather than downloaded. No pack contains kneading çiğköfte or
	// rolling a dürüm, and none ever will; what exists are motions of the right
	// shape. A drill held low is a pair of hands working something on a counter
	// in front of you, and at the distance the player watches from that is what
	// kneading and chopping look like.
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimWork;   // knead, chop
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimReach;  // take an ingredient
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimServe;  // hand it over
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimWrap;   // roll and pack

	EWorkAnim WorkAnim = EWorkAnim::None;
	// Which one is playing, so the same sequence is not restarted every frame.
	UPROPERTY() TObjectPtr<class UAnimSequence> AnimPlaying;

	// Swaps the primitives for a mannequin, or leaves them alone without the
	// pack. Seed picks the body so a regular looks the same on every visit.
	void TrySetupSkeletalBody(int32 Seed);
	void UpdateSkeletalAnim(bool bWalking);
};
