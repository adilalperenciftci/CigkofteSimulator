#include "World/CigWorldBuilder.h"
#include "World/CigkofteStation.h"
#include "World/CigFloatText.h"
#include "Game/CigkofteGameMode.h"
#include "Events/CigEventSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Vehicles/CigCar.h"
#include "Cat/CigCat.h"
#include "World/CigMeshLibrary.h"
#include "Core/CigText.h"
#include "Core/CigLog.h"
#include "Core/CigUpgrades.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#include "Core/CigSpawnUtils.h"

void UCigWorldBuilder::OnInit()
{
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// A food shop's walls are tiled, and that one surface does more for the room
	// than any prop in it. Null without the pack, which leaves the painted boxes
	// exactly as they were.
	//
	// Red rather than the white this started with. Seen from the counter the
	// white instance renders as a flat cool grey, and a cool grey interior is
	// the one thing docs/Art/VISUAL_STYLE_GUIDE.md rules out by name - the shop
	// is supposed to read warm, with the daylight from the street as the only
	// cool thing in frame. Red tile is also what a Turkish lokanta actually has
	// on its walls.
	WallTileMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/ModularBuildingSet/materials/Bricks_Tiles/tile_trim_clean/tile_trim_clean_color_red.tile_trim_clean_color_red"));
	BrickMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/ModularBuildingSet/materials/Bricks_Tiles/brick_wall_orange.brick_wall_orange"));

	if (!CubeMesh || !BaseMaterial)
	{
		UE_LOG(LogCig, Warning, TEXT("Engine BasicShapes bulunamadı; dünya görselleri eksik kurulabilir."));
	}
}

AStaticMeshActor* UCigWorldBuilder::SpawnBox(const FVector& Loc, const FVector& Scale, const FLinearColor& Color, UStaticMesh* Mesh,
	UMaterialInterface* MaterialOverride)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(Loc, FRotator::ZeroRotator, CigAlwaysSpawnParams());
	if (!A)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = A->GetStaticMeshComponent();
	C->SetMobility(EComponentMobility::Movable);
	if (UStaticMesh* UseMesh = Mesh ? Mesh : CubeMesh.Get())
	{
		C->SetStaticMesh(UseMesh);
	}
	if (MaterialOverride)
	{
		// A real surface brings its own colour; no MID and no parameter, which
		// also means these boxes share one material instead of one each.
		C->SetMaterial(0, MaterialOverride);
	}
	else if (BaseMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, A);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
	}
	A->SetActorScale3D(Scale);
	return A;
}

AStaticMeshActor* UCigWorldBuilder::SpawnProp(UStaticMesh* Mesh, const FVector& Loc, float TargetHeight, float Yaw,
	const FLinearColor& FallbackColor, bool bCollision)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// TargetHeight <= 0 leaves the mesh at its own scale (real-size buildings).
	if (Mesh && TargetHeight <= 0.f)
	{
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(Loc, FRotator(0.f, Yaw, 0.f), CigAlwaysSpawnParams());
		if (!A)
		{
			return nullptr;
		}
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		const FBoxSphereBounds B = Mesh->GetBounds();
		A->SetActorLocation(Loc - FVector(0.f, 0.f, B.Origin.Z - B.BoxExtent.Z));
		A->SetActorEnableCollision(bCollision);
		if (bCollision)
		{
			C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		return A;
	}

	// No asset: fall back to a roughly sized coloured box
	if (!Mesh)
	{
		const float S = TargetHeight / 100.f;
		AStaticMeshActor* Box = SpawnBox(Loc + FVector(0.f, 0.f, TargetHeight * 0.5f), FVector(S * 0.7f, S * 0.7f, S), FallbackColor);
		if (Box)
		{
			Box->SetActorRotation(FRotator(0.f, Yaw, 0.f));
			Box->SetActorEnableCollision(bCollision);
		}
		return Box;
	}

	AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(Loc, FRotator(0.f, Yaw, 0.f), CigAlwaysSpawnParams());
	if (!A)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = A->GetStaticMeshComponent();
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);

	// Scale from bounds to the target height, but never past the footprint that
	// height implies.
	//
	// Height alone is the wrong measure for anything wider than it is tall. The
	// flatbread station uses a baguette mesh, which is long and low: scaling its
	// 8cm height to 28 made it fourteen times its own length, and the shop
	// shipped a rack of loaves the size of the counter. A prop is allowed to be
	// twice as wide as it is tall - most tableware is - and is clamped beyond
	// that, which leaves every correctly proportioned prop untouched.
	const FBoxSphereBounds B = Mesh->GetBounds();
	const float RawHeight = FMath::Max(1.f, B.BoxExtent.Z * 2.f);
	const float RawWidth = FMath::Max(1.f, FMath::Max(B.BoxExtent.X, B.BoxExtent.Y) * 2.f);
	constexpr float MaxWidthOverHeight = 2.f;
	const float Scale = FMath::Min(TargetHeight / RawHeight,
		(TargetHeight * MaxWidthOverHeight) / RawWidth);
	A->SetActorScale3D(FVector(Scale));

	// Sit the mesh base on the ground (Loc.Z)
	const float BottomOffset = (B.Origin.Z - B.BoxExtent.Z) * Scale;
	A->SetActorLocation(Loc - FVector(0.f, 0.f, BottomOffset));

	A->SetActorEnableCollision(bCollision);
	if (bCollision)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	return A;
}

ACigkofteStation* UCigWorldBuilder::SpawnStation(ECigStation Type, const FVector& Loc, const FLinearColor& Color, const FString& Label, float LabelYaw)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	ACigkofteStation* S = World->SpawnActor<ACigkofteStation>(Loc, FRotator::ZeroRotator, CigAlwaysSpawnParams());
	if (S)
	{
		S->Setup(Type, Color, Label, LabelYaw);
		Stations.Add(Type, S);
	}
	return S;
}

UStaticMesh* UCigWorldBuilder::PreferDukkan(const TCHAR* DukkanName, UStaticMesh* KenneyFallback) const
{
	if (UStaticMesh* M = CigMesh::Dukkan(DukkanName))
	{
		return M;
	}
	return KenneyFallback;
}

UStaticMesh* UCigWorldBuilder::PreferPark(const TCHAR* Sub, const TCHAR* ParkName, UStaticMesh* KenneyFallback) const
{
	if (UStaticMesh* M = CigMesh::Park(Sub, ParkName))
	{
		return M;
	}
	return KenneyFallback;
}

ACigkofteStation* UCigWorldBuilder::FindStation(ECigStation Type) const
{
	const TWeakObjectPtr<ACigkofteStation>* Found = Stations.Find(Type);
	return Found ? Found->Get() : nullptr;
}

void UCigWorldBuilder::SetStationLabelsDebug(bool bDebug)
{
	bStationLabelsDebug = bDebug;
	for (const TPair<ECigStation, TWeakObjectPtr<ACigkofteStation>>& Pair : Stations)
	{
		if (ACigkofteStation* S = Pair.Value.Get())
		{
			S->SetLabelDebug(bDebug);
		}
	}
	// Debug wants every name at once, including the far row - that is the whole
	// point of the overhead mode. Restore them here rather than waiting for the
	// next range tick to notice.
	if (bDebug)
	{
		for (const TPair<ECigStation, TWeakObjectPtr<ACigkofteStation>>& Pair : Stations)
		{
			if (ACigkofteStation* S = Pair.Value.Get())
			{
				S->SetLabelVisible(true);
			}
		}
	}
}

void UCigWorldBuilder::UpdateSystem(float DeltaSeconds)
{
	LabelRangeTimer += DeltaSeconds;
	if (LabelRangeTimer >= LabelRangeInterval)
	{
		LabelRangeTimer = 0.f;
		UpdateStationLabelRange();
	}
}

void UCigWorldBuilder::UpdateStationLabelRange()
{
	if (bStationLabelsDebug)
	{
		return; // debug shows the lot
	}

	const UWorld* World = GetWorld();
	const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Pawn)
	{
		return;
	}

	// Squared distance, in 2D. Height is irrelevant here - the player is on the
	// floor and so are the counters - and comparing squares avoids twenty square
	// roots for a test whose answer is a bool.
	const FVector Here = Pawn->GetActorLocation();
	constexpr float RangeSq = LabelVisibleRange * LabelVisibleRange;

	for (const TPair<ECigStation, TWeakObjectPtr<ACigkofteStation>>& Pair : Stations)
	{
		ACigkofteStation* S = Pair.Value.Get();
		if (!S)
		{
			continue;
		}

		const FVector There = S->GetActorLocation();
		bool bShow = FVector::DistSquared2D(Here, There) <= RangeSq;

		// Hide a name being read from behind. A UTextRenderComponent seen from
		// its back side draws mirrored, and the screenshot pass came back with a
		// street view full of backwards signage - "PAKETLEME" and "LAVAS" spelled
		// right to left across the front of the shop. Every station label faces
		// the same way, so one dot product against its forward vector settles it.
		if (bShow)
		{
			const FVector ToPlayer = (Here - There).GetSafeNormal2D();
			bShow = FVector::DotProduct(ToPlayer, S->LabelFacing()) > 0.f;
		}

		S->SetLabelVisible(bShow);
	}
}

AActor* UCigWorldBuilder::SpawnWorldText(const FVector& Loc, const FString& Text, float Size, const FColor& Color, float Yaw)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	AActor* A = World->SpawnActor<AActor>(Loc, FRotator(0.f, Yaw, 0.f), CigAlwaysSpawnParams());
	if (!A)
	{
		return nullptr;
	}
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(A);
	A->SetRootComponent(T);
	T->RegisterComponent();
	T->SetMobility(EComponentMobility::Movable);
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetText(FText::FromString(Text));
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Color);
	T->SetWorldLocationAndRotation(Loc, FRotator(0.f, Yaw, 0.f));
	return A;
}

void UCigWorldBuilder::SpawnFloatText(const FVector& Loc, const FString& Text, const FColor& Color, float Size)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	ACigFloatText* T = World->SpawnActor<ACigFloatText>(Loc, FRotator::ZeroRotator, CigAlwaysSpawnParams());
	if (T)
	{
		T->Init(Text, Color, Size);
	}
}

void UCigWorldBuilder::BuildWorld()
{
	BuildKitchen();
	BuildFurniture();
	BuildSeatingArea();
	BuildCity();
	BuildRivalShops();
	BuildDistricts();
	SpawnLights();

	if (UWorld* World = GetWorld())
	{
		World->SpawnActor<APlayerStart>(FVector(0.f, 0.f, 110.f), FRotator(0.f, 0.f, 0.f));
	}
}

