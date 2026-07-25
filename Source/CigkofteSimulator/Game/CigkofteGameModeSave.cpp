// The GameMode's save marshalling: the two large functions that write system
// state into UCigSaveGame and read it back.
//
// Why a separate file: these 440 lines took up a third of CigkofteGameMode.cpp
// and have nothing to do with coordination logic - it is field-by-field
// copying. In its own translation unit GameMode becomes readable, and touching
// the save schema no longer rebuilds the coordination code.
//
// The functions are still ACigkofteGameMode members: they need access to every
// system, and changing the signature would ripple through all the callers.

#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "World/CigWorldBuilder.h"
#include "Cooking/CigCookingSystem.h"
#include "Orders/CigOrderSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigPricingSystem.h"
#include "Events/CigEventSystem.h"
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
#include "Vehicles/CigCar.h"
#include "Save/CigSaveGame.h"
#include "Core/CigLog.h"
#include "Core/CigUnlocks.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigText.h"
#include "Core/CigInput.h"

void ACigkofteGameMode::CaptureSave(UCigSaveGame& Save) const
{
	Save.Day = Days ? Days->Day : 1;

	if (Economy)
	{
		Save.Money = Economy->Money;
		Save.PricePolicy = Economy->PricePolicy;
		Save.GloveLevel = Economy->GloveLevel;
		Save.IsotLevel = Economy->IsotLevel;
		Save.AdCount = Economy->AdCount;
		Save.bOwnHouse = Economy->bOwnHouse;
		Save.UpgradeOwned.SetNum((int32)ECigUpgrade::COUNT);
		for (int32 i = 0; i < (int32)ECigUpgrade::COUNT; ++i)
		{
			Save.UpgradeOwned[i] = Economy->UpgradeOwned[i];
		}
		Save.CurrentSupplier = Economy->CurrentSupplier;
		Save.IngredientTier = Economy->IngredientTier;
		Save.SupplierRelation.SetNum(CigSupplierCount);
		for (int32 i = 0; i < CigSupplierCount; ++i)
		{
			Save.SupplierRelation[i] = Economy->SupplierRelation[i];
		}
	}

	if (Pricing)
	{
		Save.UrunCarpanlari.SetNum(CigUrunCount);
		for (int32 i = 0; i < CigUrunCount; ++i)
		{
			Save.UrunCarpanlari[i] = Pricing->Carpanlar[i];
		}
	}

	if (Skills)
	{
		Save.SkillPoints = Skills->Points;
		Save.SkillSpent = Skills->Spent;
		Save.PrestigeCount = Skills->PrestigeCount;
		Save.SkillRanks.SetNum((int32)ECigSkill::COUNT);
		for (int32 i = 0; i < (int32)ECigSkill::COUNT; ++i)
		{
			Save.SkillRanks[i] = Skills->Rank[i];
		}
	}

	if (Achievements)
	{
		Save.AchievementsUnlocked.SetNum((int32)ECigAchievement::COUNT);
		for (int32 i = 0; i < (int32)ECigAchievement::COUNT; ++i)
		{
			Save.AchievementsUnlocked[i] = Achievements->Unlocked[i];
		}
	}

	if (Progression)
	{
		Save.XP = Progression->XP;
		Save.Level = Progression->Level;
		Save.Rep = Progression->Rep;
		Save.TotalServed = Progression->TotalServed;
		Save.TotalDeliveries = Progression->TotalDeliveries;
		Save.TotalEarned = Progression->TotalEarned;
		Save.BestDayEarnings = Progression->BestDayEarnings;
		Save.TotalAngryCustomers = Progression->TotalAngryCustomers;
		Save.TotalPerfectOrders = Progression->TotalPerfectOrders;
	}

	if (Inventory)
	{
		Save.Stock.SetNum(CigStockCount);
		Save.StockQuality.SetNum(CigStockCount);
		for (int32 i = 0; i < CigStockCount; ++i)
		{
			Save.Stock[i] = Inventory->Stock[i];
			Save.StockQuality[i] = Inventory->StockQuality[i];
		}
		Save.Garnish = Inventory->Garnish;
	}

	if (Cooking)
	{
		Save.CurrentRecipe = Cooking->CurrentRecipe;
		Save.bSecretRecipeUnlocked = Cooking->bSecretRecipeUnlocked;
	}

	if (Hygiene)
	{
		Save.CounterDirt = Hygiene->CounterDirt;
		Save.TrashFill = Hygiene->TrashFill;
		Save.DishPile = Hygiene->DishPile;
	}

	if (Customers)
	{
		Save.Loyals.Empty();
		for (const FCigLoyalCustomer& L : Customers->Loyals)
		{
			FCigSaveLoyal S;
			S.Id = L.Id;
			S.Name = L.Name;
			S.Seed = L.Seed;
			S.FavSpice = (uint8)L.Favorite.Spice;
			S.FavPortion = L.Favorite.Portion;
			S.FavToppings = L.Favorite.ToppingMask;
			S.bFavAyran = L.Favorite.bWantsAyran;
			S.bFavPacked = L.Favorite.bPacked;
			S.FavSide = (uint8)L.Favorite.Side;
			S.Visits = L.Visits;
			S.Satisfaction = L.Satisfaction;
			S.Trust = L.Trust;
			S.LastVisitDay = L.LastVisitDay;
			S.AvgTip = L.AvgTip;
			S.Traits = L.Traits;
			S.RememberedMistakes = L.RememberedMistakes;
			Save.Loyals.Add(S);
		}
		Save.NextLoyalId = Customers->NextLoyalId;
	}

	if (Rivals)
	{
		Save.Rivals.Empty();
		for (const FCigRival& R : Rivals->Rivals)
		{
			FCigSaveRival S;
			S.Name = R.Name;
			S.Price = R.Price;
			S.Quality = R.Quality;
			S.Hygiene = R.Hygiene;
			S.Popularity = R.Popularity;
			S.AdPower = R.AdPower;
			S.Capacity = R.Capacity;
			S.Attitude = R.Attitude;
			S.bOpen = R.bOpen;
			S.BadDays = R.BadDays;
			Save.Rivals.Add(S);
		}
	}

	if (Reviews)
	{
		Save.Reviews.Empty();
		for (const FCigReview& R : Reviews->Reviews)
		{
			FCigSaveReview S;
			S.Author = R.Author;
			S.Text = R.Text;
			S.Stars = R.Stars;
			S.Day = R.Day;
			Save.Reviews.Add(S);
		}
		Save.FoodScore = Reviews->FoodScore;
		Save.ServiceScore = Reviews->ServiceScore;
		Save.PriceScore = Reviews->PriceScore;
		Save.HygieneScore = Reviews->HygieneScore;
		Save.AtmosphereScore = Reviews->AtmosphereScore;
	}

	if (Staff)
	{
		const FCigApprentice& A = Staff->Apprentice;
		Save.bApprenticeHired = A.bHired;
		Save.ApprenticeName = A.Name;
		Save.ApprenticeLevel = A.Level;
		Save.ApprenticeXP = A.XP;
		Save.ApprenticeMorale = A.Morale;
		Save.ApprenticeSalary = A.Salary;
		Save.ApprenticeTask = (uint8)A.Task;
		Save.ApprenticeSpec = (uint8)A.Spec;
		Save.ApprenticeDaysSinceRaise = A.DaysSinceRaise;
		Save.bApprenticeWantsRaise = A.bWantsRaise;
		Save.ApprenticeArketip = A.Arketip;
		Save.ApprenticeHiz = A.Hiz;
		Save.ApprenticeTitizlik = A.Titizlik;
		Save.ApprenticeGulerYuz = A.GulerYuz;
		Save.ApprenticeOdenmemisGun = A.OdenmemisGun;
		Save.StaffTransferTeklifi = Staff->TransferTeklifi;
	}

	if (Events)
	{
		const FCigTopluSiparis& T = Events->TopluSiparis;
		Save.bTopluTeklifVar = T.bTeklifVar;
		Save.bTopluKabulEdildi = T.bKabulEdildi;
		Save.TopluTeslimGunu = T.TeslimGunu;
		Save.TopluIstenenAdet = T.IstenenAdet;
		Save.TopluOdul = T.Odul;
		Save.TopluBaslangicServis = T.BaslangicServis;
	}

	if (CatSys)
	{
		Save.CatName = CatSys->CatName;
		Save.CatFood = CatSys->Food;
		Save.CatAttention = CatSys->Attention;
	}

	if (PlayerCar)
	{
		Save.CarFuel = PlayerCar->Fuel;
		Save.CarDamage = PlayerCar->Damage;
	}

	if (Quests)
	{
		Save.StoryStage = (uint8)Quests->StoryStage;
		Save.StoryProgress = Quests->StoryProgress;
		Save.bTutorialActive = Quests->bTutorialActive;
		Save.TutorialStep = (uint8)Quests->TutorialStep;
	}

	// Position of the random stream: the current state is written alongside the
	// starting seed so a load does not replay the same day from scratch.
	if (const UCigRandomSubsystem* Rng = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigRandomSubsystem>() : nullptr)
	{
		Save.RngInitialSeed = Rng->InitialSeed();
		Save.RngStateSeed = Rng->StateSeed();
	}

	Save.Settings.FOV = Settings.FOV;
	Save.Settings.MouseSensitivity = Settings.MouseSensitivity;
	Save.Settings.bHeadBob = Settings.bHeadBob;
	Save.Settings.MasterVolume = Settings.MasterVolume;
	Save.Settings.EffectsVolume = Settings.EffectsVolume;
	Save.Settings.MusicVolume = Settings.MusicVolume;
	Save.Settings.QualityLevel = Settings.QualityLevel;
	Save.Settings.WindowMode = Settings.WindowMode;
	Save.Settings.ResolutionIndex = Settings.ResolutionIndex;
	Save.Settings.UIScaleMult = Settings.UIScaleMult;
	Save.Settings.bScreenFlash = Settings.bScreenFlash;
	Save.Settings.ColorBlindMode = Settings.ColorBlindMode;
	Save.Settings.Language = Settings.Language;
	Save.Settings.KeyBindings = CigInput::SaveBindings();
}

