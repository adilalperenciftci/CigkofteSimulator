#include "Player/CigkoftePlayerCharacter.h"
#include "Core/CigInput.h"
#include "Game/CigkofteGameMode.h"
#include "UI/CigTabletWidget.h"
#include "Game/CigDaySystem.h"
#include "World/CigkofteStation.h"
#include "World/CigWorldBuilder.h"
#include "Placement/CigPlacementSystem.h"
#include "Orders/CigOrderSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Cat/CigCat.h"
#include "Cat/CigCatSystem.h"
#include "Delivery/CigDeliverySystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Inventory/CigStockCrate.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Vehicles/CigCar.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ACigkoftePlayerCharacter::ACigkoftePlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	// So the pause key (P) is still caught while the game is paused.
	PrimaryActorTick.bTickEvenWhenPaused = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 88.f);
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->JumpZVelocity = 450.f;
	GetCharacterMovement()->AirControl = 0.3f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 66.f));
	Camera->bUsePawnControlRotation = true;
}

ACigkofteGameMode* ACigkoftePlayerCharacter::GM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACigkofteGameMode>() : nullptr;
}

bool ACigkoftePlayerCharacter::Pressed(APlayerController* PC, const FKey& Key, const FKey& PadKey)
{
	if (PadKey.IsValid() && PC->WasInputKeyJustPressed(PadKey))
	{
		bGamepadActive = true;
		return true;
	}
	return Key.IsValid() && PC->WasInputKeyJustPressed(Key);
}

bool ACigkoftePlayerCharacter::Held(APlayerController* PC, const FKey& Key, const FKey& PadKey)
{
	if (PadKey.IsValid() && PC->IsInputKeyDown(PadKey))
	{
		bGamepadActive = true;
		return true;
	}
	return Key.IsValid() && PC->IsInputKeyDown(Key);
}

void ACigkoftePlayerCharacter::PollGamepadLook(APlayerController* PC, float Sens)
{
	const float LookX = PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX);
	const float LookY = PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY);
	if (FMath::Abs(LookX) < 0.15f && FMath::Abs(LookY) < 0.15f)
	{
		return;
	}
	bGamepadActive = true;
	// Analog look scales with frame time (unlike a mouse delta, which is instant).
	const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const float Speed = 150.f * Sens * Delta;
	AddControllerYawInput(LookX * Speed);
	AddControllerPitchInput(LookY * Speed);
}

void ACigkoftePlayerCharacter::PollGamepadMove(APlayerController* PC, float& Fwd, float& Right)
{
	const float MoveX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const float MoveY = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	if (FMath::Abs(MoveY) > 0.2f)
	{
		Fwd += MoveY;
		bGamepadActive = true;
	}
	if (FMath::Abs(MoveX) > 0.2f)
	{
		Right += MoveX;
		bGamepadActive = true;
	}
}

void ACigkoftePlayerCharacter::PollTabletInput(APlayerController* PC, ACigkofteGameMode* Mode)
{
	// Renaming the shop, on the tab that shows its name.
	//
	// A key rather than a click: the game runs with the cursor hidden, so until
	// something hands the field focus there is no way to reach it at all. F2 is
	// the rename key everywhere else and is not one of the rebindable gameplay
	// actions, so it cannot collide with a player's own binding.
	if (Mode->TabletTab == ECigTabletTab::Dukkan && PC->WasInputKeyJustPressed(EKeys::F2))
	{
		if (Mode->TabletWidget)
		{
			Mode->TabletWidget->FocusShopName();
		}
		return;
	}

	// Tabs: arrow keys or shoulder buttons
	if (Pressed(PC, EKeys::Left, EKeys::Gamepad_LeftShoulder))
	{
		Mode->CycleTabletTab(-1);
	}
	if (Pressed(PC, EKeys::Right, EKeys::Gamepad_RightShoulder))
	{
		Mode->CycleTabletTab(1);
	}
	if (Pressed(PC, EKeys::Up, EKeys::Gamepad_DPad_Up))
	{
		Mode->TabletScrollBy(-1);
	}
	if (Pressed(PC, EKeys::Down, EKeys::Gamepad_DPad_Down))
	{
		Mode->TabletScrollBy(1);
	}
	// On a gamepad, A picks the first row; up/down scrolls the list.
	if (CigInput::WasPressed(PC, ECigAction::Interact))
	{
		Mode->TabletKey(1);
	}

	static const FKey NumKeys[10] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
	for (int32 i = 0; i < 10; ++i)
	{
		if (PC->WasInputKeyJustPressed(NumKeys[i]))
		{
			Mode->TabletKey(i == 9 ? 10 : i + 1);
		}
	}
}

