#include "Customers/CigkofteCustomer.h"
#include "Core/CigText.h"
#include "Orders/CigOrderSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"

ACigkofteCustomer::ACigkofteCustomer()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	auto MakePart = [this](const TCHAR* Name, bool bQuery = false)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(bQuery ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		return C;
	};

	SkelBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkelBody"));
	SkelBody->SetupAttachment(RootComponent);
	SkelBody->SetMobility(EComponentMobility::Movable);
	SkelBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkelBody->SetVisibility(false);

	Body = MakePart(TEXT("Body"), true);
	Body->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	Body->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.6f));

	Head = MakePart(TEXT("Head"));
	Head->SetRelativeLocation(FVector(0.f, 0.f, 182.f));
	Head->SetRelativeScale3D(FVector(0.4f));

	LeftArm = MakePart(TEXT("LeftArm"));
	LeftArm->SetRelativeLocation(FVector(0.f, -42.f, 115.f));
	LeftArm->SetRelativeScale3D(FVector(0.14f, 0.14f, 0.65f));

	RightArm = MakePart(TEXT("RightArm"));
	RightArm->SetRelativeLocation(FVector(0.f, 42.f, 115.f));
	RightArm->SetRelativeScale3D(FVector(0.14f, 0.14f, 0.65f));

	Hat = MakePart(TEXT("Hat"));
	Hat->SetRelativeLocation(FVector(0.f, 0.f, 205.f));
	Hat->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.12f));
	Hat->SetVisibility(false);

	Bag = MakePart(TEXT("Bag"));
	Bag->SetRelativeLocation(FVector(0.f, -55.f, 90.f));
	Bag->SetRelativeScale3D(FVector(0.22f, 0.12f, 0.3f));
	Bag->SetVisibility(false);

	GlassL = MakePart(TEXT("GlassL"));
	GlassL->SetRelativeLocation(FVector(17.f, -8.f, 184.f));
	GlassL->SetRelativeScale3D(FVector(0.07f));
	GlassL->SetVisibility(false);

	GlassR = MakePart(TEXT("GlassR"));
	GlassR->SetRelativeLocation(FVector(17.f, 8.f, 184.f));
	GlassR->SetRelativeScale3D(FVector(0.07f));
	GlassR->SetVisibility(false);

	OrderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("OrderText"));
	OrderText->SetupAttachment(Root);
	OrderText->SetMobility(EComponentMobility::Movable);
	OrderText->SetRelativeLocation(FVector(0.f, 0.f, 245.f));
	OrderText->SetHorizontalAlignment(EHTA_Center);
	OrderText->SetWorldSize(20.f);
	OrderText->SetTextRenderColor(FColor::Green);

	TraitText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TraitText"));
	TraitText->SetupAttachment(Root);
	TraitText->SetMobility(EComponentMobility::Movable);
	TraitText->SetRelativeLocation(FVector(0.f, 0.f, 272.f));
	TraitText->SetHorizontalAlignment(EHTA_Center);
	TraitText->SetWorldSize(15.f);
	TraitText->SetTextRenderColor(FColor(200, 200, 255));

	// Phase offset for the sway animation - it touches no game state, so it is
	// deliberately left on the global RNG (see Core/CigRandomSubsystem.h).
	IdleSeed = FMath::FRandRange(0.f, 100.f);
}

