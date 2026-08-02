#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Placement/CigPlacementSystem.h"
#include "Game/CigDaySystem.h"
#include "World/CigWorldBuilder.h"
#include "World/CigkofteStation.h"
#include "Cooking/CigCookingSystem.h"
#include "Orders/CigOrderSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigPricingSystem.h"
#include "Economy/CigSaleSystem.h"
#include "Economy/CigInspectionSystem.h"
#include "Economy/CigSocialSystem.h"
#include "Economy/CigRivalSystem.h"
#include "Economy/CigReviewSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Progression/CigAchievementSystem.h"
#include "Quests/CigQuestSystem.h"
#include "Events/CigEventSystem.h"
#include "Delivery/CigDeliverySystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Staff/CigStaffSystem.h"
#include "Cat/CigCatSystem.h"
#include "Player/CigkoftePlayerCharacter.h"
#include "Player/CigPlayerController.h"
#include "UI/CigkofteHUD.h"
#include "Vehicles/CigCar.h"
#include "Save/CigSaveGame.h"
#include "Save/CigSaveSubsystem.h"
#include "Audio/CigAudioSubsystem.h"
#include "Game/CigReleaseSelfTest.h"
#include "Core/CigLog.h"
#include "Core/CigUnlocks.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigText.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

ACigkofteGameMode::ACigkofteGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = ACigkoftePlayerCharacter::StaticClass();
	HUDClass = ACigkofteHUD::StaticClass();
	PlayerControllerClass = ACigPlayerController::StaticClass();
}

void ACigkofteGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Before CreateSystems, because saving is reachable the moment a day can be
	// broadcast. The test harness sets the same flag in the same place and for the
	// same reason - a headless run once replaced a real save with its own day one.
	bReleaseSelfTest = CigReleaseSelfTest::IsRequested();
	if (bReleaseSelfTest)
	{
		bSaveDisabled = true;
	}

	CreateSystems();

	WorldBuilder->BuildWorld();

	// The player's car
	PlayerCar = GetWorld()->SpawnActor<ACigCar>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (PlayerCar)
	{
		PlayerCar->SetParked(FVector(-1300.f, 500.f, 0.f), 90.f, FLinearColor(0.9f, 0.45f, 0.05f));
	}

	// Starting locks (level 1): later stations and districts are closed
	WorldBuilder->RefreshUnlocks(Progression ? Progression->Level : 1, false);

	// Load a save if there is one.
	//
	// Skipped entirely under the self-test: bSaveDisabled stops the writes, but a
	// check whose result depends on whatever is in the developer's day-14 save is
	// not a check. The self-test verifies a fresh game.
	if (!bReleaseSelfTest)
	{
		if (UCigSaveSubsystem* SaveSys = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigSaveSubsystem>() : nullptr)
		{
			if (SaveSys->HasSave())
			{
				bLoadedFromSave = SaveSys->LoadInto(this);
			}
		}
	}
	ApplySettings(bReleaseSelfTest
		? ECigSettingsPersistence::RuntimeOnly
		: ECigSettingsPersistence::PersistPlatformConfig);
	RefreshTabletWidget();

	if (bReleaseSelfTest)
	{
		SelfTestExitCode = (uint8)CigReleaseSelfTest::Run(*this);
		bSelfTestExitPending = true;
	}

	UE_LOG(LogCig, Log, TEXT("Oyun kuruldu (%d sistem, kayıt: %s)"), AllSystems.Num(), bLoadedFromSave ? TEXT("yüklendi") : TEXT("yok"));
}

void ACigkofteGameMode::CreateSystems()
{
	// Build the systems in dependency order. The bus goes first: everything
	// below subscribes to it during InitSystem.
	CreateSystem(Bus);
	CreateSystem(Placement);
	CreateSystem(WorldBuilder);
	CreateSystem(Days);
	CreateSystem(Inventory);
	CreateSystem(Hygiene);
	CreateSystem(Progression);
	CreateSystem(Economy);
	CreateSystem(Cooking);
	CreateSystem(Orders);
	CreateSystem(Events);
	CreateSystem(Reviews);
	CreateSystem(Rivals);
	// After Rivals: pricing reads their prices to answer with its own.
	CreateSystem(Pricing);
	// After Pricing: every sale is priced off the list it publishes.
	CreateSystem(Sales);
	CreateSystem(Inspection);
	CreateSystem(Social);
	CreateSystem(Quests);
	CreateSystem(Customers);
	CreateSystem(Delivery);
	CreateSystem(Staff);
	CreateSystem(CatSys);
	CreateSystem(Skills);
	CreateSystem(Achievements);

	for (UCigSystem* Sys : AllSystems)
	{
		Sys->InitSystem(this);
	}
}

