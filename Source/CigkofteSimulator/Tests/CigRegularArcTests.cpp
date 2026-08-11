// Does a regular customer actually come back, and are they the same person?
//
// The loyalty *arithmetic* is already covered: CigLoyalty::AfterServe is pure
// and Cigkofte.Loyalty pins its two awkward decisions. What nothing looked at is
// the half that lives in UCigCustomerSystem - the book of regulars itself.
//
// That half is where a regular is born (MaybeCreateLoyal, behind three gates and
// a roll), where one is chosen to come back (InitializeArrival, behind a roll and
// a once-a-day gate), where a walkout is charged against them, and where a
// recycled body has to forget who it was. All four are private, all four are
// reached only by serving or spawning a customer in a running shop, and every
// failure they can have is silent: a regular that never returns, a regular that
// returns twice in one afternoon, or - the worst one - a stranger who walks in
// wearing the last regular's name because the pooled actor kept it.
//
// So everything below goes through the public boundary: SpawnCustomer,
// ServeFront, RemoveCustomer, and the game mode's own CaptureSave/ApplySave.
// Nothing here was made public for the sake of being tested.
//
// The shared random stream is the difficulty. Both rolls sit behind draws whose
// count varies (RollTraits takes a variable number), so the seeds here are
// probed rather than written down: a candidate is tried, and the one that lands
// on the wanted side is used. If the stream's arithmetic ever changes, these
// tests re-probe instead of failing.

