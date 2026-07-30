#include "Debug/CigCheatManager.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Cooking/CigCookingSystem.h"
#include "Quests/CigQuestSystem.h"
#include "Events/CigEventSystem.h"
#include "Delivery/CigDeliverySystem.h"
#include "Cat/CigCatSystem.h"
#include "Staff/CigStaffSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Cat/CigCat.h"
#include "Save/CigSaveSubsystem.h"
#include "World/CigWorldBuilder.h"
#include "Core/CigUpgrades.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigBalance.h"
#include "Inventory/CigStorage.h"
#include "Inventory/CigStockCrate.h"
#include "UI/CigTabletData.h"
#include "Core/CigText.h"
#include "Core/CigLog.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CsvProfiler.h"

ACigkofteGameMode* UCigCheatManager::GM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACigkofteGameMode>() : nullptr;
}

namespace
{
	UCigRandomSubsystem* CheatRng(const UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UCigRandomSubsystem>() : nullptr;
	}
}

void UCigCheatManager::CigSeed()
{
	UCigRandomSubsystem* Rng = CheatRng(GetWorld());
	if (!Rng)
	{
		return;
	}
	const FString Text = FString::Printf(TEXT("[DEBUG] RNG seed %d (durum %d, %lld çekim)"),
		Rng->InitialSeed(), Rng->StateSeed(), Rng->DrawCount());
	UE_LOG(LogCig, Log, TEXT("%s"), *Text);
	if (ACigkofteGameMode* Mode = GM())
	{
		Mode->AddMessage(Text);
	}
}

void UCigCheatManager::CigReloadBalance()
{
	CigBalance::Reload();
	if (ACigkofteGameMode* Mode = GM())
	{
		// State that is already established (bought upgrades, spent ranks) does not
		// change; the new numbers apply to calculations from here on.
		Mode->AddMessage(TEXT("[DEBUG] Denge tabloları yeniden yüklendi."));
	}
}

void UCigCheatManager::CigTabletDump()
{
	ACigkofteGameMode* Mode = GM();
	if (!Mode)
	{
		return;
	}
	for (int32 i = 0; i < (int32)ECigTabletTab::COUNT; ++i)
	{
		const ECigTabletTab Tab = (ECigTabletTab)i;
		const int32 Count = CigTablet::BuildRows(Mode, Tab).Num();
		UE_LOG(LogCig, Log, TEXT("TABLET sekme %d (%s): %d satir"), i, *CigTablet::TabName(Tab), Count);
	}
}

void UCigCheatManager::CigSetSeed(int32 Seed)
{
	UCigRandomSubsystem* Rng = CheatRng(GetWorld());
	if (!Rng)
	{
		return;
	}
	Rng->SeedWith(Seed);
	if (ACigkofteGameMode* Mode = GM())
	{
		Mode->AddMessage(FString::Printf(TEXT("[DEBUG] RNG seed %d olarak ayarlandı."), Seed));
	}
}

void UCigCheatManager::StartDayNow()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		Mode->StartFirstDayIfIntro();
		// A day now begins in preparation. Every one of these callers wants
		// the shop trading - a screenshot of an empty queue, a benchmark of
		// an idle room and a demo reel of nobody arriving are all the wrong
		// thing - so they open it themselves.
		if (Mode->Days) { Mode->Days->OpenShop(); }
	}
}

void UCigCheatManager::AddMoney(int32 Amount)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Economy)
		{
			Mode->Economy->Money += Amount;
			Mode->AddMessage(FString::Printf(TEXT("[DEBUG] Para +%d"), Amount));
		}
	}
}

void UCigCheatManager::AddSkillPoints(int32 Amount)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Skills)
		{
			Mode->Skills->GrantPoint(FMath::Max(1, Amount));
		}
	}
}

void UCigCheatManager::UpgradeSkill(int32 Index)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Skills && Index >= 0 && Index < (int32)ECigSkill::COUNT)
		{
			Mode->Skills->Upgrade((ECigSkill)Index);
		}
	}
}

