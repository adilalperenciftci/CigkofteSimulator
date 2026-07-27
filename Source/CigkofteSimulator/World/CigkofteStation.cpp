#include "World/CigkofteStation.h"
#include "World/CigMeshLibrary.h"
#include "Core/CigText.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

namespace
{
	UStaticMesh* CigLoadPrimitiveMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	UMaterialInterface* CigBaseMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
}

ACigkofteStation::ACigkofteStation()
{
	// Every station must be able to tick for the pop effect; tick stays off when idle.
	PrimaryActorTick.bCanEverTick = true;

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	SetRootComponent(Base);
	Base->SetMobility(EComponentMobility::Movable);

	Top = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Top"));
	Top->SetupAttachment(Base);
	Top->SetMobility(EComponentMobility::Movable);

	Dough = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dough"));
	Dough->SetupAttachment(Base);
	Dough->SetMobility(EComponentMobility::Movable);
	Dough->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dough->SetVisibility(false);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Base);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetVisibility(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Base);
	Label->SetMobility(EComponentMobility::Movable);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(26.f);
	Label->SetTextRenderColor(FColor::White);
}

void ACigkofteStation::Setup(ECigStation InType, const FLinearColor& Color, const FString& LabelText, float LabelYaw)
{
	StationType = InType;
	TopColor = Color;

	UStaticMesh* Cube = CigLoadPrimitiveMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = CigLoadPrimitiveMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = CigLoadPrimitiveMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Mat = CigBaseMaterial();
	if (!Cube || !Cylinder || !Sphere)
	{
		return;
	}

	Base->SetStaticMesh(Cube);
	Top->SetStaticMesh(Cylinder);
	Dough->SetStaticMesh(Sphere);

	if (Mat)
	{
		BaseMID = UMaterialInstanceDynamic::Create(Mat, this);
		TopMID = UMaterialInstanceDynamic::Create(Mat, this);
		DoughMID = UMaterialInstanceDynamic::Create(Mat, this);
		Base->SetMaterial(0, BaseMID);
		Top->SetMaterial(0, TopMID);
		Dough->SetMaterial(0, DoughMID);

		// The counter body is neutral, the top takes the station's colour.
		BaseMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.42f, 0.40f));
		TopMID->SetVectorParameterValue(TEXT("Color"), Color);
		DoughMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.76f, 0.60f, 0.42f));
	}

	FVector BaseScale(0.9f, 0.9f, 0.9f);
	FVector TopScale(0.6f, 0.6f, 0.22f);
	float TopZ = 100.f;

	switch (StationType)
	{
	case ECigStation::Yogurma:
		BaseScale = FVector(1.1f, 1.1f, 0.8f);
		TopScale = FVector(0.95f, 0.95f, 0.3f);
		TopZ = 92.f;
		break;
	case ECigStation::Servis:
		BaseScale = FVector(1.0f, 4.0f, 1.0f);
		TopScale = FVector(0.5f, 0.5f, 0.06f);
		TopZ = 104.f;
		break;
	case ECigStation::Lavabo:
		BaseScale = FVector(0.8f, 0.8f, 0.85f);
		TopScale = FVector(0.55f, 0.55f, 0.12f);
		TopZ = 92.f;
		break;
	case ECigStation::Cop:
		BaseScale = FVector(0.55f, 0.55f, 0.05f);
		TopScale = FVector(0.5f, 0.5f, 0.55f);
		TopZ = 60.f;
		break;
	case ECigStation::Eldiven:
	case ECigStation::IsotPlus:
	case ECigStation::Reklam:
		BaseScale = FVector(0.8f, 0.8f, 0.9f);
		TopScale = FVector(0.45f, 0.45f, 0.3f);
		TopZ = 102.f;
		break;
	case ECigStation::Dograma:
		BaseScale = FVector(0.9f, 0.9f, 0.9f);
		TopScale = FVector(0.7f, 0.5f, 0.06f);
		TopZ = 95.f;
		break;
	case ECigStation::Lavas:
		BaseScale = FVector(0.9f, 1.2f, 0.9f);
		TopScale = FVector(0.75f, 0.75f, 0.05f);
		TopZ = 96.f;
		break;
	case ECigStation::Paketleme:
		BaseScale = FVector(0.9f, 1.2f, 0.9f);
		TopScale = FVector(0.6f, 0.8f, 0.18f);
		TopZ = 98.f;
		break;
	case ECigStation::Buzdolabi:
		BaseScale = FVector(0.9f, 0.9f, 2.1f);
		TopScale = FVector(0.5f, 0.7f, 0.08f);
		TopZ = 215.f;
		break;
	case ECigStation::Temizlik:
		BaseScale = FVector(0.7f, 0.7f, 0.8f);
		TopScale = FVector(0.4f, 0.4f, 0.4f);
		TopZ = 95.f;
		break;
	case ECigStation::Bulasik:
		BaseScale = FVector(0.8f, 0.9f, 0.85f);
		TopScale = FVector(0.6f, 0.7f, 0.10f);
		TopZ = 92.f;
		break;
	case ECigStation::Cay:
		BaseScale = FVector(0.6f, 0.6f, 0.9f);
		TopScale = FVector(0.35f, 0.35f, 0.35f);
		TopZ = 105.f;
		break;
	case ECigStation::MamaKabi:
		BaseScale = FVector(0.35f, 0.35f, 0.1f);
		TopScale = FVector(0.28f, 0.28f, 0.15f);
		TopZ = 16.f;
		break;
	case ECigStation::Tarif:
		BaseScale = FVector(0.15f, 1.2f, 1.6f);
		TopScale = FVector(0.1f, 1.0f, 1.2f);
		TopZ = 110.f;
		break;
	case ECigStation::YanUrun:
		BaseScale = FVector(0.9f, 1.6f, 0.9f);
		TopScale = FVector(0.7f, 1.3f, 0.10f);
		TopZ = 96.f;
		break;
	default:
		break;
	}

	bAlwaysTick = (StationType == ECigStation::Yogurma);

	Base->SetRelativeScale3D(BaseScale);
	// The base is at the root, so child scales are compensated for.
	TopBaseScale = TopScale / BaseScale;
	Top->SetRelativeScale3D(TopBaseScale);
	Top->SetRelativeLocation(FVector(0.f, 0.f, (TopZ - 50.f * BaseScale.Z) / BaseScale.Z));
	Dough->SetRelativeScale3D(FVector(0.5f) / BaseScale);
	Dough->SetRelativeLocation(FVector(0.f, 0.f, (TopZ + 25.f - 50.f * BaseScale.Z) / BaseScale.Z));

	LabelBaseText = LabelText;
	Label->SetText(FText::FromString(LabelText));
	const float LabelZ = FMath::Max(185.f, TopZ + 70.f);
	Label->SetRelativeLocation(FVector(0.f, 0.f, (LabelZ - 50.f * BaseScale.Z) / BaseScale.Z));
	Label->SetRelativeScale3D(FVector(1.f) / BaseScale);
	Label->SetWorldRotation(FRotator(0.f, LabelYaw, 0.f));

	// The actor sits on the ground: the base cube is raised by half its height.
	AddActorWorldOffset(FVector(0.f, 0.f, 50.f * BaseScale.Z));

	// Last, so everything above still describes the box the layout reserved.
	// Without the pack this does nothing and the coloured primitives stand.
	ApplyStationMesh(MeshForStation(StationType), BaseScale);

	UpdateTickState();
}