#include "Misc/AutomationTest.h"
#include "Core/CigBalance.h"
#include "Core/CigRandomSubsystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigPricingSystem.h"
#include "Events/CigEventSystem.h"
#include "Game/CigDaySystem.h"
#include "Game/CigkofteGameMode.h"
#include "Orders/CigOrderSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Save/CigSaveGame.h"
#include "Save/CigSaveSubsystem.h"
#include "Tests/CigTestShop.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Every name in here carries the file's own prefix. Separate .cpp files share
	// a translation unit under unity builds, so a plain `Rig` or `OpenShop` is a
	// collision waiting for the next test file that wants one.
	struct FCigRegularTestsRig
	{
		UCigCustomerSystem*  Cust   = nullptr;
		UCigDaySystem*       Days   = nullptr;
		UCigOrderSystem*     Orders = nullptr;
		UCigRandomSubsystem* Rng    = nullptr;
	};

	// Opens the shop and hands back an empty queue.
	//
	// Not a convenience. StartDay rolls the day's event and one of them ("Okul
	// Çıkışı") puts two customers straight in, so a test that counted the queue
	// afterwards passed or failed depending on the clock. They leave happy, which
	// costs the shop nothing, so every baseline taken after this is a clean one.
	//
	// Open rather than merely started: UpdateSystem returns early unless the day
	// is playing, and the recycling this file depends on runs before that gate but
	// the spawn timer does not.
	bool CigRegularTestsOpenEmptyShop(FAutomationTestBase& Test, FCigTestShop& Shop, FCigRegularTestsRig& Out)
	{
		Out.Cust   = Shop.GM->Customers.Get();
		Out.Days   = Shop.GM->Days.Get();
		Out.Orders = Shop.GM->Orders.Get();
		Out.Rng    = Shop.GI ? Shop.GI->GetSubsystem<UCigRandomSubsystem>() : nullptr;
		if (!Out.Cust || !Out.Days || !Out.Orders || !Out.Rng)
		{
			Test.AddError(TEXT("Sistemler yok."));
			return false;
		}

		Out.Days->StartDay(false);
		Out.Days->OpenShop();
		if (!Out.Days->IsPlaying())
		{
			Test.AddError(TEXT("Dukkan acilmadi."));
			return false;
		}

		while (Out.Cust->Queue.Num() > 0)
		{
			ACigkofteCustomer* C = Out.Cust->Queue[0].Get();
			if (!C)
			{
				Out.Cust->Queue.RemoveAt(0);
				continue;
			}
			Out.Cust->RemoveCustomer(C, /*bAngry=*/false);
		}
		if (Out.Cust->Queue.Num() != 0)
		{
			Test.AddError(TEXT("Acilis kuyrugu bosaltilamadi."));
			return false;
		}

		// The book starts empty whatever the morning did, so "one regular exists"
		// below is always a claim about the one this test made.
		Out.Cust->Loyals.Empty();
		Out.Cust->NextLoyalId = 1;
		return true;
	}

	// Fixes the order and the wrap so that the only draws ServeFront can take from
	// the shared stream are the ones the regular book takes.
	//
	// Each unwanted branch is closed by a number rather than by hope: a tip is
	// only rolled above quality 60, the family chain needs the trait, and the
	// dine-in roll is skipped entirely for a packed order. What is left is
	// MaybeCreateLoyal, which is the point.
	//
	// bWantAccurate picks between a wrap that matches the order exactly (100) and
	// one whose heat is two steps off (75), which is the only difference between
	// being eligible for the book and not.
	bool CigRegularTestsPrepareServe(FAutomationTestBase& Test, ACigkofteCustomer& C,
		UCigOrderSystem& Orders, UWorld* World, bool bWantAccurate)
	{
		C.bArrived = true;
		C.bLeaving = false;
		C.Traits   = ECigTrait::None;
		C.bVIP     = false;
		C.LoyalId  = -1;
		C.LoyalName.Reset();

		C.Spec = FCigOrderSpec();
		C.Spec.Spice   = ECigSpice::CokAci;
		C.Spec.Portion = 1;
		C.Spec.bPacked = true;              // packed: no dine-in roll on the way out

		Orders.Wrap = FCigWrapBuild();
		Orders.Wrap.bActive      = true;
		Orders.Wrap.bWrapped     = true;
		Orders.Wrap.Portions     = 1;
		Orders.Wrap.DoughQuality = 50.f;    // under 60: no tip roll
		Orders.Wrap.Spice        = bWantAccurate ? ECigSpice::CokAci : ECigSpice::AzAci;
		Orders.Wrap.bPacked      = true;
		Orders.Wrap.StartTime    = World ? World->GetTimeSeconds() : 0.f;

		const FCigOrderScore Score = UCigOrderSystem::ScoreWrap(Orders.Wrap, C.Spec, 0.f);
		const bool bEligible = Score.Accuracy >= 80.f;
		if (bEligible != bWantAccurate)
		{
			Test.AddError(FString::Printf(
				TEXT("Test kurulumu bozuk: dogruluk %.1f, beklenen esik durumu %d."),
				Score.Accuracy, bWantAccurate ? 1 : 0));
			return false;
		}
		return true;
	}

	// Rewinds the stream to a seed whose next draw lands on the wanted side of the
	// 0.25 roll in MaybeCreateLoyal. Safe to call after the customer has been
	// spawned, because reseeding only moves the stream and the serve below is the
	// next thing to draw from it.
	bool CigRegularTestsSeedCreationRoll(FAutomationTestBase& Test, UCigRandomSubsystem& Rng, bool bWantCreated)
	{
		for (int32 Candidate = 1; Candidate <= 128; ++Candidate)
		{
			Rng.SeedWith(Candidate);
			const bool bCreates = Rng.FRand() < 0.25f;
			if (bCreates == bWantCreated)
			{
				Rng.SeedWith(Candidate);
				return true;
			}
		}
		Test.AddError(FString::Printf(
			TEXT("Sadakat cekimi icin tohum bulunamadi (istenen olusum %d)."), bWantCreated ? 1 : 0));
		return false;
	}

	// Spawns customers, one probed seed at a time, until one of them walks in as
	// the regular with this id. Returns the seed that did it through OutSeed, so a
	// test can replay exactly that arrival and show that the day gate - and only
	// the day gate - changed the answer.
	//
	// Probing by spawning rather than by simulating the roll is deliberate:
	// InitializeArrival calls RollTraits first and that takes a variable number of
	// draws, so any attempt to predict where the 0.30 roll lands would be guessing
	// at the production code's arithmetic instead of using it.
	ACigkofteCustomer* CigRegularTestsSpawnUntilRegular(FAutomationTestBase& Test,
		FCigRegularTestsRig& Rig, int32 LoyalId, int32 MaxSeeds, int32* OutSeed)
	{
		for (int32 Candidate = 1; Candidate <= MaxSeeds; ++Candidate)
		{
			Rig.Rng->SeedWith(Candidate);
			ACigkofteCustomer* C = Rig.Cust->SpawnCustomer();
			if (!C)
			{
				Test.AddError(TEXT("Musteri gelmedi."));
				return nullptr;
			}
			if (C->LoyalId == LoyalId)
			{
				if (OutSeed) { *OutSeed = Candidate; }
				return C;
			}
			// Not the one. Out through the real exit and back through the real
			// recycle, so the next attempt starts from the state the game would be
			// in rather than from a queue quietly emptied by hand.
			Rig.Cust->RemoveCustomer(C, /*bAngry=*/false);
			C->Deactivate();
			Rig.Cust->UpdateSystem(0.f);
		}
		Test.AddError(FString::Printf(
			TEXT("%d tohum denendi, sadik musteri (%d) donmedi."), MaxSeeds, LoyalId));
		return nullptr;
	}

	bool CigRegularTestsSameSpec(const FCigOrderSpec& A, const FCigOrderSpec& B)
	{
		return A.Spice == B.Spice
			&& A.Portion == B.Portion
			&& A.ToppingMask == B.ToppingMask
			&& A.bWantsAyran == B.bWantsAyran
			&& A.bPacked == B.bPacked
			&& A.Side == B.Side;
	}

	// A regular with nothing default about it, so a field that fails to travel
	// shows up as a wrong value rather than as a coincidentally right one.
	FCigLoyalCustomer CigRegularTestsMakeRecord(int32 Id, int32 Day)
	{
		FCigLoyalCustomer L;
		L.Id                 = Id;
		L.Name               = TEXT("Nazan Abla");
		L.Seed               = 987654;
		L.Favorite.Spice     = ECigSpice::CokAci;
		L.Favorite.Portion   = 2;
		L.Favorite.ToppingMask = (1 << 0) | (1 << 2);
		L.Favorite.bWantsAyran = true;
		L.Favorite.bPacked   = true;
		L.Favorite.Side      = ECigSide::Cay;
		L.Visits             = 3;
		L.Satisfaction       = 72.f;
		L.Trust              = 64.f;
		L.LastVisitDay       = Day - 1;
		L.AvgTip             = 7.f;
		// Generous, not Indecisive: an indecisive regular re-rolls their order 40%
		// of the time, which would make "they asked for their favourite" a claim
		// about a coin toss.
		L.Traits             = (uint16)ECigTrait::Generous;
		L.RememberedMistakes = 2;
		return L;
	}
}