void ACigkofteCustomer::InitVisuals(int32 Seed)
{
	// Appearance derives entirely from Seed, which the caller takes from the
	// deterministic stream. A negative Seed is only for callers that do not care.
	VisualSeed = Seed >= 0 ? Seed : FMath::Rand();
	FRandomStream Rand(VisualSeed);

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Cube || !Cylinder || !Sphere)
	{
		return;
	}

	Body->SetStaticMesh(Cylinder);
	Head->SetStaticMesh(Sphere);
	LeftArm->SetStaticMesh(Cylinder);
	RightArm->SetStaticMesh(Cylinder);
	Hat->SetStaticMesh(Cone ? Cone : Cylinder);
	Bag->SetStaticMesh(Cube);
	GlassL->SetStaticMesh(Sphere);
	GlassR->SetStaticMesh(Sphere);

	// Variation in height and proportions
	HeightScale = Rand.FRandRange(0.85f, 1.15f);
	const float Width = Rand.FRandRange(0.45f, 0.7f);
	Body->SetRelativeScale3D(FVector(Width, Width, 1.6f * HeightScale));
	BodyBaseZ = 80.f * HeightScale;
	Head->SetRelativeLocation(FVector(0.f, 0.f, 182.f * HeightScale));
	Hat->SetRelativeLocation(FVector(0.f, 0.f, 205.f * HeightScale));
	GlassL->SetRelativeLocation(FVector(17.f, -8.f, 184.f * HeightScale));
	GlassR->SetRelativeLocation(FVector(17.f, 8.f, 184.f * HeightScale));
	OrderText->SetRelativeLocation(FVector(0.f, 0.f, 245.f * HeightScale));
	TraitText->SetRelativeLocation(FVector(0.f, 0.f, 272.f * HeightScale));

	static const FLinearColor Shirts[] = {
		FLinearColor(0.15f, 0.30f, 0.55f),
		FLinearColor(0.55f, 0.15f, 0.15f),
		FLinearColor(0.15f, 0.45f, 0.25f),
		FLinearColor(0.50f, 0.35f, 0.10f),
		FLinearColor(0.35f, 0.20f, 0.45f),
		FLinearColor(0.25f, 0.25f, 0.28f)
	};
	static const FLinearColor Skins[] = {
		FLinearColor(0.85f, 0.62f, 0.45f),
		FLinearColor(0.75f, 0.55f, 0.40f),
		FLinearColor(0.92f, 0.72f, 0.55f),
		FLinearColor(0.55f, 0.38f, 0.28f)
	};

	if (Mat)
	{
		BodyMID = UMaterialInstanceDynamic::Create(Mat, this);
		HeadMID = UMaterialInstanceDynamic::Create(Mat, this);
		BodyMID->SetVectorParameterValue(TEXT("Color"), Shirts[Rand.RandRange(0, UE_ARRAY_COUNT(Shirts) - 1)]);
		HeadMID->SetVectorParameterValue(TEXT("Color"), Skins[Rand.RandRange(0, UE_ARRAY_COUNT(Skins) - 1)]);
		Body->SetMaterial(0, BodyMID);
		Head->SetMaterial(0, HeadMID);
		LeftArm->SetMaterial(0, BodyMID);
		RightArm->SetMaterial(0, BodyMID);
		Hat->SetMaterial(0, BodyMID);
		Bag->SetMaterial(0, BodyMID);

		UMaterialInstanceDynamic* GlassMID = UMaterialInstanceDynamic::Create(Mat, this);
		GlassMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.05f, 0.08f));
		GlassL->SetMaterial(0, GlassMID);
		GlassR->SetMaterial(0, GlassMID);
	}

	// Accessory variation
	Hat->SetVisibility(Rand.FRand() < 0.30f);
	Bag->SetVisibility(Rand.FRand() < 0.25f);
	const bool bGlasses = Rand.FRand() < 0.25f;
	GlassL->SetVisibility(bGlasses);
	GlassR->SetVisibility(bGlasses);

	// Last, because it reads HeightScale and hides everything set above when it
	// succeeds. Without the character pack it does nothing and the primitive
	// customer - hat, bag, glasses and all - is what walks in.
	TrySetupSkeletalBody(VisualSeed);
}

void ACigkofteCustomer::ApplyOrderVisuals()
{
	FString Line = CigSpiceNameAscii(Spec.Spice);
	Line += Spec.Portion >= 2 ? *CigText::Get(TEXT("customer.tag.portion")) : *CigText::Get(TEXT("customer.tag.wrap"));
	if (Spec.bWantsAyran)
	{
		Line += *CigText::Get(TEXT("customer.tag.ayran"));
	}
	if (Spec.bPacked)
	{
		Line += *CigText::Get(TEXT("customer.tag.packed"));
	}
	if (bVIP)
	{
		Line = CigText::Get(TEXT("customer.tag.vip")) + Line;
	}
	OrderText->SetText(FText::FromString(Line));
	OrderText->SetWorldSize(bVIP ? 26.f : 20.f);

	// Trait label: show at most 2 traits
	TArray<FString> TraitNames;
	for (int32 i = 0; i < CigTraitCount && TraitNames.Num() < 2; ++i)
	{
		const ECigTrait T = (ECigTrait)(1 << i);
		if (EnumHasAnyFlags(Traits, T) && T != ECigTrait::SecretCritic) // the undercover critic does not give themselves away
		{
			TraitNames.Add(CigTraitName(T));
		}
	}
	if (!LoyalName.IsEmpty())
	{
		TraitNames.Insert(LoyalName, 0);
	}
	TraitText->SetText(FText::FromString(FString::Join(TraitNames, TEXT(" | "))));
	TraitText->SetVisibility(TraitNames.Num() > 0);

	// Some traits show in the appearance
	if (EnumHasAnyFlags(Traits, ECigTrait::Tourist))
	{
		Hat->SetVisibility(true);
	}
	if (EnumHasAnyFlags(Traits, ECigTrait::Student))
	{
		Bag->SetVisibility(true);
	}
	if (EnumHasAnyFlags(Traits, ECigTrait::QualityFocused | ECigTrait::SecretCritic))
	{
		GlassL->SetVisibility(true);
		GlassR->SetVisibility(true);
	}
	if (bVIP && BodyMID)
	{
		BodyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.75f, 0.1f));
	}
	else if (EnumHasAnyFlags(Traits, ECigTrait::Influencer) && BodyMID)
	{
		BodyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.3f, 0.75f));
	}
}