void ACigkoftePlayerCharacter::PollKeybindInput(APlayerController* PC, ACigkofteGameMode* Mode)
{
	if (!PC || !Mode)
	{
		return;
	}

	// While capturing: the FIRST key pressed goes to the binding, not the game.
	if (Mode->bAwaitingKey)
	{
		// Scan every key the engine knows about to find the one pressed this
		// frame. EnhancedInput would provide a capture API; without it, this.
		static TArray<FKey> AllKeys;
		if (AllKeys.Num() == 0)
		{
			EKeys::GetAllKeys(AllKeys);
		}
		for (const FKey& K : AllKeys)
		{
			if (PC->WasInputKeyJustPressed(K) && Mode->KeybindCaptureKey(K))
			{
				break;
			}
		}
		return;
	}

	if (Pressed(PC, EKeys::Up, EKeys::Gamepad_DPad_Up))
	{
		Mode->KeybindNav(-1);
	}
	if (Pressed(PC, EKeys::Down, EKeys::Gamepad_DPad_Down))
	{
		Mode->KeybindNav(1);
	}
	if (Pressed(PC, EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom))
	{
		Mode->KeybindBeginCapture();
	}
	if (PC->WasInputKeyJustPressed(EKeys::R))
	{
		Mode->KeybindResetAll();
	}
	// Escape/O goes back to the settings screen.
	if (PC->WasInputKeyJustPressed(EKeys::Escape) || CigInput::WasPressed(PC, ECigAction::Settings))
	{
		Mode->ToggleKeybinds();
	}
}

void ACigkoftePlayerCharacter::PollSettingsInput(APlayerController* PC, ACigkofteGameMode* Mode)
{
	if (Pressed(PC, EKeys::Up, EKeys::Gamepad_DPad_Up))
	{
		Mode->SettingsNav(-1);
	}
	if (Pressed(PC, EKeys::Down, EKeys::Gamepad_DPad_Down))
	{
		Mode->SettingsNav(1);
	}
	if (Pressed(PC, EKeys::Left, EKeys::Gamepad_DPad_Left))
	{
		Mode->SettingsAdjust(-1);
	}
	if (Pressed(PC, EKeys::Right, EKeys::Gamepad_DPad_Right) || Pressed(PC, EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom))
	{
		Mode->SettingsAdjust(1);
	}
}

