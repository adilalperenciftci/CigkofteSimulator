#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APlayerController;

// Rebindable game actions.
//
// The project does not use EnhancedInput - input is polled straight off the
// PlayerController (PC->IsInputKeyDown(EKeys::W)). That hard-coded the keys and
// left the player no way to change them. This thin layer sits in between: the
// action name is fixed, the key bound to it is data.
//
// Scope: gameplay actions are rebindable. Menu navigation keys (arrows, Enter,
// Esc) are deliberately fixed, so a player cannot lock themselves inside a menu
// with no way back to the settings.
enum class ECigAction : uint8
{
	MoveForward = 0,
	MoveBack,
	MoveLeft,
	MoveRight,
	Run,
	Jump,
	Interact,     // station / car / cat / delivery
	Knead,        // one kneading beat
	Wrap,         // roll the wrap
	Shelf,        // move a rolled wrap onto the shelf
	Tablet,
	Settings,
	Pause,
	COUNT
};

namespace CigInput
{
	// Human-readable action name (shown on the settings screen). From Config/Text.
	FString ActionName(ECigAction Action);

	// The keys currently bound.
	FKey Key(ECigAction Action);
	FKey PadKey(ECigAction Action);

	// Rebinds. If the key is already used by another action it is taken away
	// from it - leaving two actions on one key would be a silent conflict.
	void SetKey(ECigAction Action, const FKey& NewKey);

	// Returns every binding to its factory setting.
	void ResetToDefaults();

	// Whether it differs from the default (for display on the settings screen).
	bool IsRemapped(ECigAction Action);

	// --- Queries: the caller never learns which key it is ---
	bool IsDown(const APlayerController* PC, ECigAction Action);
	bool WasPressed(const APlayerController* PC, ECigAction Action);
	bool WasReleased(const APlayerController* PC, ECigAction Action);

	// --- Saving ---
	// Keys are stored by name (FKey::GetFName), so a save stays readable even if
	// an engine update shuffles the numeric codes.
	TArray<FString> SaveBindings();
	void LoadBindings(const TArray<FString>& Names);
}