void ACigkofteStation::UpdateTickState()
{
	// Kneading always ticks; the rest only while a pop is running.
	SetActorTickEnabled(bAlwaysTick || PopTime > 0.f || Pulse > 0.01f);
}

void ACigkofteStation::Pop()
{
	PopTime = 0.28f;
	SetActorTickEnabled(true);
}

void ACigkofteStation::SetHighlighted(bool bOn)
{
	if (bHighlighted == bOn || !TopMID)
	{
		return;
	}
	bHighlighted = bOn;
	if (bLocked)
	{
		// The colour does not change while locked; the locked colour is kept.
		return;
	}
	TopMID->SetVectorParameterValue(TEXT("Color"), bOn ? TopColor * 1.6f + FLinearColor(0.1f, 0.1f, 0.1f) : TopColor);
}

void ACigkofteStation::SetLocked(bool bLock, int32 RequiredLevel)
{
	LockLevel = RequiredLevel;
	if (bLocked == bLock)
	{
		return;
	}
	const bool bWasLocked = bLocked;
	bLocked = bLock;

	// Locked: a pale grey body with a "SEVIYE N" label. Unlocked: back to its own colour.
	const FLinearColor LockedTop(0.22f, 0.22f, 0.24f);
	const FLinearColor LockedBase(0.26f, 0.25f, 0.24f);
	if (TopMID)
	{
		TopMID->SetVectorParameterValue(TEXT("Color"), bLocked ? LockedTop : TopColor);
	}
	if (BaseMID)
	{
		BaseMID->SetVectorParameterValue(TEXT("Color"), bLocked ? LockedBase : FLinearColor(0.45f, 0.42f, 0.40f));
	}
	if (Label)
	{
		Label->SetText(FText::FromString(bLocked ? FString::Printf(TEXT("SEVIYE %d"), LockLevel) : LabelBaseText));
		Label->SetTextRenderColor(bLocked ? FColor(150, 150, 155) : FColor::White);
	}
	if (Dough && bLocked)
	{
		Dough->SetVisibility(false);
	}

	if (bWasLocked && !bLocked)
	{
		Pop(); // unlock feedback
	}
}

