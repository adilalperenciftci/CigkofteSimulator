#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/CigkofteTypes.h"
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
	void InitAmbient(const FVector2D& WanderMin, const FVector2D& WanderMax);
	void SetTarget(const FVector& InTarget);
	void Leave(bool bAngry, const FVector& ExitPos);
	void SetPatienceColor(float Frac01);

	// After serving, sends them to a table; on arrival they sit and start eating.
	void GoToSeat(const FVector& SeatPos, float SeatYaw);
	bool IsSeated() const { return bSeated; }

	// --- Pooling ---
	// A customer reaching the exit does not destroy itself; it hides and raises
	// this flag. UCigCustomerSystem sweeps them into the pool each frame, so a
	// busy day reuses the same actors instead of doing several SpawnActor and
	// Destroy calls per second.
	bool bAwaitingRecycle = false;

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

	// Which working animation the apprentice should be playing, or none to fall
	// back to idle and walking. Set by the staff system from the job it has been
	// given; nothing else on this actor decides it.
	enum class EWorkAnim : uint8 { None, Work, Reach, Serve, Wrap };
	void SetWorkAnim(EWorkAnim InWork);

private:
	FVector Target = FVector::ZeroVector;
	FVector2D WanderLo = FVector2D::ZeroVector;
	FVector2D WanderHi = FVector2D::ZeroVector;
	float WalkPhase = 0.f;
	float HopTime = 0.f;
	float IdleSeed = 0.f;
	float BodyBaseZ = 80.f;
	float HeightScale = 1.f;
	float SeatYaw = 0.f;
	float EatPhase = 0.f;

	void PickWanderTarget();
	void ApplyWalkAnim(bool bWalking, float DeltaSeconds);

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
