#include "Quests/CigQuestSystem.h"
#include "Core/CigText.h"
#include "Core/CigBalance.h"
#include "World/CigWorldBuilder.h"
#include "World/CigkofteStation.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigReviewSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Cooking/CigCookingSystem.h"
#include "Vehicles/CigCar.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigLog.h"

void UCigQuestSystem::OnInit()
{
	// The story opener only shows on a new game (after a load StoryStage > 0)

	// Subscribe to game events. The bus is torn down with the GameMode, so there
	// is no need to unsubscribe separately.
	UCigEventBus& B = Bus();
	B.Served.AddUObject(this, &UCigQuestSystem::NotifyServe);
	B.Delivered.AddUObject(this, &UCigQuestSystem::NotifyDelivery);
	B.Washed.AddUObject(this, &UCigQuestSystem::NotifyWash);
	B.Chopped.AddUObject(this, &UCigQuestSystem::NotifyChop);
	B.Cleaned.AddUObject(this, &UCigQuestSystem::NotifyClean);
	B.StockOrdered.AddUObject(this, &UCigQuestSystem::NotifyStockOrdered);
	B.UpgradeBought.AddUObject(this, &UCigQuestSystem::NotifyUpgrade);
	B.StaffHired.AddUObject(this, &UCigQuestSystem::NotifyHire);
	B.WrapStarted.AddUObject(this, &UCigQuestSystem::NotifyWrapStarted);
	B.WrapFinished.AddUObject(this, &UCigQuestSystem::NotifyWrapFinished);
	B.DoughPrepared.AddUObject(this, &UCigQuestSystem::NotifyDoughPrepared);
	B.InspectorVisited.AddUObject(this, &UCigQuestSystem::NotifyInspector);
	B.ShopScoreChanged.AddUObject(this, &UCigQuestSystem::NotifyShopScore);
	B.RivalBeaten.AddUObject(this, &UCigQuestSystem::NotifyRivalBeaten);
	B.RivalClosed.AddUObject(this, &UCigQuestSystem::NotifyRivalClosed);
	B.IngredientAdded.AddUObject(this, &UCigQuestSystem::NotifyIngredientAdded);
	B.CustomerArrived.AddUObject(this, &UCigQuestSystem::NotifyCustomerArrived);
}

void UCigQuestSystem::OnDayStart(int32 Day)
{
	GenerateQuest();

	if (Day == 1 && StoryStage == ECigStoryStage::PayFirstRent)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.rentintro")), FLinearColor(1.f, 0.75f, 0.5f));
	}
}

void UCigQuestSystem::OnDayEnd(int32 Day)
{
	// Story: has the first rent been paid?
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (StoryStage == ECigStoryStage::PayFirstRent && Eco && Eco->Money >= 0)
	{
		AdvanceStory();
	}

	// Festival goal
	if (StoryStage == ECigStoryStage::WinFestival && GM && GM->Days && GM->Days->DayServed >= 15)
	{
		AdvanceStory();
	}

	if (bTutorialActive && TutorialStep == ECigTutorialStep::FinishDay)
	{
		AdvanceTutorial(ECigTutorialStep::FinishDay);
	}
}

// ---------------------------------------------------------------- daily quest

void UCigQuestSystem::GenerateQuest()
{
	const int32 Day = GM && GM->Days ? GM->Days->Day : 1;

	QuestType = Rng().RandRange(0, 5);
	QuestProgress = 0;
	bQuestDone = false;

	switch (QuestType)
	{
	case 0:
		QuestTarget = 4 + Day;
		QuestDesc = CigText::Format(TEXT("quest.daily.serve"), QuestTarget);
		break;
	case 1:
		QuestTarget = 2 + Day / 2;
		QuestDesc = CigText::Format(TEXT("quest.daily.portion"), QuestTarget);
		break;
	case 2:
		QuestTarget = 2;
		QuestDesc = CigText::Get(TEXT("quest.daily.delivery"));
		break;
	case 3:
		QuestTarget = 3 + Day / 2;
		QuestDesc = CigText::Format(TEXT("quest.daily.accurate"), QuestTarget);
		break;
	case 4:
		QuestTarget = 3;
		QuestDesc = CigText::Get(TEXT("quest.daily.wash"));
		break;
	default:
		QuestTarget = 3;
		QuestDesc = CigText::Get(TEXT("quest.daily.clean"));
		break;
	}

	QuestReward = 100 + Day * 30 + (QuestType == 2 ? 60 : 0);
	GM->AddMessage(CigText::Format(TEXT("msg.quest.dailynew"), *QuestDesc, QuestReward), FLinearColor(0.9f, 0.7f, 1.f));
}