void UCigCheatManager::DoPrestige()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Skills)
		{
			Mode->Skills->DoPrestige();
		}
	}
}

void UCigCheatManager::GiveSides(int32 Amount)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Inventory)
		{
			const int32 N = FMath::Max(1, Amount);
			Mode->Inventory->Add(CigStockIcliKofte, N);
			Mode->Inventory->Add(CigStockCorba, N);
			Mode->Inventory->Add(CigStockKunefe, N);
			Mode->Inventory->Add(CigStockCayBardak, N);
			Mode->AddMessage(FString::Printf(TEXT("[DEBUG] Yan ürün stoğu +%d"), N));
		}
	}
}

void UCigCheatManager::SetMoney(int32 Amount)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Economy)
		{
			Mode->Economy->Money = Amount;
		}
	}
}

void UCigCheatManager::SetDay(int32 Day)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Days)
		{
			Mode->Days->Day = FMath::Max(1, Day);
			Mode->AddMessage(FString::Printf(TEXT("[DEBUG] Gün = %d"), Mode->Days->Day));
		}
	}
}

void UCigCheatManager::SkipDay()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Days && Mode->Days->IsPlaying())
		{
			Mode->Days->TimeLeft = 0.5f;
		}
	}
}

void UCigCheatManager::SpawnCustomer()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Customers)
		{
			Mode->Customers->SpawnCustomer();
		}
	}
}

void UCigCheatManager::SpawnInfluencer()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Customers)
		{
			Mode->Customers->SpawnCustomer(false, true);
		}
	}
}

void UCigCheatManager::SetReputation(float Rep)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Progression)
		{
			Mode->Progression->Rep = FMath::Clamp(Rep, 0.f, 100.f);
		}
	}
}

void UCigCheatManager::SetHygiene(float Value)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Hygiene)
		{
			// Pulls overall hygiene roughly to the target by levelling all the dirt
			const float Dirt = FMath::Clamp(100.f - Value, 0.f, 100.f);
			Mode->Hygiene->HandDirt = Dirt;
			Mode->Hygiene->CounterDirt = Dirt;
			Mode->Hygiene->ChopDirt = Dirt;
			Mode->Hygiene->TrashFill = Dirt;
			Mode->Hygiene->DishPile = Dirt;
			Mode->Hygiene->CatFur = Dirt;
		}
	}
}

void UCigCheatManager::CompleteQuest()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Quests)
		{
			Mode->Quests->CompleteQuestNow();
		}
	}
}

void UCigCheatManager::TriggerInspector()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Customers)
		{
			Mode->Customers->TriggerInspectorNow();
		}
	}
}

void UCigCheatManager::TriggerEvent(int32 EventIndex)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Events)
		{
			Mode->Events->TriggerEvent(EventIndex);
		}
	}
}

void UCigCheatManager::RefillStock()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Inventory)
		{
			// Filled to what the shop can actually hold, not to a flat 30.
			//
			// Thirty of everything is 300 cold units against a 60 capacity, and
			// the tablet duly read "Buzdolabı 300/40" with every perishable
			// marked full - a debug command producing a state the game's own
			// rules forbid, which then looks like the rule is broken.
			const bool bBigFridge = Mode->Economy && Mode->Economy->HasUpgrade(ECigUpgrade::BuyukBuzdolabi);
			for (int32 i = 0; i < CigStockCount; ++i)
			{
				const int32 Room = CigStorage::RoomFor(Mode->Inventory->Stock, i, bBigFridge);
				Mode->Inventory->Stock[i] += FMath::Min(Room, FMath::Max(0, 30 - Mode->Inventory->Stock[i]));
			}
			Mode->AddMessage(TEXT("[DEBUG] Stoklar dolduruldu"));
		}
	}
}

void UCigCheatManager::UnlockRecipe()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Cooking)
		{
			Mode->Cooking->UnlockSecretRecipe();
		}
	}
}

