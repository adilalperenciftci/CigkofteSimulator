#include "Orders/CigOrderSystem.h"
#include "Orders/CigToppingVisual.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "Cooking/CigCookingSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Core/CigUnlocks.h"
#include "Core/CigRandomSubsystem.h"
#include "World/CigMeshLibrary.h"
#include "World/CigWorldBuilder.h"
#include "World/CigkofteStation.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	// The assembly visual sits on the flatbread station's counter. Its position
	// is derived from the station so it follows any change to the world layout,
	// leaving no coordinate that has to be updated in two places.
	const FVector GWrapVisualOffset(0.f, 0.f, 118.f);
	// If the station is missing (world not built yet), fall back to the old fixed point.
	const FVector GWrapVisualFallback(-350.f, -250.f, 118.f);
}

FVector UCigOrderSystem::WrapVisualPos() const
{
	const UCigWorldBuilder* WB = GM ? GM->WorldBuilder.Get() : nullptr;
	const ACigkofteStation* Lavas = WB ? WB->FindStation(ECigStation::Lavas) : nullptr;
	return Lavas ? Lavas->GetActorLocation() + GWrapVisualOffset : GWrapVisualFallback;
}

void UCigOrderSystem::StartWrap()
{
	UCigInventorySystem* Inv = GM ? GM->Inventory.Get() : nullptr;
	if (!Inv)
	{
		return;
	}
	if (Wrap.bActive)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.wrapexists")), FLinearColor(1.f, 0.8f, 0.3f));
		return;
	}
	if (!Inv->HasStock(CigStockLavas))
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.nolavas")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	Inv->Consume(CigStockLavas);
	Wrap = FCigWrapBuild();
	Wrap.bActive = true;
	Wrap.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	GM->AddMessage(CigText::Get(TEXT("msg.order.wrapstarted")), FLinearColor(0.9f, 0.85f, 0.6f));
	Bus().WrapStarted.Broadcast();
	UpdateWrapVisual();
}

void UCigOrderSystem::AddPortion()
{
	UCigCookingSystem* Cook = GM ? GM->Cooking.Get() : nullptr;
	if (!Cook || !Wrap.bActive)
	{
		return;
	}
	if (Wrap.bWrapped)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.alreadywrapped")), FLinearColor(1.f, 0.8f, 0.3f));
		return;
	}
	if (Wrap.Portions >= 2)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.maxportion")), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	if (!Cook->Dough.IsValid())
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.nodough")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	const ECigSpice DoughSpice = Cook->Dough.Spice;
	const int32 DoughRecipe = Cook->Dough.Recipe;
	const float Q = Cook->UseServings(1);
	if (Q < 0.f)
	{
		return;
	}

	// With more than one portion the quality is averaged
	Wrap.DoughQuality = (Wrap.DoughQuality * Wrap.Portions + Q) / (Wrap.Portions + 1);
	Wrap.Portions++;
	Wrap.Spice = DoughSpice;
	Wrap.Recipe = DoughRecipe;
	GM->AddMessage(CigText::Format(TEXT("msg.order.portionadded"), Wrap.Portions, *CigSpiceName(Wrap.Spice)), FLinearColor(0.7f, 1.f, 0.7f));
	UpdateWrapVisual();
}

void UCigOrderSystem::ToggleTopping(ECigTopping T)
{
	UCigInventorySystem* Inv = GM ? GM->Inventory.Get() : nullptr;
	if (!Inv || !Wrap.bActive || Wrap.bWrapped)
	{
		return;
	}

	const uint8 Bit = 1 << (uint8)T;
	if (Wrap.ToppingMask & Bit)
	{
		// A removed topping is thrown away, never put back
		Wrap.ToppingMask &= ~Bit;
		GM->AddMessage(CigText::Format(TEXT("msg.order.toppingremoved"), *CigToppingName(T)), FLinearColor(0.8f, 0.8f, 0.8f));
		UpdateToppingVisuals();
		return;
	}

	// Lettuce needs chopped garnish; the rest come straight from stock
	if (T == ECigTopping::Marul)
	{
		if (Inv->Garnish <= 0)
		{
			GM->AddMessage(CigText::Get(TEXT("msg.order.nogarnish")), FLinearColor(1.f, 0.4f, 0.3f));
			return;
		}
		Inv->Garnish--;
	}
	else
	{
		const int32 StockIdx = CigStockIndexForTopping(T);
		if (!Inv->HasStock(StockIdx))
		{
			GM->AddMessage(CigText::Format(TEXT("msg.order.toppingout"), *CigToppingName(T)), FLinearColor(1.f, 0.4f, 0.3f));
			return;
		}
		Inv->Consume(StockIdx);
	}

	Wrap.ToppingMask |= Bit;
	GM->AddMessage(CigText::Format(TEXT("msg.order.toppingadded"), *CigToppingName(T)), FLinearColor(0.7f, 1.f, 0.7f));
	// The mask is what the visuals read, so it has to be pushed on the same
	// keypress. Without this the player added a topping, saw the message, and saw
	// nothing appear on the flatbread until some other action redrew the wrap.
	UpdateToppingVisuals();
}