void ACigkoftePlayerCharacter::PollInput(APlayerController* PC)
{
	ACigkofteGameMode* Mode = GM();

	// Nothing else in this function reads input while a field has focus.
	//
	// First, and above look and movement rather than below them. The first
	// attempt at this gate sat further down, past the mouse delta and the WASD
	// block, so typing a shop name still turned the camera and walked the player -
	// the exact thing the gate exists to stop, and invisible from the gate's own
	// tests because they check the decision rather than where it is applied.
	//
	// Above the pause handling too: Escape belongs to the field while the field
	// has the keyboard, or leaving a half-typed name would open the pause menu
	// behind it.
	if (Mode && CigInput::Scope(Mode->bTextEntryActive, Mode->bTabletOpen, Mode->bBuildMode) == ECigInputScope::TextEntry)
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape))
		{
			Mode->EndTextEntry();
		}
		return;
	}

	// --- Pause menu (Esc/P/Start; works while paused too) ---
	if (Mode && (CigInput::WasPressed(PC, ECigAction::Pause) || PC->WasInputKeyJustPressed(EKeys::Escape)
		|| Pressed(PC, EKeys::Invalid, EKeys::Gamepad_Special_Right)))
	{
		if (Mode->bSettingsOpen)
		{
			Mode->ToggleSettings(); // leave settings (returns to the menu if paused)
		}
		else
		{
			Mode->TogglePauseMenu();
		}
		return;
	}

	if (UGameplayStatics::IsGamePaused(this))
	{
		if (!Mode)
		{
			return;
		}
		// While paused: settings, or menu navigation
		if (Mode->bSettingsOpen)
		{
			if (CigInput::WasPressed(PC, ECigAction::Settings))
			{
				Mode->ToggleSettings();
			}
			else
			{
				PollSettingsInput(PC, Mode);
			}
		}
		else if (Mode->bPauseMenuOpen)
		{
			if (Pressed(PC, EKeys::Up, EKeys::Gamepad_DPad_Up))
			{
				Mode->PauseMenuNav(-1);
			}
			if (Pressed(PC, EKeys::Down, EKeys::Gamepad_DPad_Down))
			{
				Mode->PauseMenuNav(1);
			}
			if (Pressed(PC, EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom) || PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::SpaceBar))
			{
				Mode->PauseMenuSelect();
			}
		}
		return;
	}

	// --- Key binding screen first, because while capturing a pressed key goes
	// to the binding rather than to the game ---
	if (Mode && Mode->bSettingsOpen && Mode->bKeybindsOpen)
	{
		PollKeybindInput(PC, Mode);
		return;
	}

	// --- Lock out other input while the settings screen is open ---
	if (Mode && Mode->bSettingsOpen)
	{
		if (CigInput::WasPressed(PC, ECigAction::Settings))
		{
			Mode->ToggleSettings();
		}
		else
		{
			PollSettingsInput(PC, Mode);
		}
		return;
	}

	// --- Main menu navigation (during the intro phase) ---
	if (Mode && Mode->Days)
	{
		const UCigDaySystem* DaysSys = Mode->Days.Get();
		if (DaysSys->Phase == ECigPhase::Intro && Mode->IntroTime >= ACigkofteGameMode::SplashDuration)
		{
			if (Pressed(PC, EKeys::Up, EKeys::Gamepad_DPad_Up))
			{
				Mode->TitleNav(-1);
			}
			if (Pressed(PC, EKeys::Down, EKeys::Gamepad_DPad_Down))
			{
				Mode->TitleNav(1);
			}
			if (Pressed(PC, EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom))
			{
				Mode->TitleSelect();
			}
		}
	}

	// --- Look ---
	float MouseX = 0.f;
	float MouseY = 0.f;
	PC->GetInputMouseDelta(MouseX, MouseY);
	const float Sens = Mode ? Mode->Settings.MouseSensitivity : 1.f;
	AddControllerYawInput(MouseX * Sens);
	AddControllerPitchInput(-MouseY * Sens);
	PollGamepadLook(PC, Sens);

	// --- Movement ---
	float Fwd = 0.f;
	float Right = 0.f;
	if (CigInput::IsDown(PC, ECigAction::MoveForward)) { Fwd += 1.f; }
	if (CigInput::IsDown(PC, ECigAction::MoveBack)) { Fwd -= 1.f; }
	if (CigInput::IsDown(PC, ECigAction::MoveRight)) { Right += 1.f; }
	if (CigInput::IsDown(PC, ECigAction::MoveLeft)) { Right -= 1.f; }
	PollGamepadMove(PC, Fwd, Right);
	if (Fwd != 0.f) { AddMovementInput(GetActorForwardVector(), FMath::Clamp(Fwd, -1.f, 1.f)); }
	if (Right != 0.f) { AddMovementInput(GetActorRightVector(), FMath::Clamp(Right, -1.f, 1.f)); }

	// --- Jump (when not in the car) ---
	if (!bDriving)
	{
		if (CigInput::WasPressed(PC, ECigAction::Jump))
		{
			Jump();
			Energy = FMath::Max(0.f, Energy - 0.5f);
		}
		if (CigInput::WasReleased(PC, ECigAction::Jump))
		{
			StopJumping();
		}
	}

	const float EnergyMult = Energy < 30.f ? 0.9f : 1.f;
	if (bDriving)
	{
		ACigCar* Car = Mode ? Mode->PlayerCar.Get() : nullptr;
		const bool bNoFuel = Car && Car->Fuel <= 0.f;
		GetCharacterMovement()->MaxWalkSpeed = bNoFuel ? 500.f : 2400.f;
	}
	else
	{
		const bool bSprint = CigInput::IsDown(PC, ECigAction::Run);
		// The HizliAyak skill makes running faster.
		const float SkillRun = (Mode && Mode->Skills) ? Mode->Skills->RunSpeedMult() : 1.f;
		GetCharacterMovement()->MaxWalkSpeed = (bSprint ? 900.f * SkillRun : 600.f) * EnergyMult;
	}

	if (!Mode)
	{
		return;
	}

	// --- Save shortcuts and debug ---
	if (PC->WasInputKeyJustPressed(EKeys::F5))
	{
		Mode->RequestSave();
		Mode->AddMessage(TEXT("Oyun kaydedildi."), FLinearColor(0.6f, 0.9f, 1.f));
	}
	if (PC->WasInputKeyJustPressed(EKeys::F9))
	{
		Mode->RequestLoad();
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::F1))
	{
		Mode->bShowDebugHUD = !Mode->bShowDebugHUD;
		// The station labels are part of the same decision: signage while
		// playing, overhead capitals while inspecting a layout.
		if (Mode->WorldBuilder)
		{
			Mode->WorldBuilder->SetStationLabelsDebug(Mode->bShowDebugHUD);
		}
	}
	if (CigInput::WasPressed(PC, ECigAction::Settings))
	{
		Mode->ToggleSettings();
		return;
	}

	// --- Tablet ---
	if (CigInput::WasPressed(PC, ECigAction::Tablet))
	{
		Mode->ToggleTablet();
	}
	if (Mode->bTabletOpen)
	{
		PollTabletInput(PC, Mode);
		return; // no world interaction while the tablet is open
	}

	// --- Build mode ---
	//
	// Placed here, beside the tablet, because it swallows the same things: the
	// player still walks and looks (both happen above this point) but the shop
	// stops being a place to cook in. A station is furniture while the mode is on,
	// and pressing Interact on it must not start kneading.
	if (PC->WasInputKeyJustPressed(EKeys::B))
	{
		Mode->ToggleBuildMode();
	}
	if (CigInput::Scope(Mode->bTextEntryActive, Mode->bTabletOpen, Mode->bBuildMode) == ECigInputScope::BuildMode)
	{
		// Interact picks up what is selected, and puts it down where the ghost is.
		// The same key for both because it is the same gesture from the player's
		// side - reach out, let go - and because a commit key they had to find
		// separately would be a key they could fail to find while holding a table.
		if (CigInput::WasPressed(PC, ECigAction::Interact))
		{
			if (Mode->bBuildPositioning)
			{
				Mode->CommitBuildMove();
			}
			else
			{
				Mode->BeginBuildMove();
			}
		}
		// Taking things off the floor and putting them back. Only while nothing is
		// picked up: removing what you are holding would be two gestures at once.
		if (!Mode->bBuildPositioning)
		{
			if (PC->WasInputKeyJustPressed(EKeys::Delete))
			{
				Mode->RemoveBuildSelection();
			}
			if (PC->WasInputKeyJustPressed(EKeys::Z))
			{
				Mode->RestoreLastStored();
			}
		}
		if (Mode->bBuildPositioning)
		{
			if (PC->WasInputKeyJustPressed(EKeys::R))
			{
				// One direction only. Quarter turns wrap after four presses, so a
				// second key to go back would be a key for impatience rather than
				// for reach.
				Mode->RotateBuildCandidate(1);
			}
			// Cancel. Escape would be the obvious key and is not available: it is
			// handled far above this point for the pause menu and never reaches
			// here. Whether X is a key anybody finds is a question for a playtest,
			// but leaving cancel unbound would be worse than binding it badly -
			// there would be no way to put a table down unmoved.
			if (PC->WasInputKeyJustPressed(EKeys::X)
				|| PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
			{
				Mode->EndBuildMove();
			}
		}
		return;
	}

	// --- Wrap assembly shortcuts ---
	UCigOrderSystem* Orders = Mode->Orders.Get();
	if (Orders && Orders->Wrap.bActive)
	{
		static const FKey ToppingKeys[8] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight };
		for (int32 i = 0; i < 7; ++i)
		{
			if (PC->WasInputKeyJustPressed(ToppingKeys[i]))
			{
				Orders->ToggleTopping((ECigTopping)i);
			}
		}
		if (PC->WasInputKeyJustPressed(ToppingKeys[7]))
		{
			Orders->ToggleAyran();
		}
		if (PC->WasInputKeyJustPressed(EKeys::Nine))
		{
			Orders->CycleSide();
		}

		// Gamepad: LB/RB move the cursor, B applies the selected row
		const int32 CursorCount = 9; // 0-6 toppings, 7 ayran, 8 side
		if (Pressed(PC, EKeys::Invalid, EKeys::Gamepad_LeftShoulder))
		{
			GamepadWrapCursor = (GamepadWrapCursor + CursorCount - 1) % CursorCount;
		}
		if (Pressed(PC, EKeys::Invalid, EKeys::Gamepad_RightShoulder))
		{
			GamepadWrapCursor = (GamepadWrapCursor + 1) % CursorCount;
		}
		if (Pressed(PC, EKeys::Invalid, EKeys::Gamepad_FaceButton_Right))
		{
			if (GamepadWrapCursor < 7)
			{
				Orders->ToggleTopping((ECigTopping)GamepadWrapCursor);
			}
			else if (GamepadWrapCursor == 7)
			{
				Orders->ToggleAyran();
			}
			else
			{
				Orders->CycleSide();
			}
		}

		if (CigInput::WasPressed(PC, ECigAction::Wrap))
		{
			Orders->FinishWrap();
		}
	}
	if (CigInput::WasPressed(PC, ECigAction::Shelf))
	{
		if (Orders)
		{
			Orders->ShelfWrap();
		}
	}

	// --- Interaction ---
	if (CigInput::WasPressed(PC, ECigAction::Interact))
	{
		if (bDriving)
		{
			// In the car: try a delivery, otherwise get out
			bool bDelivered = false;
			if (Mode->Delivery)
			{
				bDelivered = Mode->Delivery->TryDeliverAt(GetActorLocation());
			}
			if (!bDelivered)
			{
				ToggleCar();
			}
		}
		else if (bCarFocused)
		{
			ToggleCar();
		}
		else if (bCatFocused)
		{
			if (Mode->CatSys)
			{
				Mode->CatSys->Pet();
			}
		}
		else if (FocusedCrate && Mode->Inventory)
		{
			// Unloading is work, not service: it goes through whenever the
			// stations do, so the morning's deliveries can be put away before
			// the door opens.
			const UCigDaySystem* Days = Mode->Days.Get();
			if (Days && Days->CanWork())
			{
				Mode->Inventory->UnloadCrate(FocusedCrate);
			}
		}
		else
		{
			Mode->HandleInteract(FocusedStation);
		}
	}

	// --- Kneading ---
	if (CigInput::WasPressed(PC, ECigAction::Knead) && !bDriving)
	{
		const UCigDaySystem* Days = Mode->Days.Get();
		if (Days && Days->Phase == ECigPhase::Intro)
		{
			Mode->HandleInteract(nullptr); // skip the splash / start the day
		}
		else if (FocusedStation && FocusedStation->StationType == ECigStation::Yogurma
			&& Mode->IsStationInteractionAvailable(FocusedStation))
		{
			Mode->KneadPress();
		}
	}

	// --- Restart ---
	if (PC->WasInputKeyJustPressed(EKeys::R))
	{
		const UCigDaySystem* Days = Mode->Days.Get();
		if (Days && Days->Phase == ECigPhase::GameOver)
		{
			Mode->RestartGame();
		}
	}
}

