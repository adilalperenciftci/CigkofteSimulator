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
	// Strength 0-1: how hard the batch reacts to a stroke.
	//
	// The kneading rhythm has always been the mechanic - a stroke landing between
	// 0.25 s and 0.85 s after the last one is worth nearly twice a mistimed one -
	// and it has always been invisible. The player pressed, the bar moved by an
	// amount they could not account for, and nothing on the counter said why. The
	// dough now answers the stroke it was given.
	void PulseDough(float Strength = 1.f);

	// Briefly swells the top on any interaction, then settles (game feel).
	void Pop();

	// The top glows slightly when looked at (interaction feedback).
	void SetHighlighted(bool bOn);

	// Level lock: while closed the station stays pale grey, its label reads
	// "SEVIYE N" and interaction is refused. It pops briefly on unlock.
	void SetLocked(bool bLock, int32 RequiredLevel);

	// Rewrites the label from the current language. Called by SetLocked whether or
	// not the lock moved, because a language change moves the wording without
	// moving the lock.
	void RefreshLockLabel();

	// The signage text for the current language, or the key itself if the table
	// has no row for it.
	FString ResolvedLabel() const;

	bool IsLocked() const { return bLocked; }
	int32 GetLockLevel() const { return LockLevel; }

	FString GetPromptText() const;

	// The label is two things wearing one component. In play it is signage:
	// small, warm, close to the counter it names. With the debug HUD on it goes
	// back to what it was - large white capitals floating overhead, readable
	// from anywhere, which is what you want when you are checking a layout and
	// not what you want in a screenshot.
	void SetLabelDebug(bool bDebug);
	// Shows or hides the signage. Driven by distance from CigWorldBuilder, which
	// owns the reasoning; this end only does what it is told and skips the call
	// when nothing would change.
	void SetLabelVisible(bool bVisible);
	// The direction the signage reads from. Text drawn from behind is mirrored,
	// so the range check needs to know which side is the front.
	FVector LabelFacing() const;

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

	// The measure being lifted out of the tub. Only the ingredient stations have
	// one; everywhere else it stays hidden and costs nothing.
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Scoop;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ScoopMID;

	// The chopping board: one whole head and a fixed set of cut pieces.
	//
	// Pooled rather than spawned. A chop is four keypresses and a garnish is ten
	// of those, so spawning a fragment per stroke would allocate and destroy a
	// few hundred actors across a day for something that is never more than six
	// things on a board. They are built once and only ever shown or hidden.
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ChopPieces;
	static constexpr int32 ChopPieceCount = 6;

	// Done strokes out of Needed. The board shows a whole head at zero, loses it
	// to fragments as the count rises, and is swept when the garnish is finished.
	void SetChopState(int32 Done, int32 Needed);
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
	// The scoop's run, counting down. Its rest pose is kept so the motion can be
	// expressed as an offset from it rather than recomputed from the station
	// layout on every frame.
	float ScoopTime = 0.f;
	float ScoopRestZ = 0.f;
	FVector ScoopScale = FVector::OneVector;
	static constexpr float ScoopDuration = 0.55f;
	bool bHighlighted = false;
	bool bAlwaysTick = false;
	bool bLocked = false;
	int32 LockLevel = 1;
	FLinearColor TopColor = FLinearColor::White;
	FVector TopBaseScale = FVector::OneVector;
	// The label's text key, not its text.
	//
	// Resolved on every rewrite rather than stored, because the station's sign has
	// to follow a language change and Setup runs once at world build. Falls back to
	// treating the key as literal text, so anything not in the table still shows
	// what it was given.
	FString LabelKey;
	// Kept so the label can be repositioned later without re-deriving it.
	float LabelTopZ = 100.f;
	bool bLabelDebug = false;

	void ApplyDoughTransform();
	void UpdateTickState();

	// Picks the model for a station type, or nullptr when the pack is absent.
	static UStaticMesh* MeshForStation(ECigStation Type);
	// Fits the model inside the collision box the layout was built around and
	// hides the primitives. No-op without a mesh, which is what keeps the shop
	// standing on a machine that does not have the pack.
	// Overhang: how far the model may run past its footprint while still being
	// bounded exactly by the box's height. 1 means "fit inside the box", which is
	// right for everything that is not a counter.
	void ApplyStationMesh(UStaticMesh* Mesh, const FVector& BaseScale, float FootprintOverhang);
};