void UCigOrderSystem::ToggleAyran()
{
	UCigInventorySystem* Inv = GM ? GM->Inventory.Get() : nullptr;
	if (!Inv || !Wrap.bActive)
	{
		return;
	}
	if (Wrap.bAyran)
	{
		Wrap.bAyran = false;
		Inv->Add(CigStockAyran, 1); // an unopened ayran goes back
		GM->AddMessage(CigText::Get(TEXT("msg.order.ayranremoved")), FLinearColor(0.8f, 0.8f, 0.8f));
		return;
	}
	if (!Inv->HasStock(CigStockAyran))
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.noayran")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}
	Inv->Consume(CigStockAyran);
	Wrap.bAyran = true;
	GM->AddMessage(CigText::Get(TEXT("msg.order.ayranadded")), FLinearColor(0.7f, 1.f, 0.7f));
}

void UCigOrderSystem::CycleSide()
{
	UCigInventorySystem* Inv = GM ? GM->Inventory.Get() : nullptr;
	if (!Inv || !Wrap.bActive)
	{
		if (GM)
		{
			GM->AddMessage(CigText::Get(TEXT("msg.order.needwrapforside")), FLinearColor(1.f, 0.7f, 0.3f));
		}
		return;
	}

	// The current item goes back and the next one in stock is put on the tray.
	const int32 OldIdx = CigSideStockIndex(Wrap.Side);
	if (OldIdx >= 0)
	{
		Inv->Add(OldIdx, 1);
	}

	const int32 Count = (int32)ECigSide::COUNT;
	for (int32 Step = 1; Step <= Count; ++Step)
	{
		const ECigSide Next = (ECigSide)(((int32)Wrap.Side + Step) % Count);
		if (Next == ECigSide::Yok)
		{
			Wrap.Side = ECigSide::Yok;
			GM->AddMessage(CigText::Get(TEXT("msg.order.noside")), FLinearColor(0.8f, 0.8f, 0.8f));
			return;
		}
		const int32 Idx = CigSideStockIndex(Next);
		if (Inv->HasStock(Idx))
		{
			Inv->Consume(Idx);
			Wrap.Side = Next;
			GM->AddMessage(CigText::Format(TEXT("msg.order.sideadded"), *CigSideName(Next), CigSidePrice(Next)),
				FLinearColor(0.7f, 1.f, 0.7f));
			return;
		}
	}

	Wrap.Side = ECigSide::Yok;
	GM->AddMessage(CigText::Get(TEXT("msg.order.nosidestock")), FLinearColor(1.f, 0.4f, 0.3f));
}

void UCigOrderSystem::TogglePack()
{
	if (!Wrap.bActive)
	{
		return;
	}
	Wrap.bPacked = !Wrap.bPacked;
	GM->AddMessage(Wrap.bPacked ? CigText::Get(TEXT("msg.order.packed")) : CigText::Get(TEXT("msg.order.plate")), FLinearColor(0.85f, 0.8f, 0.6f));
	UpdateWrapVisual();
}

