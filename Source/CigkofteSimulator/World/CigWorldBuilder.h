#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "Core/CigUnlocks.h"
#include "CigWorldBuilder.generated.h"

class ACigkofteStation;
class AStaticMeshActor;
class ADirectionalLight;
class APointLight;
class ASkyLight;
class UStaticMesh;
class UMaterialInterface;
class ACigCar;
class ACigCat;

// A delivery address: one door in the city.
struct FCigAddress
{
	FVector Pos = FVector::ZeroVector;
	FString Label;
};

// Builds the shop and the city entirely from code, and applies upgrade visuals.
UCLASS()
class UCigWorldBuilder : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;
	virtual void UpdateSystem(float DeltaSeconds) override;

	void BuildWorld();

	// --- Spawn helpers (other systems use these too) ---
	// MaterialOverride replaces the flat colour entirely - pass one for a
	// surface that should read as a material (tiled wall, brick) rather than as
	// a painted box. Colour is ignored when it is set.
	AStaticMeshActor* SpawnBox(const FVector& Loc, const FVector& Scale, const FLinearColor& Color, UStaticMesh* Mesh = nullptr,
		UMaterialInterface* MaterialOverride = nullptr);

	// Surfaces from ModularBuildingSet, or null without the pack. Loaded once in
	// OnInit; a missing one just leaves the painted box it replaces.
	UPROPERTY() TObjectPtr<UMaterialInterface> WallTileMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> BrickMaterial;

	// Places an imported low poly mesh, scaling it from its bounds to the target
	// height. With a null mesh (no asset) it falls back to a FallbackColor box.
	AStaticMeshActor* SpawnProp(UStaticMesh* Mesh, const FVector& Loc, float TargetHeight, float Yaw,
		const FLinearColor& FallbackColor, bool bCollision = false);
	ACigkofteStation* SpawnStation(ECigStation Type, const FVector& Loc, const FLinearColor& Color, const FString& Label, float LabelYaw);

	// Tries the restaurant pack first, then falls back to the given Kenney mesh.
	// With neither it returns nullptr and SpawnProp draws a primitive box.
	UStaticMesh* PreferDukkan(const TCHAR* DukkanName, UStaticMesh* KenneyFallback) const;

	// The same for the CityPark pack (buildings, trees, benches, fountain, cafe furniture).
	UStaticMesh* PreferPark(const TCHAR* Sub, const TCHAR* ParkName, UStaticMesh* KenneyFallback) const;
	AActor* SpawnWorldText(const FVector& Loc, const FString& Text, float Size, const FColor& Color, float Yaw);
	void SpawnFloatText(const FVector& Loc, const FString& Text, const FColor& Color, float Size = 34.f);

	// Rotates the sun with the day's progress, turns on the evening lights and
	// reflects the active weather event (rain, heat, power cut) into the world.
	// T: 0 morning, 1 evening.
	void UpdateSun(float T);

	// --- Atmosphere (the HUD reads this too) ---
	int32 Weather = 0;        // 0 clear, 1 rainy, 2 scorching
	float Evening = 0.f;      // 0 daytime .. 1 evening gloom
	bool bPowerOut = false;

	// Builds the world visual for a purchased shop upgrade.
	void ApplyUpgradeVisual(int32 UpgradeIndex);

	void SetHouseOwned();

	// Updates station and district locks for a level. With bAnnounce=true the
	// newly opened ones are posted to the message feed (used on level-up).
	void RefreshUnlocks(int32 Level, bool bAnnounce);

	ACigkofteStation* FindStation(ECigStation Type) const;

	// Switches every station label between signage and the old overhead debug
	// text. Follows the debug HUD, so one key controls both.
	void SetStationLabelsDebug(bool bDebug);

	// Signage is hidden past this distance.
	//
	// The counters stand in rows 270uu apart in depth, so from anywhere back in
	// the room the far row's names project on top of the near row's - "BULGUR"
	// across the fridge, "BAHARAT" across "BULASIK". Legible while working at a
	// counter, illegible in every wide shot, which is what a store page is made
	// of. A label is there to name the thing you are standing at; at 700uu the
	// nearest row is named and the back wall is quiet.
	static constexpr float LabelVisibleRange = 700.f;

	// A locked district: the barrier actors are destroyed on unlock and its
	// delivery addresses join the address pool at that moment.
	struct FCigDistrictState
	{
		ECigDistrict Id = ECigDistrict::SemtPazari;
		FVector Center = FVector::ZeroVector;
		bool bOpen = false;
		int32 PedCount = 0;      // pedestrians to bring to life on unlock
		float PedRadius = 1600.f;
		TArray<TWeakObjectPtr<AActor>> GateActors;
		TArray<FCigAddress> PendingAddresses;
	};
	TArray<FCigDistrictState> Districts;

	// --- Shared resources ---
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> ConeMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY() TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY() TObjectPtr<ASkyLight> SkyLightActor;
	UPROPERTY() TArray<TObjectPtr<APointLight>> ShopLights;      // bright in the evening, out during a cut
	// Warm key over the counter. The four shop lights are spread evenly around
	// the room, which lights it but says nothing about where to look; this one
	// puts the brightest thing in the frame on the food and the hands making it.
	// Goes out with the others in a power cut.
	UPROPERTY() TObjectPtr<APointLight> CounterLight;
	// Daylight coming in at the doorway. Cool, and the only cool light in the
	// shop - it is what makes the interior read as warm by comparison rather
	// than just as orange. Fades out as the day does, and is unaffected by a
	// power cut, being the sun.
	UPROPERTY() TObjectPtr<APointLight> DoorLight;
	UPROPERTY() TArray<TObjectPtr<AStaticMeshActor>> StreetBulbs; // street lamp globes
	UPROPERTY() TObjectPtr<AStaticMeshActor> GroundActor;         // goes wet in the rain
	UPROPERTY() TObjectPtr<AActor> HouseSign;
	FVector HousePos = FVector::ZeroVector;

	TArray<FCigAddress> Addresses;

	// Positions of the rival shop signs (for reports and messages).
	TArray<FVector> RivalShopPos;

	// Seating: chair position plus the yaw to face once seated. Read by the customer system.
	struct FCigSeat
	{
		FVector Pos = FVector::ZeroVector;
		float Yaw = 0.f;
		bool bOccupied = false;
	};
	TArray<FCigSeat> Seats;

	// Note: not a UPROPERTY because the key is a non-UENUM enum; the stations
	// live in the level, so there is no GC risk.
	TMap<ECigStation, TWeakObjectPtr<ACigkofteStation>> Stations;

	// Finds a free chair and claims it; -1 if there is none.
	int32 ReserveSeat();
	void ReleaseSeat(int32 Index);