bool ACigkofteGameMode::HasAllSystems(int32& OutCount) const
{
	OutCount = AllSystems.Num();
	if (OutCount == 0)
	{
		return false;
	}
	for (const UCigSystem* S : AllSystems)
	{
		if (!S)
		{
			return false;
		}
	}
	return true;
}

void ACigkofteGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The self-test exits here rather than at the end of InitGame.
	//
	// InitGame runs inside LoadMap, so requesting exit there unwinds a half-loaded
	// map. Reaching the first Tick proves the map finished and the game loop
	// started, which is itself the strongest liveness evidence a build with no log
	// can produce, and it costs one frame. The report is already on disk, so even
	// a failure during shutdown leaves a readable verdict. The return keeps that
	// one frame from advancing any system.
	//
	// Forced, because the unforced form does not carry the status. Measured: with
	// Force=false a run whose report said RESULT FAIL 6 still left the process
	// with exit code 0, which would have made the harness read every failure as a
	// pass. Forcing is safe here precisely because the report is already on disk.
	if (bSelfTestExitPending)
	{
		bSelfTestExitPending = false;
		FPlatformMisc::RequestExitWithStatus(true, SelfTestExitCode, TEXT("CigReleaseSelfTest"));
		return;
	}

	for (int32 i = Messages.Num() - 1; i >= 0; --i)
	{
		Messages[i].TimeLeft -= DeltaSeconds;
		if (Messages[i].TimeLeft <= 0.f)
		{
			Messages.RemoveAt(i);
		}
	}

	if (Days && Days->Phase == ECigPhase::Intro)
	{
		IntroTime += DeltaSeconds;
	}

	if (FlashTimer > 0.f)
	{
		FlashTimer = FMath::Max(0.f, FlashTimer - DeltaSeconds);
	}

	// Menu music: on the title screen (after the splash) or in the pause menu
	if (UCigAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigAudioSubsystem>() : nullptr)
	{
		const bool bTitleMenu = Days && Days->Phase == ECigPhase::Intro && IntroTime >= SplashDuration;
		Audio->TickMenuMusic(DeltaSeconds, bTitleMenu || bPauseMenuOpen);

		// The ambience bed follows the clock and the weather the world builder
		// is already driving, so it needs no state of its own.
		const bool bGunAkiyor = Days && Days->IsPlaying();
		Audio->TickAmbience(DeltaSeconds,
			WorldBuilder ? WorldBuilder->Evening : 0.f,
			WorldBuilder ? WorldBuilder->Weather : 0,
			bGunAkiyor);
	}

	for (UCigSystem* Sys : AllSystems)
	{
		if (Sys)
		{
			Sys->UpdateSystem(DeltaSeconds);
		}
	}
}

// ---------------------------------------------------------------- broadcasts

void ACigkofteGameMode::BroadcastDayStart(int32 Day)
{
	for (UCigSystem* Sys : AllSystems)
	{
		if (Sys && Sys != Days.Get())
		{
			Sys->OnDayStart(Day);
		}
	}

	RequestSave();
}

void ACigkofteGameMode::BroadcastDayEnd(int32 Day)
{
	for (UCigSystem* Sys : AllSystems)
	{
		if (Sys && Sys != Days.Get())
		{
			Sys->OnDayEnd(Day);
		}
	}
	RequestSave();
}

// ---------------------------------------------------------------- messages & audio

void ACigkofteGameMode::AddMessage(const FString& Text, const FLinearColor& Color)
{
	// Messages also go to the log so runtime tests can follow the flow.
	UE_LOG(LogCig, Log, TEXT("MSG: %s"), *Text);

	FCigMessage M;
	M.Text = Text;
	M.Color = Color;
	M.TimeLeft = 6.f;
	Messages.Insert(M, 0);
	if (Messages.Num() > 6)
	{
		Messages.SetNum(6);
	}
}

void ACigkofteGameMode::SpawnFloatText(const FVector& Loc, const FString& Text, const FColor& Color, float Size)
{
	if (WorldBuilder)
	{
		WorldBuilder->SpawnFloatText(Loc, Text, Color, Size);
	}
}

void ACigkofteGameMode::RequestFlash(const FLinearColor& Color, float Duration)
{
	FlashColor = Color;
	FlashDuration = Duration;
	FlashTimer = Duration;
}