void ACigkofteGameMode::ApplySave(const UCigSaveGame& Save)
{
	// Restore the RNG first, so that even if one of the Apply steps below takes
	// a draw, the stream continues from the right position.
	if (UCigRandomSubsystem* Rng = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCigRandomSubsystem>() : nullptr)
	{
		if (Save.RngStateSeed != 0)
		{
			Rng->RestoreState(Save.RngInitialSeed, Save.RngStateSeed);
		}
		// A 0 means the save predates v3; carry on with the fresh seed produced
		// in Initialize (see MigrateV2ToV3).
	}

	if (Days)
	{
		Days->Day = FMath::Max(1, Save.Day);
	}

	if (Economy)
	{
		Economy->Money = Save.Money;
		Economy->PricePolicy = FMath::Clamp(Save.PricePolicy, 0, 2);
		Economy->GloveLevel = Save.GloveLevel;
		Economy->IsotLevel = Save.IsotLevel;
		Economy->AdCount = Save.AdCount;
		Economy->bOwnHouse = Save.bOwnHouse;
		for (int32 i = 0; i < (int32)ECigUpgrade::COUNT && i < Save.UpgradeOwned.Num(); ++i)
		{
			Economy->UpgradeOwned[i] = Save.UpgradeOwned[i];
			if (Save.UpgradeOwned[i] && WorldBuilder)
			{
				WorldBuilder->ApplyUpgradeVisual(i);
			}
		}
		Economy->CurrentSupplier = FMath::Clamp(Save.CurrentSupplier, 0, CigSupplierCount - 1);
		Economy->IngredientTier = FMath::Clamp(Save.IngredientTier, 0, 2);
		for (int32 i = 0; i < CigSupplierCount && i < Save.SupplierRelation.Num(); ++i)
		{
			Economy->SupplierRelation[i] = Save.SupplierRelation[i];
		}
		if (Save.bOwnHouse && WorldBuilder)
		{
			WorldBuilder->SetHouseOwned();
		}
	}

	if (Pricing)
	{
		// A short array means a pre-v7 save, or a table that has grown since;
		// either way the missing products stay at list price. Clamping to the
		// table's range also repairs a markup that a balance edit put out of
		// bounds after the save was written.
		for (int32 i = 0; i < CigUrunCount; ++i)
		{
			const FCigPricingRow& Row = CigBalance::Pricing(i);
			const float Kayitli = Save.UrunCarpanlari.IsValidIndex(i) ? Save.UrunCarpanlari[i] : 1.f;
			Pricing->Carpanlar[i] = FMath::Clamp(Kayitli, Row.MinCarpan, Row.MaxCarpan);
		}
	}

	if (Progression)
	{
		Progression->XP = Save.XP;
		Progression->Level = FMath::Clamp(Save.Level, 1, UCigProgressionSystem::MaxLevel);
		Progression->Rep = FMath::Clamp(Save.Rep, 0.f, 100.f);
		Progression->TotalServed = Save.TotalServed;
		Progression->TotalDeliveries = Save.TotalDeliveries;
		Progression->TotalEarned = Save.TotalEarned;
		Progression->BestDayEarnings = Save.BestDayEarnings;
		Progression->TotalAngryCustomers = Save.TotalAngryCustomers;
		Progression->TotalPerfectOrders = Save.TotalPerfectOrders;

		// Rebuild the station and district locks from the level in the save.
		if (WorldBuilder)
		{
			WorldBuilder->RefreshUnlocks(Progression->Level, false);
		}
	}

	if (Skills)
	{
		Skills->Points = FMath::Max(0, Save.SkillPoints);
		Skills->Spent = FMath::Max(0, Save.SkillSpent);
		Skills->PrestigeCount = FMath::Max(0, Save.PrestigeCount);
		for (int32 i = 0; i < (int32)ECigSkill::COUNT && i < Save.SkillRanks.Num(); ++i)
		{
			Skills->Rank[i] = FMath::Clamp(Save.SkillRanks[i], 0, CigSkillDef((ECigSkill)i).MaxRank);
		}
	}

	if (Achievements)
	{
		for (int32 i = 0; i < (int32)ECigAchievement::COUNT && i < Save.AchievementsUnlocked.Num(); ++i)
		{
			Achievements->Unlocked[i] = Save.AchievementsUnlocked[i];
		}
	}

	if (Inventory)
	{
		for (int32 i = 0; i < CigStockCount && i < Save.Stock.Num(); ++i)
		{
			Inventory->Stock[i] = FMath::Max(0, Save.Stock[i]);
		}
		for (int32 i = 0; i < CigStockCount && i < Save.StockQuality.Num(); ++i)
		{
			Inventory->StockQuality[i] = FMath::Clamp(Save.StockQuality[i], 0.5f, 1.5f);
		}
		Inventory->Garnish = FMath::Clamp(Save.Garnish, 0, UCigInventorySystem::MaxGarnish);
	}

	if (Cooking)
	{
		Cooking->bSecretRecipeUnlocked = Save.bSecretRecipeUnlocked;
		Cooking->CurrentRecipe = FMath::Clamp(Save.CurrentRecipe, 0, CigRecipeCount - 1);
		if (!Cooking->IsRecipeUnlocked(Cooking->CurrentRecipe))
		{
			Cooking->CurrentRecipe = 0;
		}
	}

	if (Hygiene)
	{
		Hygiene->CounterDirt = FMath::Clamp(Save.CounterDirt, 0.f, 100.f);
		Hygiene->TrashFill = FMath::Clamp(Save.TrashFill, 0.f, 100.f);
		Hygiene->DishPile = FMath::Clamp(Save.DishPile, 0.f, 100.f);
	}

	if (Customers)
	{
		Customers->Loyals.Empty();
		for (const FCigSaveLoyal& S : Save.Loyals)
		{
			FCigLoyalCustomer L;
			L.Id = S.Id;
			L.Name = S.Name;
			L.Seed = S.Seed;
			L.Favorite.Spice = (ECigSpice)FMath::Clamp((int32)S.FavSpice, 0, 2);
			L.Favorite.Portion = FMath::Clamp(S.FavPortion, 1, 2);
			L.Favorite.ToppingMask = S.FavToppings;
			L.Favorite.bWantsAyran = S.bFavAyran;
			L.Favorite.bPacked = S.bFavPacked;
			L.Favorite.Side = (ECigSide)FMath::Clamp((int32)S.FavSide, 0, (int32)ECigSide::COUNT - 1);
			L.Visits = S.Visits;
			L.Satisfaction = S.Satisfaction;
			L.Trust = S.Trust;
			L.LastVisitDay = S.LastVisitDay;
			L.AvgTip = S.AvgTip;
			L.Traits = S.Traits;
			L.RememberedMistakes = S.RememberedMistakes;
			Customers->Loyals.Add(L);
		}
		Customers->NextLoyalId = FMath::Max(1, Save.NextLoyalId);
	}

	if (Rivals && Save.Rivals.Num() > 0)
	{
		Rivals->Rivals.Empty();
		for (const FCigSaveRival& S : Save.Rivals)
		{
			FCigRival R;
			R.Name = S.Name;
			R.Price = S.Price;
			R.Quality = S.Quality;
			R.Hygiene = S.Hygiene;
			R.Popularity = S.Popularity;
			R.AdPower = S.AdPower;
			R.Capacity = S.Capacity;
			R.Attitude = S.Attitude;
			R.bOpen = S.bOpen;
			R.BadDays = S.BadDays;
			Rivals->Rivals.Add(R);
		}
	}

	if (Reviews)
	{
		Reviews->Reviews.Empty();
		for (const FCigSaveReview& S : Save.Reviews)
		{
			FCigReview R;
			R.Author = S.Author;
			R.Text = S.Text;
			R.Stars = S.Stars;
			R.Day = S.Day;
			Reviews->Reviews.Add(R);
		}
		Reviews->FoodScore = Save.FoodScore;
		Reviews->ServiceScore = Save.ServiceScore;
		Reviews->PriceScore = Save.PriceScore;
		Reviews->HygieneScore = Save.HygieneScore;
		Reviews->AtmosphereScore = Save.AtmosphereScore;
	}

	if (Staff && Save.bApprenticeHired)
	{
		Staff->Apprentice.bHired = true;
		Staff->Apprentice.Name = Save.ApprenticeName;
		Staff->Apprentice.Level = FMath::Clamp(Save.ApprenticeLevel, 1, 6);
		Staff->Apprentice.XP = Save.ApprenticeXP;
		Staff->Apprentice.Morale = FMath::Clamp(Save.ApprenticeMorale, 0.f, 100.f);
		Staff->Apprentice.Salary = FMath::Max(100, Save.ApprenticeSalary);
		Staff->Apprentice.Task = (ECigStaffTask)FMath::Clamp((int32)Save.ApprenticeTask, 0, (int32)ECigStaffTask::COUNT - 1);
		Staff->Apprentice.Spec = (ECigStaffSpec)FMath::Clamp((int32)Save.ApprenticeSpec, 0, 5);
		Staff->Apprentice.DaysSinceRaise = Save.ApprenticeDaysSinceRaise;
		Staff->Apprentice.bWantsRaise = Save.bApprenticeWantsRaise;
		Staff->Apprentice.Arketip = Save.ApprenticeArketip;
		Staff->Apprentice.OdenmemisGun = Save.ApprenticeOdenmemisGun;
		Staff->TransferTeklifi = Save.StaffTransferTeklifi;
		// A zero here means a save written before traits existed; a neutral 1
		// reproduces exactly how that apprentice used to work.
		Staff->Apprentice.Hiz = Save.ApprenticeHiz > 0.f ? Save.ApprenticeHiz : 1.f;
		Staff->Apprentice.Titizlik = Save.ApprenticeTitizlik > 0.f ? Save.ApprenticeTitizlik : 1.f;
		Staff->Apprentice.GulerYuz = Save.ApprenticeGulerYuz > 0.f ? Save.ApprenticeGulerYuz : 1.f;
	}

	if (Events)
	{
		FCigTopluSiparis& T = Events->TopluSiparis;
		T.bTeklifVar = Save.bTopluTeklifVar;
		T.bKabulEdildi = Save.bTopluKabulEdildi;
		T.TeslimGunu = Save.TopluTeslimGunu;
		T.IstenenAdet = Save.TopluIstenenAdet;
		T.Odul = Save.TopluOdul;
		T.BaslangicServis = Save.TopluBaslangicServis;
		Staff->RestoreNPC();
	}

	if (CatSys)
	{
		CatSys->CatName = Save.CatName.IsEmpty() ? TEXT("Pamuk") : Save.CatName;
		CatSys->Food = FMath::Clamp(Save.CatFood, 0.f, 100.f);
		CatSys->Attention = FMath::Clamp(Save.CatAttention, 0.f, 100.f);
	}

	if (PlayerCar)
	{
		PlayerCar->Fuel = FMath::Clamp(Save.CarFuel, 0.f, 100.f);
		PlayerCar->Damage = FMath::Clamp(Save.CarDamage, 0.f, 100.f);
	}

	if (Quests)
	{
		Quests->StoryStage = (ECigStoryStage)FMath::Min((int32)Save.StoryStage, (int32)ECigStoryStage::Completed);
		Quests->StoryProgress = Save.StoryProgress;
		Quests->bTutorialActive = Save.bTutorialActive;
		Quests->TutorialStep = (ECigTutorialStep)FMath::Min((int32)Save.TutorialStep, (int32)ECigTutorialStep::Done);
	}

	Settings.FOV = FMath::Clamp(Save.Settings.FOV, 60.f, 120.f);
	Settings.MouseSensitivity = FMath::Clamp(Save.Settings.MouseSensitivity, 0.2f, 3.f);
	Settings.bHeadBob = Save.Settings.bHeadBob;
	Settings.MasterVolume = FMath::Clamp(Save.Settings.MasterVolume, 0.f, 1.f);
	Settings.EffectsVolume = FMath::Clamp(Save.Settings.EffectsVolume, 0.f, 1.f);
	Settings.MusicVolume = FMath::Clamp(Save.Settings.MusicVolume, 0.f, 1.f);
	Settings.QualityLevel = FMath::Clamp(Save.Settings.QualityLevel, 0, 3);
	Settings.WindowMode = FMath::Clamp(Save.Settings.WindowMode, 0, 2);
	Settings.ResolutionIndex = FMath::Clamp(Save.Settings.ResolutionIndex, 0, 3);
	Settings.UIScaleMult = FMath::Clamp(Save.Settings.UIScaleMult, 0.7f, 1.6f);
	Settings.bScreenFlash = Save.Settings.bScreenFlash;
	Settings.ColorBlindMode = FMath::Clamp(Save.Settings.ColorBlindMode, 0, (int32)ECigColorBlindMode::COUNT - 1);
	Settings.Language = FMath::Clamp(Save.Settings.Language, 0, CigText::LanguageCount() - 1);
	CigInput::LoadBindings(Save.Settings.KeyBindings);
}