void ACigkofteCustomer::InitAmbient(const FVector2D& WanderMin, const FVector2D& WanderMax)
{
	bAmbient = true;
	WanderLo = WanderMin;
	WanderHi = WanderMax;
	// Pedestrians have no order. TextRender defaults to "Text", so the empty
	// check did not catch it and the word "Text" showed up along the street.
	OrderText->SetText(FText::GetEmpty());
	OrderText->SetVisibility(false);
	TraitText->SetText(FText::GetEmpty());
	TraitText->SetVisibility(false);
	PickWanderTarget();
}

void ACigkofteCustomer::PickWanderTarget()
{
	// Pedestrian wandering is decorative and stays out of the deterministic stream.
	Target = FVector(FMath::FRandRange(WanderLo.X, WanderHi.X), FMath::FRandRange(WanderLo.Y, WanderHi.Y), 0.f);
}

void ACigkofteCustomer::SetTarget(const FVector& InTarget)
{
	Target = InTarget;
}

void ACigkofteCustomer::Leave(bool bAngry, const FVector& ExitPos)
{
	bLeaving = true;
	bArrived = false;
	bSeatMode = false;
	bSeated = false;
	bHappy = !bAngry;
	HopTime = 0.f;
	Target = ExitPos;
	OrderText->SetText(FText::FromString(bAngry ? CigText::Get(TEXT("customer.leave.angry")) : CigText::Get(TEXT("customer.leave.happy"))));
	OrderText->SetTextRenderColor(bAngry ? FColor::Red : FColor::Cyan);
	OrderText->SetVisibility(true);
	TraitText->SetVisibility(false);
}

void ACigkofteCustomer::Deactivate()
{
	bAwaitingRecycle = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void ACigkofteCustomer::Reactivate(const FVector& SpawnPos)
{
	// All gameplay state has to be reset: a customer taken from the pool must not
	// carry the previous one's order, patience or regular identity.
	Spec = FCigOrderSpec();
	Traits = ECigTrait::None;
	LoyalId = -1;
	LoyalName.Reset();
	Patience = MaxPatience = 45.f;
	bArrived = false;
	bArrivalNotified = false;
	bLeaving = false;
	bAmbient = false;
	bHappy = false;
	bVIP = false;
	bSeatMode = false;
	bSeated = false;
	HopTime = 0.f;
	EatPhase = 0.f;
	bAwaitingRecycle = false;

	SetActorLocation(SpawnPos);
	SetActorRotation(FRotator::ZeroRotator);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);

	if (OrderText) { OrderText->SetVisibility(false); }
	if (TraitText) { TraitText->SetVisibility(false); }
}

void ACigkofteCustomer::GoToSeat(const FVector& SeatPos, float InSeatYaw)
{
	bSeatMode = true;
	bSeated = false;
	bArrived = false;
	bLeaving = false;
	Target = SeatPos;
	SeatYaw = InSeatYaw;
	OrderText->SetText(FText::FromString(CigText::Get(TEXT("customer.seated"))));
	OrderText->SetTextRenderColor(FColor(120, 220, 255));
	OrderText->SetVisibility(true);
	TraitText->SetVisibility(false);
}

void ACigkofteCustomer::SetPatienceColor(float Frac01)
{
	const FLinearColor Full = bVIP ? FLinearColor(1.f, 0.85f, 0.1f) : FLinearColor::Green;
	const FLinearColor C = FMath::Lerp(FLinearColor::Red, Full, FMath::Clamp(Frac01, 0.f, 1.f));
	OrderText->SetTextRenderColor(C.ToFColor(false));
}