void UCigCheatManager::UnlockAllUpgrades()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Economy)
		{
			for (int32 i = 0; i < (int32)ECigUpgrade::COUNT; ++i)
			{
				if (!Mode->Economy->UpgradeOwned[i])
				{
					Mode->Economy->UpgradeOwned[i] = true;
					if (Mode->WorldBuilder)
					{
						Mode->WorldBuilder->ApplyUpgradeVisual(i);
					}
				}
			}
			Mode->AddMessage(TEXT("[DEBUG] Tüm upgrade'ler açıldı"));
		}
	}
}

void UCigCheatManager::SetTimeScale(float Scale)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(Scale, 0.1f, 10.f));
	}
}

void UCigCheatManager::StartDelivery()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Delivery)
		{
			Mode->Delivery->SpawnOrder();
		}
	}
}

void UCigCheatManager::SaveGame()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		Mode->RequestSave();
	}
}

void UCigCheatManager::LoadGame()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		Mode->RequestLoad();
	}
}

void UCigCheatManager::ResetSave()
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (UCigSaveSubsystem* SaveSys = Mode->GetGameInstance() ? Mode->GetGameInstance()->GetSubsystem<UCigSaveSubsystem>() : nullptr)
		{
			SaveSys->DeleteSave();
			Mode->AddMessage(TEXT("[DEBUG] Kayıt silindi (yeni oyun için yeniden başlat)"));
		}
	}
}

void UCigCheatManager::AddXPCheat(int32 Amount)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->Progression)
		{
			Mode->Progression->AddXP(Amount);
		}
	}
}

void UCigCheatManager::RenameCat(const FString& NewName)
{
	if (ACigkofteGameMode* Mode = GM())
	{
		if (Mode->CatSys)
		{
			Mode->CatSys->SetCatName(NewName);
		}
	}
}

// ============================== Showcase tour ==============================

void UCigCheatManager::CigShots()
{
	ShotIndex = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ShotTimer, this, &UCigCheatManager::ShotStep, 1.6f, true, 1.f);
	}
}

void UCigCheatManager::CigTour()
{
	ShotIndex = 0;
	if (UWorld* World = GetWorld())
	{
		// The first step is delayed so districts can open and meshes can compile
		World->GetTimerManager().SetTimer(ShotTimer, this, &UCigCheatManager::TourStep, 2.2f, true, 2.f);
	}
}

void UCigCheatManager::TourStep()
{
	UWorld* World = GetWorld();
	ACigkofteGameMode* Mode = GM();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !Mode || !PC)
	{
		return;
	}

	auto Place = [PC](const FVector& Loc, float Yaw, float Pitch)
	{
		if (APawn* P = PC->GetPawn())
		{
			P->SetActorLocation(Loc);
		}
		PC->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	};
	auto Shot = [](const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(FString(Name), /*bShowUI=*/false, /*bAddFilenameSuffix=*/false);
	};

	switch (ShotIndex++)
	{
	case 0: // start the day, unlock every district, fill the stock
		Mode->StartFirstDayIfIntro();
		// A day now begins in preparation. Every one of these callers wants
		// the shop trading - a screenshot of an empty queue, a benchmark of
		// an idle room and a demo reel of nobody arriving are all the wrong
		// thing - so they open it themselves.
		if (Mode->Days) { Mode->Days->OpenShop(); }
		if (Mode->Economy) { Mode->Economy->Money = 12000; }
		if (Mode->Progression) { Mode->Progression->AddXP(3600); }
		RefillStock();
		GiveSides(20);
		// Clear weather, for the same reason CigShots clears it: this tour is how
		// a visual change gets checked, and rain streaks across every frame make
		// two passes hard to compare.
		if (Mode->Events) { Mode->Events->TumOlaylariBitir(); }
		break;

	case 1: // shop interior: counters and the new plate/bowl props
		Place(FVector(-250.f, 0.f, 150.f), 0.f, -8.f);
		break;
	case 2:
		Shot(TEXT("tour_01_dukkan"));
		break;

	case 3: // seating area: cafe tables and chairs
		Place(FVector(-150.f, 750.f, 150.f), 120.f, -10.f);
		break;
	case 4:
		Shot(TEXT("tour_02_oturma"));
		break;

	case 5: // street: CityPark buildings, trees, lamp posts
		Place(FVector(-1800.f, 0.f, 220.f), 180.f, -5.f);
		break;
	case 6:
		Shot(TEXT("tour_03_cadde"));
		break;

	case 7: // Cumhuriyet Meydani: the fountain and the statue
		Place(FVector(-1800.f, 9600.f, 220.f), 90.f, -6.f);
		break;
	case 8:
		Shot(TEXT("tour_04_meydan"));
		break;

	case 9: // Semt Pazari: bazaar stalls, spices, sacks
		Place(FVector(-1800.f, -9800.f, 220.f), -90.f, -6.f);
		break;
	case 10:
		Shot(TEXT("tour_05_pazar"));
		break;

	case 11: // cat: a close-up of the animated model
		if (Mode->CatSys && Mode->CatSys->Cat)
		{
			const FVector CatPos = Mode->CatSys->Cat->GetActorLocation();
			Place(CatPos + FVector(160.f, 0.f, 70.f), 180.f, -18.f);
		}
		break;
	case 12:
		Shot(TEXT("tour_06_kedi"));
		break;

	default:
		UE_LOG(LogCig, Log, TEXT("CigTour tamam."));
		PC->ConsoleCommand(TEXT("quit"), true);
		break;
	}
}