void UCigOrderSystem::FinishWrap()
{
	if (!Wrap.bActive)
	{
		return;
	}
	if (Wrap.bWrapped)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.wrapalreadydone")), FLinearColor(0.8f, 0.8f, 0.8f));
		return;
	}
	if (Wrap.Portions <= 0)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.emptywrap")), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	Wrap.bWrapped = true;
	GM->PlaySound(ECigSound::WrapCloth);
	GM->AddMessage(CigText::Get(TEXT("msg.order.wrapfinished")), FLinearColor(0.5f, 1.f, 0.5f));
	Bus().WrapFinished.Broadcast();
	UpdateWrapVisual();
}

void UCigOrderSystem::ShelfWrap()
{
	if (!Wrap.bActive || !Wrap.bWrapped)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.needwrapped")), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	if (Shelf.Num() >= MaxShelf)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.order.shelffull")), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	FCigPackagedWrap P;
	P.Build = Wrap;
	P.Build.bPacked = true;
	P.Temp = 100.f;
	Shelf.Add(P);
	Wrap = FCigWrapBuild();
	GM->AddMessage(CigText::Format(TEXT("msg.order.shelved"), Shelf.Num(), MaxShelf), FLinearColor(0.6f, 0.9f, 1.f));
	UpdateWrapVisual();
}

void UCigOrderSystem::DiscardWrap()
{
	if (!Wrap.bActive)
	{
		return;
	}
	// A side on the tray returns to stock if it was never opened.
	if (UCigInventorySystem* Inv = GM ? GM->Inventory.Get() : nullptr)
	{
		const int32 SideIdx = CigSideStockIndex(Wrap.Side);
		if (SideIdx >= 0)
		{
			Inv->Add(SideIdx, 1);
		}
	}
	Wrap = FCigWrapBuild();
	GM->AddMessage(CigText::Get(TEXT("msg.order.discarded")), FLinearColor(0.8f, 0.8f, 0.8f));
	UpdateWrapVisual();
}

void UCigOrderSystem::UpdateSystem(float DeltaSeconds)
{
	// Packages on the shelf cool down
	for (FCigPackagedWrap& P : Shelf)
	{
		P.Temp = FMath::Max(0.f, P.Temp - DeltaSeconds * 0.8f);
	}

	// Hide the visual if the wrap was consumed elsewhere (served or shelved)
	if (!Wrap.bActive && WrapVisual && !WrapVisual->IsHidden())
	{
		WrapVisual->SetActorHiddenInGame(true);
	}
}

void UCigOrderSystem::UpdateWrapVisual()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Pick mesh and size based on state
	UStaticMesh* Mesh = nullptr;
	float TargetH = 55.f;
	if (Wrap.bActive)
	{
		if (Wrap.bWrapped)
		{
			// Rolled wrap: presented as takeaway or on a plate
			Mesh = Wrap.bPacked ? CigMesh::Food(TEXT("bag")) : CigMesh::Food(TEXT("sub"));
			TargetH = Wrap.bPacked ? 45.f : 34.f;
		}
		else if (Wrap.Portions > 0)
		{
			// Filled, still-open flatbread
			Mesh = CigMesh::Food(TEXT("taco"));
			TargetH = 32.f;
		}
		else
		{
			// Empty flatbread laid out
			Mesh = CigMesh::Food(TEXT("plate-dinner"));
			TargetH = 14.f;
		}
	}

	if (!Wrap.bActive || !Mesh)
	{
		// Hide the visual
		if (WrapVisual)
		{
			WrapVisual->SetActorHiddenInGame(true);
		}
		UpdateToppingVisuals();
		return;
	}

	if (!WrapVisual)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		WrapVisual = World->SpawnActor<AStaticMeshActor>(WrapVisualPos(), FRotator::ZeroRotator, P);
		if (WrapVisual)
		{
			WrapVisual->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			WrapVisual->SetActorEnableCollision(false);
		}
	}
	if (!WrapVisual)
	{
		return;
	}

	UStaticMeshComponent* C = WrapVisual->GetStaticMeshComponent();
	C->SetStaticMesh(Mesh);
	const FBoxSphereBounds B = Mesh->GetBounds();
	const float Scale = TargetH / FMath::Max(1.f, B.BoxExtent.Z * 2.f);
	WrapVisual->SetActorScale3D(FVector(Scale));
	WrapVisual->SetActorLocation(WrapVisualPos() - FVector(0.f, 0.f, (B.Origin.Z - B.BoxExtent.Z) * Scale));
	WrapVisual->SetActorHiddenInGame(false);

	UpdateToppingVisuals();
}

