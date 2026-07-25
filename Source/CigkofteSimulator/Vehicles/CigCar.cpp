#include "CigCar.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

ACigCar::ACigCar()
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

	BodyMesh = MakePart(TEXT("Body"));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	BodyMesh->SetRelativeScale3D(FVector(3.4f, 1.5f, 0.8f));

	Cabin = MakePart(TEXT("Cabin"));
	Cabin->SetRelativeLocation(FVector(-25.f, 0.f, 125.f));
	Cabin->SetRelativeScale3D(FVector(1.7f, 1.35f, 0.6f));

	const FVector WheelPos[4] = {
		FVector(95.f, -80.f, 30.f), FVector(95.f, 80.f, 30.f),
		FVector(-95.f, -80.f, 30.f), FVector(-95.f, 80.f, 30.f)
	};
	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* Wh = MakePart(*FString::Printf(TEXT("Wheel%d"), i));
		Wh->SetRelativeLocation(WheelPos[i]);
		Wh->SetRelativeScale3D(FVector(0.6f, 0.25f, 0.6f));
		Wh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		Wheels.Add(Wh);
	}
}

void ACigCar::Init(float LaneX, float Dir, const FLinearColor& Color, float InSpeed)
{
	Speed = InSpeed;
	MoveDir = Dir;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	BodyMesh->SetStaticMesh(Cube);
	Cabin->SetStaticMesh(Cube);

	BodyMID = UMaterialInstanceDynamic::Create(Mat, this);
	BodyMID->SetVectorParameterValue(TEXT("Color"), Color);
	BodyMesh->SetMaterial(0, BodyMID);
	Cabin->SetMaterial(0, BodyMID);

	UMaterialInstanceDynamic* WheelMID = UMaterialInstanceDynamic::Create(Mat, this);
	WheelMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.05f, 0.05f));
	for (UStaticMeshComponent* Wh : Wheels)
	{
		Wh->SetStaticMesh(Cylinder);
		Wh->SetMaterial(0, WheelMID);
	}

	SetActorLocation(FVector(LaneX, FMath::FRandRange(-7000.f, 7000.f), 0.f));
	SetActorRotation(FRotator(0.f, Dir > 0.f ? 90.f : -90.f, 0.f));
}

void ACigCar::SetParked(const FVector& Pos, float Yaw, const FLinearColor& Color)
{
	Init(0.f, 1.f, Color, 0.f);
	bAutoDrive = false;
	SetActorLocation(Pos);
	SetActorRotation(FRotator(0.f, Yaw, 0.f));
	SetInteractCollision(true);
}

void ACigCar::SetInteractCollision(bool bEnabled)
{
	if (BodyMesh)
	{
		BodyMesh->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void ACigCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAutoDrive)
	{
		return;
	}

	FVector Pos = GetActorLocation();
	Pos.Y += MoveDir * Speed * DeltaSeconds;
	if (Pos.Y > 7600.f)
	{
		Pos.Y = -7600.f;
	}
	else if (Pos.Y < -7600.f)
	{
		Pos.Y = 7600.f;
	}
	SetActorLocation(Pos);
}