void UCigWorldBuilder::BuildKitchen()
{
	// Shop floor and walls (the front is open to the street)
	SpawnBox(FVector(150.f, 0.f, 2.f), FVector(17.5f, 26.f, 0.05f), FLinearColor(0.42f, 0.34f, 0.24f));
	// The four walls of the shop. Tiled where the pack is installed, painted
	// cream where it is not.
	SpawnBox(FVector(1000.f, 0.f, 200.f), FVector(0.4f, 26.f, 4.f), FLinearColor(0.65f, 0.55f, 0.45f), nullptr, WallTileMaterial);
	SpawnBox(FVector(150.f, 1300.f, 200.f), FVector(17.f, 0.4f, 4.f), FLinearColor(0.65f, 0.55f, 0.45f), nullptr, WallTileMaterial);
	SpawnBox(FVector(150.f, -1300.f, 200.f), FVector(17.f, 0.4f, 4.f), FLinearColor(0.65f, 0.55f, 0.45f), nullptr, WallTileMaterial);
	SpawnBox(FVector(-700.f, 900.f, 200.f), FVector(0.4f, 8.f, 4.f), FLinearColor(0.65f, 0.55f, 0.45f), nullptr, BrickMaterial);
	SpawnBox(FVector(-700.f, -900.f, 200.f), FVector(0.4f, 8.f, 4.f), FLinearColor(0.65f, 0.55f, 0.45f), nullptr, BrickMaterial);

	// The shop sign, on a board.
	//
	// The letters are a TextRender, drawn on nothing and readable from both
	// sides, so from inside the shop the name read back to front across the top
	// of the frame - which is exactly where the README's first screenshot is
	// taken from. A real sign has a board behind it.
	//
	// The board goes at -710, between the letters at -720 and the shop. The
	// street is further out at -X: it sees the letters first and the board behind
	// them, while the inside sees the board and nothing else. Putting it at -733
	// gets this exactly backwards and hides the sign from the customers.
	SpawnBox(FVector(-710.f, 0.f, 430.f), FVector(0.15f, 6.f, 1.3f), FLinearColor(0.10f, 0.09f, 0.08f));
	SpawnWorldText(FVector(-720.f, 0.f, 430.f), TEXT("CIGKOFTECI"), 90.f, FColor(255, 140, 40), 180.f);

	// Two posts holding the canopy up.
	//
	// The awning runs the width of the shop front, and the middle of that width
	// is the doorway - so three of the six stripes had nothing behind them and
	// hung in the air. They are not wrong to be there; a shop entrance has a
	// canopy. It just needs to be standing on something.
	for (float PostY : { -520.f, 520.f })
	{
		SpawnBox(FVector(-830.f, PostY, 170.f), FVector(0.12f, 0.12f, 3.4f), FLinearColor(0.24f, 0.20f, 0.16f));
	}

	// Awning (red and white stripes)
	for (int32 i = 0; i < 6; ++i)
	{
		const bool bRed = (i % 2 == 0);
		AStaticMeshActor* Awn = SpawnBox(FVector(-760.f, -750.f + i * 300.f, 360.f), FVector(1.6f, 3.f, 0.1f),
			bRed ? FLinearColor(0.75f, 0.1f, 0.08f) : FLinearColor(0.9f, 0.88f, 0.82f));
		if (Awn)
		{
			Awn->SetActorRotation(FRotator(0.f, 0.f, -12.f));
			Awn->SetActorEnableCollision(false);
		}
	}

	// The seating area is built in BuildSeatingArea() from imported furniture.

	// Flower boxes in front of the window
	for (float Y : { -520.f, 520.f })
	{
		if (AStaticMeshActor* Pot = SpawnBox(FVector(-770.f, Y, 30.f), FVector(0.7f, 1.5f, 0.6f), FLinearColor(0.45f, 0.25f, 0.12f))) { Pot->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Bush = SpawnBox(FVector(-770.f, Y, 85.f), FVector(0.9f, 1.6f, 0.5f), FLinearColor(0.15f, 0.45f, 0.12f), SphereMesh)) { Bush->SetActorEnableCollision(false); }
	}

	// Ingredient stations (back area)
	SpawnStation(ECigStation::Bulgur, FVector(600.f, -500.f, 0.f), CigIngredientColor(ECigIngredient::Bulgur), TEXT("BULGUR"), 180.f);
	SpawnStation(ECigStation::Isot, FVector(600.f, -250.f, 0.f), CigIngredientColor(ECigIngredient::Isot), TEXT("ISOT"), 180.f);
	SpawnStation(ECigStation::Salca, FVector(600.f, 0.f, 0.f), CigIngredientColor(ECigIngredient::Salca), TEXT("SALCA"), 180.f);
	SpawnStation(ECigStation::Su, FVector(600.f, 250.f, 0.f), CigIngredientColor(ECigIngredient::Su), TEXT("SU"), 180.f);
	SpawnStation(ECigStation::Baharat, FVector(600.f, 500.f, 0.f), CigIngredientColor(ECigIngredient::Baharat), TEXT("BAHARAT"), 180.f);

	SpawnStation(ECigStation::Yogurma, FVector(250.f, -650.f, 0.f), FLinearColor(0.72f, 0.72f, 0.76f), TEXT("YOGURMA"), 180.f);
	SpawnStation(ECigStation::Dograma, FVector(600.f, 750.f, 0.f), FLinearColor(0.25f, 0.65f, 0.20f), TEXT("DOGRAMA"), 180.f);
	SpawnStation(ECigStation::Lavabo, FVector(250.f, 650.f, 0.f), FLinearColor(0.85f, 0.90f, 0.95f), TEXT("LAVABO"), 180.f);
	SpawnStation(ECigStation::Cop, FVector(250.f, 950.f, 0.f), FLinearColor(0.15f, 0.15f, 0.17f), TEXT("COP"), 180.f);

	// Wrap assembly line (close to serving)
	SpawnStation(ECigStation::Lavas, FVector(-350.f, -250.f, 0.f), FLinearColor(0.93f, 0.88f, 0.72f), TEXT("LAVAS"), 180.f);
	SpawnStation(ECigStation::Paketleme, FVector(-350.f, 250.f, 0.f), FLinearColor(0.65f, 0.50f, 0.30f), TEXT("PAKETLEME"), 180.f);

	// Shop-running stations
	SpawnStation(ECigStation::Buzdolabi, FVector(870.f, -650.f, 0.f), FLinearColor(0.80f, 0.85f, 0.92f), TEXT("BUZDOLABI"), 180.f);
	SpawnStation(ECigStation::Temizlik, FVector(870.f, 650.f, 0.f), FLinearColor(0.30f, 0.75f, 0.75f), TEXT("TEMIZLIK"), 180.f);
	SpawnStation(ECigStation::Bulasik, FVector(250.f, 350.f, 0.f), FLinearColor(0.55f, 0.65f, 0.75f), TEXT("BULASIK"), 180.f);
	SpawnStation(ECigStation::Cay, FVector(870.f, 950.f, 0.f), FLinearColor(0.72f, 0.30f, 0.12f), TEXT("CAY"), 180.f);
	SpawnStation(ECigStation::MamaKabi, FVector(-200.f, 1100.f, 0.f), FLinearColor(0.85f, 0.60f, 0.70f), TEXT("MAMA"), 180.f);
	SpawnStation(ECigStation::Tarif, FVector(870.f, -950.f, 0.f), FLinearColor(0.55f, 0.42f, 0.75f), TEXT("TARIF"), 180.f);
	SpawnStation(ECigStation::YanUrun, FVector(-350.f, 650.f, 0.f), FLinearColor(0.90f, 0.65f, 0.25f), TEXT("YAN URUN"), 180.f);

	// Shop counter for upgrades - back wall
	SpawnStation(ECigStation::Eldiven, FVector(870.f, -350.f, 0.f), FLinearColor(0.9f, 0.55f, 0.1f), TEXT("ELDIVEN"), 180.f);
	SpawnStation(ECigStation::IsotPlus, FVector(870.f, 0.f, 0.f), FLinearColor(0.6f, 0.1f, 0.1f), TEXT("ISOT+"), 180.f);
	SpawnStation(ECigStation::Reklam, FVector(870.f, 350.f, 0.f), FLinearColor(0.2f, 0.6f, 0.9f), TEXT("REKLAM"), 180.f);

	// Service counter
	SpawnStation(ECigStation::Servis, FVector(-600.f, 0.f, 0.f), FLinearColor(0.50f, 0.30f, 0.15f), TEXT("SERVIS"), 0.f);
}

void UCigWorldBuilder::BuildFurniture()
{
	// Imported low poly furniture; without the asset SpawnProp falls back to a
	// coloured box.
	// Fridges (next to the Buzdolabi station)
	SpawnProp(CigMesh::Furniture(TEXT("kitchenFridgeLarge")), FVector(950.f, -780.f, 0.f), 230.f, -90.f, FLinearColor(0.85f, 0.90f, 0.96f));
	SpawnProp(CigMesh::Furniture(TEXT("kitchenFridge")), FVector(950.f, -560.f, 0.f), 200.f, -90.f, FLinearColor(0.80f, 0.85f, 0.92f));

	// Stove and cupboards along the back wall (decorative counter feel)
	SpawnProp(CigMesh::Furniture(TEXT("kitchenStove")), FVector(950.f, 760.f, 0.f), 110.f, -90.f, FLinearColor(0.3f, 0.3f, 0.33f));
	for (float Y = -150.f; Y <= 550.f; Y += 200.f)
	{
		SpawnProp(CigMesh::Furniture(TEXT("kitchenCabinet")), FVector(965.f, Y, 0.f), 100.f, -90.f, FLinearColor(0.55f, 0.42f, 0.30f));
	}

	// Sink (sink / dishwashing area)
	SpawnProp(CigMesh::Furniture(TEXT("kitchenSink")), FVector(250.f, 500.f, 0.f), 100.f, 180.f, FLinearColor(0.8f, 0.85f, 0.9f));

	// Bins (next to the rubbish station)
	SpawnProp(PreferPark(TEXT("Props"), TEXT("SM_TrashCan01"), CigMesh::Furniture(TEXT("trashcan"))), FVector(130.f, 950.f, 0.f), 95.f, 0.f, FLinearColor(0.15f, 0.15f, 0.17f));

	// Atmosphere: plant pots, radio, lamp
	SpawnProp(CigMesh::Furniture(TEXT("pottedPlant")), FVector(-680.f, -820.f, 0.f), 150.f, 0.f, FLinearColor(0.2f, 0.5f, 0.2f));
	SpawnProp(CigMesh::Furniture(TEXT("pottedPlant")), FVector(-680.f, 820.f, 0.f), 150.f, 0.f, FLinearColor(0.2f, 0.5f, 0.2f));
	SpawnProp(CigMesh::Furniture(TEXT("plantSmall1")), FVector(-560.f, 0.f, 104.f), 45.f, 0.f, FLinearColor(0.25f, 0.55f, 0.25f));
	SpawnProp(CigMesh::Furniture(TEXT("radio")), FVector(950.f, 300.f, 100.f), 45.f, -90.f, FLinearColor(0.12f, 0.12f, 0.14f));

	// Food props on top of the ingredient stations (a small visual cue, no collision)
	SpawnProp(CigMesh::Food(TEXT("bowl")), FVector(600.f, -500.f, 100.f), 24.f, 180.f, CigIngredientColor(ECigIngredient::Bulgur));
	SpawnProp(CigMesh::Food(TEXT("shaker-pepper")), FVector(600.f, -250.f, 100.f), 32.f, 180.f, CigIngredientColor(ECigIngredient::Isot));
	SpawnProp(CigMesh::Food(TEXT("bottle-ketchup")), FVector(600.f, 0.f, 100.f), 38.f, 180.f, CigIngredientColor(ECigIngredient::Salca));
	SpawnProp(CigMesh::Food(TEXT("glass")), FVector(600.f, 250.f, 100.f), 32.f, 180.f, CigIngredientColor(ECigIngredient::Su));
	SpawnProp(CigMesh::Food(TEXT("mortar-pestle")), FVector(600.f, 500.f, 100.f), 30.f, 180.f, CigIngredientColor(ECigIngredient::Baharat));

	// Board and greens on the chopping station (real salad/knife if the restaurant pack is present)
	SpawnProp(CigMesh::Food(TEXT("cutting-board")), FVector(600.f, 750.f, 100.f), 14.f, 180.f, FLinearColor(0.6f, 0.45f, 0.25f));
	SpawnProp(PreferDukkan(TEXT("salad"), CigMesh::Food(TEXT("cabbage"))),
		FVector(600.f, 780.f, 108.f), 26.f, 180.f, FLinearColor(0.3f, 0.7f, 0.25f));
	SpawnProp(PreferDukkan(TEXT("knife_1"), CigMesh::Food(TEXT("cooking-knife"))),
		FVector(600.f, 700.f, 104.f), 20.f, 200.f, FLinearColor(0.7f, 0.72f, 0.75f));

	// Visuals for the flatbread / packaging stations
	SpawnProp(PreferDukkan(TEXT("Bagets_005"), CigMesh::Food(TEXT("taco"))),
		FVector(-350.f, -250.f, 100.f), 28.f, 180.f, FLinearColor(0.9f, 0.85f, 0.6f));
	SpawnProp(CigMesh::Food(TEXT("styrofoam-dinner")), FVector(-350.f, 250.f, 100.f), 20.f, 180.f, FLinearColor(0.85f, 0.85f, 0.8f));

	// Sides counter: soup bowl, plate and glass
	SpawnProp(PreferDukkan(TEXT("22_Serving_large_bowl007"), CigMesh::Food(TEXT("bowl-soup"))),
		FVector(-350.f, 600.f, 100.f), 26.f, 180.f, FLinearColor(0.75f, 0.45f, 0.20f));
	SpawnProp(PreferDukkan(TEXT("21_Stack_of_plates_01_middle008"), CigMesh::Food(TEXT("plate-dinner"))),
		FVector(-350.f, 660.f, 100.f), 16.f, 180.f, FLinearColor(0.9f, 0.88f, 0.85f));
	SpawnProp(PreferDukkan(TEXT("12_Glass_03_for_juice005"), CigMesh::Food(TEXT("cup-tea"))),
		FVector(-350.f, 710.f, 100.f), 22.f, 180.f, FLinearColor(0.72f, 0.30f, 0.12f));

	// Stacks of plates and bowls on the service counter (from the restaurant pack)
	SpawnProp(PreferDukkan(TEXT("21_Stack_of_plates_01_large008"), nullptr),
		FVector(-600.f, -260.f, 104.f), 20.f, 0.f, FLinearColor(0.9f, 0.88f, 0.85f));
	SpawnProp(PreferDukkan(TEXT("21_Stack_of_bowls_014"), nullptr),
		FVector(-600.f, 260.f, 104.f), 18.f, 0.f, FLinearColor(0.88f, 0.86f, 0.82f));

	// Plates waiting to be washed at the dish station
	SpawnProp(PreferDukkan(TEXT("21_Plates_served_with_bowl007"), nullptr),
		FVector(250.f, 350.f, 100.f), 16.f, 180.f, FLinearColor(0.85f, 0.85f, 0.8f));
}