// ---------------------------------------------------------------- being born

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularBornTest,
	"Cigkofte.Regulars.AGoodServeCanTurnAStrangerIntoARegular",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularBornTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigRegularTestsRig Rig;
	if (!CigRegularTestsOpenEmptyShop(*this, Shop, Rig)) { return false; }

	ACigkofteCustomer* C = Rig.Cust->SpawnCustomer();
	if (!C) { AddError(TEXT("Musteri gelmedi.")); return false; }
	if (Rig.Cust->FrontCustomer() != C)
	{
		AddError(TEXT("Musteri kuyrugun basinda degil."));
		return false;
	}
	if (!CigRegularTestsPrepareServe(*this, *C, *Rig.Orders, Shop.World, /*bWantAccurate=*/true)) { return false; }

	// Read after the serve is prepared, because the spec is what the record has to
	// copy and the actor's own seed is what it has to remember.
	const FCigOrderSpec Served = C->Spec;
	const int32 ServedSeed = C->VisualSeed;
	const int32 Today = Rig.Days->Day;

	if (!CigRegularTestsSeedCreationRoll(*this, *Rig.Rng, /*bWantCreated=*/true)) { return false; }
	const int64 DrawsBefore = Rig.Rng->DrawCount();

	Rig.Cust->ServeFront();

	if (Rig.Cust->Loyals.Num() != 1)
	{
		AddError(FString::Printf(TEXT("Bir sadik musteri beklendi, %d var."), Rig.Cust->Loyals.Num()));
		return false;
	}
	const FCigLoyalCustomer& L = Rig.Cust->Loyals[0];

	// Two draws and no more: the chance and the name. Anything else on the serve
	// path taking from the shared stream would show up here before it showed up as
	// a mysteriously different customer three spawns later.
	TestEqual(TEXT("Sadakat yolu tam iki cekim yapmali"),
		(int32)(Rig.Rng->DrawCount() - DrawsBefore), 2);

	TestEqual(TEXT("Ilk sadik musteri 1 numarayi almali"), L.Id, 1);
	TestEqual(TEXT("Sonraki numara ilerlemeli"), Rig.Cust->NextLoyalId, 2);
	TestFalse(TEXT("Sadik musterinin adi olmali"), L.Name.IsEmpty());
	TestEqual(TEXT("Gorsel tohum musteriden gelmeli"), L.Seed, ServedSeed);
	TestTrue(TEXT("Favori siparis servis edilen siparis olmali"),
		CigRegularTestsSameSpec(L.Favorite, Served));
	TestEqual(TEXT("Ilk ziyaret sayilmali"), L.Visits, 1);
	TestEqual(TEXT("Baslangic memnuniyeti"), L.Satisfaction, 70.f, 0.01f);
	TestEqual(TEXT("Baslangic guveni"), L.Trust, 55.f, 0.01f);
	TestEqual(TEXT("Son ziyaret bugun olmali"), L.LastVisitDay, Today);
	TestEqual(TEXT("Bahsissiz servis ortalama bahsisi sifir birakmali"), L.AvgTip, 0.f, 0.01f);
	TestEqual(TEXT("Ozellikler musteriden gelmeli"), (int32)L.Traits, (int32)(uint16)ECigTrait::None);
	TestEqual(TEXT("Yeni sadik musterinin hatirladigi hata olmamali"), L.RememberedMistakes, 0);

	// The actor served is a stranger at the moment of the serve: the record is
	// made from them, not handed to them. They only become somebody with a name
	// the next time they walk in.
	TestEqual(TEXT("Servis edilen musteri o an hala yabanci olmali"), C->LoyalId, -1);
	return true;
}