void ACigkoftePlayerCharacter::ToggleCar()
{
	ACigkofteGameMode* Mode = GM();
	if (!Mode || !Mode->PlayerCar)
	{
		return;
	}
	ACigCar* Car = Mode->PlayerCar.Get();

	if (!bDriving)
	{
		const int32 Level = Mode->Progression ? Mode->Progression->Level : 1;
		if (Level < 4)
		{
			Mode->AddMessage(TEXT("Araba için Seviye 4 gerek! (XP kazan: servis + paket)"), FLinearColor(1.f, 0.6f, 0.2f));
			return;
		}
		if (FVector::Dist2D(GetActorLocation(), Car->GetActorLocation()) > 450.f)
		{
			return;
		}
		bDriving = true;
		Car->SetInteractCollision(false);
		Car->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Car->SetActorRelativeLocation(FVector(0.f, 0.f, -85.f));
		Car->SetActorRelativeRotation(FRotator::ZeroRotator);
		Camera->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
		Mode->PlaySound(ECigSound::CarEngine);
		Mode->AddMessage(TEXT("Arabaya bindin! WASD sür, E ile teslim et/in."), FLinearColor(0.6f, 0.9f, 1.f));
	}
	else
	{
		bDriving = false;
		Car->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		FVector ParkPos = GetActorLocation() + GetActorRightVector() * 180.f;
		ParkPos.Z = 0.f;
		Car->SetActorLocation(ParkPos);
		Car->SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
		Car->SetInteractCollision(true);
		Camera->SetRelativeLocation(FVector(0.f, 0.f, 66.f));
		Mode->AddMessage(TEXT("Arabadan indin."), FLinearColor(0.8f, 0.8f, 0.8f));
	}
}