void UCigWorldBuilder::BuildSeatingArea()
{
	// Seating area: the front-side part of the shop. Two chairs per table, one
	// Seat record per chair. The customer system claims them via ReserveSeat.
	auto BuildTable = [this](float X, float Y)
	{
		// Table (restaurant pack > Kenney round > Kenney flat)
		UStaticMesh* TableMesh = CigMesh::Furniture(TEXT("tableRound"));
		if (!TableMesh)
		{
			TableMesh = CigMesh::Furniture(TEXT("table"));
		}
		TableMesh = PreferDukkan(TEXT("table_061"), TableMesh);
		TableMesh = PreferPark(TEXT("Props"), TEXT("SM_CafeTable01"), TableMesh); // the cafe table fits best
		SpawnProp(TableMesh, FVector(X, Y, 0.f), 95.f, 0.f, FLinearColor(0.55f, 0.38f, 0.20f), true);

		// Table setting
		SpawnProp(PreferDukkan(TEXT("21_Plates_served_with_bowl008"), nullptr),
			FVector(X, Y, 96.f), 14.f, FMath::FRandRange(0.f, 360.f), FLinearColor(0.9f, 0.88f, 0.85f));

		// Two chairs, on opposite sides of the table
		const float SeatOffset = 95.f;
		struct { float DX; float DY; float Yaw; } Slots[] = {
			{ 0.f, -SeatOffset, 90.f },
			{ 0.f, SeatOffset, -90.f }
		};
		for (const auto& Slot : Slots)
		{
			const FVector SeatPos(X + Slot.DX, Y + Slot.DY, 0.f);
			SpawnProp(PreferPark(TEXT("Props"), TEXT("SM_CafeChair01"), CigMesh::Furniture(TEXT("chairCushion"))), SeatPos, 90.f, Slot.Yaw + 180.f, FLinearColor(0.4f, 0.28f, 0.15f), false);
			FCigSeat Seat;
			Seat.Pos = FVector(X + Slot.DX, Y + Slot.DY, 0.f);
			Seat.Yaw = Slot.Yaw; // facing the table
			Seats.Add(Seat);
		}
	};

	// Layout along the right wall (Y+ side) and near the entrance
	BuildTable(-300.f, 950.f);
	BuildTable(-300.f, -950.f);
	BuildTable(-500.f, 650.f);
	BuildTable(-500.f, -650.f);

	// A small sofa corner (atmosphere)
	SpawnProp(CigMesh::Furniture(TEXT("loungeSofa")), FVector(-650.f, 1050.f, 0.f), 90.f, 90.f, FLinearColor(0.4f, 0.3f, 0.5f));
}

int32 UCigWorldBuilder::ReserveSeat()
{
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		if (!Seats[i].bOccupied)
		{
			Seats[i].bOccupied = true;
			return i;
		}
	}
	return -1;
}

void UCigWorldBuilder::ReleaseSeat(int32 Index)
{
	if (Seats.IsValidIndex(Index))
	{
		Seats[Index].bOccupied = false;
	}
}

void UCigWorldBuilder::SpawnLights()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Sun = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 800.f), FRotator(-55.f, 40.f, 0.f));
	if (Sun)
	{
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sun->GetLightComponent()->SetIntensity(6.f);
	}

	const FVector LightSpots[] = { FVector(150.f, -600.f, 450.f), FVector(150.f, 600.f, 450.f), FVector(-600.f, 0.f, 450.f), FVector(600.f, 0.f, 450.f) };
	for (const FVector& Spot : LightSpots)
	{
		APointLight* P = World->SpawnActor<APointLight>(Spot, FRotator::ZeroRotator);
		if (P)
		{
			P->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			P->GetLightComponent()->SetIntensity(6000.f);
			// Fill light, and fill light does not need to cast.
			//
			// A movable point light renders six cube faces of shadow depth for
			// every primitive inside its radius. With four of these at 2800uu
			// over 626 static meshes, the shadow pass was averaging 361 draw
			// calls against the base pass's 46, and the render thread was
			// spending 18.01 ms of an 18.04 ms frame. Four overlapping fills in
			// one room produce shadows that largely cancel each other anyway -
			// the sun casts the shadows that give the scene its shape, and the
			// counter key light below keeps its own.
			P->GetLightComponent()->SetCastShadows(false);
			if (UPointLightComponent* PC = Cast<UPointLightComponent>(P->GetLightComponent()))
			{
				PC->SetAttenuationRadius(2800.f);
				PC->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
			}
			ShopLights.Add(P);
		}
	}

	// The key light, over the counter the player works at. SpawnLights runs last
	// in BuildWorld, so the stations exist and it can sit where the work
	// actually happens rather than at a guessed coordinate.
	{
		const ACigkofteStation* Yogurma = FindStation(ECigStation::Yogurma);
		const ACigkofteStation* Servis = FindStation(ECigStation::Servis);
		FVector Where(300.f, 0.f, 330.f);
		if (Yogurma && Servis)
		{
			const FVector Mid = (Yogurma->GetActorLocation() + Servis->GetActorLocation()) * 0.5f;
			Where = FVector(Mid.X, Mid.Y, 330.f);
		}

		CounterLight = World->SpawnActor<APointLight>(Where, FRotator::ZeroRotator);
		if (CounterLight)
		{
			CounterLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			if (UPointLightComponent* PC = Cast<UPointLightComponent>(CounterLight->GetLightComponent()))
			{
				// Warmer and tighter than the room lights: roughly 3000K against
				// their 4500K, and a radius that falls off before the walls so
				// the counter reads as the lit place rather than the room being
				// uniformly brighter.
				PC->SetLightColor(FLinearColor(1.f, 0.78f, 0.52f));
				PC->SetAttenuationRadius(1250.f);
				PC->SetIntensity(9000.f);
				// This one used to keep its shadows, on the argument that a bowl
				// casting onto the counter is the reason the light is there.
				// The argument was fine and the measurement beat it.
				//
				// Per viewpoint, the shop interior was spending 495 draw calls a
				// frame on shadow depths against the street's 59 - in the one
				// room the whole game is played in. A movable point light draws
				// six cube faces for every primitive in its radius, and at 1250
				// this radius holds the counters, the walls and the furniture.
				// Switching the small props off it first moved 523 to 495 and
				// the frame time not at all, which is what pointed here.
				PC->SetCastShadows(false);
			}
		}
	}

	// Just inside the doorway. The front wall is two brick pieces at
	// (-700, ±900) with the entrance between them, so this sits in the gap and
	// throws cool light back into a room lit by warm bulbs.
	DoorLight = World->SpawnActor<APointLight>(FVector(-620.f, 0.f, 260.f), FRotator::ZeroRotator);
	if (DoorLight)
	{
		DoorLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		if (UPointLightComponent* PC = Cast<UPointLightComponent>(DoorLight->GetLightComponent()))
		{
			PC->SetLightColor(FLinearColor(0.62f, 0.74f, 0.95f)); // roughly 6000K
			PC->SetAttenuationRadius(1500.f);
			PC->SetIntensity(5000.f);
			// Same reasoning as the room fills. This one exists to put a cool
			// tone on the doorway wall for the interior to read warm against,
			// and a colour cast costs nothing to render.
			PC->SetCastShadows(false);
		}
	}

	AActor* AtmoHolder = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (AtmoHolder)
	{
		USkyAtmosphereComponent* Atmo = NewObject<USkyAtmosphereComponent>(AtmoHolder);
		AtmoHolder->SetRootComponent(Atmo);
		Atmo->RegisterComponent();
	}

	ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.f, 0.f, 600.f), FRotator::ZeroRotator);
	if (Sky)
	{
		USkyLightComponent* SC = Sky->GetLightComponent();
		SC->SetMobility(EComponentMobility::Movable);
		SC->bRealTimeCapture = true;
		SC->SetIntensity(1.0f);
		SC->MarkRenderStateDirty();
		SkyLightActor = Sky;
	}
}