void UCigOrderSystem::UpdateToppingVisuals()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Inside a rolled wrap nothing is visible, and an inactive counter shows
	// nothing at all.
	const bool bShowAny = Wrap.bActive && !Wrap.bWrapped;
	const FVector Base = WrapVisualPos();

	// One slot per topping per piece, allocated once and reused.
	//
	// The pool is sized from the table rather than from the enum, so adding a
	// piece to a topping does not need this line changed - and so nothing is
	// spawned while the player is mid-wrap, which is the one moment they are
	// pressing keys quickly.
	const int32 Pieces = CigToppingVisual::MaxPieces();
	const int32 Slots = (int32)ECigTopping::COUNT * Pieces;
	if (ToppingVisuals.Num() != Slots)
	{
		ToppingVisuals.SetNum(Slots);
	}

	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		const ECigTopping T = (ECigTopping)i;
		const FCigToppingPlacement& P = CigToppingVisual::Placement(T);
		const bool bWant = bShowAny && Wrap.HasTopping(T);

		for (int32 Piece = 0; Piece < Pieces; ++Piece)
		{
			const int32 Slot = i * Pieces + Piece;
			AStaticMeshActor* A = ToppingVisuals[Slot];

			// A topping that asks for two pieces leaves the rest of its slots
			// hidden rather than unallocated, so the pool never has to grow.
			if (!bWant || Piece >= P.Count)
			{
				if (A)
				{
					A->SetActorHiddenInGame(true);
				}
				continue;
			}

			if (!A)
			{
				UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
				UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr,
					TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
				if (!Sphere)
				{
					continue;
				}
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				A = World->SpawnActor<AStaticMeshActor>(Base, FRotator::ZeroRotator, Params);
				if (!A)
				{
					continue;
				}
				UStaticMeshComponent* SC = A->GetStaticMeshComponent();
				SC->SetMobility(EComponentMobility::Movable);
				SC->SetStaticMesh(Sphere);
				A->SetActorEnableCollision(false);
				if (Mat)
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, A);
					MID->SetVectorParameterValue(TEXT("Color"), P.Color);
					SC->SetMaterial(0, MID);
				}
				ToppingVisuals[Slot] = A;
			}

			A->SetActorScale3D(FVector(P.Scale));
			A->SetActorLocation(Base + CigToppingVisual::PieceOffset(T, Piece));
			A->SetActorHiddenInGame(false);
		}
	}
}