void ACigkofteGameMode::PlaySound(ECigSound Sound)
{
	if (UCigAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigAudioSubsystem>() : nullptr)
	{
		Audio->Play(Sound);
	}
}

bool ACigkofteGameMode::PlayerDriving() const
{
	const ACigkoftePlayerCharacter* Player = Cast<ACigkoftePlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	return Player && Player->bDriving;
}

// ---------------------------------------------------------------- interaction

void ACigkofteGameMode::StartFirstDayIfIntro()
{
	if (Days && Days->Phase == ECigPhase::Intro)
	{
		Days->StartDay(false);
	}
}

bool ACigkofteGameMode::HasExistingSave() const
{
	const UCigSaveSubsystem* SaveSys = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigSaveSubsystem>() : nullptr;
	return SaveSys && SaveSys->HasSave();
}

void ACigkofteGameMode::TitleNav(int32 Dir)
{
	TitleCursor = (TitleCursor + Dir + TitleItemCount) % TitleItemCount;
	PlaySound(ECigSound::UINav);
}

void ACigkofteGameMode::TitleSelect()
{
	PlaySound(ECigSound::UIClick);
	switch (TitleCursor)
	{
	case 0: // Continue / Start (starts from the loaded save if there is one)
		StartFirstDayIfIntro();
		break;
	case 1: // New game: wipe the save, clean world
		// A fresh random stream. The subsystem lives on the GameInstance, so it
		// survives a level reload and has to be reseeded explicitly.
		if (UCigRandomSubsystem* Rng = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigRandomSubsystem>() : nullptr)
		{
			Rng->Reseed();
		}
		if (UCigSaveSubsystem* SaveSys = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigSaveSubsystem>() : nullptr)
		{
			if (SaveSys->HasSave())
			{
				SaveSys->DeleteSave();
				RestartGame();
				return;
			}
		}
		StartFirstDayIfIntro();
		break;
	case 2: // Settings
		bSettingsOpen = true;
		break;
	case 3: // Quit
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
		break;
	default:
		break;
	}
}

bool ACigkofteGameMode::IsStationInteractionAvailable(const ACigkofteStation* Station) const
{
	return Station && WorldBuilder && WorldBuilder->FindStation(Station->StationType) == Station;
}

