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
};