// ---------------------------------------------------------------- benchmark

namespace
{
	// Where the benchmark stands, and what each viewpoint is there to cost.
	// Shared with CigTour on purpose: a route that drifts from the one the
	// screenshots use stops being comparable to what anybody has looked at.
	struct FCigBenchStop
	{
		const TCHAR* Name;
		FVector Location;
		float Yaw;
		float Pitch;
	};

	// Any fixed value would do; what matters is that it is the same one every
	// run, so two builds are compared against the same day.
	constexpr int32 CigBenchSeed = 1337;

	const FCigBenchStop CigBenchStops[] = {
		{ TEXT("dukkan"), FVector(-250.f,     0.f, 150.f),   0.f,  -8.f },
		{ TEXT("oturma"), FVector(-150.f,   750.f, 150.f), 120.f, -10.f },
		{ TEXT("cadde"),  FVector(-1800.f,    0.f, 220.f), 180.f,  -5.f },
		{ TEXT("meydan"), FVector(-1800.f, 9600.f, 220.f),  90.f,  -6.f },
		{ TEXT("pazar"),  FVector(-1800.f,-9800.f, 220.f), -90.f,  -6.f },
	};

	constexpr int32 CigBenchStopCount = UE_ARRAY_COUNT(CigBenchStops);
}

void UCigCheatManager::CigBench(int32 SecondsPerStop)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Clamped rather than trusted: a zero or negative period makes SetTimer fire
	// every frame, and the run would teleport through the whole route before the
	// profiler had a single frame at any of it.
	const float Period = (float)FMath::Clamp(SecondsPerStop, 2, 120);

	BenchIndex = 0;
	UE_LOG(LogCig, Log, TEXT("CigBench: %d durak x %.0f sn."), CigBenchStopCount, Period);

	// The first step is delayed by one period for the same reason CigTour delays
	// its own: districts open and shaders compile in those first seconds, and
	// measuring them would report a hitch that no player ever sees twice.
	World->GetTimerManager().SetTimer(BenchTimer, this, &UCigCheatManager::BenchStep, Period, true, Period);
}

