// The GameMode's menu and tablet UI state: tablet tabs, the pause menu and the
// settings screen.
//
// Why a separate file: none of this is a gameplay rule - it is UI state and
// input routing. Keeping it in the same file as system coordination had made
// GameMode unreadable.
//
// The functions are still ACigkofteGameMode members. Lifting them into a real
// UCigTabletState class would mean changing the signatures and every caller
// (PlayerController, HUD); that step has not been taken yet.

#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "World/CigWorldBuilder.h"
#include "Cooking/CigCookingSystem.h"
#include "Economy/CigPricingSystem.h"
#include "Economy/CigInspectionSystem.h"
#include "Economy/CigSocialSystem.h"
#include "Events/CigEventSystem.h"
#include "Core/CigBalance.h"
#include "Orders/CigOrderSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigRivalSystem.h"
#include "Economy/CigReviewSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Progression/CigAchievementSystem.h"
#include "Quests/CigQuestSystem.h"
#include "Delivery/CigDeliverySystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Staff/CigStaffSystem.h"
#include "Cat/CigCatSystem.h"
#include "Placement/CigPlacementSystem.h"
#include "Navigation/CigNavSystem.h"
#include "Engine/StaticMeshActor.h"
#include "Vehicles/CigCar.h"
#include "Audio/CigAudioSubsystem.h"
#include "Core/CigLog.h"
#include "Core/CigUnlocks.h"
#include "Core/CigText.h"
#include "Core/CigInput.h"
#include "UI/CigTabletWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Engine/Engine.h"

// ---------------------------------------------------------------- tablet

void ACigkofteGameMode::ToggleTablet()
{
	bTabletOpen = !bTabletOpen;
	if (bTabletOpen)
	{
		bSettingsOpen = false;
	}
	else
	{
		// Closing the tablet takes any field on it with it. Otherwise the flag
		// would outlive the widget that set it and the game would sit there
		// refusing every key with nothing on screen to explain why.
		EndTextEntry();
	}
	PlaySound(ECigSound::UIClick);
	RefreshTabletWidget();
}

void ACigkofteGameMode::ToggleBuildMode()
{
	bBuildMode = !bBuildMode;
	if (bBuildMode)
	{
		// Both panels close on the way in. CigInput::Scope ranks build mode below
		// them anyway, so leaving one open would silently swallow build mode's own
		// keys - the player would be in the mode and it would not answer.
		if (bTabletOpen)
		{
			ToggleTablet();
		}
		bSettingsOpen = false;
	}
	else
	{
		// A selection that outlived the mode would leave a highlight on the floor
		// with nothing listening to it, and a ghost that outlived it would leave a
		// coloured box standing in the shop.
		EndBuildMove();
		BuildSelection = FCigBuildSelection();
	}
	AddMessage(CigText::Get(bBuildMode ? TEXT("msg.buildmode.on") : TEXT("msg.buildmode.off")),
		FLinearColor(0.6f, 0.85f, 1.f));
	PlaySound(ECigSound::UIClick);
}

bool ACigkofteGameMode::BeginBuildMove()
{
	if (!bBuildMode || bBuildPositioning || !BuildSelection.IsValid() || !Placement)
	{
		return false;
	}

	const FCigPlacementRecord* Record = Placement->FindPlacement(BuildSelection.StableId);
	if (!Record)
	{
		return false;
	}

	bBuildPositioning = true;
	BuildMovingId = Record->StableId;
	// Starting from where it already stands means the first thing the player sees
	// is a green ghost exactly over the table: the mode says "this is what you have
	// picked up" before it says anything about where it could go.
	BuildCandidate = Record->Transform;
	EvaluateBuildCandidate();
	PlaySound(ECigSound::UIClick);
	return true;
}

void ACigkofteGameMode::EndBuildMove()
{
	bBuildPositioning = false;
	BuildMovingId = NAME_None;
	BuildVerdict = FCigBuildVerdict();
	if (BuildGhost)
	{
		BuildGhost->Destroy();
		BuildGhost = nullptr;
	}
}