void UCigWorldBuilder::BuildCity()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Wide ground - districts stretch west and north/south
	GroundActor = SpawnBox(FVector(-7000.f, 0.f, -10.f), FVector(175.f, 275.f, 0.2f), FLinearColor(0.30f, 0.32f, 0.28f));

	// North-south main street (runs past the shop; district junctions at both ends)
	SpawnBox(FVector(-1800.f, 0.f, 3.f), FVector(7.f, 262.f, 0.04f), FLinearColor(0.12f, 0.12f, 0.13f));
	for (float Y = -12600.f; Y <= 12600.f; Y += 600.f)
	{
		SpawnBox(FVector(-1800.f, Y, 6.f), FVector(0.2f, 1.6f, 0.02f), FLinearColor(0.85f, 0.85f, 0.7f));
	}

	// Pavements
	SpawnBox(FVector(-1350.f, 0.f, 4.f), FVector(2.f, 262.f, 0.05f), FLinearColor(0.45f, 0.45f, 0.44f));
	SpawnBox(FVector(-2250.f, 0.f, 4.f), FVector(2.f, 262.f, 0.05f), FLinearColor(0.45f, 0.45f, 0.44f));

	// Buildings and delivery doors
	int32 DoorNo = 1;
	auto SpawnBuilding = [this, &DoorNo](float X, float Y, bool bDoorFacesEast)
	{
		const float SX = FMath::FRandRange(4.5f, 6.5f);
		const float SY = FMath::FRandRange(4.5f, 6.5f);
		const float SZ = FMath::FRandRange(6.f, 15.f);
		const float Tone = FMath::FRandRange(0.35f, 0.65f);
		const FLinearColor Color(Tone, Tone * FMath::FRandRange(0.85f, 1.f), Tone * FMath::FRandRange(0.7f, 0.95f));

		// Use a real building if the CityPark set is present; the address/door logic is unchanged
		static const TCHAR* ParkBuildings[] = { TEXT("SM_CafeBuilding01"), TEXT("SM_CafeBuilding02"), TEXT("SM_CafeBuilding03"),
			TEXT("SM_CafeBuilding04"), TEXT("SM_CafeBuilding05"), TEXT("SM_House01"), TEXT("SM_House02_1") };
		UStaticMesh* BuildingMesh = CigMesh::Park(TEXT("Buildings"), ParkBuildings[FMath::RandRange(0, UE_ARRAY_COUNT(ParkBuildings) - 1)]);
		if (BuildingMesh)
		{
			// Real buildings keep their own scale; forcing a height made them wide
			// enough to block the street. Read the size off the mesh instead and
			// push the facade back from the road.
			const FBoxSphereBounds B = BuildingMesh->GetBounds();
			const float PushX = (bDoorFacesEast ? -1.f : 1.f) * B.BoxExtent.X;
			SpawnProp(BuildingMesh, FVector(X + PushX, Y, 0.f), 0.f, bDoorFacesEast ? 90.f : -90.f, Color, true);
		}
		else
		{
			SpawnBox(FVector(X, Y, 50.f * SZ), FVector(SX, SY, SZ), Color);
		}

		// Roof and windows only for the box building; a real mesh already has them
		if (!BuildingMesh)
		{
			if (AStaticMeshActor* Roof = SpawnBox(FVector(X, Y, 100.f * SZ + 12.f), FVector(SX + 0.5f, SY + 0.5f, 0.3f), FLinearColor(0.30f, 0.12f, 0.08f)))
			{
				Roof->SetActorEnableCollision(false);
			}
		}

		const float FaceX = bDoorFacesEast ? X + 50.f * SX : X - 50.f * SX;
		const float WinX = bDoorFacesEast ? FaceX + 4.f : FaceX - 4.f;

		const int32 Rows = BuildingMesh ? 0 : FMath::Clamp((int32)(SZ / 4.f), 1, 3);
		for (int32 R = 0; R < Rows; ++R)
		{
			for (float WY : { Y - 25.f * SY, Y + 25.f * SY })
			{
				if (AStaticMeshActor* Win = SpawnBox(FVector(WinX, WY, 260.f + R * 220.f), FVector(0.06f, 1.0f, 1.2f), FLinearColor(0.55f, 0.75f, 0.9f)))
				{
					Win->SetActorEnableCollision(false);
				}
			}
		}

		const float DoorX = bDoorFacesEast ? FaceX + 90.f : FaceX - 90.f;
		FCigAddress Addr;
		Addr.Pos = FVector(DoorX, Y, 0.f);
		Addr.Label = FString::Printf(TEXT("Çiğköfte Sk. No:%d"), DoorNo++);
		Addresses.Add(Addr);

		if (!BuildingMesh)
		{
			if (AStaticMeshActor* Door = SpawnBox(FVector(bDoorFacesEast ? FaceX + 4.f : FaceX - 4.f, Y, 110.f), FVector(0.1f, 1.4f, 2.2f), FLinearColor(0.3f, 0.18f, 0.08f)))
			{
				Door->SetActorEnableCollision(false);
			}
		}
		// Door number
		SpawnWorldText(FVector(bDoorFacesEast ? FaceX + 8.f : FaceX - 8.f, Y, 260.f), FString::Printf(TEXT("No:%d"), DoorNo - 1), 34.f,
			FColor(240, 230, 200), bDoorFacesEast ? 0.f : 180.f);
	};

	for (float Y = -6600.f; Y <= 6600.f; Y += 1320.f)
	{
		SpawnBuilding(-2900.f, Y, true);
		if (FMath::Abs(Y) > 2200.f)
		{
			SpawnBuilding(-1000.f, Y, false);
		}
	}

	// Trees - CityPark trees when available, otherwise a trunk+crown primitive
	auto SpawnTree = [this](float X, float Y)
	{
		static const TCHAR* ParkTrees[] = { TEXT("SM_AmurCork01"), TEXT("SM_AmurCork02"), TEXT("SM_AmurCork03"),
			TEXT("SM_AmurCork04"), TEXT("SM_AmurCork05"), TEXT("SM_AmurCork06") };
		if (UStaticMesh* T = CigMesh::Park(TEXT("Flora/Trees"), ParkTrees[FMath::RandRange(0, UE_ARRAY_COUNT(ParkTrees) - 1)]))
		{
			SpawnProp(T, FVector(X, Y, 0.f), FMath::FRandRange(520.f, 760.f), FMath::FRandRange(0.f, 360.f), FLinearColor(0.15f, 0.4f, 0.12f));
			return;
		}
		if (AStaticMeshActor* Trunk = SpawnBox(FVector(X, Y, 110.f), FVector(0.25f, 0.25f, 2.2f), FLinearColor(0.30f, 0.20f, 0.10f), CylinderMesh)) { Trunk->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Crown = SpawnBox(FVector(X, Y, 300.f), FVector(1.8f, 1.8f, 1.8f), FLinearColor(0.12f, 0.35f + FMath::FRandRange(0.f, 0.15f), 0.10f), SphereMesh)) { Crown->SetActorEnableCollision(false); }
	};
	for (float Y = -7000.f; Y <= 7000.f; Y += 1500.f)
	{
		SpawnTree(-2350.f, Y + FMath::FRandRange(-150.f, 150.f));
		if (FMath::Abs(Y) > 1700.f)
		{
			SpawnTree(-1250.f, Y + FMath::FRandRange(-150.f, 150.f));
		}
	}

	// Pedestrians
	for (int32 i = 0; i < 8; ++i)
	{
		ACigkofteCustomer* Ped = World->SpawnActor<ACigkofteCustomer>(
			FVector(FMath::FRandRange(-2200.f, -1500.f), FMath::FRandRange(-6000.f, 6000.f), 0.f),
			FRotator::ZeroRotator, CigAlwaysSpawnParams());
		if (Ped)
		{
			Ped->InitVisuals();
			Ped->InitAmbient(FVector2D(-2250.f, -6500.f), FVector2D(-1450.f, 6500.f));
		}
	}

	// Traffic
	static const FLinearColor CarColors[] = {
		FLinearColor(0.7f, 0.1f, 0.1f), FLinearColor(0.1f, 0.2f, 0.6f),
		FLinearColor(0.85f, 0.85f, 0.85f), FLinearColor(0.1f, 0.1f, 0.12f)
	};
	for (int32 i = 0; i < 4; ++i)
	{
		ACigCar* Car = World->SpawnActor<ACigCar>(FVector::ZeroVector, FRotator::ZeroRotator, CigAlwaysSpawnParams());
		if (Car)
		{
			const float Dir = (i % 2 == 0) ? 1.f : -1.f;
			Car->Init(Dir > 0.f ? -1950.f : -1650.f, Dir, CarColors[i], FMath::FRandRange(550.f, 800.f));
		}
	}

	// Street lamps (lit in the evening)
	for (float Y = -6000.f; Y <= 6000.f; Y += 2400.f)
	{
		for (float X : { -2150.f, -1450.f })
		{
			if (UStaticMesh* Post = CigMesh::Park(TEXT("Props"), TEXT("SM_LampPost01")))
			{
				SpawnProp(Post, FVector(X, Y, 0.f), 300.f, 0.f, FLinearColor(0.25f, 0.26f, 0.28f));
			}
			else if (AStaticMeshActor* Pole = SpawnBox(FVector(X, Y, 130.f), FVector(0.12f, 0.12f, 2.6f), FLinearColor(0.25f, 0.26f, 0.28f), CylinderMesh)) { Pole->SetActorEnableCollision(false); }
			if (AStaticMeshActor* Lamp = SpawnBox(FVector(X, Y, 275.f), FVector(0.45f, 0.45f, 0.45f), FLinearColor(1.f, 0.92f, 0.55f), SphereMesh))
			{
				Lamp->SetActorEnableCollision(false);
				StreetBulbs.Add(Lamp);
			}
		}
	}

	// Clouds
	for (int32 i = 0; i < 7; ++i)
	{
		if (AStaticMeshActor* Cloud = SpawnBox(
			FVector(FMath::FRandRange(-7000.f, 5000.f), FMath::FRandRange(-7000.f, 7000.f), FMath::FRandRange(3200.f, 4200.f)),
			FVector(FMath::FRandRange(7.f, 12.f), FMath::FRandRange(4.f, 7.f), FMath::FRandRange(1.5f, 2.5f)),
			FLinearColor(0.95f, 0.95f, 0.98f), SphereMesh))
		{
			Cloud->SetActorEnableCollision(false);
		}
	}

	// House for sale (bought from the tablet at level 5)
	{
		HousePos = FVector(-1000.f, 1800.f, 0.f);
		SpawnBox(HousePos + FVector(0.f, 0.f, 250.f), FVector(5.f, 5.f, 5.f), FLinearColor(0.92f, 0.85f, 0.70f));
		if (AStaticMeshActor* Roof = SpawnBox(HousePos + FVector(0.f, 0.f, 545.f), FVector(5.6f, 5.6f, 0.5f), FLinearColor(0.65f, 0.15f, 0.10f))) { Roof->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Door = SpawnBox(HousePos + FVector(-254.f, 0.f, 110.f), FVector(0.1f, 1.4f, 2.2f), FLinearColor(0.2f, 0.5f, 0.2f))) { Door->SetActorEnableCollision(false); }
		HouseSign = SpawnWorldText(HousePos + FVector(-300.f, 0.f, 620.f), TEXT("SATILIK EV - 5000 TL"), 60.f, FColor(255, 200, 60), 180.f);
	}
}

void UCigWorldBuilder::BuildRivalShops()
{
	// Rival shops: small businesses with colourful signs along the street.
	struct FRivalShopDef { FVector Pos; FLinearColor Color; const TCHAR* Sign; };
	const FRivalShopDef Shops[] = {
		{ FVector(-2900.f, 3300.f, 0.f), FLinearColor(0.75f, 0.15f, 0.10f), TEXT("KING CIGKOFTE") },
		{ FVector(-2900.f, -3300.f, 0.f), FLinearColor(0.10f, 0.35f, 0.65f), TEXT("URFA SULTAN") },
		{ FVector(-2900.f, 0.f, 0.f), FLinearColor(0.15f, 0.55f, 0.25f), TEXT("EKO CIG") }
	};

	for (const FRivalShopDef& Def : Shops)
	{
		SpawnBox(Def.Pos + FVector(0.f, 0.f, 200.f), FVector(4.5f, 5.5f, 4.f), FLinearColor(0.55f, 0.50f, 0.46f));
		if (AStaticMeshActor* Band = SpawnBox(Def.Pos + FVector(230.f, 0.f, 380.f), FVector(0.3f, 5.f, 0.7f), Def.Color))
		{
			Band->SetActorEnableCollision(false);
		}
		SpawnWorldText(Def.Pos + FVector(260.f, 0.f, 380.f), Def.Sign, 52.f, FColor(255, 235, 200), 0.f);
		RivalShopPos.Add(Def.Pos);
	}
}

// ============================== Neighbourhood districts ==============================
// The city is not one flat plane: there are junctions at both ends of the main
// street, two streets running west from them, and six districts in total. Each
// unlocks at a level; while locked, a barrier and a "SEVIYE N" sign block the way.

UCigWorldBuilder::FCigDistrictState& UCigWorldBuilder::AddDistrict(ECigDistrict Id, const FVector& Center)
{
	FCigDistrictState D;
	D.Id = Id;
	D.Center = Center;
	Districts.Add(D);
	return Districts.Last();
}

void UCigWorldBuilder::BuildDistrictRoads()
{
	// Two streets running west from the junctions at each end of the main street
	for (float JY : { -9200.f, 9200.f })
	{
		SpawnBox(FVector(-8150.f, JY, 3.f), FVector(65.f, 7.f, 0.04f), FLinearColor(0.12f, 0.12f, 0.13f));
		for (float X = -14200.f; X <= -2400.f; X += 600.f)
		{
			SpawnBox(FVector(X, JY, 6.f), FVector(1.6f, 0.2f, 0.02f), FLinearColor(0.85f, 0.85f, 0.7f));
		}
		// Pavements
		SpawnBox(FVector(-8150.f, JY - 450.f, 4.f), FVector(65.f, 2.f, 0.05f), FLinearColor(0.45f, 0.45f, 0.44f));
		SpawnBox(FVector(-8150.f, JY + 450.f, 4.f), FVector(65.f, 2.f, 0.05f), FLinearColor(0.45f, 0.45f, 0.44f));

		// Junction street lamps
		for (float X = -13500.f; X <= -3000.f; X += 3500.f)
		{
			if (UStaticMesh* Post = CigMesh::Park(TEXT("Props"), TEXT("SM_LampPost01")))
			{
				SpawnProp(Post, FVector(X, JY + 520.f, 0.f), 300.f, 0.f, FLinearColor(0.25f, 0.26f, 0.28f));
			}
			else if (AStaticMeshActor* Pole = SpawnBox(FVector(X, JY + 520.f, 130.f), FVector(0.12f, 0.12f, 2.6f), FLinearColor(0.25f, 0.26f, 0.28f), CylinderMesh)) { Pole->SetActorEnableCollision(false); }
			if (AStaticMeshActor* Lamp = SpawnBox(FVector(X, JY + 520.f, 275.f), FVector(0.45f, 0.45f, 0.45f), FLinearColor(1.f, 0.92f, 0.55f), SphereMesh))
			{
				Lamp->SetActorEnableCollision(false);
				StreetBulbs.Add(Lamp);
			}
		}
	}
}