void UCigCheatManager::BenchStep()
{
	UWorld* World = GetWorld();
	ACigkofteGameMode* Mode = GM();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !Mode || !PC)
	{
		return;
	}

	const int32 Step = BenchIndex++;

	if (Step == 0)
	{
		// The heaviest state the game reaches, reproduced the same way each run.
		if (UCigRandomSubsystem* Rng = CheatRng(World))
		{
			Rng->SeedWith(CigBenchSeed);
		}
		Mode->StartFirstDayIfIntro();
		// A day now begins in preparation. Every one of these callers wants
		// the shop trading - a screenshot of an empty queue, a benchmark of
		// an idle room and a demo reel of nobody arriving are all the wrong
		// thing - so they open it themselves.
		if (Mode->Days) { Mode->Days->OpenShop(); }
		if (Mode->Economy) { Mode->Economy->Money = 12000; }
		if (Mode->Progression) { Mode->Progression->AddXP(3600); }
		RefillStock();
		GiveSides(20);
		UnlockAllUpgrades();
		return;
	}

	const int32 StopIndex = Step - 1;
	if (StopIndex >= CigBenchStopCount)
	{
		UE_LOG(LogCig, Log, TEXT("CigBench tamam: %d durak."), CigBenchStopCount);
		PC->ConsoleCommand(TEXT("CsvProfile Stop"), true);
		World->GetTimerManager().ClearTimer(BenchTimer);
		PC->ConsoleCommand(TEXT("quit"), true);
		return;
	}

	const FCigBenchStop& Stop = CigBenchStops[StopIndex];
	if (APawn* P = PC->GetPawn())
	{
		P->SetActorLocation(Stop.Location);
	}
	PC->SetControlRotation(FRotator(Stop.Pitch, Stop.Yaw, 0.f));

	if (StopIndex == 0)
	{
		// Started here, not in CigBench, so the capture never contains the world
		// build or the first-frame shader work.
		PC->ConsoleCommand(TEXT("CsvProfile Start"), true);

		// The setup step calls RefillStock, GiveSides and UnlockAllUpgrades, and
		// each announces itself in the message feed. Harmless in a profile and
		// not harmless in the demo recording, which uses this same route: the
		// clip opened on "[DEBUG] Stoklar dolduruldu".
		Mode->Messages.Empty();
	}

	// One event per stop. Without it the CSV is a single run of frames and the
	// one expensive viewpoint disappears into the average of the other four.
	//
	// Emitted next tick, not now. The profiler begins capturing on the frame
	// after the Start above, so the first stop's marker was written into a
	// capture that did not exist yet and was silently dropped - leaving the one
	// section the shop is actually played in unsliceable. Every stop takes the
	// same path so they are all offset identically.
	const TCHAR* StopName = Stop.Name;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([StopName]()
	{
		CSV_EVENT_GLOBAL(TEXT("CigBench/%s"), StopName);
	}));

	UE_LOG(LogCig, Log, TEXT("CigBench durak %d/%d: %s"), StopIndex + 1, CigBenchStopCount, Stop.Name);
}