// ---------------------------------------------------------------- the gates

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularPoorServeTest,
	"Cigkofte.Regulars.APoorServeNeverMakesARegular",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularPoorServeTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigRegularTestsRig Rig;
	if (!CigRegularTestsOpenEmptyShop(*this, Shop, Rig)) { return false; }

	ACigkofteCustomer* C = Rig.Cust->SpawnCustomer();
	if (!C) { AddError(TEXT("Musteri gelmedi.")); return false; }
	if (!CigRegularTestsPrepareServe(*this, *C, *Rig.Orders, Shop.World, /*bWantAccurate=*/false)) { return false; }

	// The stream is left on a seed that *would* create one. If the accuracy gate
	// were missing, this test would produce a regular rather than merely fail to
	// prove anything.
	if (!CigRegularTestsSeedCreationRoll(*this, *Rig.Rng, /*bWantCreated=*/true)) { return false; }
	const int64 DrawsBefore = Rig.Rng->DrawCount();

	Rig.Cust->ServeFront();

	TestEqual(TEXT("Dusuk dogrulukta sadik musteri olusmamali"), Rig.Cust->Loyals.Num(), 0);
	TestEqual(TEXT("Numara sayaci ilerlememeli"), Rig.Cust->NextLoyalId, 1);
	// The gate is in front of the roll, so a refused serve does not even cost a
	// draw. That is what stops the stream drifting between a shop that serves well
	// and one that does not.
	TestEqual(TEXT("Kapi cekimden once oldugu icin hic cekim olmamali"),
		(int32)(Rig.Rng->DrawCount() - DrawsBefore), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularNotCreatedTwiceTest,
	"Cigkofte.Regulars.AnExistingRegularIsNotCreatedTwice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularNotCreatedTwiceTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigRegularTestsRig Rig;
	if (!CigRegularTestsOpenEmptyShop(*this, Shop, Rig)) { return false; }

	ACigkofteCustomer* C = Rig.Cust->SpawnCustomer();
	if (!C) { AddError(TEXT("Musteri gelmedi.")); return false; }
	if (!CigRegularTestsPrepareServe(*this, *C, *Rig.Orders, Shop.World, /*bWantAccurate=*/true)) { return false; }

	// The book is arranged after the spawn on purpose: a non-empty book is what
	// SpawnCustomer draws a returning regular from, so filling it first would have
	// decided which branch the arrival took instead of which branch the serve did.
	const int32 Today = Rig.Days->Day;
	FCigLoyalCustomer Existing = CigRegularTestsMakeRecord(77, Today);
	Rig.Cust->Loyals.Add(Existing);
	Rig.Cust->NextLoyalId = 78;

	// This customer already is that regular. Setting the identity by hand is the
	// smallest honest way to say so: the alternative is a second seed probe whose
	// only purpose would be to reach a branch this test is not about.
	C->LoyalId   = Existing.Id;
	C->LoyalName = Existing.Name;

	if (!CigRegularTestsSeedCreationRoll(*this, *Rig.Rng, /*bWantCreated=*/true)) { return false; }
	const int64 DrawsBefore = Rig.Rng->DrawCount();

	Rig.Cust->ServeFront();

	TestEqual(TEXT("Zaten sadik olan icin ikinci kayit acilmamali"), Rig.Cust->Loyals.Num(), 1);
	TestEqual(TEXT("Numara sayaci ilerlememeli"), Rig.Cust->NextLoyalId, 78);
	TestEqual(TEXT("Kayit ayni kisiye ait kalmali"), Rig.Cust->Loyals[0].Id, Existing.Id);
	TestEqual(TEXT("Sadakat guncellemesi cekim yapmamali"),
		(int32)(Rig.Rng->DrawCount() - DrawsBefore), 0);

	// The serve still reaches them: a perfect wrap moves their satisfaction, which
	// is how we know the branch that ran was the loyalty update and not nothing at
	// all. The arithmetic itself belongs to Cigkofte.Loyalty.
	TestTrue(TEXT("Kusursuz servis mevcut sadik musteriyi etkilemeli"),
		Rig.Cust->Loyals[0].Satisfaction > Existing.Satisfaction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularBookFullTest,
	"Cigkofte.Regulars.TheRegularBookIsFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularBookFullTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigRegularTestsRig Rig;
	if (!CigRegularTestsOpenEmptyShop(*this, Shop, Rig)) { return false; }

	ACigkofteCustomer* C = Rig.Cust->SpawnCustomer();
	if (!C) { AddError(TEXT("Musteri gelmedi.")); return false; }
	if (!CigRegularTestsPrepareServe(*this, *C, *Rig.Orders, Shop.World, /*bWantAccurate=*/true)) { return false; }

	// Twelve is the book's whole capacity. Filled after the spawn for the same
	// reason as above.
	const int32 Today = Rig.Days->Day;
	for (int32 i = 0; i < 12; ++i)
	{
		Rig.Cust->Loyals.Add(CigRegularTestsMakeRecord(100 + i, Today));
	}
	Rig.Cust->NextLoyalId = 112;

	if (!CigRegularTestsSeedCreationRoll(*this, *Rig.Rng, /*bWantCreated=*/true)) { return false; }
	const int64 DrawsBefore = Rig.Rng->DrawCount();

	Rig.Cust->ServeFront();

	TestEqual(TEXT("Defter doluyken on ucuncu kayit acilmamali"), Rig.Cust->Loyals.Num(), 12);
	TestEqual(TEXT("Numara sayaci ilerlememeli"), Rig.Cust->NextLoyalId, 112);
	TestEqual(TEXT("Kapasite kapisi da cekimden once olmali"),
		(int32)(Rig.Rng->DrawCount() - DrawsBefore), 0);
	return true;
}

// ---------------------------------------------------------------- coming back

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularReturnsTest,
	"Cigkofte.Regulars.ARegularComesBackWithTheirOwnOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularReturnsTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigRegularTestsRig Rig;
	if (!CigRegularTestsOpenEmptyShop(*this, Shop, Rig)) { return false; }

	const int32 Today = Rig.Days->Day;
	const FCigLoyalCustomer Seeded = CigRegularTestsMakeRecord(4242, Today);
	Rig.Cust->Loyals.Add(Seeded);
	Rig.Cust->NextLoyalId = 4243;

	int32 WinningSeed = 0;
	ACigkofteCustomer* C =
		CigRegularTestsSpawnUntilRegular(*this, Rig, Seeded.Id, 256, &WinningSeed);
	if (!C) { return false; }

	// --- the same person, not a person with the same label ---
	TestEqual(TEXT("Donen musteri ayni kimligi tasimali"), C->LoyalId, Seeded.Id);
	TestEqual(TEXT("Donen musteri ayni adi tasimali"), C->LoyalName, Seeded.Name);
	TestEqual(TEXT("Donen musteri ayni gorunumu tasimali"), C->VisualSeed, Seeded.Seed);
	TestTrue(TEXT("Donen musteri favori siparisini istemeli"),
		CigRegularTestsSameSpec(C->Spec, Seeded.Favorite));

	// Their own traits, plus Regular. Not a fresh roll: a regular whose
	// personality changed between visits is not a regular.
	TestTrue(TEXT("Donen musteri Regular ozelligini almali"),
		EnumHasAnyFlags(C->Traits, ECigTrait::Regular));
	TestEqual(TEXT("Donen musteri kayitli ozelliklerini tasimali"),
		(int32)(uint16)C->Traits, (int32)(Seeded.Traits | (uint16)ECigTrait::Regular));

	// --- the book was updated exactly once ---
	if (Rig.Cust->Loyals.Num() != 1)
	{
		AddError(FString::Printf(TEXT("Tek kayit beklendi, %d var."), Rig.Cust->Loyals.Num()));
		return false;
	}
	const FCigLoyalCustomer& After = Rig.Cust->Loyals[0];
	TestEqual(TEXT("Ziyaret sayisi tam bir artmali"), After.Visits, Seeded.Visits + 1);
	TestEqual(TEXT("Son ziyaret gunu bugun olmali"), After.LastVisitDay, Today);
	// Arriving is not being served. Nothing about how the visit went is known yet,
	// so the relationship numbers must be untouched at the door.
	TestEqual(TEXT("Gelis memnuniyeti degistirmemeli"), After.Satisfaction, Seeded.Satisfaction, 0.01f);
	TestEqual(TEXT("Gelis guveni degistirmemeli"), After.Trust, Seeded.Trust, 0.01f);
	TestEqual(TEXT("Gelis hata hafizasini degistirmemeli"), After.RememberedMistakes, Seeded.RememberedMistakes);

	// --- a regular waits longer, by exactly the shop's own arithmetic ---
	//
	// Recomputed from the public multipliers rather than copied from the
	// production line, so this fails if a factor is dropped and passes if the
	// factors are merely reordered.
	const UCigEconomySystem* Eco = Shop.GM->Economy.Get();
	const UCigPricingSystem* Fiyatlar = Shop.GM->Pricing.Get();
	float Expected = FMath::Max(25.f, 55.f - 2.5f * (float)Today);
	Expected *= CigBalance::TraitPatienceMult((uint16)C->Traits);
	Expected *= 1.3f + Seeded.Trust / 300.f;
	if (Eco)
	{
		if (Eco->HasUpgrade(ECigUpgrade::Klima))        { Expected *= 1.2f; }
		if (Eco->HasUpgrade(ECigUpgrade::MuzikSistemi)) { Expected *= 1.08f; }
	}
	if (Fiyatlar)
	{
		if (Fiyatlar->PahaliGoruluyor())   { Expected *= 0.85f; }
		else if (Fiyatlar->UygunFiyatli()) { Expected *= 1.15f; }
	}
	if (Shop.GM->Events) { Expected *= Shop.GM->Events->PatienceMult(); }

	TestFalse(TEXT("Bu seviyede VIP cikmamali"), C->bVIP);
	TestEqual(TEXT("Sadik musterinin sabri guvenine gore hesaplanmali"), C->MaxPatience, Expected, 0.01f);
	TestEqual(TEXT("Sabir tam dolu baslamali"), C->Patience, C->MaxPatience, 0.01f);

	// --- and the same seed no longer produces them ---
	//
	// The sharpest form of the once-a-day rule: the exact arrival that just
	// happened, replayed from the exact seed that produced it, now hands back a
	// stranger. Nothing changed except the day stamped on the record.
	Rig.Cust->RemoveCustomer(C, /*bAngry=*/false);
	C->Deactivate();
	Rig.Cust->UpdateSystem(0.f);

	Rig.Rng->SeedWith(WinningSeed);
	ACigkofteCustomer* Again = Rig.Cust->SpawnCustomer();
	if (!Again) { AddError(TEXT("Ikinci musteri gelmedi.")); return false; }
	TestNotEqual(TEXT("Ayni tohum ayni gun sadik musteriyi tekrar getirmemeli"),
		Again->LoyalId, Seeded.Id);
	TestEqual(TEXT("Ikinci gelis ziyaret sayisini artirmamali"),
		Rig.Cust->Loyals[0].Visits, Seeded.Visits + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularOncePerDayTest,
	"Cigkofte.Regulars.ARegularVisitsOnceADay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularOncePerDayTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigRegularTestsRig Rig;
	if (!CigRegularTestsOpenEmptyShop(*this, Shop, Rig)) { return false; }

	const int32 Today = Rig.Days->Day;
	FCigLoyalCustomer Seeded = CigRegularTestsMakeRecord(555, Today);
	// Already served today. Nothing else about them is unusual, so the only reason
	// they can have for staying away is the day gate.
	Seeded.LastVisitDay = Today;
	Rig.Cust->Loyals.Add(Seeded);
	Rig.Cust->NextLoyalId = 556;

	// A sweep rather than one attempt: with a 30% chance of the book being
	// consulted at all, a single spawn that produced a stranger would be evidence
	// of nothing.
	const int32 Attempts = 64;
	int32 Returned = 0;
	for (int32 Candidate = 1; Candidate <= Attempts; ++Candidate)
	{
		Rig.Rng->SeedWith(Candidate);
		ACigkofteCustomer* C = Rig.Cust->SpawnCustomer();
		if (!C) { AddError(TEXT("Musteri gelmedi.")); return false; }
		if (C->LoyalId == Seeded.Id) { ++Returned; }
		Rig.Cust->RemoveCustomer(C, /*bAngry=*/false);
		C->Deactivate();
		Rig.Cust->UpdateSystem(0.f);
	}

	TestEqual(TEXT("Bugun gelmis sadik musteri tekrar gelmemeli"), Returned, 0);
	TestEqual(TEXT("Ziyaret sayaci hic artmamali"), Rig.Cust->Loyals[0].Visits, Seeded.Visits);
	TestEqual(TEXT("Son ziyaret gunu degismemeli"), Rig.Cust->Loyals[0].LastVisitDay, Today);

	// The gate is the day, not the book: move the stamp back and the same sweep
	// finds them. Without this the test above would also pass against a shop whose
	// regulars never return at all.
	Rig.Cust->Loyals[0].LastVisitDay = Today - 1;
	int32 ReturnedAfter = 0;
	for (int32 Candidate = 1; Candidate <= Attempts; ++Candidate)
	{
		Rig.Rng->SeedWith(Candidate);
		ACigkofteCustomer* C = Rig.Cust->SpawnCustomer();
		if (!C) { AddError(TEXT("Musteri gelmedi.")); return false; }
		if (C->LoyalId == Seeded.Id)
		{
			++ReturnedAfter;
			// Put the stamp back so the rest of the sweep keeps asking the same
			// question rather than answering the first one twice.
			Rig.Cust->Loyals[0].LastVisitDay = Today - 1;
		}
		Rig.Cust->RemoveCustomer(C, /*bAngry=*/false);
		C->Deactivate();
		Rig.Cust->UpdateSystem(0.f);
	}
	TestTrue(TEXT("Dun gelmis sadik musteri bugun gelebilmeli"), ReturnedAfter > 0);
	return true;
}

// ---------------------------------------------------------------- walking out

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularAngryTest,
	"Cigkofte.Regulars.WalkingOutAngryCostsARegularsTrust",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularAngryTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigRegularTestsRig Rig;
	if (!CigRegularTestsOpenEmptyShop(*this, Shop, Rig)) { return false; }

	const int32 Today = Rig.Days->Day;
	FCigLoyalCustomer Seeded = CigRegularTestsMakeRecord(9001, Today);
	Rig.Cust->Loyals.Add(Seeded);
	Rig.Cust->NextLoyalId = 9002;

	ACigkofteCustomer* C = CigRegularTestsSpawnUntilRegular(*this, Rig, Seeded.Id, 256, nullptr);
	if (!C) { return false; }
	C->bArrived = true;

	// Round numbers so the penalty is read rather than inferred.
	Rig.Cust->Loyals[0].Satisfaction       = 60.f;
	Rig.Cust->Loyals[0].Trust              = 50.f;
	Rig.Cust->Loyals[0].RememberedMistakes = 1;

	Rig.Cust->RemoveCustomer(C, /*bAngry=*/true);

	TestEqual(TEXT("Kizgin ayrilis guveni on bes dusurmeli"), Rig.Cust->Loyals[0].Trust, 35.f, 0.01f);
	TestEqual(TEXT("Kizgin ayrilis memnuniyeti yirmi dusurmeli"), Rig.Cust->Loyals[0].Satisfaction, 40.f, 0.01f);
	TestEqual(TEXT("Kizgin ayrilis bir hata hatirlatmali"), Rig.Cust->Loyals[0].RememberedMistakes, 2);

	// And it cannot dig below zero. A regular who has been let down four times is
	// at rock bottom, not in debt - a negative trust would be read as a discount
	// by anything that multiplies by it.
	Rig.Cust->Loyals[0].Satisfaction = 5.f;
	Rig.Cust->Loyals[0].Trust        = 10.f;

	// A fresh body wearing the same identity rather than the one that just left:
	// the customer that walked out is already leaving, and asking it to walk out
	// again would be testing a state the game never reaches.
	ACigkofteCustomer* Second = Rig.Cust->SpawnCustomer();
	if (!Second) { AddError(TEXT("Ikinci musteri gelmedi.")); return false; }
	Second->bArrived = true;
	Second->LoyalId  = Seeded.Id;

	Rig.Cust->RemoveCustomer(Second, /*bAngry=*/true);

	TestEqual(TEXT("Guven sifirin altina inmemeli"), Rig.Cust->Loyals[0].Trust, 0.f, 0.01f);
	TestEqual(TEXT("Memnuniyet sifirin altina inmemeli"), Rig.Cust->Loyals[0].Satisfaction, 0.f, 0.01f);
	TestEqual(TEXT("Ikinci kizgin ayrilis da hatirlanmali"), Rig.Cust->Loyals[0].RememberedMistakes, 3);

	// A regular leaving angry does not close their record. They are still on the
	// books, which is the whole reason the penalty is worth charging.
	TestEqual(TEXT("Kizgin ayrilis kaydi silmemeli"), Rig.Cust->Loyals.Num(), 1);
	return true;
}

