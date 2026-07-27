#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/CigkofteTypes.h"
#include "Cooking/CigDoughVisual.h"
#include "CigkofteStation.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UCLASS()
class ACigkofteStation : public AActor
{
	GENERATED_BODY()

public:
	ACigkofteStation();

	virtual void Tick(float DeltaSeconds) override;

	// Sets mesh, colour and label from the type; called once after spawning.
	void Setup(ECigStation InType, const FLinearColor& Color, const FString& LabelText, float LabelYaw);

	// Kneading station: what the batch looks like now. Cheap to call every
	// stroke - it drops out when nothing visible has changed.
	void UpdateDough(const FCigDoughVisual& Visual);

	// Triggers the dough squash animation on a kneading stroke.
	void PulseDough();

	// Briefly swells the top on any interaction, then settles (game feel).
	void Pop();

	// The top glows slightly when looked at (interaction feedback).
	void SetHighlighted(bool bOn);

	// Level lock: while closed the station stays pale grey, its label reads
	// "SEVIYE N" and interaction is refused. It pops briefly on unlock.
	void SetLocked(bool bLock, int32 RequiredLevel);
	bool IsLocked() const { return bLocked; }
	int32 GetLockLevel() const { return LockLevel; }

	FString GetPromptText() const;

	ECigStation StationType = ECigStation::Bulgur;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Base;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Top;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Dough;

	// The real furniture, when the pack is installed. It is a separate component
	// rather than a mesh swap on Base for one reason: Base is the root and it
	// carries the interaction collision, sized in world units the shop layout
	// was built around. Swapping its mesh would resize that volume and move
	// where the player can stand. So the box stays, invisible, and the model
	// sits inside it with collision off.
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BaseMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TopMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DoughMID;

private:
	// The last state pushed in, and the colour last written to the material.
	// Kept so a repeated push costs nothing: the kneading station updates on
	// every stroke and every ingredient, and a SetVectorParameterValue that
	// writes the value already there still queues a render-thread command.
	FCigDoughVisual CurVisual;
	FLinearColor LastDoughColor = FLinearColor::Transparent;
	float Pulse = 0.f;
	float PopTime = 0.f;
	bool bHighlighted = false;
	bool bAlwaysTick = false;
	bool bLocked = false;
	int32 LockLevel = 1;
	FLinearColor TopColor = FLinearColor::White;
	FVector TopBaseScale = FVector::OneVector;
	FString LabelBaseText;

	void ApplyDoughTransform();
	void UpdateTickState();

	// Picks the model for a station type, or nullptr when the pack is absent.
	static UStaticMesh* MeshForStation(ECigStation Type);
	// Fits the model inside the collision box the layout was built around and
	// hides the primitives. No-op without a mesh, which is what keeps the shop
	// standing on a machine that does not have the pack.
	void ApplyStationMesh(UStaticMesh* Mesh, const FVector& BaseScale);
};