void ACigkofteGameMode::SetBuildCandidateLocation(const FVector& FloorPoint)
{
	if (!bBuildPositioning)
	{
		return;
	}

	// Only the floor plane moves. Height is the authority's business - it refuses
	// anything off the floor - and letting the player's aim carry Z would make
	// every candidate fail InvalidFloor for a reason they could not see.
	FVector Location = FloorPoint;
	Location.Z = BuildCandidate.GetLocation().Z;

	if (Location.Equals(BuildCandidate.GetLocation(), 1.f))
	{
		return; // below the authority's own snap, so nothing would change
	}

	BuildCandidate.SetLocation(Location);
	EvaluateBuildCandidate();
}

void ACigkofteGameMode::RotateBuildCandidate(int32 QuarterTurns)
{
	if (!bBuildPositioning || QuarterTurns == 0)
	{
		return;
	}

	FRotator Rotation = BuildCandidate.GetRotation().Rotator();
	Rotation.Yaw += 90.f * QuarterTurns;
	BuildCandidate.SetRotation(Rotation.Quaternion());
	EvaluateBuildCandidate();
	PlaySound(ECigSound::UIClick);
}

void ACigkofteGameMode::EvaluateBuildCandidate()
{
	if (!bBuildPositioning || !Placement)
	{
		return;
	}

	const FCigPlacementRecord* Record = Placement->FindPlacement(BuildMovingId);
	if (!Record)
	{
		// The record went away underneath a move in progress. Nothing was changed,
		// so putting it down is the whole of the recovery.
		EndBuildMove();
		return;
	}

	const FCigPlacementRequest Request = CigBuildVerdict::MakeMoveRequest(*Record, BuildCandidate);
	const FCigPlacementResult Validation = Placement->ValidatePlacement(Request);

	// The grid is only asked when the rectangles were happy. Two reasons, and the
	// cheaper one is not the important one: rebuilding a hypothetical grid costs
	// more than rectangle tests, but the reason that matters is that a refused
	// candidate already has a better answer to give.
	bool bClosesRoute = false;
	FName ClosedRoute = NAME_None;
	if (Validation.bAccepted && Nav)
	{
		FCigPlacementRecord Candidate;
		if (CigBuildVerdict::MakeCandidateRecord(Request, Validation.NormalizedTransform, Candidate))
		{
			bClosesRoute = Nav->WouldCloseRequiredRoute(Candidate, ClosedRoute);
		}
	}

	BuildVerdict = CigBuildVerdict::Combine(Validation, bClosesRoute, ClosedRoute);

	// --- The ghost ---
	if (!WorldBuilder)
	{
		return;
	}

	const FCigPlacementFootprint& Footprint = Record->Footprint;
	// SpawnBox works in units of a 100cm cube. The ghost is deliberately the
	// footprint rather than the mesh: what the player is fighting for is floor
	// space, and the footprint is the thing the authority actually judges.
	const FVector Scale(
		FMath::Max(Footprint.Size.X, 1.f) / 100.f,
		FMath::Max(Footprint.Size.Y, 1.f) / 100.f,
		BuildGhostHeight / 100.f);

	const FTransform& Where = BuildVerdict.NormalizedTransform;
	const FVector Centre = Where.GetLocation()
		+ Where.GetRotation().RotateVector(FVector(Footprint.CenterOffset.X, Footprint.CenterOffset.Y, 0.f))
		+ FVector(0.f, 0.f, BuildGhostHeight * 0.5f);

	if (!BuildGhost)
	{
		BuildGhost = WorldBuilder->SpawnBox(Centre, Scale, CigBuildVerdict::Tint(BuildVerdict));
		if (BuildGhost)
		{
			// A preview must not be something the world can collide with, or the
			// player could be pushed by their own intention.
			BuildGhost->SetActorEnableCollision(false);
		}
	}
	else
	{
		BuildGhost->SetActorTransform(FTransform(Where.GetRotation(), Centre, Scale));
		WorldBuilder->TintBox(BuildGhost, CigBuildVerdict::Tint(BuildVerdict));
	}
}

