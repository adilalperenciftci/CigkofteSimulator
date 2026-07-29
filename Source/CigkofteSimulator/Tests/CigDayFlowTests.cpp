// One whole day, played through the systems a player's hands would drive.
//
// Everything else in Tests/ enters somewhere in the middle: a sale is handed a
// finished FCigSatisTalebi, a save is handed a state that was set by assignment.
// Nothing walked the chain that actually produces those inputs - stock into the
// bowl, kneading into dough, dough into a wrap, a wrap into a customer's hands -
// so a break between two of those steps would compile, pass 69 tests and only
// show up to someone holding a controller.
//
// This is the closest thing to a played session that runs without a screen: the
// only thing simulated away is the walking. The customer is marked as arrived
// rather than pathfinding to the counter, because a queue slot and a NavMesh are
// not what this is testing.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.DayFlow; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Tests/CigTestShop.h"
#include "Cooking/CigCookingSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigReviewSystem.h"
#include "Game/CigDaySystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Orders/CigOrderSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Save/CigSaveGame.h"
#include "Save/CigSaveSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Kneading in rhythm, because kneading is a rhythm mechanic and a test that
	// ignores that does not test the same thing a player does.
	//
	// KneadPress reads the wall clock: a press 0.25-0.85s after the last one
	// gains 7, anything under 0.15s gains 2. A loop pressing as fast as the CPU
	// allows therefore needs fifty presses to reach 100, which puts KneadCount
	// far past the recipe's MaxKnead of 24 and costs 2 quality per press over.
	// The first version of this test did exactly that and produced a wrap of
	// quality 32 - correct behaviour ("panic kneading achieves nothing"), and a
	// useless setup for judging what a competent batch does downstream.
	//
	// So it sleeps into the window. Fifteen presses at 0.30s costs the suite
	// about five seconds and buys a batch made the way the game asks for it.
	constexpr float KneadRhythmSeconds = 0.30f;
	constexpr int32 MaxKneadPresses = 30;

	// Returns how many presses it took, or -1 if the dough never formed. The
	// count has to come from here: FinishDough resets KneadCount for the next
	// batch, so reading it afterwards reports zero and any assertion against it
	// passes without meaning anything.
	int32 YogurVeBitir(UCigCookingSystem& Cook)
	{
		for (int32 i = 1; i <= MaxKneadPresses; ++i)
		{
			FPlatformProcess::Sleep(KneadRhythmSeconds);
			Cook.KneadPress();
			if (Cook.Dough.IsValid())
			{
				return i;
			}
		}
		return -1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigTestShopLeavesSaveAloneTest,
	"Cigkofte.DayFlow.TestShopNeverWritesThePlayersSave",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigTestShopLeavesSaveAloneTest::RunTest(const FString& /*Parameters*/)
{
	// This is a data-loss test, which is why it comes first.
	//
	// FCigTestShop never loads the player's save, and that was mistaken for
	// safety. It does not need to load one: a test shop is a real GameMode on a
	// real game instance, so RequestSave reaches the same slot the player's game
	// does, and BroadcastDayStart calls it. Running the suite on a machine with
	// a saved game replaced a day-3 file with the test world's day 1 - found by
	// comparing the file's hash before and after a headless run.
	const FString Yol = FString::Printf(TEXT("%sSaveGames/%s.sav"),
		*FPaths::ProjectSavedDir(), UCigSaveSubsystem::SlotName());

	const bool bVardi = IFileManager::Get().FileExists(*Yol);
	const FDateTime Once = bVardi ? IFileManager::Get().GetTimeStamp(*Yol) : FDateTime::MinValue();

	{
		FCigTestShop Shop;
		if (!Shop.Build(*this))
		{
			return false;
		}
		// Both broadcasts, which is every automatic save the day loop performs.
		Shop.GM->Days->StartDay(false);
		Shop.GM->Days->EndDay();
		// And the explicit one, in case a future caller reaches it directly.
		Shop.GM->RequestSave();
	}

	if (bVardi)
	{
		TestEqual(TEXT("Test dükkânı oyuncunun kayıt dosyasına dokunmamalı"),
			IFileManager::Get().GetTimeStamp(*Yol), Once);
	}
	else
	{
		TestFalse(TEXT("Test dükkânı olmayan kaydı oluşturmamalı"),
			IFileManager::Get().FileExists(*Yol));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigFullDayFlowTest,
	"Cigkofte.DayFlow.OneDayFromStockToSave",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigFullDayFlowTest::RunTest(const FString& /*Parameters*/)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}

	UCigDaySystem* Days = Shop.GM->Days.Get();
	UCigCookingSystem* Cook = Shop.GM->Cooking.Get();
	UCigInventorySystem* Inv = Shop.GM->Inventory.Get();
	UCigOrderSystem* Orders = Shop.GM->Orders.Get();
	UCigCustomerSystem* Customers = Shop.GM->Customers.Get();
	UCigEconomySystem* Eco = Shop.GM->Economy.Get();
	UCigProgressionSystem* Prog = Shop.GM->Progression.Get();
	UCigReviewSystem* Reviews = Shop.GM->Reviews.Get();

	if (!Days || !Cook || !Inv || !Orders || !Customers || !Eco || !Prog || !Reviews)
	{
		AddError(TEXT("Gün akışı için gereken sistemlerden biri kurulmadı."));
		return false;
	}

	// --- The morning, then the door ---------------------------------------
	// A day now starts in preparation and is opened by hand. The distinction is
	// the whole of Stage 2.1 and it is worth walking rather than skipping: the
	// stations have to work before the shop opens, and the queue has to stay out
	// until it does.
	Days->StartDay(false);
	TestFalse(TEXT("Gün hazırlıkta başlamalı, serviste değil"), Days->IsPlaying());
	TestTrue(TEXT("Hazırlıkta istasyonlar çalışabilmeli"), Days->CanWork());
	TestEqual(TEXT("Gün açılışı günün hasılatını sıfırlamalı"), Days->DayEarnings, 0);
	TestEqual(TEXT("Gün açılışı günün servis sayısını sıfırlamalı"), Days->DayServed, 0);

	// Preparing is real work: a batch kneaded now is a batch that ages before it
	// is sold. This walks the same path the player does rather than setting the
	// phase by hand.
	Days->OpenShop();
	TestTrue(TEXT("Dükkân açılınca servis fazına geçmeli"), Days->IsPlaying());
	TestTrue(TEXT("Serviste de çalışılabilmeli"), Days->CanWork());

	// --- A customer orders first ------------------------------------------
	// The order comes before the cooking, exactly as it does at the counter:
	// everything below is prepared against what this customer asked for, so the
	// accuracy the sale is scored on is earned rather than arranged.
	Customers->SpawnCustomer();
	ACigkofteCustomer* Musteri = Customers->FrontCustomer();
	if (!Musteri)
	{
		AddError(TEXT("Müşteri kuyruğa girmedi; servis doğrulanamıyor."));
		return false;
	}
	// Only the walk to the counter is skipped. The order spec, the traits and
	// the patience are all what SpawnCustomer produced.
	Musteri->bArrived = true;
	const FCigOrderSpec Siparis = Musteri->Spec;

	// --- Ingredients leave the shelf --------------------------------------
	// Five bulgur, three water, two paste, one spice: the first recipe's ratios.
	// The isot count is the one decision that answers the order, because heat is
	// the only part of the wrap that is decided in the bowl rather than on the
	// counter - SpiceFromBowl reads it as a fraction of the whole batch.
	const int32 IsotAdedi =
		Siparis.Spice == ECigSpice::AzAci ? 0 : (Siparis.Spice == ECigSpice::Orta ? 2 : 4);

	const int32 BulgurOnce = Inv->Stock[(int32)ECigIngredient::Bulgur];
	const int32 SuOnce = Inv->Stock[(int32)ECigIngredient::Su];
	const int32 LavasOnce = Inv->Stock[CigStockLavas];

	for (int32 i = 0; i < 5; ++i) { Cook->AddIngredient(ECigIngredient::Bulgur); }
	for (int32 i = 0; i < 3; ++i) { Cook->AddIngredient(ECigIngredient::Su); }
	for (int32 i = 0; i < 2; ++i) { Cook->AddIngredient(ECigIngredient::Salca); }
	Cook->AddIngredient(ECigIngredient::Baharat);
	for (int32 i = 0; i < IsotAdedi; ++i) { Cook->AddIngredient(ECigIngredient::Isot); }

	TestEqual(TEXT("Kaseye giren bulgur stoktan düşmeli"),
		Inv->Stock[(int32)ECigIngredient::Bulgur], BulgurOnce - 5);
	TestEqual(TEXT("Kaseye giren su stoktan düşmeli"),
		Inv->Stock[(int32)ECigIngredient::Su], SuOnce - 3);
	TestEqual(TEXT("Kasedeki toplam malzeme eklenenle eşleşmeli"),
		Cook->BowlTotal(), 11 + IsotAdedi);
	TestEqual(TEXT("Kasenin acılığı sipariş edilen acılık olmalı"),
		(int32)Cook->SpiceFromBowl(), (int32)Siparis.Spice);

	// --- Kneading turns the bowl into dough -------------------------------
	const int32 Basis = YogurVeBitir(*Cook);
	if (Basis < 0)
	{
		AddError(FString::Printf(
			TEXT("%d yoğurma basışında hamur oluşmadı (ilerleme %.1f)."),
			MaxKneadPresses, Cook->KneadProgress));
		return false;
	}

	TestTrue(TEXT("Hamur porsiyon vermeli"), Cook->Dough.Servings > 0);
	TestTrue(TEXT("Hamurun kalitesi ölçülmüş olmalı"), Cook->Dough.Quality > 0.f);
	TestEqual(TEXT("Hamur oluşunca kase boşalmalı"), Cook->BowlTotal(), 0);
	// The rhythm has to have been kept, or the assertions further down are
	// judging a batch nobody would have made on purpose.
	const FCigRecipe& Tarif = UCigCookingSystem::Recipe(Cook->CurrentRecipe);
	TestTrue(TEXT("Ritimle yoğrulan hamur tarifin aralığında bitmeli"),
		Basis >= Tarif.MinKnead && Basis <= Tarif.MaxKnead);
	TestTrue(TEXT("Ritimle yoğrulan hamur iyi kalitede olmalı"), Cook->Dough.Quality >= 70.f);
	AddInfo(FString::Printf(TEXT("Yoğurma: %d basış (tarif aralığı %d-%d), kalite %.1f, %d birim"),
		Basis, Tarif.MinKnead, Tarif.MaxKnead, Cook->Dough.Quality, Cook->Dough.Servings));

	const int32 PorsiyonOnce = Cook->Dough.Servings;

	// --- A wrap is built --------------------------------------------------
	Orders->StartWrap();
	TestTrue(TEXT("Lavaş serilince dürüm başlamalı"), Orders->Wrap.bActive);
	TestEqual(TEXT("Lavaş stoktan düşmeli"), Inv->Stock[CigStockLavas], LavasOnce - 1);

	for (int32 i = 0; i < Siparis.Portion; ++i)
	{
		Orders->AddPortion();
	}
	TestEqual(TEXT("Dürüme sipariş edilen porsiyon girmeli"), Orders->Wrap.Portions, Siparis.Portion);
	TestEqual(TEXT("Porsiyonlar hamurdan düşmeli"),
		Cook->Dough.Servings, PorsiyonOnce - Siparis.Portion);
	TestTrue(TEXT("Dürüm hamurun kalitesini taşımalı"), Orders->Wrap.DoughQuality > 0.f);

	// Toppings. Lettuce is the one that cannot be taken straight off the shelf:
	// it has to be chopped first, which is a separate station and a separate
	// player action, so the test does that work too rather than skipping it.
	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		const ECigTopping T = (ECigTopping)i;
		if (!Siparis.WantsTopping(T))
		{
			continue;
		}
		if (T == ECigTopping::Marul)
		{
			for (int32 Chop = 0; Chop < 8 && Inv->Garnish <= 0; ++Chop)
			{
				Inv->ChopPress();
			}
		}
		Orders->ToggleTopping(T);
	}

	if (Siparis.bWantsAyran) { Orders->ToggleAyran(); }
	if (Siparis.bPacked) { Orders->TogglePack(); }

	// The sides counter cycles rather than selects, so reaching a specific one
	// means pressing until it comes round. The bound is the menu length.
	for (int32 i = 0; i < (int32)ECigSide::COUNT && Orders->Wrap.Side != Siparis.Side; ++i)
	{
		Orders->CycleSide();
	}

	Orders->FinishWrap();
	TestTrue(TEXT("Dürüm sarılmış olmalı"), Orders->Wrap.bWrapped);

	// What the shop is about to be judged on, recorded before the wrap leaves
	// the counter: ServeFront scores it again internally and the two have to
	// agree, or the number the sale used is not the one this test prepared.
	const FCigOrderScore Puan = UCigOrderSystem::ScoreWrap(Orders->Wrap, Siparis, 0.f);
	const float TeslimKalite = Orders->Wrap.DoughQuality;
	AddInfo(FString::Printf(TEXT("Sipariş doğruluğu: %.1f, hamur kalitesi: %.1f"),
		Puan.Accuracy, TeslimKalite));

	const int32 ParaOnce = Eco->Money;
	const float ItibarOnce = Prog->Rep;
	const int32 ServisOnce = Prog->TotalServed;
	const int32 GunHasilatOnce = Days->DayEarnings;
	const int32 GunServisOnce = Days->DayServed;
	const int32 YorumOnce = Reviews->Reviews.Num();
	// The category scores are what RecordServe moves immediately, and they move
	// deterministically. The written reviews do not: end of day produces
	// RandRange(0, 3) of them, so a served day with no review is normal and an
	// assertion that one appeared is a coin toss. An earlier version of this
	// test made exactly that assertion, then indexed Reviews[0] anyway and took
	// the whole suite down with it on the run where the roll came up zero.
	const float ServisPuanOnce = Reviews->ServiceScore;

	Customers->ServeFront();

	TestTrue(TEXT("Servis kasaya para koymalı"), Eco->Money > ParaOnce);
	TestEqual(TEXT("Servis toplam servis sayısını artırmalı"), Prog->TotalServed, ServisOnce + 1);
	TestEqual(TEXT("Servis günün servis sayısını artırmalı"), Days->DayServed, GunServisOnce + 1);
	TestTrue(TEXT("Servis günün hasılatına işlenmeli"), Days->DayEarnings > GunHasilatOnce);
	TestEqual(TEXT("Kasadaki artış günün hasılatındaki artışla aynı olmalı"),
		Eco->Money - ParaOnce, Days->DayEarnings - GunHasilatOnce);
	// Reputation is not paid for showing up. SatisiIsle raises it only when the
	// order was right (85) and the food was good (70), which is the rule this
	// asserts rather than asserting that some number moved: an earlier version
	// of this test expected movement from a wrap thrown together without reading
	// the order, and the shop was correct to leave the score alone.
	if (Puan.Accuracy >= 85.f && TeslimKalite >= 70.f)
	{
		TestTrue(TEXT("Doğru ve kaliteli servis itibarı yükseltmeli"), Prog->Rep > ItibarOnce);
	}
	else
	{
		AddInfo(FString::Printf(
			TEXT("Doğruluk %.1f / kalite %.1f itibar eşiğinin altında; itibar %.2f -> %.2f."),
			Puan.Accuracy, TeslimKalite, ItibarOnce, Prog->Rep));
		TestTrue(TEXT("Eşiğin altındaki servis itibarı yükseltmemeli"), Prog->Rep <= ItibarOnce);
	}
	TestFalse(TEXT("Teslim edilen dürüm tezgâhtan kalkmalı"), Orders->Wrap.bActive);
	// The sale reached the review system. A perfect order served to a patient
	// customer can only raise the service score.
	TestTrue(TEXT("Servis, yorum sisteminin hizmet puanını yükseltmeli"),
		Reviews->ServiceScore > ServisPuanOnce);

	const int32 ParaSatisSonrasi = Eco->Money;

	// --- The day closes ---------------------------------------------------
	Days->EndDay();

	TestTrue(TEXT("Gün sonu kira kesmeli"), Days->LastRent > 0);

	// Zero to three, from the day's recorded serves. Both bounds matter: more
	// than three would mean the roll is not what it claims, and any review at
	// all on a day with one serve has to have come from that serve.
	const int32 Uretilen = Reviews->Reviews.Num() - YorumOnce;
	TestTrue(TEXT("Gün sonu en fazla üç yorum üretmeli"), Uretilen >= 0 && Uretilen <= 3);
	AddInfo(FString::Printf(TEXT("Gün sonu %d yorum üretti (0-3 arası normaldir)."), Uretilen));
	for (int32 i = 0; i < Uretilen; ++i)
	{
		TestTrue(TEXT("Her yorum sistemin kendi kimliğini almalı"), Reviews->Reviews[i].Id > 0);
	}

	// --- Save and load ----------------------------------------------------
	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);
	Save->SaveVersion = UCigSaveSubsystem::CurrentVersion;

	const int32 GunSonuPara = Eco->Money;
	const int32 GunSonuServis = Prog->TotalServed;
	const float GunSonuItibar = Prog->Rep;
	const int32 GunSonuYorum = Reviews->Reviews.Num();
	const int32 GunSonuGun = Days->Day;

	// Wreck the live state so anything that comes back has to come back through
	// the save file.
	Eco->Money = 0;
	Prog->TotalServed = 0;
	Prog->Rep = 0.f;
	Days->Day = 1;
	Reviews->Reviews.Empty();

	Shop.GM->ApplySave(*Save);

	TestEqual(TEXT("Yüklemeden sonra para dönmeli"), Eco->Money, GunSonuPara);
	TestEqual(TEXT("Yüklemeden sonra servis sayısı dönmeli"), Prog->TotalServed, GunSonuServis);
	TestEqual(TEXT("Yüklemeden sonra itibar dönmeli"), Prog->Rep, GunSonuItibar, 0.01f);
	TestEqual(TEXT("Yüklemeden sonra gün dönmeli"), Days->Day, GunSonuGun);
	TestEqual(TEXT("Yüklemeden sonra yorumlar dönmeli"), Reviews->Reviews.Num(), GunSonuYorum);
	TestEqual(TEXT("Kayıt güncel sürümde olmalı"),
		Save->SaveVersion, UCigSaveSubsystem::CurrentVersion);

	// The money that survived is the money the sale made, which is the whole
	// point of walking the chain: a save that restores a number no serve ever
	// produced would pass a round-trip test written against assignments.
	TestTrue(TEXT("Dönen para satıştan gelen parayla tutarlı olmalı"),
		Eco->Money <= ParaSatisSonrasi);

	Save->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