void ACigkoftePlayerCharacter::UpdateFocus()
{
	if (FocusedStation)
	{
		FocusedStation->SetHighlighted(false);
	}
	if (FocusedCrate)
	{
		FocusedCrate->SetHighlighted(false);
	}
	FocusedStation = nullptr;
	FocusedCrate = nullptr;
	bCarFocused = false;
	bCatFocused = false;
	if (!Camera || bDriving)
	{
		return;
	}

	// Build mode looks at the same world and asks a different question. Reaching
	// it here rather than alongside means the gameplay highlights are already
	// cleared above: a counter left glowing as "interact with me" while the mode
	// treats it as furniture would be telling the player two things at once.
	if (ACigkofteGameMode* BuildMode = GM())
	{
		if (BuildMode->bBuildMode)
		{
			UpdateBuildSelection();
			return;
		}
	}

	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * 450.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CigInteract), false, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		ACigkofteGameMode* Mode = GM();
		FocusedStation = Cast<ACigkofteStation>(Hit.GetActor());
		if (FocusedStation && (!Mode || !Mode->IsStationInteractionAvailable(FocusedStation)))
		{
			FocusedStation = nullptr;
		}
		else if (FocusedStation)
		{
			FocusedStation->SetHighlighted(true);
		}

		if (Mode && Mode->PlayerCar && Hit.GetActor() == Mode->PlayerCar.Get())
		{
			bCarFocused = true;
		}
		if (Cast<ACigCat>(Hit.GetActor()))
		{
			bCatFocused = true;
		}
		FocusedCrate = Cast<ACigStockCrate>(Hit.GetActor());
		if (FocusedCrate)
		{
			FocusedCrate->SetHighlighted(true);
		}
	}
}

