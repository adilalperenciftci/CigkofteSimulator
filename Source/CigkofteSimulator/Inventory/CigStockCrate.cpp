#include "Inventory/CigStockCrate.h"
#include "Inventory/CigStorage.h"
#include "Core/CigText.h"
#include "Core/CigkofteTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

namespace
{
	// The crate is a box in the item's colour rather than a model. A produce
	// crate mesh exists in the market pack, but it is photoscanned and reads as
	// something to look at rather than something to act on; a flat colour with
	// the item's name on it says "this is nine lettuces and you should deal with
	// it" from across the room, which is the only job this has.
	UStaticMesh* CrateMesh()
	{
		static UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		return Cube;
	}

	UMaterialInterface* CrateMaterial()
	{
		static UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		return Mat;
	}

	// A colour per stock item.
	//
	// The five ingredients already have one, and the seven toppings have one in
	// the wrap placement table - but neither covers all eighteen, and a crate
	// with no colour is a grey box with a name on it. Reused where they exist so
	// a crate of isot is the isot the player already knows.
	FLinearColor CrateColor(int32 Item)
	{
		if (Item >= 0 && Item < (int32)ECigIngredient::COUNT)
		{
			return CigIngredientColor((ECigIngredient)Item);
		}
		switch (Item)
		{
		case CigStockMarul:      return FLinearColor(0.35f, 0.68f, 0.24f);
		case CigStockAyran:      return FLinearColor(0.94f, 0.94f, 0.92f);
		case CigStockMaydanoz:   return FLinearColor(0.20f, 0.52f, 0.18f);
		case CigStockDomates:    return FLinearColor(0.82f, 0.18f, 0.14f);
		case CigStockTursu:      return FLinearColor(0.55f, 0.62f, 0.24f);
		case CigStockSogan:      return FLinearColor(0.93f, 0.90f, 0.84f);
		case CigStockLimon:      return FLinearColor(0.95f, 0.85f, 0.25f);
		case CigStockNarEksisi:  return FLinearColor(0.45f, 0.12f, 0.16f);
		case CigStockLavas:      return FLinearColor(0.93f, 0.88f, 0.72f);
		case CigStockIcliKofte:  return FLinearColor(0.62f, 0.42f, 0.24f);
		case CigStockCorba:      return FLinearColor(0.80f, 0.52f, 0.20f);
		case CigStockKunefe:     return FLinearColor(0.88f, 0.66f, 0.28f);
		case CigStockCayBardak:  return FLinearColor(0.72f, 0.30f, 0.12f);
		default:                 return FLinearColor(0.58f, 0.52f, 0.44f);
		}
	}
}

ACigStockCrate::ACigStockCrate()
{
	PrimaryActorTick.bCanEverTick = false;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);
	Pivot->SetMobility(EComponentMobility::Movable);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Pivot);
	Body->SetMobility(EComponentMobility::Movable);
	// Blocks the interaction trace and nothing else. A crate the player can walk
	// through still reads as being in the way, and one that blocks movement would
	// let a delivery wall the shop off from itself.
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	// Hung off the pivot, not off the body.
	//
	// A child's relative location is multiplied by its parent's scale, and the body
	// is scaled to 0.4 in Z to make a crate out of a cube - so standing the text
	// "0.6 above the lid" put it 0.24 units above the body's centre, which is inside
	// the box. The only part of "Marul x9" that rendered was the "9" poking out of
	// the end. Off the pivot the number is the number, and the inverse-scale dance
	// the label needed to keep its letters square goes away with it.
	Label->SetupAttachment(Pivot);
	Label->SetMobility(EComponentMobility::Movable);
	Label->SetHorizontalAlignment(EHTA_Center);
	// Legible from across the room, which is the whole point of writing it on the
	// crate rather than in the message feed. At 14 the name was a smudge from the
	// counter - close enough to read only by walking up to it, by which time the
	// prompt says the same thing.
	Label->SetWorldSize(20.f);
	Label->SetTextRenderColor(FColor(30, 26, 22));
	Label->SetCastShadow(false);
}

void ACigStockCrate::Setup(int32 InItem, int32 InAmount, float InQuality)
{
	Item = InItem;
	Amount = FMath::Max(0, InAmount);
	Quality = InQuality;

	if (!Body || !CrateMesh())
	{
		return;
	}

	Body->SetStaticMesh(CrateMesh());
	// Roughly a crate: 60 wide, 45 deep, 40 tall, raised so it sits on the floor.
	Body->SetRelativeScale3D(FVector(0.6f, 0.45f, 0.4f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, 20.f));

	// Cold goods get a paler, bluer crate. It is the one distinction that
	// matters while it is standing there: a perishable losing quality by the
	// second and a sack of bulgur that does not care look different.
	BaseColor = CrateColor(Item);
	if (CigStorage::ClassOf(Item) == ECigStorageClass::Cold)
	{
		BaseColor = FMath::Lerp(BaseColor, FLinearColor(0.72f, 0.85f, 0.95f), 0.35f);
	}

	if (UMaterialInterface* Mat = CrateMaterial())
	{
		BodyMID = UMaterialInstanceDynamic::Create(Mat, this);
		BodyMID->SetVectorParameterValue(TEXT("Color"), BaseColor);
		Body->SetMaterial(0, BodyMID);
	}

	if (Label)
	{
		// Eight units clear of the lid, which the body puts at 40.
		Label->SetRelativeLocation(FVector(0.f, 0.f, 48.f));
	}
	RefreshLabel();
}

void ACigStockCrate::RefreshLabel()
{
	if (Label)
	{
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s x%d"), *CigStockName(Item), Amount)));
	}
}

void ACigStockCrate::SetHighlighted(bool bOn)
{
	if (bHighlighted == bOn || !BodyMID)
	{
		return;
	}
	bHighlighted = bOn;
	BodyMID->SetVectorParameterValue(TEXT("Color"), bOn ? BaseColor * 1.6f : BaseColor);
}

void ACigStockCrate::AgeBy(float DeltaSeconds)
{
	// Only perishables. A crate of bulgur standing by the door for an afternoon
	// is untidy; a crate of lettuce is a loss, and the quality it carries into
	// the pantry is where that has to show up - the same number a bad supplier
	// moves, so it feeds the price and the reviews without any new machinery.
	if (CigStorage::ClassOf(Item) != ECigStorageClass::Cold)
	{
		return;
	}

	// A full day out costs roughly a third of the quality. Enough to matter over
	// a shift, not enough to make an ignored crate worthless in a minute.
	Quality = FMath::Max(0.45f, Quality - DeltaSeconds * (0.35f / 180.f));

	if (BodyMID)
	{
		// It visibly wilts, so the cost is legible before the player unloads it
		// and reads a number they cannot check.
		const float Wilt = FMath::GetMappedRangeValueClamped(FVector2D(0.45f, 1.f), FVector2D(1.f, 0.f), Quality);
		BodyMID->SetVectorParameterValue(TEXT("Color"),
			FMath::Lerp(bHighlighted ? BaseColor * 1.6f : BaseColor, FLinearColor(0.35f, 0.33f, 0.28f), Wilt * 0.6f));
	}
}

FString ACigStockCrate::GetPromptText() const
{
	return CigText::Format(TEXT("prompt.crate"), *CigStockName(Item), Amount);
}