void ACigkofteGameMode::HandleInteract(ACigkofteStation* Station)
{
	if (!Days)
	{
		return;
	}
	if (Days->Phase == ECigPhase::Intro)
	{
		// The first press skips the splash; after that it picks a menu entry
		if (IntroTime < SplashDuration)
		{
			IntroTime = SplashDuration;
		}
		else if (!bSettingsOpen)
		{
			TitleSelect();
		}
		return;
	}
	if (Station && !IsStationInteractionAvailable(Station))
	{
		return;
	}
	// Preparation. Every station works, and the service counter is the door: you
	// step up to it and open the shop. No new key for it - the counter is where
	// a shopkeeper stands to open, and during service it is where they serve,
	// which is the same context-by-state pattern every other station already
	// follows.
	if (Days->Phase == ECigPhase::Opening)
	{
		if (Station && Station->StationType == ECigStation::Servis)
		{
			Days->OpenShop();
			return;
		}
		// Anything else is preparation and falls through to the normal handling.
	}
	else if (Days->Phase == ECigPhase::Closing)
	{
		// The counter is the door at both ends of the day: it opens the shop in
		// the morning and closes the books at night. Everything else is cleaning
		// and falls through.
		if (Station && Station->StationType == ECigStation::Servis)
		{
			Days->FinishClosing();
			return;
		}
	}
	else if (Days->Phase != ECigPhase::Playing)
	{
		return;
	}

	if (!Station)
	{
		// E on empty space: check for a delivery point
		if (Delivery)
		{
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				Delivery->TryDeliverAt(Pawn->GetActorLocation());
			}
		}
		return;
	}

	// Level lock: a station that has not unlocked yet does nothing.
	if (Station->IsLocked())
	{
		AddMessage(CigText::Format(TEXT("msg.station.locked"),
			*CigStationLabel(Station->StationType), Station->GetLockLevel()), FLinearColor(0.8f, 0.8f, 0.85f));
		PlaySound(ECigSound::Failure);
		RequestFlash(FLinearColor(0.9f, 0.8f, 0.2f), 0.3f);
		return;
	}

	Station->Pop(); // interaction feedback (game feel)
	if (UCigAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigAudioSubsystem>() : nullptr)
	{
		Audio->PlayStation(Station->StationType);
	}

	switch (Station->StationType)
	{
	case ECigStation::Bulgur:
	case ECigStation::Isot:
	case ECigStation::Salca:
	case ECigStation::Su:
	case ECigStation::Baharat:
		if (Cooking)
		{
			Cooking->AddIngredient(static_cast<ECigIngredient>(static_cast<uint8>(Station->StationType)));
		}
		break;
	case ECigStation::Yogurma:
		if (Orders && Orders->Wrap.bActive && !Orders->Wrap.bWrapped)
		{
			Orders->AddPortion();
		}
		else if (Cooking)
		{
			Cooking->KneadPress();
		}
		break;
	case ECigStation::Servis:
		if (Customers)
		{
			Customers->ServeFront();
		}
		break;
	case ECigStation::Lavabo:
		if (Hygiene)
		{
			Hygiene->WashHands();
		}
		break;
	case ECigStation::Cop:
		if (Orders && Orders->Wrap.bActive)
		{
			Orders->DiscardWrap();
		}
		else if (Cooking && (Cooking->BowlTotal() > 0 || Cooking->Dough.IsValid()))
		{
			Cooking->DumpAll();
		}
		else if (Hygiene)
		{
			Hygiene->EmptyTrash();
		}
		break;
	case ECigStation::Eldiven:
	case ECigStation::IsotPlus:
	case ECigStation::Reklam:
		if (Economy)
		{
			Economy->BuyStationUpgrade(Station->StationType);
		}
		break;
	case ECigStation::Dograma:
		if (Inventory)
		{
			Inventory->ChopPress();
		}
		break;
	case ECigStation::Lavas:
		if (Orders)
		{
			Orders->StartWrap();
		}
		break;
	case ECigStation::Paketleme:
		if (Orders)
		{
			Orders->TogglePack();
		}
		break;
	case ECigStation::YanUrun:
		if (Orders)
		{
			Orders->CycleSide();
		}
		break;
	case ECigStation::Buzdolabi:
		if (Cooking)
		{
			Cooking->FridgeInteract();
		}
		break;
	case ECigStation::Temizlik:
		if (Hygiene)
		{
			Hygiene->CleanSurfaces();
		}
		break;
	case ECigStation::Bulasik:
		if (Hygiene)
		{
			Hygiene->WashDishes();
		}
		break;
	case ECigStation::Cay:
		DrinkTea();
		break;
	case ECigStation::MamaKabi:
		if (CatSys)
		{
			CatSys->Feed();
		}
		break;
	case ECigStation::Tarif:
		if (Cooking)
		{
			Cooking->CycleRecipe();
		}
		break;
	default:
		break;
	}
}

void ACigkofteGameMode::KneadPress()
{
	if (!Days)
	{
		return;
	}
	if (Days->Phase == ECigPhase::Intro)
	{
		Days->StartDay(false);
		return;
	}
	// Kneading is the whole point of the preparation phase.
	if (Days->CanWork() && Cooking)
	{
		Cooking->KneadPress();
	}
}

void ACigkofteGameMode::DrinkTea()
{
	ACigkoftePlayerCharacter* Player = Cast<ACigkoftePlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || !Economy)
	{
		return;
	}
	if (Player->Energy > 85.f)
	{
		AddMessage(CigText::Get(TEXT("msg.tea.fullenergy")), FLinearColor(0.8f, 0.8f, 0.8f));
		return;
	}
	if (!Economy->TrySpend(15))
	{
		AddMessage(CigText::Get(TEXT("msg.tea.nomoney")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}
	Player->Energy = FMath::Min(100.f, Player->Energy + 40.f);
	Player->Stress = FMath::Max(0.f, Player->Stress - 25.f);
	AddMessage(CigText::Get(TEXT("msg.tea.drank")), FLinearColor(0.72f, 0.45f, 0.25f));
}

void ACigkofteGameMode::RestartGame()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")));
}

// ---------------------------------------------------------------- saving

void ACigkofteGameMode::RequestSave()
{
	if (bSaveDisabled)
	{
		return;
	}
	if (UCigSaveSubsystem* SaveSys = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigSaveSubsystem>() : nullptr)
	{
		SaveSys->SaveNow(this);
	}
}

void ACigkofteGameMode::RequestLoad()
{
	// Reopen the map for a clean world; InitGame finds and loads the save.
	RestartGame();
}