FString ACigkofteCustomer::OrderString() const
{
	FString S = UCigOrderSystem::DescribeSpec(Spec);
	if (bVIP)
	{
		S = CigText::Get(TEXT("customer.desc.vip")) + S;
	}
	if (!LoyalName.IsEmpty())
	{
		S = LoyalName + TEXT(" — ") + S;
	}
	return S;
}

void ACigkofteCustomer::TrySetupSkeletalBody(int32 Seed)
{
	// Two bodies from the UE5 mannequin set, chosen by seed so a regular walks
	// in looking like themselves. These are placeholders in the art sense - see
	// docs/Art/FAB_ASSET_PLAN.md, they are sci-fi mannequins next to Kenney
	// furniture - but a person shaped like a person beats a cylinder with a
	// sphere on it, and the skeleton is the one every animation pack targets.
	const bool bSecond = (Seed % 2) != 0;

	// MC_Sample first, and mesh and animation have to come from the same place.
	//
	// The listing says its animations are rigged to the UE5 mannequin, and the
	// hierarchy may well match, but the sequences reference their own USkeleton
	// asset (SKM_MCUE5v2_Skeleton). PlayAnimation across two different skeleton
	// assets does not play - it fails quietly and leaves the mesh in bind pose,
	// which would have shipped as customers standing frozen in the queue. So the
	// body comes from whichever pack the animations come from.
	bool bMocap = true;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, bSecond
		? TEXT("/Game/MC_Sample/Demo/Characters/MCUE5v2/Meshes/SKM_MCUE5Fv2.SKM_MCUE5Fv2")
		: TEXT("/Game/MC_Sample/Demo/Characters/MCUE5v2/Meshes/SKM_MCUE5v2.SKM_MCUE5v2"));

	if (!Mesh)
	{
		// Without that pack, the UE5 mannequin and its own animations.
		bMocap = false;
		Mesh = LoadObject<USkeletalMesh>(nullptr, bSecond
			? TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn.SKM_Quinn")
			: TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"));
	}

	if (!Mesh || !SkelBody)
	{
		return; // neither pack: the primitives stay
	}

	SkelBody->SetSkeletalMesh(Mesh);
	SkelBody->SetVisibility(true);
	// Feet on the ground, facing the way the actor faces. The mannequin models
	// point down -Y, hence the yaw.
	SkelBody->SetRelativeLocation(FVector::ZeroVector);
	SkelBody->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	SkelBody->SetRelativeScale3D(FVector(HeightScale));

	if (bMocap)
	{
		// Idle variety matters more than it sounds: six customers queueing on
		// one idle move in lockstep, which reads as a bug rather than as a
		// queue. Seed picks between three.
		const int32 Which = VisualSeed % 3;
		AnimIdle = LoadObject<UAnimSequence>(nullptr, Which == 1
			? TEXT("/Game/MC_Sample/Animations/Idle/am_Stand_Idle_03_LookAround.am_Stand_Idle_03_LookAround")
			: (Which == 2
				? TEXT("/Game/MC_Sample/Animations/Idle/am_Stand_Idle_06_ScratchArm.am_Stand_Idle_06_ScratchArm")
				: TEXT("/Game/MC_Sample/Animations/Idle/am_Stand_Conv_Talk_05_Generic.am_Stand_Conv_Talk_05_Generic")));

		// NoRM is the in-place cut. The actor is moved by VInterpConstantTo in
		// Tick, so the root-motion version would fight it and slide.
		AnimWalk = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MC_Sample/Animations/Swagger/am_Loco_Walk_Swagger_NoRM.am_Loco_Walk_Swagger_NoRM"));

		// A piano performance: seated, hands forward, which at a table reads as
		// someone leaning over their food. It is the closest thing the pack has
		// to sitting and eating, and it replaces the stopgap that dropped the
		// body 45cm and left the legs straight.
		AnimSit = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MC_Sample/Animations/Piano/am_SitPiano_Play_01.am_SitPiano_Play_01"));
		AnimHappy = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MC_Sample/Animations/Emotions/am_Stand_React_Excited_01.am_Stand_React_Excited_01"));
		AnimAngry = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MC_Sample/Animations/Emotions/am_Stand_Emotion_Frustrated_01_All.am_Stand_Emotion_Frustrated_01_All"));

		// The work set, chosen by what the motion is rather than by what the clip
		// was named for. All four are already in the project and on this
		// skeleton, so they cost nothing and need no retargeting.
		//
		// Drill held low: standing over a surface, both hands working something
		// in front at waist height, repeating. That is kneading, and it is
		// chopping, and it is the closest thing to either that any general
		// animation pack contains - because no pack contains the real ones.
		AnimWork = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MC_Sample/Animations/Drill/am_StandDrillLow_01_Drill.am_StandDrillLow_01_Drill"));
		// Reaching into a machine, which is the same shape as reaching into a tub.
		AnimReach = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MC_Sample/Animations/VendingMachine/am_Vend_Start.am_Vend_Start"));
		// Taking the item out and offering it forward: the hand-off.
		AnimServe = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MC_Sample/Animations/VendingMachine/am_Vend_Success_GrabItem.am_Vend_Success_GrabItem"));
		// Both hands held in front, working over something held between them.
		// Named for a spellbook and shaped like rolling a wrap.
		AnimWrap = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MC_Sample/Animations/Spellbook/am_SpellBook_02_Read_Loop_01.am_SpellBook_02_Read_Loop_01"));
	}
	else
	{
		// Mannequin fallback: locomotion only, no sit and no reactions.
		AnimIdle = LoadObject<UAnimSequence>(nullptr, bSecond
			? TEXT("/Game/Characters/Mannequins/Animations/Quinn/MF_Idle.MF_Idle")
			: TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle"));
		// In place for the same reason as above: Tick moves the actor.
		AnimWalk = LoadObject<UAnimSequence>(nullptr, bSecond
			? TEXT("/Game/Characters/Mannequins/Animations/Quinn/MF_Walk_Fwd.MF_Walk_Fwd")
			: TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Walk_InPlace.MM_Walk_InPlace"));
	}

	// The primitives were the whole customer; they are now the fallback.
	for (UStaticMeshComponent* Part : { Body.Get(), Head.Get(), LeftArm.Get(), RightArm.Get(),
		Hat.Get(), Bag.Get(), GlassL.Get(), GlassR.Get() })
	{
		if (Part)
		{
			Part->SetVisibility(false);
		}
	}

	bSkeletal = true;
	UpdateSkeletalAnim(false);
}

void ACigkofteCustomer::SetWorkAnim(EWorkAnim InWork)
{
	if (WorkAnim == InWork)
	{
		return;
	}
	WorkAnim = InWork;
	// Pushed straight through rather than waiting for the next movement update:
	// the apprentice changes job while standing still, and the walking tick is
	// the only other thing that would notice.
	UpdateSkeletalAnim(false);
}

void ACigkofteCustomer::UpdateSkeletalAnim(bool bWalking)
{
	if (!bSkeletal || !SkelBody)
	{
		return;
	}
	// A seated animation exists now (MC_Sample), so the body no longer has to be
	// shoved downwards to fake one. The drop is kept only for the case where
	// that pack is absent and the old stopgap is all there is - without it the
	// customer stands to attention at the table.
	const float SeatDrop = (bSeated && !AnimSit) ? -45.f * HeightScale : 0.f;
	SkelBody->SetRelativeLocation(FVector(0.f, 0.f, SeatDrop));

	UAnimSequence* Want = (bWalking && AnimWalk) ? AnimWalk : AnimIdle;

	// Work beats standing but loses to walking: an apprentice kneading while
	// sliding across the shop is the same defect the reaction animations had.
	if (!bWalking && WorkAnim != EWorkAnim::None)
	{
		UAnimSequence* Job = nullptr;
		switch (WorkAnim)
		{
		case EWorkAnim::Work:  Job = AnimWork;  break;
		case EWorkAnim::Reach: Job = AnimReach; break;
		case EWorkAnim::Serve: Job = AnimServe; break;
		case EWorkAnim::Wrap:  Job = AnimWrap;  break;
		default: break;
		}
		if (Job)
		{
			Want = Job;
		}
	}

	if (bSeated && AnimSit)
	{
		Want = AnimSit;
	}
	// The reaction plays where the customer is still standing at the counter,
	// which is the moment it is about. Once they turn and walk off, walking
	// wins - a customer stomping in frustration while gliding to the door reads
	// as broken rather than as angry.
	else if (bLeaving && !bWalking)
	{
		if (UAnimSequence* React = bHappy ? AnimHappy : AnimAngry)
		{
			Want = React;
		}
	}
	if (!Want || Want == AnimPlaying)
	{
		return; // restarting the same sequence every frame would freeze frame 0
	}
	SkelBody->PlayAnimation(Want, true);
	AnimPlaying = Want;
}

void ACigkofteCustomer::ApplyWalkAnim(bool bWalking, float DeltaSeconds)
{
	// A real body animates itself; the primitive bob below is for the fallback.
	if (bSkeletal)
	{
		UpdateSkeletalAnim(bWalking && !bSeated);
		return;
	}

	// Seated pose: the body lowers and the arms swing as if eating.
	if (bSeated)
	{
		EatPhase += DeltaSeconds * 4.f;
		const float BodyZ = BodyBaseZ - 42.f;
		const float ArmEat = 20.f + FMath::Abs(FMath::Sin(EatPhase)) * 25.f;
		Body->SetRelativeLocation(FVector(0.f, 0.f, BodyZ));
		Body->SetRelativeRotation(FRotator(6.f, 0.f, 0.f));
		Head->SetRelativeLocation(FVector(6.f, 0.f, BodyZ + 100.f * HeightScale));
		LeftArm->SetRelativeRotation(FRotator(ArmEat, 0.f, 0.f));
		RightArm->SetRelativeRotation(FRotator(ArmEat, 0.f, 0.f));
		return;
	}

	float BodyZ = BodyBaseZ;
	float Roll = 0.f;
	float ArmSwing = 0.f;

	if (bWalking)
	{
		WalkPhase += DeltaSeconds * 10.f;
		BodyZ += FMath::Abs(FMath::Sin(WalkPhase)) * 6.f;
		Roll = FMath::Sin(WalkPhase) * 4.f;
		ArmSwing = FMath::Sin(WalkPhase) * 35.f;
	}
	else
	{
		// A slight sway while waiting
		const float T = GetWorld()->GetTimeSeconds();
		Roll = FMath::Sin(T * 1.5f + IdleSeed) * 2.f;
		ArmSwing = FMath::Sin(T * 1.2f + IdleSeed) * 5.f;
	}

	// A customer leaving happy gives a little hop
	if (bLeaving && bHappy && HopTime < 1.4f)
	{
		HopTime += DeltaSeconds;
		BodyZ += FMath::Abs(FMath::Sin(HopTime * 10.f)) * 18.f;
	}

	Body->SetRelativeLocation(FVector(0.f, 0.f, BodyZ));
	Body->SetRelativeRotation(FRotator(0.f, 0.f, Roll));
	Head->SetRelativeLocation(FVector(0.f, 0.f, BodyZ + 102.f * HeightScale));
	LeftArm->SetRelativeRotation(FRotator(ArmSwing, 0.f, 0.f));
	RightArm->SetRelativeRotation(FRotator(-ArmSwing, 0.f, 0.f));
}

void ACigkofteCustomer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector Pos = GetActorLocation();
	const float Speed = bAmbient ? 220.f : 300.f;
	const FVector NewPos = FMath::VInterpConstantTo(Pos, Target, DeltaSeconds, Speed);
	SetActorLocation(NewPos);

	const float Dist = FVector::Dist2D(NewPos, Target);
	const bool bWalking = Dist > 35.f;

	// Face the target while walking, face the shop (+X) while queueing
	if (bWalking)
	{
		const FVector Dir = (Target - NewPos).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		}
	}
	else if (!bAmbient && !bLeaving)
	{
		SetActorRotation(FRotator::ZeroRotator);
	}

	ApplyWalkAnim(bWalking, DeltaSeconds);

	if (bAmbient)
	{
		if (!bWalking)
		{
			PickWanderTarget();
		}
		return;
	}

	// Heading to a table / seated
	if (bSeatMode)
	{
		if (!bSeated && Dist < 40.f)
		{
			bSeated = true;
			SetActorRotation(FRotator(0.f, SeatYaw, 0.f));
		}
		if (bSeated)
		{
			SetActorRotation(FRotator(0.f, SeatYaw, 0.f));
		}
		return;
	}

	if (bLeaving)
	{
		if (Dist < 30.f)
		{
			// Wait to return to the pool rather than being destroyed (see bAwaitingRecycle).
			Deactivate();
		}
	}
	else
	{
		bArrived = Dist < 35.f;
	}
}