void UCigQuestSystem::QuestEvent(int32 Type, int32 Amount)
{
	if (bQuestDone || Type != QuestType)
	{
		return;
	}
	QuestProgress += Amount;
	if (QuestProgress >= QuestTarget)
	{
		bQuestDone = true;
		if (GM->Economy)
		{
			GM->Economy->Earn(QuestReward);
		}
		if (GM->Progression)
		{
			GM->Progression->AddXP(20);
		}
		GM->PlaySound(ECigSound::QuestComplete);
		GM->AddMessage(CigText::Format(TEXT("msg.quest.dailydone"), QuestReward), FLinearColor(0.5f, 1.f, 0.5f));
	}
}

void UCigQuestSystem::CompleteQuestNow()
{
	if (!bQuestDone)
	{
		QuestProgress = QuestTarget;
		QuestEvent(QuestType, 0);
	}
}

// ---------------------------------------------------------------- notifications

void UCigQuestSystem::NotifyServe(float Accuracy, float Quality, int32 Portions)
{
	QuestEvent(Portions >= 2 ? 1 : 0);
	if (Accuracy >= 80.f)
	{
		QuestEvent(3);
	}
	if (bTutorialActive && TutorialStep == ECigTutorialStep::Serve)
	{
		AdvanceTutorial(ECigTutorialStep::Serve);
	}
	if (StoryStage == ECigStoryStage::Serve10 && Accuracy >= 70.f)
	{
		StoryProgress++;
		if (StoryProgress >= 10)
		{
			AdvanceStory();
		}
	}
}

void UCigQuestSystem::NotifyDelivery()
{
	QuestEvent(2);
	DeliveriesDone++;
	if (StoryStage == ECigStoryStage::OpenDelivery)
	{
		StoryProgress++;
		if (StoryProgress >= 3)
		{
			AdvanceStory();
		}
	}
}

void UCigQuestSystem::NotifyWash()
{
	QuestEvent(4);
	if (bTutorialActive && TutorialStep == ECigTutorialStep::Clean)
	{
		AdvanceTutorial(ECigTutorialStep::Clean);
	}
}

void UCigQuestSystem::NotifyChop() {}

void UCigQuestSystem::NotifyClean()
{
	QuestEvent(5);
	if (bTutorialActive && TutorialStep == ECigTutorialStep::Clean)
	{
		AdvanceTutorial(ECigTutorialStep::Clean);
	}
}

void UCigQuestSystem::NotifyStockOrdered()
{
	if (bTutorialActive && TutorialStep == ECigTutorialStep::OrderStock)
	{
		AdvanceTutorial(ECigTutorialStep::OrderStock);
	}
}

void UCigQuestSystem::NotifyUpgrade()
{
	UpgradesBought++;
	if (StoryStage == ECigStoryStage::RenovateShop && UpgradesBought >= 3)
	{
		AdvanceStory();
	}
	if (StoryStage == ECigStoryStage::SecondBranch && GM && GM->Economy && GM->Economy->HasUpgrade(ECigUpgrade::YeniSube))
	{
		AdvanceStory();
	}
}

void UCigQuestSystem::NotifyHire()
{
	if (StoryStage == ECigStoryStage::HireApprentice)
	{
		AdvanceStory();
	}
}

void UCigQuestSystem::NotifyWrapStarted()
{
	if (bTutorialActive && TutorialStep == ECigTutorialStep::PrepareWrap)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.tutorial.wraphint")), FLinearColor(0.6f, 0.9f, 1.f));
	}
}

void UCigQuestSystem::NotifyWrapFinished()
{
	if (bTutorialActive && TutorialStep == ECigTutorialStep::PrepareWrap)
	{
		AdvanceTutorial(ECigTutorialStep::PrepareWrap);
	}
}