void UCigCheatManager::ShotStep()
{
	UWorld* World = GetWorld();
	ACigkofteGameMode* Mode = GM();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !Mode || !PC)
	{
		return;
	}

	// Places the player at a point and points them in the requested direction.
	auto Place = [PC](const FVector& Loc, float Yaw, float Pitch)
	{
		if (APawn* P = PC->GetPawn())
		{
			P->SetActorLocation(Loc);
		}
		PC->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	};
	auto Shot = [World, Mode](const TCHAR* Name)
	{
		// The message feed is cleared first. Setting the shop up for a shot means
		// calling RefillStock and GiveSides, and those announce themselves with
		// "[DEBUG] Stoklar dolduruldu" - which then sits in the corner of the
		// screenshot at the top of the README. The feed is real UI and belongs in
		// the picture; the scaffolding that built the scene does not.
		if (Mode) { Mode->Messages.Empty(); }
		FScreenshotRequest::RequestScreenshot(FString(Name), /*bShowUI=*/true, /*bAddFilenameSuffix=*/false);
	};

	switch (ShotIndex++)
	{
	case 0: // show the shop stocked and upgraded
		// English, because these pictures are read on GitHub.
		//
		// The repository is English throughout and every screenshot in it was a
		// Turkish HUD, so the one part of the README a visitor cannot skim was the
		// part doing the explaining. The game still ships Turkish first - this
		// changes the shot, not the default. Station signs stay as they are: isot
		// and salça are what the ingredients are called.
		//
		// Through the settings path rather than CigText::SetLanguage directly. The
		// world's signage is written once when a lock is applied, not rebuilt each
		// frame, so setting the language alone left "SEVIYE 6" standing in an
		// otherwise English shop - which is what a player switching language would
		// have seen too.
		Mode->Settings.Language = 1;
		Mode->ApplySettings();
		Mode->StartFirstDayIfIntro();
		// A day now begins in preparation. Every one of these callers wants
		// the shop trading - a screenshot of an empty queue, a benchmark of
		// an idle room and a demo reel of nobody arriving are all the wrong
		// thing - so they open it themselves.
		if (Mode->Days) { Mode->Days->OpenShop(); }
		if (Mode->Economy) { Mode->Economy->Money = 5400; }
		if (Mode->Progression) { Mode->Progression->AddXP(900); Mode->Progression->Rep = 82.f; }
		RefillStock();
		// Clear weather, deliberately.
		//
		// Rain, heat and power cuts are rolled from the day's dice, and the last
		// screenshot pass came back with rain streaks over every frame including
		// the one at the top of the README. A hero shot is a choice, not a die
		// roll - the weather layers stay in the game and out of the pictures.
		if (Mode->Events) { Mode->Events->TumOlaylariBitir(); }
		// A delivery standing by the door for the whole tour. Queued with no time
		// left rather than ordered: a real order takes the supplier's delivery
		// time, which is longer than this entire run.
		if (Mode->Inventory)
		{
			FCigPendingOrder Kasa;
			Kasa.Item = CigStockMarul;
			Kasa.Amount = 9;
			Kasa.Quality = 1.f;
			Kasa.TimeLeft = 0.f;
			Mode->Inventory->PendingOrders.Add(Kasa);
		}
		break;

	case 1: // called early so the customers have time to walk in
		if (Mode->Customers)
		{
			Mode->Customers->SpawnCustomer();
			Mode->Customers->SpawnCustomer();
			Mode->Customers->SpawnCustomer(true);
		}
		break;

	case 2: // nothing: one step of walking time
		// The customers spawned last step start out on the street. At the old
		// spacing the service shot was taken while they were still crossing it,
		// so the picture at the top of the README had an empty counter and three
		// people in the middle distance. An idle step is the cheapest fix.
		break;

	case 3: // the shop front: seating, awning, the queue and the street
		// Two attempts at reframing this as a counter shot were abandoned, and
		// the reason is worth writing down: the service counter at (-600, 0)
		// does not read from the shop floor at any pitch. Its station spawns and
		// its label draws, but there is no counter to photograph - which is a
		// defect in the world, not in the framing, and is recorded in
		// KNOWN_LIMITATIONS.md rather than papered over with a camera angle.
		//
		// So this stays the establishing shot it always was, and the kitchen
		// leads the README instead.
		Place(FVector(120.f, 0.f, 110.f), 180.f, -7.f);
		break;

	case 4:
		Shot(TEXT("01_servis"));
		break;

	case 5: // ingredient stations - the shot that leads the README
		// Off the centre line on purpose. Standing at y=0 puts SALCA at (600,0)
		// and the locked IsotPlus at (870,0) in exact line, so the back one's
		// "SEVIYE 6" printed across the front one's name. Ninety units to the
		// side separates them, and a counter seen slightly off-axis is a better
		// photograph than one seen dead on.
		Place(FVector(300.f, 90.f, 110.f), -6.f, -14.f);
		// A measure is taken on the way past, so the shot catches the scoop in
		// the air. It is the one piece of motion at this station and a still
		// frame of an idle counter does not show it exists.
		if (Mode->Cooking) { Mode->Cooking->AddIngredient(ECigIngredient::Salca); }
		break;

	case 6:
		Shot(TEXT("02_mutfak"));
		break;

	case 7: // kneading station
		Place(FVector(-30.f, -640.f, 110.f), 0.f, -12.f);
		break;

	case 8:
		Shot(TEXT("03_yogurma"));
		break;

	case 9: // hire, so there is a body in the shop doing the job
		// The apprentice is the only figure the player ever sees work: the
		// player themselves is in first person. Without one hired, every
		// working animation in the build is unreachable from a screenshot.
		if (Mode->Staff && !Mode->Staff->Apprentice.bHired)
		{
			Mode->Staff->Hire();
		}
		break;

	case 10: // the chopping board, part-way through a garnish
		// Two strokes in, so the board carries a part-cut head and the pieces
		// taken off it. An untouched board shows only the head, which says
		// nothing about what chopping looks like.
		if (Mode->Inventory)
		{
			Mode->Inventory->ChopPress();
			Mode->Inventory->ChopPress();
		}
		Place(FVector(380.f, 700.f, 130.f), 10.f, -22.f);
		break;

	case 11:
		Shot(TEXT("07_dograma"));
		break;

	case 12: // the apprentice, working
		// Framed off the NPC rather than from a fixed spot: the staff system
		// moves them to whichever station their job is at, so a hardcoded camera
		// finds an empty counter about four times in five.
		if (Mode->Staff && Mode->Staff->ApprenticeNPC)
		{
			const FVector NPC = Mode->Staff->ApprenticeNPC->GetActorLocation();
			Place(NPC + FVector(-190.f, -120.f, 130.f), 32.f, -8.f);
		}
		break;

	case 13:
		Shot(TEXT("08_cirak"));
		break;

	case 14: // tablet: stock, where the fridge line lives
		// The flag alone does nothing visible. The tablet is a UMG widget
		// and RefreshTabletWidget is what puts it on the viewport, so
		// setting bTabletOpen by hand - which this helper has always done -
		// produced a screenshot of the room with no tablet in it. Both
		// "tablet" pictures in the README were that.
		Mode->bTabletOpen = true;
		Mode->TabletTab = ECigTabletTab::Stok;
		Mode->RefreshTabletWidget();
		break;

	case 15:
		Shot(TEXT("09_tablet_stok"));
		break;

	case 16: // tablet: shop upgrades
		Mode->TabletTab = ECigTabletTab::Dukkan;
		Mode->RefreshTabletWidget();
		break;

	case 17:
		Shot(TEXT("04_tablet_dukkan"));
		break;

	case 18:
		Mode->TabletTab = ECigTabletTab::Gorevler;
		Mode->RefreshTabletWidget();
		break;

	case 19:
		Shot(TEXT("05_tablet_gorevler"));
		break;

	case 20:
		Mode->bTabletOpen = false;
		Mode->RefreshTabletWidget();
		Place(FVector(-100.f, 700.f, 110.f), 135.f, -10.f);
		break;

	case 21:
		Shot(TEXT("06_salon"));
		break;

	case 22: // the delivery, framed off the crate itself
		// Pointed at the actor rather than at where the actor is supposed to be.
		// Two attempts at guessing this from the shop layout produced frames with
		// no crate in them, one of which was hiding a real bug - the crate was at
		// the world origin - and one of which was simply the wrong direction.
		//
		// Standing 200 units off it and looking straight at it produced a frame of
		// the street: the shop front is open, so a short lens that close to the
		// counter looks out over it and past the crate entirely. Pulled back into
		// the room instead, wide enough that the counter run and the crate are both
		// in shot - the picture is meant to show a delivery blocking the place it
		// arrived in, which needs the place in it too.
		if (Mode->Inventory && Mode->Inventory->Crates.Num() > 0)
		{
			if (const ACigStockCrate* Kasa = Mode->Inventory->Crates[0].Get())
			{
				const FVector K = Kasa->GetActorLocation();
				const FVector Cam(-120.f, -680.f, 190.f);
				const FVector D = K - Cam;
				Place(Cam, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)),
					FMath::RadiansToDegrees(FMath::Atan2(D.Z + 20.f, D.Size2D())));
			}
		}
		break;

	case 23:
		Shot(TEXT("10_teslimat"));
		break;

	default:
		World->GetTimerManager().ClearTimer(ShotTimer);
		UE_LOG(LogCig, Log, TEXT("CigShots tamam."));
		PC->ConsoleCommand(TEXT("quit"), true);
		break;
	}
}