UStaticMesh* ACigkofteStation::MeshForStation(ECigStation Type)
{
	// One pack for the whole shop, on purpose. Furniture from three sources at
	// three fidelities is what the style guide exists to prevent, and a bakery
	// counter next to a bakery shelf already matches without any work.
	switch (Type)
	{
	case ECigStation::Servis:     return CigMesh::Market(TEXT("SM_BakeryCounter01"));
	case ECigStation::Yogurma:    return CigMesh::Market(TEXT("SM_BakeryCounter02"));
	case ECigStation::Paketleme:  return CigMesh::Market(TEXT("SM_BakeryStand01"));
	case ECigStation::Buzdolabi:  return CigMesh::Market(TEXT("SM_Refrigerator_01"));
	case ECigStation::Tarif:      return CigMesh::Market(TEXT("SM_BakeryMenu"));
	case ECigStation::YanUrun:    return CigMesh::Market(TEXT("SM_BakeryRack"));
	case ECigStation::Dograma:    return CigMesh::Market(TEXT("SM_BakeryCounter02"));
	case ECigStation::Lavas:      return CigMesh::Market(TEXT("SM_BreadShelf"));
	case ECigStation::Bulasik:    return CigMesh::Market(TEXT("SM_SteelTray"));

	// The ingredient stations are crates of produce, which is what they are.
	// Different woods and colours keep them apart at a glance; the label is no
	// longer the only thing distinguishing one from the next.
	case ECigStation::Bulgur:     return CigMesh::Market(TEXT("SM_FruitCrate_Wood_01"));
	case ECigStation::Isot:       return CigMesh::Market(TEXT("SM_FruitCrate_Green"));
	case ECigStation::Salca:      return CigMesh::Market(TEXT("SM_FruitCrate_Blue"));
	case ECigStation::Su:         return CigMesh::Market(TEXT("SM_WoodBox_01"));
	case ECigStation::Baharat:    return CigMesh::Market(TEXT("SM_WoodBox_02"));

	// These already had good Kenney models in the world builder; reuse them
	// rather than introducing a second look for the same object.
	case ECigStation::Lavabo:     return CigMesh::Furniture(TEXT("kitchenSink"));
	case ECigStation::Cop:        return CigMesh::Furniture(TEXT("trashcan"));

	default:                      return nullptr;
	}
}

void ACigkofteStation::ApplyStationMesh(UStaticMesh* Mesh, const FVector& BaseScale)
{
	if (!Mesh || !Visual)
	{
		return;
	}

	Visual->SetStaticMesh(Mesh);
	Visual->SetVisibility(true);

	// Fit the model inside the box on all three axes, not just height. Fitting
	// to height alone looked right in principle and was wrong on screen: a
	// produce crate is wider than it is tall, so scaling it until it was as tall
	// as its box made it several times too wide, and the ingredient stations ran
	// into each other across the shop.
	const FBoxSphereBounds B = Mesh->GetBounds();
	const FVector RawSize = B.BoxExtent * 2.f;
	const FVector TargetBox = 100.f * BaseScale.GetAbs(); // the cube is 100uu a side
	const float WorldScale = FMath::Min3(
		TargetBox.X / FMath::Max(RawSize.X, 1.f),
		TargetBox.Y / FMath::Max(RawSize.Y, 1.f),
		TargetBox.Z / FMath::Max(RawSize.Z, 1.f));

	// Visual hangs off Base, which is already scaled, so undo that.
	Visual->SetRelativeScale3D(FVector(WorldScale) / BaseScale);

	// Sit the model's underside on the box's underside.
	const float BoxBottom = -50.f * BaseScale.Z;
	const float MeshBottom = (B.Origin.Z - B.BoxExtent.Z) * WorldScale;
	Visual->SetRelativeLocation(FVector(0.f, 0.f, (BoxBottom - MeshBottom) / BaseScale.Z));

	// The primitives were the placeholder for exactly this. The dough ball is
	// not one of them - it is live state, and it stays.
	Base->SetVisibility(false);
	Top->SetVisibility(false);
}