void UCigWorldBuilder::BuildGate(FCigDistrictState& D, const FVector& Loc, bool bHorizontal)
{
	const FCigDistrictDef& Def = CigDistrictDef(D.Id);

	// Construction barrier: yellow and black striped blocks
	const FVector Span = bHorizontal ? FVector(0.6f, 2.2f, 1.2f) : FVector(2.2f, 0.6f, 1.2f);
	for (int32 i = -2; i <= 2; ++i)
	{
		const FVector Off = bHorizontal ? FVector(0.f, i * 220.f, 0.f) : FVector(i * 220.f, 0.f, 0.f);
		const FLinearColor C = (i % 2 == 0) ? FLinearColor(0.85f, 0.65f, 0.05f) : FLinearColor(0.1f, 0.1f, 0.1f);
		if (AStaticMeshActor* Block = SpawnBox(Loc + Off + FVector(0.f, 0.f, 60.f), Span, C))
		{
			D.GateActors.Add(Block);
		}
	}

	// Sign post and board
	if (AStaticMeshActor* Pole = SpawnBox(Loc + FVector(0.f, 0.f, 200.f), FVector(0.15f, 0.15f, 4.f), FLinearColor(0.3f, 0.3f, 0.32f), CylinderMesh))
	{
		Pole->SetActorEnableCollision(false);
		D.GateActors.Add(Pole);
	}
	const float SignYaw = bHorizontal ? (Loc.Y > 0.f ? 180.f : 0.f) : 90.f;
	if (AStaticMeshActor* Board = SpawnBox(Loc + FVector(0.f, 0.f, 430.f),
		bHorizontal ? FVector(0.2f, 6.f, 1.6f) : FVector(6.f, 0.2f, 1.6f), FLinearColor(0.10f, 0.12f, 0.16f)))
	{
		Board->SetActorEnableCollision(false);
		D.GateActors.Add(Board);
	}
	if (AActor* T1 = SpawnWorldText(Loc + FVector(bHorizontal ? (Loc.Y > 0.f ? -14.f : 14.f) : 14.f, 0.f, 470.f),
		CigText::Format(TEXT("level.locked"), Def.Level), 62.f, FColor(255, 200, 60), SignYaw))
	{
		D.GateActors.Add(T1);
		D.GateLevelSign = T1;
	}
	if (AActor* T2 = SpawnWorldText(Loc + FVector(bHorizontal ? (Loc.Y > 0.f ? -14.f : 14.f) : 14.f, 0.f, 390.f),
		Def.Name, 40.f, FColor(220, 220, 230), SignYaw))
	{
		D.GateActors.Add(T2);
	}
	if (AActor* T3 = SpawnWorldText(Loc + FVector(bHorizontal ? (Loc.Y > 0.f ? -14.f : 14.f) : 14.f, 0.f, 330.f),
		Def.Teaser, 30.f, FColor(150, 190, 150), SignYaw))
	{
		D.GateActors.Add(T3);
	}
}

void UCigWorldBuilder::SpawnPedestrians(const FVector& Center, float Radius, int32 Count)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Pos = Center + FVector(FMath::FRandRange(-Radius, Radius), FMath::FRandRange(-Radius, Radius), 0.f);
		ACigkofteCustomer* Ped = World->SpawnActor<ACigkofteCustomer>(Pos, FRotator::ZeroRotator, CigAlwaysSpawnParams());
		if (Ped)
		{
			Ped->InitVisuals();
			Ped->InitAmbient(FVector2D(Center.X - Radius, Center.Y - Radius), FVector2D(Center.X + Radius, Center.Y + Radius));
		}
	}
}

void UCigWorldBuilder::BuildDistricts()
{
	// AddDistrict returns a reference; reserve so growth cannot invalidate it.
	Districts.Reserve((int32)ECigDistrict::COUNT);

	BuildDistrictRoads();

	{
		FCigDistrictState& D = AddDistrict(ECigDistrict::SemtPazari, FVector(-1800.f, -11400.f, 0.f));
		D.PedCount = 7;
		D.PedRadius = 1700.f;
		BuildBazaar(D);
		BuildGate(D, FVector(-1800.f, -8300.f, 0.f), true);
	}
	{
		FCigDistrictState& D = AddDistrict(ECigDistrict::Meydan, FVector(-1800.f, 11400.f, 0.f));
		D.PedCount = 8;
		D.PedRadius = 1800.f;
		BuildSquare(D);
		BuildGate(D, FVector(-1800.f, 8300.f, 0.f), true);
	}
	{
		FCigDistrictState& D = AddDistrict(ECigDistrict::OkulPark, FVector(-6800.f, 9200.f, 0.f));
		D.PedCount = 8;
		D.PedRadius = 1600.f;
		BuildSchoolPark(D);
		BuildGate(D, FVector(-4300.f, 9200.f, 0.f), false);
	}
	{
		FCigDistrictState& D = AddDistrict(ECigDistrict::SanayiDepo, FVector(-6800.f, -9200.f, 0.f));
		D.PedCount = 4;
		D.PedRadius = 1500.f;
		BuildDepot(D);
		BuildGate(D, FVector(-4300.f, -9200.f, 0.f), false);
	}
	{
		FCigDistrictState& D = AddDistrict(ECigDistrict::Stadyum, FVector(-12300.f, -9200.f, 0.f));
		D.PedCount = 8;
		D.PedRadius = 2000.f;
		BuildStadium(D);
		BuildGate(D, FVector(-9900.f, -9200.f, 0.f), false);
	}
	{
		FCigDistrictState& D = AddDistrict(ECigDistrict::SahilKordon, FVector(-12300.f, 9200.f, 0.f));
		D.PedCount = 7;
		D.PedRadius = 1900.f;
		BuildSeaside(D);
		BuildGate(D, FVector(-9900.f, 9200.f, 0.f), false);
	}
}

void UCigWorldBuilder::BuildBazaar(FCigDistrictState& D)
{
	const FVector C = D.Center;

	// Market ground
	SpawnBox(C + FVector(0.f, 0.f, -4.f), FVector(30.f, 24.f, 0.06f), FLinearColor(0.42f, 0.40f, 0.36f));
	SpawnWorldText(C + FVector(0.f, -2100.f, 520.f), CigDistrictDef(D.Id).Name, 100.f, FColor(255, 190, 70), 0.f);

	// Awninged stalls: two rows
	static const TCHAR* Goods[] = { TEXT("cabbage"), TEXT("tomato"), TEXT("onion"), TEXT("lemon"), TEXT("salad"), TEXT("bag") };
	// Real produce from the restaurant pack to lay out on the market stall
	static const TCHAR* DukkanGoods[] = { TEXT("salad001"), TEXT("potato064"), TEXT("salad002"), TEXT("potato065"), TEXT("salad003"), TEXT("Bagets_006") };
	// Goods from the bazaar pack: folder plus mesh name pairs
	struct FBazaarGood { const TCHAR* Folder; const TCHAR* Name; };
	static const FBazaarGood BazaarGoods[] = {
		{ TEXT("His_Baz_Food_Spice_Chilli_Red_01"), TEXT("SM_His_Baz_Food_Spice_Chilli_Red_01") },
		{ TEXT("Food_Vegetable_Zucchini_Fresh_01"), TEXT("SM_Food_Vegetable_Zucchini_Fresh_01") },
		{ TEXT("His_Baz_Food_Spice_Turmeric_Pile_01"), TEXT("SM_His_Baz_Food_Spice_Turmeric_Pile_01") },
		{ TEXT("Food_Vegetable_Garlic_Fresh_01"), TEXT("SM_Food_Vegetable_Garlic_Fresh_01") },
		{ TEXT("His_Baz_Food_Spice_Powder_Pile_01"), TEXT("SM_His_Baz_Food_Spice_Powder_Pile_01") },
		{ TEXT("Food_Fruit_Quince_Fresh_01"), TEXT("SM_Food_Fruit_Quince_Fresh_01") }
	};
	int32 GoodIndex = 0;
	for (float X : { -1000.f, 1000.f })
	{
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector P = C + FVector(X, -1200.f + k * 800.f, 0.f);

			// Stall - use the bazaar pack's wooden stall when it is available
			UStaticMesh* StallMesh = CigMesh::Bazaar(TEXT("His_Baz_Stall_Frame_Wood_02"), TEXT("SM_His_Baz_Stall_Frame_Wood_02"));
			if (StallMesh)
			{
				SpawnProp(StallMesh, P, 260.f, 90.f, FLinearColor(0.52f, 0.36f, 0.20f), true);
			}
			else
			{
				if (AStaticMeshActor* Table = SpawnBox(P + FVector(0.f, 0.f, 45.f), FVector(2.f, 3.2f, 0.9f), FLinearColor(0.52f, 0.36f, 0.20f)))
				{
					Table->SetActorEnableCollision(true);
				}
				// Awning poles and striped cover
				for (float DY : { -160.f, 160.f })
				{
					if (AStaticMeshActor* Pole = SpawnBox(P + FVector(0.f, DY, 110.f), FVector(0.1f, 0.1f, 2.2f), FLinearColor(0.35f, 0.35f, 0.38f), CylinderMesh)) { Pole->SetActorEnableCollision(false); }
				}
				for (int32 s = 0; s < 4; ++s)
				{
					const FLinearColor Stripe = (s % 2 == 0) ? FLinearColor(0.75f, 0.12f, 0.10f) : FLinearColor(0.92f, 0.90f, 0.85f);
					if (AStaticMeshActor* Cloth = SpawnBox(P + FVector(0.f, -130.f + s * 90.f, 230.f), FVector(1.2f, 0.9f, 0.06f), Stripe)) { Cloth->SetActorEnableCollision(false); }
				}
			}
			// Goods on the stall
			for (int32 g = 0; g < 3; ++g)
			{
				const int32 Idx = GoodIndex++;
				const FBazaarGood& BG = BazaarGoods[Idx % UE_ARRAY_COUNT(BazaarGoods)];
				UStaticMesh* GoodMesh = CigMesh::Bazaar(BG.Folder, BG.Name);
				if (!GoodMesh)
				{
					const TCHAR* DukkanName = DukkanGoods[Idx % UE_ARRAY_COUNT(DukkanGoods)];
					GoodMesh = PreferDukkan(DukkanName, CigMesh::Food(Goods[Idx % UE_ARRAY_COUNT(Goods)]));
				}
				SpawnProp(GoodMesh, P + FVector(0.f, -110.f + g * 110.f, 92.f), 40.f, FMath::FRandRange(0.f, 360.f), FLinearColor(0.4f, 0.6f, 0.25f));
			}
			// Sacks, baskets and crates alongside (real ones with the bazaar pack)
			UStaticMesh* SackMesh = CigMesh::Bazaar(TEXT("His_Baz_Sack_Jute_Closed_01"), TEXT("SM_His_Baz_Sack_Jute_Closed_01"));
			SpawnProp(SackMesh ? SackMesh : CigMesh::Food(TEXT("barrel")),
				P + FVector(130.f, -190.f, 0.f), 80.f, FMath::FRandRange(0.f, 360.f), FLinearColor(0.45f, 0.32f, 0.18f));
			if (UStaticMesh* BasketMesh = CigMesh::Bazaar(TEXT("His_Baz_Container_Basket_Jute_Woven_01"), TEXT("SM_His_Baz_Container_Basket_Jute_Woven_01")))
			{
				SpawnProp(BasketMesh, P + FVector(140.f, 150.f, 0.f), 60.f, FMath::FRandRange(0.f, 360.f), FLinearColor(0.6f, 0.45f, 0.25f));
			}
			if (UStaticMesh* CrateMesh = CigMesh::Bazaar(TEXT("His_Baz_Storage_Crate_Wood_M_01"), TEXT("SM_His_Baz_Storage_Crate_Wood_M_01_A")))
			{
				SpawnProp(CrateMesh, P + FVector(-150.f, 170.f, 0.f), 70.f, FMath::FRandRange(0.f, 360.f), FLinearColor(0.5f, 0.36f, 0.2f));
			}
		}
	}

	// Two delivery addresses behind the market (they join the pool on unlock)
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector P = C + FVector(-2200.f, -600.f + i * 1200.f, 0.f);
		SpawnBox(P + FVector(0.f, 0.f, 300.f), FVector(5.f, 5.f, 6.f), FLinearColor(0.55f, 0.50f, 0.42f));
		if (AStaticMeshActor* Door = SpawnBox(P + FVector(254.f, 0.f, 110.f), FVector(0.1f, 1.4f, 2.2f), FLinearColor(0.3f, 0.18f, 0.08f))) { Door->SetActorEnableCollision(false); }
		FCigAddress Addr;
		Addr.Pos = P + FVector(340.f, 0.f, 0.f);
		Addr.Label = FString::Printf(TEXT("Pazar Sk. No:%d"), i + 1);
		D.PendingAddresses.Add(Addr);
	}
}