FCigOrderSpec UCigOrderSystem::MakeOrderSpec(int32 Day, ECigTrait Traits, bool bAllowAyran) const
{
	FCigOrderSpec Spec;

	// A customer cannot ask for work from stations that have not unlocked: no
	// toppings while chopping is closed, no takeaway while packaging is closed.
	const int32 Level = (GM && GM->Progression) ? GM->Progression->Level : 1;
	const bool bCanChop = Level >= CigStationUnlockLevel(ECigStation::Dograma);
	const bool bCanPack = Level >= CigStationUnlockLevel(ECigStation::Paketleme);

	if (EnumHasAnyFlags(Traits, ECigTrait::SpicyFoodLover))
	{
		Spec.Spice = ECigSpice::CokAci;
	}
	else if (EnumHasAnyFlags(Traits, ECigTrait::Tourist))
	{
		Spec.Spice = ECigSpice::AzAci; // tourists cannot take the heat
	}
	else
	{
		Spec.Spice = (ECigSpice)Rng().RandRange(0, 2);
	}

	const float PorsiyonChance = FMath::Min(0.15f + 0.05f * Day, 0.5f);
	Spec.Portion = Rng().Chance(PorsiyonChance) ? 2 : 1;
	if (EnumHasAnyFlags(Traits, ECigTrait::Family))
	{
		Spec.Portion = 2;
	}

	// Topping requests grow as the day goes on, and for gourmet customers
	int32 ToppingCount = Rng().RandRange(1, FMath::Min(2 + Day / 3, 5));
	if (EnumHasAnyFlags(Traits, ECigTrait::QualityFocused | ECigTrait::SecretCritic))
	{
		ToppingCount = FMath::Max(ToppingCount, 3);
	}
	if (EnumHasAnyFlags(Traits, ECigTrait::Student))
	{
		ToppingCount = FMath::Min(ToppingCount, 2); // students like it plain
	}
	if (!bCanChop)
	{
		ToppingCount = 0; // no toppings asked for before chopping unlocks
	}
	TArray<ECigTopping> Pool = { ECigTopping::Marul, ECigTopping::Maydanoz, ECigTopping::Domates, ECigTopping::Tursu, ECigTopping::Sogan, ECigTopping::Limon, ECigTopping::NarEksisi };
	for (int32 i = 0; i < ToppingCount && Pool.Num() > 0; ++i)
	{
		const int32 Pick = Rng().PickIndex(Pool.Num());
		Spec.SetTopping(Pool[Pick], true);
		Pool.RemoveAt(Pick);
	}
	// Some customers specifically want no onion (absent from the mask already means no)

	// Sides: once the counter is unlocked they may order from the menu
	if (Level >= CigStationUnlockLevel(ECigStation::YanUrun))
	{
		float SideChance = 0.30f;
		if (EnumHasAnyFlags(Traits, ECigTrait::Family | ECigTrait::Generous))
		{
			SideChance = 0.55f;
		}
		else if (EnumHasAnyFlags(Traits, ECigTrait::PriceSensitive | ECigTrait::Student))
		{
			SideChance = 0.15f;
		}
		if (Rng().Chance(SideChance))
		{
			Spec.Side = (ECigSide)Rng().RandRange(1, (int32)ECigSide::COUNT - 1);
		}
	}

	Spec.bWantsAyran = bAllowAyran && Rng().Chance(0.35f);
	Spec.bPacked = bCanPack && Rng().Chance(0.35f);
	if (bCanPack && EnumHasAnyFlags(Traits, ECigTrait::Student))
	{
		Spec.bPacked = true; // students grab it and go
	}
	return Spec;
}

FCigOrderScore UCigOrderSystem::ScoreWrap(const FCigWrapBuild& Build, const FCigOrderSpec& Spec, float PrepSeconds)
{
	FCigOrderScore S;
	float Score = 0.f;

	// Heat (25 points)
	if (Build.Spice == Spec.Spice)
	{
		Score += 25.f;
	}
	else if (FMath::Abs((int32)Build.Spice - (int32)Spec.Spice) == 1)
	{
		Score += 10.f;
		S.Notes.Add(CigText::Get(TEXT("order.note.spiceclose")));
	}
	else
	{
		S.Notes.Add(CigText::Get(TEXT("order.note.spicewrong")));
	}

	// Portions (15 points)
	if (Build.Portions == Spec.Portion)
	{
		Score += 15.f;
	}
	else
	{
		S.Notes.Add(Build.Portions < Spec.Portion ? CigText::Get(TEXT("order.note.portionlow")) : CigText::Get(TEXT("order.note.portionhigh")));
		Score += 5.f;
	}

	// Toppings (30 points): what was asked for against what was put on
	{
		int32 Wanted = 0;
		int32 Correct = 0;
		int32 Extra = 0;
		for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
		{
			const ECigTopping T = (ECigTopping)i;
			const bool bWant = Spec.WantsTopping(T);
			const bool bHave = Build.HasTopping(T);
			if (bWant)
			{
				Wanted++;
				if (bHave)
				{
					Correct++;
				}
				else
				{
					S.Notes.Add(CigText::Format(TEXT("order.note.toppingmissing"), *CigToppingName(T)));
				}
			}
			else if (bHave)
			{
				Extra++;
				S.Notes.Add(CigText::Format(TEXT("order.note.toppingextra"), *CigToppingName(T)));
			}
		}
		float ToppingScore = Wanted > 0 ? 30.f * ((float)Correct / (float)Wanted) : 30.f;
		ToppingScore -= Extra * 6.f;
		Score += FMath::Max(0.f, ToppingScore);
	}

	// Ayran (10 points)
	if (Build.bAyran == Spec.bWantsAyran)
	{
		Score += 10.f;
	}
	else
	{
		S.Notes.Add(Spec.bWantsAyran ? CigText::Get(TEXT("order.note.ayranmissing")) : CigText::Get(TEXT("order.note.ayranextra")));
	}

	// Takeaway vs plate (10 points)
	if (Build.bPacked == Spec.bPacked)
	{
		Score += 10.f;
	}
	else
	{
		S.Notes.Add(Spec.bPacked ? CigText::Get(TEXT("order.note.packneeded")) : CigText::Get(TEXT("order.note.plateneeded")));
	}

	// Menu side: must match if asked for; an unasked-for extra is penalised
	if (Build.Side == Spec.Side)
	{
		if (Spec.Side != ECigSide::Yok)
		{
			Score += 8.f;
		}
	}
	else if (Spec.Side != ECigSide::Yok)
	{
		Score -= 12.f;
		S.Notes.Add(CigText::Format(TEXT("order.note.sidemissing"), *CigSideName(Spec.Side)));
	}
	else
	{
		Score -= 6.f;
		S.Notes.Add(CigText::Format(TEXT("order.note.sideextra"), *CigSideName(Build.Side)));
	}

	// Prep time (10 points): full marks under 45 seconds
	if (PrepSeconds <= 45.f)
	{
		Score += 10.f;
	}
	else if (PrepSeconds <= 90.f)
	{
		Score += 10.f * (1.f - (PrepSeconds - 45.f) / 45.f);
		S.Notes.Add(CigText::Get(TEXT("order.note.prepslow")));
	}
	else
	{
		S.Notes.Add(CigText::Get(TEXT("order.note.preptoolate")));
	}

	S.Accuracy = FMath::Clamp(Score, 0.f, 100.f);
	return S;
}

