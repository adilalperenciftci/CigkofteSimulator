#include "CigCat.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

ACigCat::ACigCat()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	auto MakePart = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	};

	Body = MakePart(TEXT("Body"));
	Body->SetRelativeLocation(FVector(0.f, 0.f, 18.f));
	Body->SetRelativeScale3D(FVector(0.45f, 0.20f, 0.16f));
	// It has to be hit by the trace to be petted
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	Head = MakePart(TEXT("Head"));
	Head->SetRelativeLocation(FVector(26.f, 0.f, 30.f));
	Head->SetRelativeScale3D(FVector(0.18f));

	EarL = MakePart(TEXT("EarL"));
	EarL->SetRelativeLocation(FVector(28.f, -6.f, 41.f));
	EarL->SetRelativeScale3D(FVector(0.045f, 0.03f, 0.06f));

	EarR = MakePart(TEXT("EarR"));
	EarR->SetRelativeLocation(FVector(28.f, 6.f, 41.f));
	EarR->SetRelativeScale3D(FVector(0.045f, 0.03f, 0.06f));

	Tail = MakePart(TEXT("Tail"));
	Tail->SetRelativeLocation(FVector(-26.f, 0.f, 28.f));
	Tail->SetRelativeScale3D(FVector(0.28f, 0.035f, 0.035f));

	// The animated cat model (filled in during Init when the asset exists)
	SkelBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkelBody"));
	SkelBody->SetupAttachment(RootComponent);
	SkelBody->SetMobility(EComponentMobility::Movable);
	// Must be hit by the look trace to be petted
	SkelBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SkelBody->SetVisibility(false);
}

void ACigCat::TrySetupSkeletalCat()
{
	// Cat_Animation_Pack (taken from the EG_OYUN_CIGKOFTE_1 project)
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Cat_Animation_Pack/Demo/Cat_Model/SM_Cat.SM_Cat"));
	if (!Mesh || !SkelBody)
	{
		return; // no pack: the primitive cat is used
	}

	SkelBody->SetSkeletalMesh(Mesh);
	SkelBody->SetVisibility(true);
	// Put the model's base under its feet; the game's cat is about 35 cm tall
	SkelBody->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	SkelBody->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	AnimWalk = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Cat_Animation_Pack/Animation/anim_CreatureCat_Run.anim_CreatureCat_Run"));
	AnimSit = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Cat_Animation_Pack/Animation/anim_CreatureCat_SitIdle.anim_CreatureCat_SitIdle"));
	AnimIdle = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Cat_Animation_Pack/Animation/anim_CreatureCat_StandIdle.anim_CreatureCat_StandIdle"));

	bSkeletal = true;

	// Hide the primitive parts, collision included, so the trace reaches the skeletal mesh
	for (UStaticMeshComponent* C : { Body.Get(), Head.Get(), EarL.Get(), EarR.Get(), Tail.Get() })
	{
		if (C)
		{
			C->SetVisibility(false);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	PlayCatAnim(false);
}

void ACigCat::PlayCatAnim(bool bSit)
{
	if (!bSkeletal || !SkelBody)
	{
		return;
	}
	UAnimSequence* Want = bSit ? (AnimSit ? AnimSit : AnimIdle) : (AnimWalk ? AnimWalk : AnimIdle);
	if (Want)
	{
		SkelBody->PlayAnimation(Want, true);
	}
	bPlayingSit = bSit;
}

void ACigCat::Init()
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Body->SetStaticMesh(Cube);
	Head->SetStaticMesh(Sphere);
	EarL->SetStaticMesh(Cube);
	EarR->SetStaticMesh(Cube);
	Tail->SetStaticMesh(Cube);

	static const FLinearColor Furs[] = {
		FLinearColor(0.85f, 0.5f, 0.15f),  // ginger
		FLinearColor(0.12f, 0.12f, 0.13f), // black
		FLinearColor(0.6f, 0.6f, 0.62f)    // grey
	};

	FurMID = UMaterialInstanceDynamic::Create(Mat, this);
	FurMID->SetVectorParameterValue(TEXT("Color"), Furs[FMath::RandRange(0, 2)]);
	for (UStaticMeshComponent* C : { Body.Get(), Head.Get(), EarL.Get(), EarR.Get(), Tail.Get() })
	{
		C->SetMaterial(0, FurMID);
	}

	// Take over with the animated model when present (otherwise the primitive cat stays)
	TrySetupSkeletalCat();

	PickTarget();
}

void ACigCat::PickTarget()
{
	// The front area inside the shop
	Target = FVector(FMath::FRandRange(-450.f, 50.f), FMath::FRandRange(-1100.f, 1100.f), 0.f);
}

void ACigCat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Tail wagging only on the primitive cat; the animated model handles it itself
	if (!bSkeletal)
	{
		TailPhase += DeltaSeconds * 6.f;
		Tail->SetRelativeRotation(FRotator(20.f, FMath::Sin(TailPhase) * 30.f, 0.f));
	}

	if (bSitting)
	{
		if (bSkeletal && !bPlayingSit)
		{
			PlayCatAnim(true); // switch to the sitting animation
		}
		StateTimer -= DeltaSeconds;
		if (StateTimer <= 0.f)
		{
			bSitting = false;
			PickTarget();
		}
		return;
	}

	if (bSkeletal && bPlayingSit)
	{
		PlayCatAnim(false); // back to the walking animation
	}

	const FVector Pos = GetActorLocation();
	const FVector NewPos = FMath::VInterpConstantTo(Pos, Target, DeltaSeconds, 140.f);
	SetActorLocation(NewPos);

	const FVector Dir = (Target - NewPos).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	}

	if (FVector::Dist2D(NewPos, Target) < 25.f)
	{
		bSitting = true;
		StateTimer = FMath::FRandRange(2.f, 6.f);
	}
}