bool ACigkoftePlayerCharacter::LookAtFloor(FVector& OutFloorPoint) const
{
	if (!Camera)
	{
		return false;
	}

	// Analytic against the shop's own floor plane rather than a trace. A trace
	// would hit whatever furniture stands between the player and the point they
	// mean, so pointing at the far side of a table would put the ghost on the
	// table rather than on the floor beyond it - and the floor is what the
	// authority measures against.
	const FVector Start = Camera->GetComponentLocation();
	const FVector Dir = Camera->GetForwardVector();
	const float FloorZ = CigPlacementLayout::ShopBounds().FloorZ;

	// Looking at or above the horizon has no answer. Refusing is better than
	// returning a point behind the player, which is what the intersection would
	// give for an upward ray.
	if (Dir.Z > -0.05f)
	{
		return false;
	}

	const float Distance = (FloorZ - Start.Z) / Dir.Z;
	if (Distance <= 0.f || Distance > BuildSelectReach)
	{
		return false;
	}

	OutFloorPoint = Start + Dir * Distance;
	OutFloorPoint.Z = FloorZ;
	return true;
}

void ACigkoftePlayerCharacter::UpdateBuildSelection()
{
	ACigkofteGameMode* Mode = GM();
	if (!Mode || !Camera)
	{
		return;
	}

	UCigWorldBuilder* WB = Mode->WorldBuilder.Get();
	UCigPlacementSystem* Placement = Mode->Placement.Get();
	if (!WB || !Placement)
	{
		Mode->BuildSelection = FCigBuildSelection();
		return;
	}

	// While something is being positioned the trace answers a different question:
	// not "what am I looking at" but "where on the floor am I pointing". Resolving
	// selection as well would let the ghost's own box change what is selected
	// underneath it.
	if (Mode->bBuildPositioning)
	{
		FVector FloorPoint;
		if (LookAtFloor(FloorPoint))
		{
			Mode->SetBuildCandidateLocation(FloorPoint);
		}
		return;
	}

	// Further than the interaction trace on purpose. Interaction is about what the
	// player can touch; this is about what they are pointing at across a room they
	// are judging as a whole, and making them walk to a table to find out its name
	// would make laying out a shop a walking exercise.
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * BuildSelectReach;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CigBuildSelect), false, this);
	FHitResult Hit;
	FName Id = NAME_None;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		// One actor in, one placement out. A chair resolves to its table because
		// that is what the authority holds a record for; the chair was attached to
		// it when the shop was built and has no separate existence to select.
		Id = WB->PlacementVisuals.FindByActor(Hit.GetActor());
	}

	// Resolve runs even when the trace found nothing, so the refusal is a decided
	// value rather than a stale selection nobody got round to clearing.
	Mode->BuildSelection = CigBuildSelection::Resolve(Id, Placement->FindPlacement(Id));
}