void ACigkofteStation::UpdateDough(const FCigDoughVisual& InVisual)
{
	if (StationType != ECigStation::Yogurma || !Dough)
	{
		return;
	}
	if (InVisual.NearlyEqual(CurVisual))
	{
		return;
	}
	CurVisual = InVisual;
	ApplyDoughTransform();
}

void ACigkofteStation::PulseDough()
{
	Pulse = 1.f;
	ApplyDoughTransform();
}

void ACigkofteStation::ApplyDoughTransform()
{
	if (!CurVisual.IsVisible())
	{
		Dough->SetVisibility(false);
		return;
	}

	Dough->SetVisibility(true);
	const float S = CurVisual.Scale();
	const FVector BaseScale = Base->GetRelativeScale3D();
	const FVector Squash(S * (1.f + 0.20f * Pulse), S * (1.f + 0.20f * Pulse), S * (1.f - 0.30f * Pulse));
	Dough->SetRelativeScale3D(Squash / BaseScale);

	// The colour is the batch's, not the station's: see Cooking/CigDoughVisual.h
	// for what feeds it. Written only when it actually differs, because this
	// runs from the pulse tick as well as from every stroke.
	if (DoughMID)
	{
		const FLinearColor Renk = CurVisual.Color();
		if (!Renk.Equals(LastDoughColor, 0.002f))
		{
			DoughMID->SetVectorParameterValue(TEXT("Color"), Renk);
			LastDoughColor = Renk;
		}
	}
}

void ACigkofteStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Pulse > 0.01f)
	{
		Pulse = FMath::FInterpTo(Pulse, 0.f, DeltaSeconds, 8.f);
		ApplyDoughTransform();
	}

	if (PopTime > 0.f)
	{
		PopTime = FMath::Max(0.f, PopTime - DeltaSeconds);
		// Swells on a sine over 0..1 and returns
		const float T = 1.f - PopTime / 0.28f;
		const float Bump = FMath::Sin(T * PI) * 0.22f;
		if (Top)
		{
			Top->SetRelativeScale3D(TopBaseScale * (1.f + Bump));
		}
		if (PopTime <= 0.f && Top)
		{
			Top->SetRelativeScale3D(TopBaseScale);
			UpdateTickState();
		}
	}
}

FString ACigkofteStation::GetPromptText() const
{
	if (bLocked)
	{
		return CigText::Format(TEXT("prompt.locked"), LockLevel);
	}

	switch (StationType)
	{
	case ECigStation::Bulgur:    return CigText::Get(TEXT("prompt.bulgur"));
	case ECigStation::Isot:      return CigText::Get(TEXT("prompt.isot"));
	case ECigStation::Salca:     return CigText::Get(TEXT("prompt.salca"));
	case ECigStation::Su:        return CigText::Get(TEXT("prompt.su"));
	case ECigStation::Baharat:   return CigText::Get(TEXT("prompt.baharat"));
	case ECigStation::Yogurma:   return CigText::Get(TEXT("prompt.yogurma"));
	case ECigStation::Servis:    return CigText::Get(TEXT("prompt.servis"));
	case ECigStation::Lavabo:    return CigText::Get(TEXT("prompt.lavabo"));
	case ECigStation::Cop:       return CigText::Get(TEXT("prompt.cop"));
	case ECigStation::Dograma:   return CigText::Get(TEXT("prompt.dograma"));
	case ECigStation::Lavas:     return CigText::Get(TEXT("prompt.lavas"));
	case ECigStation::Paketleme: return CigText::Get(TEXT("prompt.paketleme"));
	case ECigStation::Buzdolabi: return CigText::Get(TEXT("prompt.buzdolabi"));
	case ECigStation::Temizlik:  return CigText::Get(TEXT("prompt.temizlik"));
	case ECigStation::Bulasik:   return CigText::Get(TEXT("prompt.bulasik"));
	case ECigStation::Cay:       return CigText::Get(TEXT("prompt.cay"));
	case ECigStation::MamaKabi:  return CigText::Get(TEXT("prompt.mamakabi"));
	case ECigStation::Tarif:     return CigText::Get(TEXT("prompt.tarif"));
	case ECigStation::YanUrun:   return CigText::Get(TEXT("prompt.yanurun"));
	default:                     return TEXT(""); // the HUD labels upgrade stations with a price from the economy
	}
}