// ---------------------------------------------------------------- the pool

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularRecycleTest,
	"Cigkofte.Regulars.ARecycledBodyForgetsWhoItWas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularRecycleTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	FCigRegularTestsRig Rig;
	if (!CigRegularTestsOpenEmptyShop(*this, Shop, Rig)) { return false; }

	const int32 Today = Rig.Days->Day;
	const FCigLoyalCustomer Seeded = CigRegularTestsMakeRecord(313, Today);
	Rig.Cust->Loyals.Add(Seeded);
	Rig.Cust->NextLoyalId = 314;

	ACigkofteCustomer* Regular = CigRegularTestsSpawnUntilRegular(*this, Rig, Seeded.Id, 256, nullptr);
	if (!Regular) { return false; }
	if (Regular->LoyalName.IsEmpty())
	{
		AddError(TEXT("Sadik musterinin adi bos; test kurulumu anlamsiz."));
		return false;
	}

	// Out through the real exit and back through the real recycle.
	Rig.Cust->RemoveCustomer(Regular, /*bAngry=*/false);
	Regular->Deactivate();
	Rig.Cust->UpdateSystem(0.f);

	// The pool hands back the most recently returned body first, so the next
	// arrival is this actor whatever else is in there. And the record now carries
	// today's date, so the next arrival cannot be the same regular again - which
	// is what makes "a stranger" the only possible answer.
	ACigkofteCustomer* Next = Rig.Cust->SpawnCustomer();
	if (!Next) { AddError(TEXT("Sonraki musteri gelmedi.")); return false; }

	TestEqual(TEXT("Havuz ayni bedeni geri vermeli"), Next, Regular);
	TestEqual(TEXT("Yeniden kullanilan beden eski kimligi tasimamali"), Next->LoyalId, -1);
	TestTrue(TEXT("Yeniden kullanilan beden eski adi tasimamali"), Next->LoyalName.IsEmpty());
	TestFalse(TEXT("Yeniden kullanilan beden Regular ozelligini tasimamali"),
		EnumHasAnyFlags(Next->Traits, ECigTrait::Regular));
	TestNotEqual(TEXT("Yeniden kullanilan beden eski gorunumu tasimamali"),
		Next->VisualSeed, Seeded.Seed);
	return true;
}