void UCigQuestSystem::NotifyDoughPrepared(float Quality)
{
	if (bTutorialActive && TutorialStep == ECigTutorialStep::Knead)
	{
		AdvanceTutorial(ECigTutorialStep::Knead);
	}
}

void UCigQuestSystem::NotifyIngredientAdded()
{
	if (bTutorialActive && TutorialStep == ECigTutorialStep::AddIngredient)
	{
		AdvanceTutorial(ECigTutorialStep::AddIngredient);
	}
}

void UCigQuestSystem::NotifyCustomerArrived()
{
	if (bTutorialActive && TutorialStep == ECigTutorialStep::TakeOrder)
	{
		AdvanceTutorial(ECigTutorialStep::TakeOrder);
	}
}

void UCigQuestSystem::NotifyInspector(float Hygiene) {}

void UCigQuestSystem::NotifyShopScore(float Stars)
{
	if (StoryStage == ECigStoryStage::ThreeStars && Stars >= 3.f)
	{
		AdvanceStory();
	}
}

void UCigQuestSystem::NotifyRivalBeaten()
{
	if (StoryStage == ECigStoryStage::BeatRival)
	{
		AdvanceStory();
	}
}

void UCigQuestSystem::NotifyRivalClosed() {}

// ---------------------------------------------------------------- story

FString UCigQuestSystem::StoryGoalText() const
{
	switch (StoryStage)
	{
	case ECigStoryStage::PayFirstRent:   return CigText::Get(TEXT("quest.story.payrent"));
	case ECigStoryStage::Serve10:        return CigText::Format(TEXT("quest.story.serve10"), StoryProgress);
	case ECigStoryStage::ThreeStars:     return CigText::Get(TEXT("quest.story.threestars"));
	case ECigStoryStage::OpenDelivery:   return CigText::Format(TEXT("quest.story.delivery3"), StoryProgress);
	case ECigStoryStage::HireApprentice: return CigText::Get(TEXT("quest.story.hire"));
	case ECigStoryStage::RenovateShop:   return CigText::Get(TEXT("quest.story.renovate"));
	case ECigStoryStage::BeatRival:      return CigText::Get(TEXT("quest.story.rival"));
	case ECigStoryStage::UpgradeCar:     return CigText::Get(TEXT("quest.story.car"));
	case ECigStoryStage::SecondBranch:   return CigText::Get(TEXT("quest.story.branch"));
	case ECigStoryStage::OwnBrand:       return CigText::Get(TEXT("quest.story.brand"));
	case ECigStoryStage::WinFestival:    return CigText::Get(TEXT("quest.story.festival"));
	case ECigStoryStage::BestInTown:     return CigText::Get(TEXT("quest.story.best"));
	default:                             return CigText::Get(TEXT("quest.story.done"));
	}
}

void UCigQuestSystem::AdvanceStory()
{
	StoryProgress = 0;
	const int32 Reward = 150 + (int32)StoryStage * 60;
	if (GM->Economy)
	{
		GM->Economy->Earn(Reward);
	}
	if (GM->Progression)
	{
		GM->Progression->AddXP(40);
	}
	GM->PlaySound(ECigSound::QuestComplete);
	GM->AddMessage(CigText::Format(TEXT("msg.quest.storydone"), Reward), FLinearColor(1.f, 0.85f, 0.4f));

	StoryStage = (ECigStoryStage)FMath::Min((int32)StoryStage + 1, (int32)ECigStoryStage::Completed);

	// Stage-specific character messages
	switch (StoryStage)
	{
	case ECigStoryStage::Serve10:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.serve10")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::ThreeStars:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.threestars")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::OpenDelivery:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.opendelivery")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::HireApprentice:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.hire")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::RenovateShop:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.renovate")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::BeatRival:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.beatrival")), FLinearColor(1.f, 0.7f, 0.6f));
		break;
	case ECigStoryStage::UpgradeCar:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.upgradecar")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::SecondBranch:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.secondbranch")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::OwnBrand:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.ownbrand")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::WinFestival:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.festival")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::BestInTown:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.bestintown")), FLinearColor(0.8f, 0.9f, 1.f));
		break;
	case ECigStoryStage::Completed:
		GM->AddMessage(CigText::Get(TEXT("msg.quest.story.completed")), FLinearColor(1.f, 0.9f, 0.5f));
		if (GM->Cooking)
		{
			GM->Cooking->UnlockSecretRecipe();
		}
		break;
	default:
		break;
	}
	GM->RequestSave();
}