void ACigkofteGameMode::BeginTextEntry(UWidget* FocusWidget)
{
	if (bTextEntryActive)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	// The flag first, because SetInputMode does not stop the polling. Input here
	// is read straight off the controller every tick and does not care which
	// widget has focus; the flag is what CigInput::Scope checks and the character
	// obeys. The input mode is what gets the letters into the field.
	bTextEntryActive = true;

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	if (FocusWidget)
	{
		Mode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
}

void ACigkofteGameMode::EndTextEntry()
{
	if (!bTextEntryActive)
	{
		return;
	}
	bTextEntryActive = false;

	// Back to game input. Not conditional on the tablet being open: the tablet has
	// never needed a cursor, and leaving GameAndUI behind would let a stray click
	// steal focus from a game that is polling keys.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void ACigkofteGameMode::RefreshTabletWidget()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	if (!TabletWidget)
	{
		TabletWidget = CreateWidget<UCigTabletWidget>(PC, UCigTabletWidget::StaticClass());
		if (!TabletWidget)
		{
			return;
		}
	}

	// Do not leave the widget on screen while the tablet is closed; UMG keeps
	// computing layout even when it is invisible.
	if (bTabletOpen)
	{
		if (!TabletWidget->IsInViewport())
		{
			TabletWidget->AddToViewport(10);
		}
		TabletWidget->RefreshFrom(this);
	}
	else if (TabletWidget->IsInViewport())
	{
		TabletWidget->RemoveFromParent();
	}
}

void ACigkofteGameMode::CycleTabletTab(int32 Dir)
{
	if (!bTabletOpen)
	{
		return;
	}
	int32 Tab = (int32)TabletTab + Dir;
	const int32 Count = (int32)ECigTabletTab::COUNT;
	Tab = (Tab % Count + Count) % Count;
	TabletTab = (ECigTabletTab)Tab;
	TabletScroll = 0;
	PlaySound(ECigSound::UINav);
	RefreshTabletWidget();
}

void ACigkofteGameMode::TabletScrollBy(int32 Dir)
{
	if (!bTabletOpen)
	{
		return;
	}
	TabletScroll = FMath::Clamp(TabletScroll + Dir, 0, 12);
}

void ACigkofteGameMode::TabletKey(int32 Num)
{
	if (!bTabletOpen)
	{
		return;
	}

	switch (TabletTab)
	{
	case ECigTabletTab::Stok:
	{
		const int32 Item = TabletScroll + Num - 1;
		if (Num >= 1 && Item >= 0 && Item < CigStockCount && Inventory)
		{
			Inventory->OrderStock(Item);
		}
		break;
	}
	case ECigTabletTab::Fiyatlar:
	{
		const int32 Urun = Num - 1;
		if (!Pricing || Urun < 0 || Urun >= CigUrunCount)
		{
			break;
		}

		// Shift turns the same key into a price cut, so one row of number keys
		// drives both directions without inventing a second control scheme.
		const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const bool bIndirim = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));

		Pricing->FiyatDegistir(Urun, bIndirim ? -UCigPricingSystem::CarpanAdimi : UCigPricingSystem::CarpanAdimi);
		AddMessage(CigText::Format(TEXT("tablet.pricechanged"),
			*CigBalance::Pricing(Urun).Label, Pricing->Fiyat(Urun)),
			FLinearColor(0.8f, 0.9f, 1.f));
		break;
	}
	case ECigTabletTab::Tarifler:
	{
		const int32 Idx = Num - 1;
		if (Cooking && Idx >= 0 && Idx < CigRecipeCount)
		{
			if (Cooking->IsRecipeUnlocked(Idx))
			{
				Cooking->CurrentRecipe = Idx;
				AddMessage(CigText::Format(TEXT("msg.ui.recipeselected"), *UCigCookingSystem::RecipeName(Idx)), FLinearColor(0.8f, 0.7f, 1.f));
			}
			else
			{
				AddMessage(CigText::Get(TEXT("msg.ui.recipelocked")), FLinearColor(1.f, 0.6f, 0.2f));
			}
		}
		break;
	}
	case ECigTabletTab::Dukkan:
	{
		if (Inspection)
		{
			if (Num == 0)
			{
				Inspection->RuhsatYenile();
				break;
			}
			if (Num == 9 && Inspection->BekleyenCeza > 0)
			{
				Inspection->RusvetVer();
				break;
			}
		}

		const int32 Idx = TabletScroll + Num - 1;
		if (Num < 1 || Idx < 0 || !Economy)
		{
			break;
		}
		const int32 UpgradeCount = (int32)ECigUpgrade::COUNT;
		if (Idx < UpgradeCount)
		{
			Economy->BuyUpgrade((ECigUpgrade)Idx);
		}
		else if (Idx == UpgradeCount)
		{
			Economy->BuyHouse();
		}
		else if (Idx == UpgradeCount + 1)
		{
			Economy->RefuelCar();
		}
		else if (Idx == UpgradeCount + 2)
		{
			Economy->RepairCar();
		}
		else if (Idx == UpgradeCount + 3)
		{
			Economy->CyclePricePolicy();
		}
		break;
	}
	case ECigTabletTab::Tedarikci:
	{
		const int32 Idx = Num - 1;
		if (Economy && Idx >= 0 && Idx < CigSupplierCount)
		{
			Economy->CurrentSupplier = Idx;
			AddMessage(CigText::Format(TEXT("msg.ui.supplierselected"), *UCigEconomySystem::SupplierName(Idx)), FLinearColor(0.7f, 0.9f, 1.f));
		}
		else if (Economy && Idx == CigSupplierCount)
		{
			Economy->CycleIngredientTier();
		}
		break;
	}
	case ECigTabletTab::Yetenekler:
	{
		if (!Skills)
		{
			break;
		}
		const int32 Idx = Num - 1;
		if (Idx >= 0 && Idx < (int32)ECigSkill::COUNT)
		{
			Skills->Upgrade((ECigSkill)Idx);
		}
		else if (Idx == (int32)ECigSkill::COUNT)
		{
			Skills->DoPrestige();
		}
		break;
	}
	case ECigTabletTab::Yorumlar:
	{
		if (!Social)
		{
			break;
		}
		switch (Num)
		{
		case 1: Social->UrunTanitimiPaylas(); break;
		case 2: Social->KampanyaDuyurusuPaylas(); break;
		case 7: Social->YorumaYanitVer(ECigYanit::Savun); break;
		case 8: Social->YorumaYanitVer(ECigYanit::OzurDile); break;
		case 9: Social->YorumaYanitVer(ECigYanit::GormezdenGel); break;
		default: break;
		}
		break;
	}
	case ECigTabletTab::Gorevler:
	{
		if (Events && Events->TopluSiparis.bTeklifVar)
		{
			if (Num == 1)
			{
				Events->TopluSiparisiKabulEt();
			}
			else if (Num == 2)
			{
				Events->TopluSiparisiReddet();
			}
		}
		break;
	}
	case ECigTabletTab::Personel:
	{
		if (!Staff)
		{
			break;
		}
		// With nobody hired the number keys pick a candidate; once someone is on
		// the payroll they drive the job, the raise and the counter-offer.
		if (!Staff->Apprentice.bHired)
		{
			Staff->HireAday(Num - 1);
			break;
		}

		if (Num == 1)
		{
			Staff->CycleTask();
		}
		else if (Num == 2)
		{
			Staff->GiveRaise();
		}
		else if (Num == 3)
		{
			Staff->KarsiTeklifVer();
		}
		break;
	}
	default:
		break;
	}
}