void UCigWorldBuilder::BuildSquare(FCigDistrictState& D)
{
	const FVector C = D.Center;

	// Square ground (cobblestone)
	if (AStaticMeshActor* Floor = SpawnBox(C + FVector(0.f, 0.f, -4.f), FVector(28.f, 28.f, 0.06f), FLinearColor(0.55f, 0.53f, 0.50f), CylinderMesh)) { Floor->SetActorEnableCollision(false); }
	SpawnWorldText(C + FVector(0.f, 2200.f, 560.f), CigDistrictDef(D.Id).Name, 100.f, FColor(255, 190, 70), 180.f);

	// Pool and fountain - use the CityPark fountain when available
	if (UStaticMesh* Fountain = CigMesh::Park(TEXT("ParkSquare"), TEXT("SM_Fountain01")))
	{
		SpawnProp(Fountain, C, 420.f, 0.f, FLinearColor(0.62f, 0.60f, 0.56f), true);
	}
	else
	{
		if (AStaticMeshActor* Rim = SpawnBox(C + FVector(0.f, 0.f, 25.f), FVector(11.f, 11.f, 0.5f), FLinearColor(0.62f, 0.60f, 0.56f), CylinderMesh)) { Rim->SetActorEnableCollision(true); }
		if (AStaticMeshActor* Water = SpawnBox(C + FVector(0.f, 0.f, 42.f), FVector(10.f, 10.f, 0.1f), FLinearColor(0.15f, 0.45f, 0.75f), CylinderMesh)) { Water->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Jet = SpawnBox(C + FVector(0.f, 0.f, 130.f), FVector(0.4f, 0.4f, 2.f), FLinearColor(0.75f, 0.85f, 0.95f), CylinderMesh)) { Jet->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Spray = SpawnBox(C + FVector(0.f, 0.f, 250.f), FVector(1.4f, 1.4f, 1.4f), FLinearColor(0.85f, 0.92f, 1.f), SphereMesh)) { Spray->SetActorEnableCollision(false); }
	}
	// Square statue
	if (UStaticMesh* Statue = CigMesh::Park(TEXT("Props"), TEXT("SM_Statue01")))
	{
		SpawnProp(Statue, C + FVector(0.f, -2600.f, 0.f), 380.f, 0.f, FLinearColor(0.55f, 0.53f, 0.5f));
	}

	// Clock tower
	const FVector TowerPos = C + FVector(-1900.f, 0.f, 0.f);
	SpawnBox(TowerPos + FVector(0.f, 0.f, 600.f), FVector(2.4f, 2.4f, 12.f), FLinearColor(0.88f, 0.84f, 0.74f));
	if (AStaticMeshActor* Face = SpawnBox(TowerPos + FVector(125.f, 0.f, 1050.f), FVector(0.1f, 1.6f, 1.6f), FLinearColor(0.95f, 0.95f, 0.9f), CylinderMesh))
	{
		Face->SetActorRotation(FRotator(0.f, 0.f, 90.f));
		Face->SetActorEnableCollision(false);
	}
	if (AStaticMeshActor* Cap = SpawnBox(TowerPos + FVector(0.f, 0.f, 1250.f), FVector(2.f, 2.f, 2.f), FLinearColor(0.55f, 0.15f, 0.10f), ConeMesh)) { Cap->SetActorEnableCollision(false); }

	// Benches and trees
	for (int32 i = 0; i < 6; ++i)
	{
		const float Ang = i * (2.f * PI / 6.f);
		const FVector P = C + FVector(FMath::Cos(Ang) * 1750.f, FMath::Sin(Ang) * 1750.f, 0.f);
		SpawnProp(PreferPark(TEXT("Props"), TEXT("SM_Bench01_1"), CigMesh::Furniture(TEXT("bench"))), P, 90.f, FMath::RadiansToDegrees(Ang) + 90.f, FLinearColor(0.45f, 0.30f, 0.16f), true);
	}
	for (int32 i = 0; i < 8; ++i)
	{
		const float Ang = i * (2.f * PI / 8.f) + 0.4f;
		const FVector P = C + FVector(FMath::Cos(Ang) * 2450.f, FMath::Sin(Ang) * 2450.f, 0.f);
		if (AStaticMeshActor* Trunk = SpawnBox(P + FVector(0.f, 0.f, 110.f), FVector(0.25f, 0.25f, 2.2f), FLinearColor(0.30f, 0.20f, 0.10f), CylinderMesh)) { Trunk->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Crown = SpawnBox(P + FVector(0.f, 0.f, 300.f), FVector(1.8f, 1.8f, 1.8f), FLinearColor(0.12f, 0.42f, 0.10f), SphereMesh)) { Crown->SetActorEnableCollision(false); }
	}

	// Delivery addresses around the square
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector P = C + FVector(1900.f, -1400.f + i * 1400.f, 0.f);
		SpawnBox(P + FVector(0.f, 0.f, 450.f), FVector(5.5f, 5.f, 9.f), FLinearColor(0.60f, 0.55f, 0.48f));
		if (AStaticMeshActor* Door = SpawnBox(P + FVector(-278.f, 0.f, 110.f), FVector(0.1f, 1.4f, 2.2f), FLinearColor(0.25f, 0.15f, 0.07f))) { Door->SetActorEnableCollision(false); }
		FCigAddress Addr;
		Addr.Pos = P + FVector(-360.f, 0.f, 0.f);
		Addr.Label = FString::Printf(TEXT("Meydan Cd. No:%d"), i + 1);
		D.PendingAddresses.Add(Addr);
	}
}

void UCigWorldBuilder::BuildSchoolPark(FCigDistrictState& D)
{
	const FVector C = D.Center;
	SpawnWorldText(C + FVector(0.f, -1500.f, 520.f), CigDistrictDef(D.Id).Name, 90.f, FColor(255, 190, 70), 0.f);

	// School building
	const FVector School = C + FVector(0.f, 1600.f, 0.f);
	SpawnBox(School + FVector(0.f, 0.f, 400.f), FVector(14.f, 6.f, 8.f), FLinearColor(0.85f, 0.80f, 0.68f));
	if (AStaticMeshActor* Roof = SpawnBox(School + FVector(0.f, 0.f, 815.f), FVector(14.6f, 6.6f, 0.3f), FLinearColor(0.45f, 0.18f, 0.12f))) { Roof->SetActorEnableCollision(false); }
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			if (AStaticMeshActor* Win = SpawnBox(School + FVector(-550.f + i * 220.f, -304.f, 250.f + Row * 260.f), FVector(1.2f, 0.06f, 1.f), FLinearColor(0.55f, 0.75f, 0.9f))) { Win->SetActorEnableCollision(false); }
		}
	}
	if (AStaticMeshActor* Door = SpawnBox(School + FVector(0.f, -304.f, 110.f), FVector(1.6f, 0.1f, 2.2f), FLinearColor(0.3f, 0.18f, 0.08f))) { Door->SetActorEnableCollision(false); }
	SpawnWorldText(School + FVector(0.f, -320.f, 560.f), TEXT("MAHALLE OKULU"), 56.f, FColor(240, 230, 210), 0.f);

	// Flagpole
	if (AStaticMeshActor* Pole = SpawnBox(School + FVector(-900.f, -600.f, 350.f), FVector(0.14f, 0.14f, 7.f), FLinearColor(0.85f, 0.85f, 0.88f), CylinderMesh)) { Pole->SetActorEnableCollision(false); }
	if (AStaticMeshActor* Flag = SpawnBox(School + FVector(-780.f, -600.f, 620.f), FVector(2.4f, 0.06f, 1.6f), FLinearColor(0.75f, 0.08f, 0.08f))) { Flag->SetActorEnableCollision(false); }

	// Park: grass, trees, benches
	if (AStaticMeshActor* Grass = SpawnBox(C + FVector(0.f, -900.f, -4.f), FVector(24.f, 10.f, 0.06f), FLinearColor(0.18f, 0.45f, 0.16f))) { Grass->SetActorEnableCollision(false); }
	for (int32 i = 0; i < 9; ++i)
	{
		const FVector P = C + FVector(-2100.f + i * 520.f, -900.f + FMath::FRandRange(-500.f, 500.f), 0.f);
		if (AStaticMeshActor* Trunk = SpawnBox(P + FVector(0.f, 0.f, 100.f), FVector(0.22f, 0.22f, 2.f), FLinearColor(0.30f, 0.20f, 0.10f), CylinderMesh)) { Trunk->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Crown = SpawnBox(P + FVector(0.f, 0.f, 270.f), FVector(1.6f, 1.6f, 1.6f), FLinearColor(0.12f, 0.40f + FMath::FRandRange(0.f, 0.12f), 0.10f), SphereMesh)) { Crown->SetActorEnableCollision(false); }
	}
	for (float X : { -1400.f, 0.f, 1400.f })
	{
		SpawnProp(PreferPark(TEXT("Props"), TEXT("SM_Bench01_1"), CigMesh::Furniture(TEXT("bench"))), C + FVector(X, -1700.f, 0.f), 90.f, 0.f, FLinearColor(0.45f, 0.30f, 0.16f), true);
	}

	// Slide
	const FVector Slide = C + FVector(1700.f, -700.f, 0.f);
	if (AStaticMeshActor* Tower = SpawnBox(Slide + FVector(0.f, 0.f, 120.f), FVector(1.f, 1.f, 2.4f), FLinearColor(0.75f, 0.55f, 0.15f))) { Tower->SetActorEnableCollision(true); }
	if (AStaticMeshActor* Ramp = SpawnBox(Slide + FVector(220.f, 0.f, 130.f), FVector(3.2f, 0.9f, 0.1f), FLinearColor(0.85f, 0.35f, 0.15f)))
	{
		Ramp->SetActorRotation(FRotator(28.f, 0.f, 0.f));
		Ramp->SetActorEnableCollision(false);
	}

	// Swing
	const FVector Swing = C + FVector(-1900.f, -300.f, 0.f);
	for (float DY : { -300.f, 300.f })
	{
		if (AStaticMeshActor* Leg = SpawnBox(Swing + FVector(0.f, DY, 110.f), FVector(0.14f, 0.14f, 2.2f), FLinearColor(0.35f, 0.36f, 0.40f), CylinderMesh)) { Leg->SetActorEnableCollision(false); }
	}
	if (AStaticMeshActor* Bar = SpawnBox(Swing + FVector(0.f, 0.f, 220.f), FVector(0.12f, 3.2f, 0.12f), FLinearColor(0.35f, 0.36f, 0.40f))) { Bar->SetActorEnableCollision(false); }
	for (float DY : { -140.f, 140.f })
	{
		if (AStaticMeshActor* Seat = SpawnBox(Swing + FVector(0.f, DY, 70.f), FVector(0.6f, 0.25f, 0.08f), FLinearColor(0.2f, 0.3f, 0.7f))) { Seat->SetActorEnableCollision(false); }
	}

	// Delivery addresses: teacher housing
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector P = C + FVector(-2600.f + i * 5200.f, 1400.f, 0.f);
		SpawnBox(P + FVector(0.f, 0.f, 320.f), FVector(4.5f, 4.5f, 6.4f), FLinearColor(0.62f, 0.58f, 0.50f));
		FCigAddress Addr;
		Addr.Pos = P + FVector(0.f, -300.f, 0.f);
		Addr.Label = FString::Printf(TEXT("Okul Sk. No:%d"), i + 1);
		D.PendingAddresses.Add(Addr);
	}
}