// ---------------------------------------------------------------- persistence

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigRegularSaveTest,
	"Cigkofte.Regulars.TheRegularBookSurvivesASaveAndLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigRegularSaveTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	if (!Cust || !Days) { AddError(TEXT("Sistemler yok.")); return false; }

	Cust->Loyals.Empty();

	// Two records that disagree with each other in every field, so a save that
	// wrote one of them twice, or read the fields back in the wrong order, cannot
	// look correct.
	FCigLoyalCustomer A;
	A.Id                   = 3;
	A.Name                 = TEXT("Hülya Abla");
	A.Seed                 = 112233;
	A.Favorite.Spice       = ECigSpice::AzAci;
	A.Favorite.Portion     = 2;
	A.Favorite.ToppingMask = (1 << 1) | (1 << 4);
	A.Favorite.bWantsAyran = true;
	A.Favorite.bPacked     = false;
	A.Favorite.Side        = ECigSide::Kunefe;
	A.Visits               = 9;
	A.Satisfaction         = 83.5f;
	A.Trust                = 41.25f;
	A.LastVisitDay         = 6;
	A.AvgTip               = 12.75f;
	A.Traits               = (uint16)(ECigTrait::Generous | ECigTrait::SpicyFoodLover);
	A.RememberedMistakes   = 4;

	FCigLoyalCustomer B;
	B.Id                   = 8;
	B.Name                 = TEXT("Kemal Amca");
	B.Seed                 = 445566;
	B.Favorite.Spice       = ECigSpice::CokAci;
	B.Favorite.Portion     = 1;
	B.Favorite.ToppingMask = (1 << 0);
	B.Favorite.bWantsAyran = false;
	B.Favorite.bPacked     = true;
	B.Favorite.Side        = ECigSide::Yok;
	B.Visits               = 2;
	B.Satisfaction         = 55.f;
	B.Trust                = 77.5f;
	B.LastVisitDay         = 7;
	B.AvgTip               = 0.f;
	B.Traits               = (uint16)ECigTrait::Student;
	B.RememberedMistakes   = 0;

	Cust->Loyals.Add(A);
	Cust->Loyals.Add(B);
	Cust->NextLoyalId = 9;
	Days->Day = 7;

	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);
	Save->SaveVersion = UCigSaveSubsystem::CurrentVersion;

	// Wreck the live book, so anything that comes back has to come back through
	// the save file rather than from the array still being there.
	Cust->Loyals.Empty();
	Cust->NextLoyalId = 1;

	Shop.GM->ApplySave(*Save);
	Save->RemoveFromRoot();

	if (Cust->Loyals.Num() != 2)
	{
		AddError(FString::Printf(TEXT("Iki kayit beklendi, %d dondu."), Cust->Loyals.Num()));
		return false;
	}
	TestEqual(TEXT("Sonraki numara donmeli"), Cust->NextLoyalId, 9);

	const FCigLoyalCustomer Expected[2] = { A, B };
	for (int32 i = 0; i < 2; ++i)
	{
		const FCigLoyalCustomer& E = Expected[i];
		const FCigLoyalCustomer& G = Cust->Loyals[i];
		const FString Tag = FString::Printf(TEXT("%d. kayit"), i);

		TestEqual(*(Tag + TEXT(": kimlik")), G.Id, E.Id);
		TestEqual(*(Tag + TEXT(": ad")), G.Name, E.Name);
		TestEqual(*(Tag + TEXT(": gorsel tohum")), G.Seed, E.Seed);
		TestTrue(*(Tag + TEXT(": favori siparis")), CigRegularTestsSameSpec(G.Favorite, E.Favorite));
		TestEqual(*(Tag + TEXT(": ziyaret")), G.Visits, E.Visits);
		TestEqual(*(Tag + TEXT(": memnuniyet")), G.Satisfaction, E.Satisfaction, 0.01f);
		TestEqual(*(Tag + TEXT(": guven")), G.Trust, E.Trust, 0.01f);
		TestEqual(*(Tag + TEXT(": son ziyaret gunu")), G.LastVisitDay, E.LastVisitDay);
		TestEqual(*(Tag + TEXT(": ortalama bahsis")), G.AvgTip, E.AvgTip, 0.01f);
		TestEqual(*(Tag + TEXT(": ozellikler")), (int32)G.Traits, (int32)E.Traits);
		TestEqual(*(Tag + TEXT(": hatirlanan hatalar")), G.RememberedMistakes, E.RememberedMistakes);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
