#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CigkoftePlayerCharacter.generated.h"

class UCameraComponent;
class ACigkofteStation;
class ACigkofteGameMode;
class ACigCat;

UCLASS()
class ACigkoftePlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACigkoftePlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY() TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<ACigkofteStation> FocusedStation;
	// A delivery crate under the crosshair. Separate from the station so the two
	// prompts cannot both claim the screen.
	UPROPERTY() TObjectPtr<class ACigStockCrate> FocusedCrate;

	bool bDriving = false;
	bool bCarFocused = false;
	bool bCatFocused = false;

	// --- Hafif enerji sistemi ---
	float Energy = 100.f;
	float Stress = 0.f;
	float Thirst = 0.f;

	// Topping selection on a gamepad: LB/RB move through the rows, B toggles one.
	// 0-6 toppings, 7 ayran, 8 side. The HUD highlights this row.
	int32 GamepadWrapCursor = 0;
	bool bGamepadActive = false;

private:
	void ToggleCar();
	ACigkofteGameMode* GM() const;

	// Either a keyboard or a gamepad key: pressed / held.
	// Using a gamepad key switches the HUD prompts over to gamepad glyphs.
	bool Pressed(APlayerController* PC, const FKey& Key, const FKey& PadKey);
	bool Held(APlayerController* PC, const FKey& Key, const FKey& PadKey);

	void PollInput(APlayerController* PC);
	void PollTabletInput(APlayerController* PC, ACigkofteGameMode* Mode);
	void PollSettingsInput(APlayerController* PC, ACigkofteGameMode* Mode);
	// The key binding screen. While capturing, every key press goes to the binding.
	void PollKeybindInput(APlayerController* PC, ACigkofteGameMode* Mode);
	void PollGamepadLook(APlayerController* PC, float Sens);
	void PollGamepadMove(APlayerController* PC, float& Fwd, float& Right);
	void UpdateFocus();
	// Build mode's answer to UpdateFocus: which placement is under the crosshair.
	// The result is a value on the game mode rather than a pointer here, because
	// what build mode edits is the authority's record, not the actor that shows it.
	void UpdateBuildSelection();
	// How far build mode's selection trace reaches. Longer than the 450 the
	// interaction trace uses: laying out a room means pointing across it.
	static constexpr float BuildSelectReach = 1200.f;
	void UpdateHeadBob(float DeltaSeconds);
	void UpdateEnergy(float DeltaSeconds);

	float BobPhase = 0.f;
	float BobOffset = 0.f;
};
