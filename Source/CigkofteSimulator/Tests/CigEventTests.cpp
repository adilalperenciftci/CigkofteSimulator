// How a bulk order settles up.
//
// TopluSiparisSonucu is the only part of the special-day system worth testing
// on its own: everything else is scheduling, but this is the rule that decides
// whether taking a wedding order was a good idea. What matters is that falling
// just short is not treated the same as not turning up.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Events; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Events/CigEventSystem.h"
#include "Core/CigBalance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBulkOrderMetTest,
	"Cigkofte.Events.BulkOrderMetPaysInFull",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBulkOrderMetTest::RunTest(const FString& /*Parameters*/)
{
	const FCigTopluSonuc Tam = UCigEventSystem::TopluSiparisSonucu(20, 20);
	TestEqual(TEXT("Siparişi karşılamak tam ödeme getirmeli"), Tam.OdulOrani, 1.f, 0.001f);
	TestTrue(TEXT("Siparişi karşılamak itibar kazandırmalı"), Tam.ItibarFarki > 0.f);

	// Overdelivering is not worth more than the order: the customer asked for
	// twenty and pays for twenty.
	const FCigTopluSonuc Fazla = UCigEventSystem::TopluSiparisSonucu(35, 20);
	TestEqual(TEXT("Fazla üretim ek ödeme getirmemeli"), Fazla.OdulOrani, 1.f, 0.001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBulkOrderShortTest,
	"Cigkofte.Events.FallingShortHurtsByDegree",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBulkOrderShortTest::RunTest(const FString& /*Parameters*/)
{
	// 16 of 20 is a near miss; 5 of 20 is a shop that should never have accepted.
	const FCigTopluSonuc KilPayi = UCigEventSystem::TopluSiparisSonucu(16, 20);
	const FCigTopluSonuc Basarisiz = UCigEventSystem::TopluSiparisSonucu(5, 20);

	TestTrue(TEXT("Kıl payı kaçırmak kısmi ödeme almalı"),
		KilPayi.OdulOrani > 0.f && KilPayi.OdulOrani < 1.f);
	TestEqual(TEXT("Açık ara kaçırmak ödeme almamalı"), Basarisiz.OdulOrani, 0.f, 0.001f);

	TestTrue(TEXT("İki durum da itibar kaybettirmeli"),
		KilPayi.ItibarFarki < 0.f && Basarisiz.ItibarFarki < 0.f);
	TestTrue(TEXT("Açık ara kaçırmak daha çok itibar kaybettirmeli"),
		Basarisiz.ItibarFarki < KilPayi.ItibarFarki);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBulkOrderEdgeTest,
	"Cigkofte.Events.BulkOrderEdges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBulkOrderEdgeTest::RunTest(const FString& /*Parameters*/)
{
	// The threshold itself has to count as the near miss, not as a failure.
	const FCigTopluSonuc Esikte = UCigEventSystem::TopluSiparisSonucu(16, 20);
	const FCigTopluSonuc EsikAlti = UCigEventSystem::TopluSiparisSonucu(15, 20);
	TestTrue(TEXT("Eşikteki teslimat kısmi ödeme almalı"), Esikte.OdulOrani > 0.f);
	TestEqual(TEXT("Eşiğin altı ödeme almamalı"), EsikAlti.OdulOrani, 0.f, 0.001f);

	// A malformed order must not divide by zero or hand out free money.
	const FCigTopluSonuc Bos = UCigEventSystem::TopluSiparisSonucu(5, 0);
	TestEqual(TEXT("Sıfır adetli sipariş ödeme üretmemeli"), Bos.OdulOrani, 0.f, 0.001f);
	TestEqual(TEXT("Sıfır adetli sipariş itibarı değiştirmemeli"), Bos.ItibarFarki, 0.f, 0.001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigContractOfferTest,
	"Cigkofte.Events.ContractOfferScalesWithTheShop",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigContractOfferTest::RunTest(const FString& /*Parameters*/)
{
	const FCigTopluTeklif Yeni = UCigEventSystem::TeklifUret(1, 0);
	const FCigTopluTeklif Buyuk = UCigEventSystem::TeklifUret(6, 0);

	TestTrue(TEXT("Büyüyen dükkâna daha büyük sipariş gelmeli"), Buyuk.IstenenAdet > Yeni.IstenenAdet);
	TestTrue(TEXT("Daha büyük sipariş daha çok ödemeli"), Buyuk.Odul > Yeni.Odul);
	TestTrue(TEXT("Teklif hazırlanmak için gün bırakmalı"), Yeni.IhbarGunu >= 1);

	// The random spread may only add work, never remove it, or a lucky roll
	// would quietly make a contract easier than the level it was offered at.
	TestTrue(TEXT("Sapma siparişi küçültmemeli"),
		UCigEventSystem::TeklifUret(1, 4).IstenenAdet >= Yeni.IstenenAdet);

	// A contract for nothing would settle in full the moment it was accepted.
	TestTrue(TEXT("Sıfır seviyede bile sipariş boş olmamalı"),
		UCigEventSystem::TeklifUret(0, 0).IstenenAdet >= 1);
	TestTrue(TEXT("Bozuk seviye negatif sipariş üretmemeli"),
		UCigEventSystem::TeklifUret(-5, -5).IstenenAdet >= 1);
	TestTrue(TEXT("Ödeme sipariş adediyle orantılı olmalı"), Yeni.Odul > Yeni.IstenenAdet);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigContractSeparationTest,
	"Cigkofte.Events.ContractAndLargeDeliveryAreSeparate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigContractSeparationTest::RunTest(const FString& /*Parameters*/)
{
	// The multi-day contract used to read its notice period and its odds from
	// the events row that also spawns the large delivery, so one number decided
	// how often a wedding called and how often a fat delivery turned up, and the
	// player saw the same name for both. They are separate tables now.
	const FCigEventRow& Teslimat = CigBalance::Event(UCigEventSystem::EventBuyukTeslimat);
	TestEqual(TEXT("Olay satırı büyük teslimat olmalı"), Teslimat.Key, FString(TEXT("BuyukTeslimat")));

	// The contract table has to actually answer, or every lookup would silently
	// fall back to its hardcoded default and the CSV would do nothing.
	const float Sentinel = -999.f;
	for (const TCHAR* Key : { TEXT("MinGun"), TEXT("TeklifSansi"), TEXT("IhbarGunu"),
		TEXT("AdetTabani"), TEXT("UcretCarpani"), TEXT("KilPayiEsigi") })
	{
		TestNotEqual(FString::Printf(TEXT("Contracts tablosunda %s bulunmalı"), Key),
			CigBalance::Contract(Key, Sentinel), Sentinel);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigEventTextTest,
	"Cigkofte.Events.EveryEventHasWording",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigEventTextTest::RunTest(const FString& /*Parameters*/)
{
	// Event wording is addressed by a key built at runtime (event.<key>.name),
	// which is precisely what the static check in Tools/check_sources.py cannot
	// see: it only follows literal CigText::Get calls. Without this test a typo
	// in a row key would show the player "event.macgunu.start" mid-game and
	// nothing would have complained.
	for (int32 i = 0; i < CigEventDefCount; ++i)
	{
		const FString Ad = UCigEventSystem::EventName(i);
		const FString Bas = UCigEventSystem::EventStartMsg(i);
		const FString Bit = UCigEventSystem::EventEndMsg(i);

		TestFalse(FString::Printf(TEXT("Olay %d adı ham anahtar olmamalı"), i), Ad.StartsWith(TEXT("event.")));
		TestFalse(FString::Printf(TEXT("Olay %d başlangıç metni ham anahtar olmamalı"), i), Bas.StartsWith(TEXT("event.")));
		TestFalse(FString::Printf(TEXT("Olay %d bitiş metni ham anahtar olmamalı"), i), Bit.StartsWith(TEXT("event.")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigEventTeardownTest,
	"Cigkofte.Events.DayLongEventsEndCleanly",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigEventTeardownTest::RunTest(const FString& /*Parameters*/)
{
	// A day-long event carries TimeLeft < 0, so UpdateSystem never retires it and
	// the day used to end by emptying the array - which dropped it without ever
	// running EndEvent. The retired count is the observable that separates the
	// two: emptying the array would also leave Active at zero.
	UCigEventSystem* Sys = NewObject<UCigEventSystem>();
	Sys->AddToRoot();

	FCigActiveEvent Zamanli;
	Zamanli.DefIndex = UCigEventSystem::EventRain;
	Zamanli.TimeLeft = 30.f;
	Sys->Active.Add(Zamanli);

	FCigActiveEvent GunBoyu;
	GunBoyu.DefIndex = 0;
	GunBoyu.TimeLeft = -1.f;
	Sys->Active.Add(GunBoyu);

	TestNotEqual(TEXT("Olaylar aktifken çarpan nötr olmamalı"), Sys->SpawnMult(), 1.f);

	TestEqual(TEXT("İki olay da EndEvent üzerinden kapanmalı"), Sys->TumOlaylariBitir(), 2);
	TestEqual(TEXT("Boş listede kapatılacak olay kalmamalı"), Sys->TumOlaylariBitir(), 0);

	TestEqual(TEXT("Gün sonunda hiçbir olay aktif kalmamalı"), Sys->Active.Num(), 0);
	TestEqual(TEXT("Çarpanlar nötre dönmeli"), Sys->SpawnMult(), 1.f, 0.001f);
	TestEqual(TEXT("Sabır çarpanı nötre dönmeli"), Sys->PatienceMult(), 1.f, 0.001f);
	TestFalse(TEXT("Buzdolabı arızası kalmamalı"), Sys->IsFridgeBroken());

	Sys->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