FString UCigOrderSystem::DescribeSpec(const FCigOrderSpec& Spec)
{
	FString S = CigSpiceName(Spec.Spice);
	S += Spec.Portion >= 2 ? CigText::Get(TEXT("order.desc.twopart")) : CigText::Get(TEXT("order.desc.single"));
	TArray<FString> Tops;
	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		if (Spec.WantsTopping((ECigTopping)i))
		{
			Tops.Add(CigToppingName((ECigTopping)i));
		}
	}
	if (Tops.Num() > 0)
	{
		S += TEXT(" | ") + FString::Join(Tops, TEXT("+"));
	}
	if (!Spec.WantsTopping(ECigTopping::Sogan))
	{
		S += CigText::Get(TEXT("order.desc.noonion"));
	}
	if (Spec.bWantsAyran)
	{
		S += CigText::Get(TEXT("order.desc.withayran"));
	}
	if (Spec.Side != ECigSide::Yok)
	{
		S += CigText::Format(TEXT("order.desc.side"), *CigSideName(Spec.Side));
	}
	S += Spec.bPacked ? CigText::Get(TEXT("order.desc.packed")) : CigText::Get(TEXT("order.desc.plate"));
	return S;
}

FString UCigOrderSystem::DescribeWrap() const
{
	if (!Wrap.bActive)
	{
		return TEXT("");
	}
	FString S = CigText::Format(TEXT("order.wrap.portion"), Wrap.Portions);
	if (Wrap.Portions > 0)
	{
		S += CigText::Format(TEXT("order.wrap.spice"), *CigSpiceName(Wrap.Spice));
	}
	TArray<FString> Tops;
	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		if (Wrap.HasTopping((ECigTopping)i))
		{
			Tops.Add(CigToppingName((ECigTopping)i));
		}
	}
	if (Tops.Num() > 0)
	{
		S += TEXT(" | ") + FString::Join(Tops, TEXT("+"));
	}
	if (Wrap.bAyran)
	{
		S += CigText::Get(TEXT("order.wrap.ayran"));
	}
	if (Wrap.Side != ECigSide::Yok)
	{
		S += CigText::Format(TEXT("order.wrap.side"), *CigSideName(Wrap.Side));
	}
	S += Wrap.bPacked ? CigText::Get(TEXT("order.wrap.packed")) : CigText::Get(TEXT("order.wrap.plate"));
	if (Wrap.bWrapped)
	{
		S += CigText::Get(TEXT("order.wrap.wrapped"));
	}
	return S;
}