private:
	// Signage visibility is re-evaluated on a clock, not every frame: it is a
	// distance test across twenty stations that nothing perceives at 60 Hz, and
	// the label only has to catch up with a walking player.
	float LabelRangeTimer = 0.f;
	static constexpr float LabelRangeInterval = 0.25f;
	bool bStationLabelsDebug = false;
	void UpdateStationLabelRange();

	void BuildKitchen();
	void BuildFurniture();
	void BuildSeatingArea();
	void BuildCity();
	void BuildRivalShops();
	void SpawnLights();

	// --- Districts (the open world that unlocks by level) ---
	void BuildDistricts();
	void BuildDistrictRoads();
	FCigDistrictState& AddDistrict(ECigDistrict Id, const FVector& Center);
	// Puts a barrier and sign at a district entrance, on the horizontal axis
	// (bHorizontal) or the vertical one.
	void BuildGate(FCigDistrictState& D, const FVector& Loc, bool bHorizontal);
	void BuildBazaar(FCigDistrictState& D);
	void BuildSquare(FCigDistrictState& D);
	void BuildSchoolPark(FCigDistrictState& D);
	void BuildDepot(FCigDistrictState& D);
	void BuildStadium(FCigDistrictState& D);
	void BuildSeaside(FCigDistrictState& D);
	void SpawnPedestrians(const FVector& Center, float Radius, int32 Count);
};