void UCigQuestSystem::CheckStoryConditions()
{
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	const UCigCustomerSystem* Cust = GM ? GM->Customers.Get() : nullptr;

	switch (StoryStage)
	{
	case ECigStoryStage::UpgradeCar:
	{
		const ACigCar* Car = GM ? GM->PlayerCar.Get() : nullptr;
		if (Car && Car->Damage <= 1.f && Car->Fuel >= 99.f)
		{
			AdvanceStory();
		}
		break;
	}
	case ECigStoryStage::OwnBrand:
		if (Eco && Cust && Eco->Money >= 5000 && Cust->Loyals.Num() >= 5)
		{
			AdvanceStory();
		}
		break;
	case ECigStoryStage::BestInTown:
		if (Prog && Prog->Rep >= 90.f)
		{
			AdvanceStory();
		}
		break;
	default:
		break;
	}
}

void UCigQuestSystem::UpdateSystem(float DeltaSeconds)
{
	StoryCheckTimer += DeltaSeconds;
	if (StoryCheckTimer >= 1.f)
	{
		StoryCheckTimer = 0.f;
		CheckStoryConditions();
	}
}

// ---------------------------------------------------------------- tutorial

FString UCigQuestSystem::TutorialText() const
{
	const int32 Adim = (int32)TutorialStep;
	if (Adim < 0 || Adim >= CigBalance::TutorialCount())
	{
		return FString();
	}
	return CigText::Get(*CigBalance::Tutorial(Adim).MetinAnahtari);
}

void UCigQuestSystem::VurguluIstasyonuGoster()
{
	// The step is easier to follow when the thing it is talking about moves. One
	// pulse as the step opens is enough: a station that never stops flashing
	// stops being a hint and starts being wallpaper.
	const int32 Adim = (int32)TutorialStep;
	if (!bTutorialActive || Adim < 0 || Adim >= CigBalance::TutorialCount())
	{
		return;
	}

	const int32 Istasyon = CigBalance::Tutorial(Adim).VurguIstasyon;
	if (Istasyon < 0 || !GM || !GM->WorldBuilder)
	{
		return;
	}

	if (ACigkofteStation* S = GM->WorldBuilder->FindStation((ECigStation)Istasyon))
	{
		S->Pop();
	}
}

void UCigQuestSystem::AdvanceTutorial(ECigTutorialStep FromStep)
{
	if (!bTutorialActive || TutorialStep != FromStep)
	{
		return;
	}
	TutorialStep = (ECigTutorialStep)((uint8)TutorialStep + 1);
	GM->PlaySound(ECigSound::Success);

	if (TutorialStep == ECigTutorialStep::Done)
	{
		bTutorialActive = false;
		GM->AddMessage(CigText::Get(TEXT("msg.tutorial.done")), FLinearColor(0.5f, 1.f, 0.5f));
		if (GM->Progression)
		{
			GM->Progression->AddXP(25);
		}
		GM->RequestSave();
	}
	else
	{
		GM->AddMessage(CigText::Format(TEXT("msg.tutorial.guide"), *TutorialText()), FLinearColor(0.6f, 0.9f, 1.f));
		VurguluIstasyonuGoster();
	}
}

void UCigQuestSystem::ResetTutorial()
{
	bTutorialActive = true;
	TutorialStep = ECigTutorialStep::AddIngredient;
	GM->AddMessage(CigText::Get(TEXT("msg.tutorial.reset")), FLinearColor(0.6f, 0.9f, 1.f));
}

void UCigQuestSystem::SkipTutorial()
{
	bTutorialActive = false;
	TutorialStep = ECigTutorialStep::Done;
	GM->AddMessage(CigText::Get(TEXT("msg.tutorial.skip")), FLinearColor(0.8f, 0.8f, 0.8f));
}