void UCigWorldBuilder::BuildDepot(FCigDistrictState& D)
{
	const FVector C = D.Center;
	SpawnWorldText(C + FVector(0.f, 1500.f, 560.f), CigDistrictDef(D.Id).Name, 84.f, FColor(255, 190, 70), 180.f);

	// Asphalt yard
	if (AStaticMeshActor* Yard = SpawnBox(C + FVector(0.f, 0.f, -4.f), FVector(30.f, 18.f, 0.06f), FLinearColor(0.26f, 0.26f, 0.27f))) { Yard->SetActorEnableCollision(false); }

	// Two warehouses
	for (float X : { -1600.f, 1600.f })
	{
		const FVector P = C + FVector(X, -900.f, 0.f);
		SpawnBox(P + FVector(0.f, 0.f, 300.f), FVector(13.f, 8.f, 6.f), FLinearColor(0.58f, 0.58f, 0.60f));
		if (AStaticMeshActor* Roof = SpawnBox(P + FVector(0.f, 0.f, 615.f), FVector(13.6f, 8.6f, 0.3f), FLinearColor(0.35f, 0.36f, 0.38f))) { Roof->SetActorEnableCollision(false); }
		// Loading door
		if (AStaticMeshActor* Gate = SpawnBox(P + FVector(0.f, 404.f, 150.f), FVector(4.f, 0.1f, 3.f), FLinearColor(0.30f, 0.32f, 0.36f))) { Gate->SetActorEnableCollision(false); }
		// Ramp
		if (AStaticMeshActor* Ramp = SpawnBox(P + FVector(0.f, 520.f, 30.f), FVector(4.f, 1.6f, 0.6f), FLinearColor(0.40f, 0.40f, 0.42f))) { Ramp->SetActorEnableCollision(true); }
	}

	// Barrels and crates
	for (int32 i = 0; i < 10; ++i)
	{
		SpawnProp(CigMesh::Food(TEXT("barrel")), C + FVector(-2400.f + i * 520.f, 500.f + FMath::FRandRange(-120.f, 120.f), 0.f), 95.f, FMath::FRandRange(0.f, 360.f), FLinearColor(0.45f, 0.32f, 0.18f));
	}
	for (int32 i = 0; i < 6; ++i)
	{
		SpawnBox(C + FVector(-1200.f + i * 400.f, 1100.f, 60.f), FVector(1.1f, 1.1f, 1.1f), FLinearColor(0.55f, 0.42f, 0.26f));
	}

	// Truck: cab and trailer
	const FVector Truck = C + FVector(2600.f, 700.f, 0.f);
	SpawnBox(Truck + FVector(0.f, 0.f, 140.f), FVector(2.2f, 4.6f, 2.6f), FLinearColor(0.75f, 0.75f, 0.78f));
	SpawnBox(Truck + FVector(0.f, -560.f, 110.f), FVector(2.f, 2.f, 2.f), FLinearColor(0.20f, 0.35f, 0.65f));
	for (float DY : { -520.f, 120.f, 420.f })
	{
		for (float DX : { -110.f, 110.f })
		{
			if (AStaticMeshActor* Wheel = SpawnBox(Truck + FVector(DX, DY, 35.f), FVector(0.35f, 0.7f, 0.7f), FLinearColor(0.08f, 0.08f, 0.09f), CylinderMesh))
			{
				Wheel->SetActorRotation(FRotator(0.f, 0.f, 90.f));
				Wheel->SetActorEnableCollision(false);
			}
		}
	}

	FCigAddress Addr;
	Addr.Pos = C + FVector(-1600.f, -380.f, 0.f);
	Addr.Label = TEXT("Sanayi Sitesi Depo:1");
	D.PendingAddresses.Add(Addr);
}

void UCigWorldBuilder::BuildStadium(FCigDistrictState& D)
{
	const FVector C = D.Center;
	SpawnWorldText(C + FVector(0.f, 3100.f, 900.f), CigDistrictDef(D.Id).Name, 120.f, FColor(255, 190, 70), 180.f);

	// Pitch
	if (AStaticMeshActor* Pitch = SpawnBox(C + FVector(0.f, 0.f, -4.f), FVector(22.f, 15.f, 0.06f), FLinearColor(0.14f, 0.45f, 0.16f))) { Pitch->SetActorEnableCollision(false); }
	if (AStaticMeshActor* Circle = SpawnBox(C + FVector(0.f, 0.f, 0.f), FVector(3.f, 3.f, 0.05f), FLinearColor(0.85f, 0.9f, 0.85f), CylinderMesh)) { Circle->SetActorEnableCollision(false); }

	// Ring of stands
	for (int32 i = 0; i < 20; ++i)
	{
		const float Ang = i * (2.f * PI / 20.f);
		const FVector P = C + FVector(FMath::Cos(Ang) * 2650.f, FMath::Sin(Ang) * 2000.f, 0.f);
		if (AStaticMeshActor* Stand = SpawnBox(P + FVector(0.f, 0.f, 180.f), FVector(4.2f, 4.2f, 3.6f), FLinearColor(0.60f, 0.60f, 0.64f)))
		{
			Stand->SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
		}
		// Band of seats (team colours)
		const FLinearColor SeatColor = (i % 2 == 0) ? FLinearColor(0.70f, 0.10f, 0.10f) : FLinearColor(0.95f, 0.85f, 0.20f);
		if (AStaticMeshActor* SeatRow = SpawnBox(P * 0.94f + C * 0.06f + FVector(0.f, 0.f, 380.f), FVector(3.6f, 3.6f, 0.4f), SeatColor)) { SeatRow->SetActorEnableCollision(false); }
	}

	// Floodlight masts
	for (int32 i = 0; i < 4; ++i)
	{
		const float SX = (i % 2 == 0) ? -1.f : 1.f;
		const float SY = (i < 2) ? -1.f : 1.f;
		const FVector P = C + FVector(SX * 3100.f, SY * 2400.f, 0.f);
		if (AStaticMeshActor* Mast = SpawnBox(P + FVector(0.f, 0.f, 800.f), FVector(0.4f, 0.4f, 16.f), FLinearColor(0.35f, 0.36f, 0.38f), CylinderMesh)) { Mast->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Rig = SpawnBox(P + FVector(0.f, 0.f, 1650.f), FVector(3.2f, 0.5f, 1.6f), FLinearColor(0.95f, 0.95f, 0.85f))) { Rig->SetActorEnableCollision(false); }
	}
}

void UCigWorldBuilder::BuildSeaside(FCigDistrictState& D)
{
	const FVector C = D.Center;
	SpawnWorldText(C + FVector(0.f, -1900.f, 560.f), CigDistrictDef(D.Id).Name, 100.f, FColor(255, 190, 70), 0.f);

	// Sea, sand and promenade
	if (AStaticMeshActor* Sea = SpawnBox(C + FVector(0.f, 4200.f, -12.f), FVector(34.f, 30.f, 0.08f), FLinearColor(0.08f, 0.35f, 0.62f))) { Sea->SetActorEnableCollision(false); }
	if (AStaticMeshActor* Sand = SpawnBox(C + FVector(0.f, 1500.f, -4.f), FVector(34.f, 8.f, 0.06f), FLinearColor(0.85f, 0.78f, 0.55f))) { Sand->SetActorEnableCollision(false); }
	if (AStaticMeshActor* Walk = SpawnBox(C + FVector(0.f, 500.f, -2.f), FVector(34.f, 5.f, 0.06f), FLinearColor(0.62f, 0.60f, 0.56f))) { Walk->SetActorEnableCollision(false); }

	// Railing
	for (int32 i = 0; i < 24; ++i)
	{
		const FVector P = C + FVector(-3300.f + i * 290.f, 900.f, 0.f);
		if (AStaticMeshActor* Post = SpawnBox(P + FVector(0.f, 0.f, 55.f), FVector(0.1f, 0.1f, 1.1f), FLinearColor(0.75f, 0.75f, 0.78f), CylinderMesh)) { Post->SetActorEnableCollision(false); }
	}
	if (AStaticMeshActor* Rail = SpawnBox(C + FVector(0.f, 900.f, 110.f), FVector(34.f, 0.1f, 0.1f), FLinearColor(0.75f, 0.75f, 0.78f))) { Rail->SetActorEnableCollision(false); }

	// Palm trees
	for (int32 i = 0; i < 7; ++i)
	{
		const FVector P = C + FVector(-2700.f + i * 900.f, 200.f, 0.f);
		if (AStaticMeshActor* Trunk = SpawnBox(P + FVector(0.f, 0.f, 190.f), FVector(0.28f, 0.28f, 3.8f), FLinearColor(0.42f, 0.30f, 0.16f), CylinderMesh)) { Trunk->SetActorEnableCollision(false); }
		for (int32 f = 0; f < 5; ++f)
		{
			const float Ang = f * (2.f * PI / 5.f);
			if (AStaticMeshActor* Frond = SpawnBox(P + FVector(FMath::Cos(Ang) * 110.f, FMath::Sin(Ang) * 110.f, 400.f), FVector(1.5f, 0.5f, 0.12f), FLinearColor(0.15f, 0.48f, 0.18f)))
			{
				Frond->SetActorRotation(FRotator(-18.f, FMath::RadiansToDegrees(Ang), 0.f));
				Frond->SetActorEnableCollision(false);
			}
		}
	}

	// Benches and parasols
	for (int32 i = 0; i < 5; ++i)
	{
		SpawnProp(PreferPark(TEXT("Props"), TEXT("SM_Bench01_1"), CigMesh::Furniture(TEXT("bench"))), C + FVector(-2400.f + i * 1200.f, 620.f, 0.f), 90.f, 180.f, FLinearColor(0.45f, 0.30f, 0.16f), true);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector P = C + FVector(-1800.f + i * 1200.f, 1500.f, 0.f);
		if (AStaticMeshActor* Pole = SpawnBox(P + FVector(0.f, 0.f, 110.f), FVector(0.09f, 0.09f, 2.2f), FLinearColor(0.8f, 0.8f, 0.82f), CylinderMesh)) { Pole->SetActorEnableCollision(false); }
		if (AStaticMeshActor* Top = SpawnBox(P + FVector(0.f, 0.f, 250.f), FVector(2.4f, 2.4f, 1.f), (i % 2 == 0) ? FLinearColor(0.9f, 0.35f, 0.25f) : FLinearColor(0.25f, 0.55f, 0.85f), ConeMesh)) { Top->SetActorEnableCollision(false); }
	}

	// Delivery addresses along the promenade
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector P = C + FVector(-2000.f + i * 4000.f, -800.f, 0.f);
		SpawnBox(P + FVector(0.f, 0.f, 500.f), FVector(5.f, 5.f, 10.f), FLinearColor(0.90f, 0.88f, 0.82f));
		FCigAddress Addr;
		Addr.Pos = P + FVector(0.f, 300.f, 0.f);
		Addr.Label = FString::Printf(TEXT("Kordon Blok No:%d"), i + 1);
		D.PendingAddresses.Add(Addr);
	}
}

