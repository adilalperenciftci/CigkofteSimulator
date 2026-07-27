// The parts that are only true once the systems are wired together.
//
// Everything else in Tests/ checks a pure function. These check the seams, and
// the seams are where this project's real defects have lived: a staff sale that
// earned money without counting as a sale, a save that captured more than it
// could restore. Neither is visible from inside any single function.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Shop; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Tests/CigTestShop.h"
#include "Economy/CigSaleSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigPricingSystem.h"
#include "Events/CigEventSystem.h"
#include "Game/CigDaySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Save/CigSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// A plain single-portion wrap, made competently. Nothing about it should
	// depend on who hands it over.
	FCigSatisTalebi DuzDurum(ECigSatisKaynagi Kaynak)
	{
		FCigSatisTalebi T;
		T.Kaynak = Kaynak;
		T.Wrap.bActive = true;
		T.Wrap.bWrapped = true;
		T.Wrap.Portions = 1;
		T.Wrap.DoughQuality = 80.f;
		T.Accuracy = 80.f;
		T.Quality = 80.f;
		return T;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffSaleCountsTest,
	"Cigkofte.Shop.StaffSaleCountsLikeAPlayerSale",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigStaffSaleCountsTest::RunTest(const FString& /*Parameters*/)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}

	// The defect: the apprentice's sale paid the player and moved nothing else.
	// It never touched TotalServed, so quests, achievements and bulk orders all
	// behaved as though the customer had never been served.
	const int32 ServisOnce = Shop.GM->Progression->TotalServed;
	const int32 ParaOnce = Shop.GM->Economy->Money;
	const int32 GunOnce = Shop.GM->Days->DayEarnings;

	const FCigSatisSonucu Sonuc = Shop.GM->Sales->SatisiIsle(DuzDurum(ECigSatisKaynagi::Personel));

	TestTrue(TEXT("Personel satışı para getirmeli"), Sonuc.Toplam > 0);
	TestEqual(TEXT("Personel satışı servis sayısını artırmalı"),
		Shop.GM->Progression->TotalServed, ServisOnce + 1);
	TestEqual(TEXT("Kasaya işlenmeli"), Shop.GM->Economy->Money, ParaOnce + Sonuc.Toplam);
	TestEqual(TEXT("Günün hasılatına işlenmeli"), Shop.GM->Days->DayEarnings, GunOnce + Sonuc.Toplam);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaleSourceParityTest,
	"Cigkofte.Shop.BothSourcesPriceTheSameWrap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSaleSourceParityTest::RunTest(const FString& /*Parameters*/)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}

	// The staff path used to price every wrap at a hardcoded 55 lira while the
	// counter used the price list. The same wrap has to be worth the same money
	// whoever carries it, or raising prices quietly excludes half the shop.
	const FCigSatisSonucu Personel = Shop.GM->Sales->SatisiIsle(DuzDurum(ECigSatisKaynagi::Personel));
	const FCigSatisSonucu Oyuncu = Shop.GM->Sales->SatisiIsle(DuzDurum(ECigSatisKaynagi::Oyuncu));

	TestEqual(TEXT("Aynı dürüm iki kaynakta da aynı brüt fiyatı etmeli"), Oyuncu.Brut, Personel.Brut);

	// The one documented difference: a combo is the player's streak. A staff
	// sale neither builds it nor breaks it.
	TestEqual(TEXT("Personel satışı kombo biriktirmemeli"), Personel.Kombo, 0);

	// This wrap is not perfect (accuracy 80 is under the 85 mark), so the
	// player's streak stays at zero too - which is what makes the gross prices
	// comparable in the first place.
	TestEqual(TEXT("Kusursuz olmayan dürüm kombo başlatmamalı"), Oyuncu.Kombo, 0);

	// The price came from the list, not from a literal.
	const int32 ListeDurum = Shop.GM->Pricing->Fiyat(CigUrunDurum);
	TestTrue(TEXT("Fiyat liste fiyatıyla ilişkili olmalı"),
		Personel.Brut > 0 && Personel.Brut <= ListeDurum);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffAdvancesContractTest,
	"Cigkofte.Shop.StaffSalesAdvanceABulkOrder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigStaffAdvancesContractTest::RunTest(const FString& /*Parameters*/)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}

	// The end-to-end version of the same defect, and the one that actually cost
	// the player money: a contract measures progress as TotalServed minus the
	// count at acceptance. With the staff path skipping that counter, an
	// apprentice could serve all day and the wedding order still settled at
	// zero delivered.
	UCigEventSystem* Olaylar = Shop.GM->Events.Get();
	if (!Olaylar)
	{
		AddError(TEXT("Olay sistemi yok."));
		return false;
	}

	Olaylar->TopluSiparis.bTeklifVar = true;
	Olaylar->TopluSiparis.IstenenAdet = 3;
	Olaylar->TopluSiparis.Odul = 300;
	Olaylar->TopluSiparis.TeslimGunu = 2;
	Olaylar->TopluSiparisiKabulEt();

	TestTrue(TEXT("Sözleşme kabul edilmiş olmalı"), Olaylar->TopluSiparis.bKabulEdildi);

	// Three wraps, every one of them made by the apprentice.
	for (int32 i = 0; i < 3; ++i)
	{
		Shop.GM->Sales->SatisiIsle(DuzDurum(ECigSatisKaynagi::Personel));
	}

	const int32 ParaOnce = Shop.GM->Economy->Money;
	Olaylar->OnDayEnd(2);

	TestEqual(TEXT("Sözleşme kapanmış olmalı"), Olaylar->TopluSiparis.IstenenAdet, 0);
	TestEqual(TEXT("Personelin yaptığı üç dürüm siparişi tam karşılamalı"),
		Shop.GM->Economy->Money, ParaOnce + 300);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigSaveRoundTripTest,
	"Cigkofte.Shop.SaveRoundTripKeepsTheShop",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigSaveRoundTripTest::RunTest(const FString& /*Parameters*/)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}

	// Migration tests prove an old file reaches the current schema. They say
	// nothing about whether the current schema can carry the shop, which is a
	// separate way to lose someone's progress: a field captured but never
	// applied looks correct in the save file and is gone on load.
	Shop.GM->Economy->Money = 4321;
	Shop.GM->Progression->Level = 7;
	Shop.GM->Progression->TotalServed = 142;
	Shop.GM->Progression->Rep = 63.5f;
	Shop.GM->Days->Day = 11;
	Shop.GM->Pricing->Carpanlar[CigUrunDurum] = 1.25f;

	UCigSaveGame* Save = NewObject<UCigSaveGame>();
	Save->AddToRoot();
	Shop.GM->CaptureSave(*Save);

	// Wreck the live state, so anything that survives has to have come back
	// through the save rather than never having left.
	Shop.GM->Economy->Money = 0;
	Shop.GM->Progression->Level = 1;
	Shop.GM->Progression->TotalServed = 0;
	Shop.GM->Progression->Rep = 0.f;
	Shop.GM->Days->Day = 1;
	Shop.GM->Pricing->Carpanlar[CigUrunDurum] = 1.f;

	Shop.GM->ApplySave(*Save);

	TestEqual(TEXT("Para geri gelmeli"), Shop.GM->Economy->Money, 4321);
	TestEqual(TEXT("Seviye geri gelmeli"), Shop.GM->Progression->Level, 7);
	TestEqual(TEXT("Servis sayısı geri gelmeli"), Shop.GM->Progression->TotalServed, 142);
	TestEqual(TEXT("İtibar geri gelmeli"), Shop.GM->Progression->Rep, 63.5f, 0.01f);
	TestEqual(TEXT("Gün geri gelmeli"), Shop.GM->Days->Day, 11);

	// The markup is a decision the player made, not balance data, so it is the
	// field most worth checking survives a round trip.
	TestEqual(TEXT("Oyuncunun fiyat marjı geri gelmeli"),
		Shop.GM->Pricing->Carpanlar[CigUrunDurum], 1.25f, 0.001f);

	Save->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