void ACigkoftePlayerCharacter::UpdateHeadBob(float DeltaSeconds)
{
	if (bDriving)
	{
		return; // no head bob in the car
	}
	ACigkofteGameMode* Mode = GM();
	const bool bBobEnabled = !Mode || Mode->Settings.bHeadBob;

	const float Speed = GetVelocity().Size2D();
	if (bBobEnabled && Speed > 60.f && GetCharacterMovement()->IsMovingOnGround())
	{
		BobPhase += DeltaSeconds * (6.f + Speed * 0.012f);
		const float Amp = FMath::Min(3.5f, Speed * 0.005f);
		BobOffset = FMath::Sin(BobPhase) * Amp;
	}
	else
	{
		BobOffset = FMath::FInterpTo(BobOffset, 0.f, DeltaSeconds, 8.f);
	}
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 66.f + BobOffset));
}

void ACigkoftePlayerCharacter::UpdateEnergy(float DeltaSeconds)
{
	ACigkofteGameMode* Mode = GM();
	const UCigDaySystem* Days = Mode ? Mode->Days.Get() : nullptr;
	if (!Days || !Days->CanWork())
	{
		return;
	}

	const float Speed = GetVelocity().Size2D();
	if (bDriving)
	{
		// Fuel burn
		if (ACigCar* Car = Mode->PlayerCar.Get())
		{
			if (Speed > 100.f)
			{
				Car->Fuel = FMath::Max(0.f, Car->Fuel - DeltaSeconds * 0.8f);
				if (Car->Fuel <= 0.f && Speed > 600.f)
				{
					Mode->AddMessage(TEXT("Depo boş! Araç sürünüyor — tabletten yakıt al."), FLinearColor(1.f, 0.5f, 0.3f));
				}
			}
		}
		return;
	}

	// The DayanikliBunye skill lowers energy drain
	const float Drain = Mode->Skills ? Mode->Skills->EnergyDrainMult() : 1.f;

	// Running builds up energy drain and thirst
	if (Speed > 700.f)
	{
		Energy = FMath::Max(0.f, Energy - DeltaSeconds * 1.2f * Drain);
		Thirst = FMath::Min(100.f, Thirst + DeltaSeconds * 1.5f);
	}
	else
	{
		Thirst = FMath::Min(100.f, Thirst + DeltaSeconds * 0.3f);
	}

	// Thirst eats into energy
	if (Thirst > 70.f)
	{
		Energy = FMath::Max(0.f, Energy - DeltaSeconds * 0.4f * Drain);
	}

	// A long queue is stressful; an empty shop is calming
	const int32 QueueNum = Mode->Customers ? Mode->Customers->Queue.Num() : 0;
	if (QueueNum >= 4)
	{
		Stress = FMath::Min(100.f, Stress + DeltaSeconds * 1.f);
	}
	else if (QueueNum == 0)
	{
		Stress = FMath::Max(0.f, Stress - DeltaSeconds * 0.8f);
		Energy = FMath::Min(100.f, Energy + DeltaSeconds * 0.5f); // a quiet moment lets you breathe
	}
}

void ACigkoftePlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Apply FOV from the settings
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Camera && !FMath::IsNearlyEqual(Camera->FieldOfView, Mode->Settings.FOV))
		{
			Camera->SetFieldOfView(Mode->Settings.FOV);
		}
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (!UGameplayStatics::IsGamePaused(this))
		{
			UpdateFocus();
			UpdateHeadBob(DeltaSeconds);
			UpdateEnergy(DeltaSeconds);
		}
		PollInput(PC);
	}
}