void UCigWorldBuilder::RefreshUnlocks(int32 Level, bool bAnnounce)
{
	// --- Stations ---
	for (const TPair<ECigStation, TWeakObjectPtr<ACigkofteStation>>& Pair : Stations)
	{
		ACigkofteStation* S = Pair.Value.Get();
		if (!S)
		{
			continue;
		}
		const int32 Required = CigStationUnlockLevel(Pair.Key);
		const bool bWasLocked = S->IsLocked();
		const bool bShouldLock = Level < Required;
		S->SetLocked(bShouldLock, Required);

		if (bAnnounce && bWasLocked && !bShouldLock)
		{
			const FString OpenName = CigStationUnlockName(Pair.Key);
			if (GM && !OpenName.IsEmpty())
			{
				GM->AddMessage(CigText::Format(TEXT("level.stationopen"), *OpenName), FLinearColor(0.45f, 1.f, 0.55f));
			}
			SpawnFloatText(S->GetActorLocation() + FVector(0.f, 0.f, 210.f), CigText::Get(TEXT("level.opened")), FColor(120, 255, 140), 40.f);
		}
	}

	// --- Neighbourhood districts ---
	for (FCigDistrictState& D : Districts)
	{
		const FCigDistrictDef& Def = CigDistrictDef(D.Id);
		if (D.bOpen || Level < Def.Level)
		{
			// Still shut. Its sign says "LEVEL N", which is the one piece of world
			// text built from a template, so it is also the one piece that goes
			// stale when the language changes - and this function is called on a
			// settings change for exactly that reason.
			if (AActor* Sign = D.GateLevelSign.Get())
			{
				if (UTextRenderComponent* T = Sign->FindComponentByClass<UTextRenderComponent>())
				{
					T->SetText(FText::FromString(CigText::Format(TEXT("level.locked"), Def.Level)));
				}
			}
			continue;
		}
		D.bOpen = true;

		for (const TWeakObjectPtr<AActor>& Weak : D.GateActors)
		{
			if (AActor* A = Weak.Get())
			{
				A->Destroy();
			}
		}
		D.GateActors.Reset();

		Addresses.Append(D.PendingAddresses);
		D.PendingAddresses.Reset();

		SpawnPedestrians(D.Center, D.PedRadius, D.PedCount);

		if (bAnnounce && GM)
		{
			GM->AddMessage(FString::Printf(TEXT("YENİ BÖLGE AÇILDI: %s — %s"), Def.Name, Def.Teaser), FLinearColor(1.f, 0.85f, 0.35f));
		}
	}
}

void UCigWorldBuilder::UpdateSun(float T)
{
	// --- Weather: read from the active event ---
	int32 DesiredWeather = 0;
	bool bDesiredPowerOut = false;
	if (GM && GM->Events)
	{
		if (GM->Events->IsEventActive(UCigEventSystem::EventRain))
		{
			DesiredWeather = 1;
		}
		else if (GM->Events->IsEventActive(UCigEventSystem::EventHeat))
		{
			DesiredWeather = 2;
		}
		bDesiredPowerOut = GM->Events->IsEventActive(UCigEventSystem::EventPowerOut);
	}
	if (DesiredWeather != Weather)
	{
		Weather = DesiredWeather;
		// The ground darkens and looks wet in the rain
		if (GroundActor)
		{
			const FLinearColor Dry(0.30f, 0.32f, 0.28f);
			const FLinearColor Wet(0.17f, 0.19f, 0.20f);
			const FLinearColor Baked(0.38f, 0.37f, 0.28f);
			const FLinearColor Use = (Weather == 1) ? Wet : (Weather == 2 ? Baked : Dry);
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(GroundActor->GetStaticMeshComponent()->GetMaterial(0)))
			{
				MID->SetVectorParameterValue(TEXT("Color"), Use);
			}
		}
	}
	bPowerOut = bDesiredPowerOut;

	// --- Sun: drops from morning to evening, reddens towards dusk ---
	Evening = FMath::Clamp((T - 0.60f) / 0.40f, 0.f, 1.f);
	if (Sun)
	{
		const float Pitch = -18.f - 46.f * FMath::Sin(PI * T);
		const float Yaw = 30.f + 120.f * T;
		Sun->SetActorRotation(FRotator(Pitch, Yaw, 0.f));

		FLinearColor SunColor = FMath::Lerp(FLinearColor(1.f, 0.98f, 0.92f), FLinearColor(1.f, 0.42f, 0.18f), Evening);
		float SunIntensity = FMath::Lerp(6.f, 0.55f, Evening);
		if (Weather == 1)
		{
			SunColor = FMath::Lerp(SunColor, FLinearColor(0.62f, 0.68f, 0.78f), 0.75f);
			SunIntensity *= 0.45f;
		}
		else if (Weather == 2)
		{
			SunColor = FMath::Lerp(SunColor, FLinearColor(1.f, 0.88f, 0.62f), 0.6f);
			SunIntensity *= 1.15f;
		}
		Sun->GetLightComponent()->SetLightColor(SunColor);
		Sun->GetLightComponent()->SetIntensity(SunIntensity);
	}

	// --- Sky light: off at night, dimmed in the rain ---
	if (SkyLightActor)
	{
		float SkyIntensity = FMath::Lerp(1.f, 0.16f, Evening);
		if (Weather == 1)
		{
			SkyIntensity *= 0.6f;
		}
		SkyLightActor->GetLightComponent()->SetIntensity(SkyIntensity);
	}

	// --- Shop lights: bright in the evening, out during a power cut ---
	const float ShopIntensity = bPowerOut ? 0.f : FMath::Lerp(4500.f, 16000.f, Evening);
	for (const TObjectPtr<APointLight>& L : ShopLights)
	{
		if (L)
		{
			L->GetLightComponent()->SetIntensity(ShopIntensity);
		}
	}

	// The counter keeps its lead over the room at every hour, and goes out with
	// everything else in a cut - a shop lit only over the counter during a power
	// failure would read as the one light that still works, which is a different
	// scene from the one the event is describing.
	if (CounterLight)
	{
		CounterLight->GetLightComponent()->SetIntensity(bPowerOut ? 0.f : ShopIntensity * 1.6f);
	}

	// --- Doorway daylight: follows the sun, ignores the power cut ---
	// It is strongest at midday and nearly gone by dusk, which is also when the
	// shop bulbs come up - so the room shifts from cool-and-warm to warm-only
	// over the day instead of holding one look. A cut does not touch it.
	if (DoorLight)
	{
		float DoorIntensity = FMath::Lerp(5000.f, 250.f, Evening);
		if (Weather == 1)
		{
			DoorIntensity *= 0.5f; // rain: less light through the door
		}
		DoorLight->GetLightComponent()->SetIntensity(DoorIntensity);
	}

	// --- Street lamps: come on towards dusk ---
	const FLinearColor BulbOff(0.30f, 0.29f, 0.24f);
	const FLinearColor BulbOn(2.4f, 2.1f, 1.25f);
	const FLinearColor BulbColor = bPowerOut ? FLinearColor(0.10f, 0.10f, 0.11f) : FMath::Lerp(BulbOff, BulbOn, FMath::Clamp((Evening - 0.25f) / 0.35f, 0.f, 1.f));
	for (const TObjectPtr<AStaticMeshActor>& Bulb : StreetBulbs)
	{
		if (!Bulb)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Bulb->GetStaticMeshComponent()->GetMaterial(0)))
		{
			MID->SetVectorParameterValue(TEXT("Color"), BulbColor);
		}
	}
}

void UCigWorldBuilder::SetHouseOwned()
{
	if (HouseSign)
	{
		HouseSign->Destroy();
		HouseSign = nullptr;
	}
	SpawnWorldText(HousePos + FVector(-300.f, 0.f, 620.f), TEXT("EVIN (gunluk +150 TL)"), 50.f, FColor(100, 255, 120), 180.f);
}

void UCigWorldBuilder::ApplyUpgradeVisual(int32 UpgradeIndex)
{
	switch ((ECigUpgrade)UpgradeIndex)
	{
	case ECigUpgrade::BuyukBuzdolabi:
		SpawnBox(FVector(870.f, -780.f, 110.f), FVector(0.9f, 0.9f, 2.2f), FLinearColor(0.85f, 0.90f, 0.96f));
		break;
	case ECigUpgrade::IkinciYogurma:
		SpawnBox(FVector(250.f, -450.f, 55.f), FVector(1.0f, 1.0f, 1.1f), FLinearColor(0.72f, 0.72f, 0.76f));
		SpawnWorldText(FVector(250.f, -450.f, 170.f), TEXT("YOGURMA 2"), 24.f, FColor::White, 180.f);
		break;
	case ECigUpgrade::HizliDograma:
		SpawnBox(FVector(600.f, 900.f, 60.f), FVector(0.8f, 0.5f, 1.2f), FLinearColor(0.7f, 0.72f, 0.75f));
		break;
	case ECigUpgrade::YeniTabela:
		SpawnWorldText(FVector(-740.f, 0.f, 540.f), TEXT("* MAHALLENIN EN IYISI *"), 56.f, FColor(255, 220, 90), 180.f);
		break;
	case ECigUpgrade::Klima:
		SpawnBox(FVector(980.f, 500.f, 330.f), FVector(1.4f, 0.5f, 0.5f), FLinearColor(0.9f, 0.92f, 0.95f));
		break;
	case ECigUpgrade::MuzikSistemi:
		SpawnBox(FVector(980.f, -500.f, 330.f), FVector(0.5f, 0.7f, 0.8f), FLinearColor(0.12f, 0.12f, 0.14f));
		SpawnWorldText(FVector(940.f, -500.f, 380.f), TEXT("MUZIK"), 22.f, FColor(180, 200, 255), 180.f);
		break;
	case ECigUpgrade::IkinciKasa:
		SpawnBox(FVector(-600.f, 450.f, 55.f), FVector(0.9f, 0.9f, 1.1f), FLinearColor(0.50f, 0.30f, 0.15f));
		SpawnWorldText(FVector(-640.f, 450.f, 170.f), TEXT("KASA 2"), 24.f, FColor::White, 0.f);
		break;
	case ECigUpgrade::DisOturma:
		for (float Y : { -350.f, 350.f })
		{
			if (AStaticMeshActor* Leg = SpawnBox(FVector(-950.f, Y, 45.f), FVector(0.14f, 0.14f, 0.9f), FLinearColor(0.25f, 0.18f, 0.10f), CylinderMesh)) { Leg->SetActorEnableCollision(false); }
			if (AStaticMeshActor* Table = SpawnBox(FVector(-950.f, Y, 95.f), FVector(0.9f, 0.9f, 0.07f), FLinearColor(0.55f, 0.38f, 0.20f), CylinderMesh)) { Table->SetActorEnableCollision(false); }
			if (AStaticMeshActor* Stool = SpawnBox(FVector(-1030.f, Y + 60.f, 25.f), FVector(0.4f, 0.4f, 0.5f), FLinearColor(0.4f, 0.28f, 0.15f))) { Stool->SetActorEnableCollision(false); }
		}
		break;
	case ECigUpgrade::BuyukCop:
		if (ACigkofteStation* Cop = FindStation(ECigStation::Cop))
		{
			Cop->SetActorScale3D(Cop->GetActorScale3D() * 1.35f);
		}
		break;
	case ECigUpgrade::HijyenEkipmani:
		SpawnBox(FVector(250.f, 500.f, 90.f), FVector(0.35f, 0.35f, 0.5f), FLinearColor(0.2f, 0.8f, 0.85f), CylinderMesh);
		break;
	case ECigUpgrade::IyiLavabo:
		if (ACigkofteStation* Lavabo = FindStation(ECigStation::Lavabo))
		{
			SpawnBox(Lavabo->GetActorLocation() + FVector(0.f, 0.f, 130.f), FVector(0.2f, 0.5f, 0.2f), FLinearColor(0.75f, 0.78f, 0.82f));
		}
		break;
	case ECigUpgrade::Dekorasyon:
		for (float Y = -900.f; Y <= 900.f; Y += 450.f)
		{
			if (AStaticMeshActor* Pot = SpawnBox(FVector(940.f, Y, 40.f), FVector(0.35f, 0.35f, 0.8f), FLinearColor(0.5f, 0.3f, 0.15f), CylinderMesh)) { Pot->SetActorEnableCollision(false); }
			if (AStaticMeshActor* Flower = SpawnBox(FVector(940.f, Y, 105.f), FVector(0.5f, 0.5f, 0.4f), FLinearColor(0.9f, 0.4f + FMath::FRandRange(0.f, 0.4f), 0.3f), SphereMesh)) { Flower->SetActorEnableCollision(false); }
		}
		break;
	case ECigUpgrade::YeniSube:
		SpawnBox(FVector(-1000.f, -1800.f, 200.f), FVector(4.f, 4.f, 4.f), FLinearColor(0.9f, 0.6f, 0.3f));
		SpawnWorldText(FVector(-1210.f, -1800.f, 430.f), TEXT("CIGKOFTECI SUBE 2"), 46.f, FColor(255, 140, 40), 180.f);
		break;
	default:
		break;
	}
}