// ---------------------------------------------------------------- pause menu

void ACigkofteGameMode::TogglePauseMenu()
{
	bPauseMenuOpen = !bPauseMenuOpen;
	PauseMenuCursor = 0;
	bSettingsOpen = false;
	bTabletOpen = false;
	UGameplayStatics::SetGamePaused(this, bPauseMenuOpen);
}

void ACigkofteGameMode::PauseMenuNav(int32 Dir)
{
	PauseMenuCursor = (PauseMenuCursor + Dir + PauseMenuItemCount) % PauseMenuItemCount;
	PlaySound(ECigSound::UINav);
}

void ACigkofteGameMode::PauseMenuSelect()
{
	PlaySound(ECigSound::UIClick);
	switch (PauseMenuCursor)
	{
	case 0: // Resume
		TogglePauseMenu();
		break;
	case 1: // Settings (the game stays paused; closing returns to the menu)
		bPauseMenuOpen = false;
		bSettingsOpen = true;
		break;
	case 2: // Save
		RequestSave();
		AddMessage(CigText::Get(TEXT("msg.ui.saved")), FLinearColor(0.6f, 0.9f, 1.f));
		break;
	case 3: // Quit
		RequestSave();
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------- settings

void ACigkofteGameMode::ToggleSettings()
{
	bSettingsOpen = !bSettingsOpen;
	if (bSettingsOpen)
	{
		bTabletOpen = false;
	}
	else
	{
		ApplySettings();
		RequestSave();
		// Go back to the pause menu if that is where we came from
		if (UGameplayStatics::IsGamePaused(this))
		{
			bPauseMenuOpen = true;
		}
	}
}

void ACigkofteGameMode::SettingsNav(int32 Dir)
{
	SettingsCursor = (SettingsCursor + Dir + SettingsCount) % SettingsCount;
	PlaySound(ECigSound::UINav);
}

void ACigkofteGameMode::SettingsAdjust(int32 Dir)
{
	// This order must match the row order in the HUD (CigkofteHUD::DrawSettings).
	switch (SettingsCursor)
	{
	case 0: Settings.ResolutionIndex = FMath::Clamp(Settings.ResolutionIndex + Dir, 0, 3); break;
	case 1: Settings.WindowMode = FMath::Clamp(Settings.WindowMode + Dir, 0, 2); break;
	case 2: Settings.QualityLevel = FMath::Clamp(Settings.QualityLevel + Dir, 0, 3); break;
	case 3: Settings.FOV = FMath::Clamp(Settings.FOV + Dir * 5.f, 60.f, 120.f); break;
	case 4: Settings.MouseSensitivity = FMath::Clamp(Settings.MouseSensitivity + Dir * 0.1f, 0.2f, 3.f); break;
	case 5: Settings.bHeadBob = !Settings.bHeadBob; break;
	case 6: Settings.MasterVolume = FMath::Clamp(Settings.MasterVolume + Dir * 0.1f, 0.f, 1.f); break;
	case 7: Settings.EffectsVolume = FMath::Clamp(Settings.EffectsVolume + Dir * 0.1f, 0.f, 1.f); break;
	case 8: Settings.MusicVolume = FMath::Clamp(Settings.MusicVolume + Dir * 0.1f, 0.f, 1.f); break;
	case 9: Settings.UIScaleMult = FMath::Clamp(Settings.UIScaleMult + Dir * 0.1f, 0.7f, 1.6f); break;
	case 10: Settings.bScreenFlash = !Settings.bScreenFlash; break;
	case 11:
		Settings.ColorBlindMode = (Settings.ColorBlindMode + Dir + (int32)ECigColorBlindMode::COUNT) % (int32)ECigColorBlindMode::COUNT;
		break;
	case 12:
	{
		const int32 N = CigText::LanguageCount();
		Settings.Language = (Settings.Language + Dir + N) % N;
		break;
	}
	case 13:
		ToggleKeybinds();
		break;
	case 14:
		if (Quests)
		{
			Quests->ResetTutorial();
		}
		break;
	default: break;
	}
	ApplySettings();
}

// ---------------------------------------------------------------- key binding

void ACigkofteGameMode::ToggleKeybinds()
{
	bKeybindsOpen = !bKeybindsOpen;
	bAwaitingKey = false;
	KeybindCursor = 0;
	PlaySound(ECigSound::UIClick);
}

void ACigkofteGameMode::KeybindNav(int32 Dir)
{
	if (bAwaitingKey)
	{
		return;   // the cursor does not move while capturing
	}
	const int32 N = (int32)ECigAction::COUNT;
	KeybindCursor = (KeybindCursor + Dir + N) % N;
	PlaySound(ECigSound::UINav);
}

void ACigkofteGameMode::KeybindBeginCapture()
{
	bAwaitingKey = true;
	PlaySound(ECigSound::UIClick);
}

void ACigkofteGameMode::KeybindResetAll()
{
	CigInput::ResetToDefaults();
	bAwaitingKey = false;
	AddMessage(CigText::Get(TEXT("settings.keybinds.reset")), FLinearColor(0.7f, 0.9f, 1.f));
	RequestSave();
}

bool ACigkofteGameMode::KeybindCaptureKey(const FKey& PressedKey)
{
	if (!bAwaitingKey || !PressedKey.IsValid())
	{
		return false;
	}

	// Escape cancels: the player has to be able to change their mind. Making
	// Escape bindable would also lock the way out of the menu.
	if (PressedKey == EKeys::Escape)
	{
		bAwaitingKey = false;
		PlaySound(ECigSound::Failure);
		return true;
	}

	// Axis keys such as mouse axes cannot trigger an action.
	if (PressedKey.IsAxis1D() || PressedKey.IsAxis2D() || PressedKey.IsAxis3D())
	{
		return false;
	}

	CigInput::SetKey((ECigAction)KeybindCursor, PressedKey);
	bAwaitingKey = false;
	PlaySound(ECigSound::Success);
	RequestSave();
	return true;
}

void ACigkofteGameMode::ApplySettings(ECigSettingsPersistence Persistence)
{
	// Language: UI text is read from the new language on the next draw.
	CigText::SetLanguage(Settings.Language);

	// The world's text is not redrawn from a template every frame - a locked
	// station's sign is set once, when the lock is applied - so changing the
	// language left "SEVIYE 6" standing over a shop whose HUD had switched to
	// English. Re-applying the current level rebuilds the signs against the
	// language that is now selected.
	if (WorldBuilder && Progression)
	{
		WorldBuilder->RefreshUnlocks(Progression->Level, /*bAnnounce=*/false);
	}

	// Audio
	if (UCigAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigAudioSubsystem>() : nullptr)
	{
		Audio->MasterVolume = Settings.MasterVolume;
		Audio->EffectsVolume = Settings.EffectsVolume;
		Audio->MusicVolume = Settings.MusicVolume;
	}

	// UGameUserSettings::ApplySettings also saves GameUserSettings.ini. The
	// release self-test deliberately starts from fresh defaults, so persisting
	// that branch would overwrite a real user's platform settings even though
	// normal CigSave writes are disabled. Runtime-only application keeps the
	// language, signage and audio work above, and leaves platform config alone.
	if (Persistence == ECigSettingsPersistence::PersistPlatformConfig && GEngine)
	{
		if (UGameUserSettings* GUS = GEngine->GetGameUserSettings())
		{
			// -ResX/-ResY on the command line wins over the saved resolution.
			//
			// This ran at startup and re-applied the settings file over whatever the
			// command line had just asked for, silently. Measure-Performance.ps1
			// passes -ResX/-ResY and labels its report with them, so two runs at
			// "1920x1080" and "1280x720" were rendering the same number of pixels
			// and reported GPU times 0.03 ms apart - which is what a resolution
			// experiment looks like when the resolution never changed. Every
			// resolution label in PERFORMANCE_BUDGET.md written before this was a
			// statement about the command line, not about the frame.
			int32 CmdResX = 0;
			const bool bResFromCmdLine = FParse::Value(FCommandLine::Get(), TEXT("ResX="), CmdResX) && CmdResX > 0;
			if (!bResFromCmdLine)
			{
				static const FIntPoint Resolutions[4] = { {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440} };
				GUS->SetScreenResolution(Resolutions[FMath::Clamp(Settings.ResolutionIndex, 0, 3)]);
				const EWindowMode::Type Modes[3] = { EWindowMode::Windowed, EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen };
				GUS->SetFullscreenMode(Modes[FMath::Clamp(Settings.WindowMode, 0, 2)]);
			}
			GUS->SetOverallScalabilityLevel(FMath::Clamp(Settings.QualityLevel, 0, 3));
			GUS->ApplySettings(false);
		}
	}
	// The player character reads FOV, sensitivity and head-bob from Settings each frame.
}
