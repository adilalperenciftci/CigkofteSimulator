// The demand curve behind the pricing system.
//
// UCigPricingSystem::TalepCarpani is a pure function, so the whole model can be
// checked without standing up a game. What matters is not the exact numbers but
// the shape: dearer must mean quieter, a richer street must absorb the same
// price better, and neither end may run away far enough to break a day.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Pricing; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Economy/CigPricingSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPricingNeutralTest,
	"Cigkofte.Pricing.ListPriceLeavesDemandAlone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigPricingNeutralTest::RunTest(const FString& /*Parameters*/)
{
	// Selling at list price in an average street is the baseline the balance
	// numbers are tuned against; anything other than 1 here shifts every day.
	TestEqual(TEXT("Liste fiyatı talebi değiştirmemeli"),
		UCigPricingSystem::TalepCarpani(1.f, 1.4f, 1.f), 1.f, 0.001f);

	// With no elasticity the price stops mattering, whatever it is.
	TestEqual(TEXT("Esneklik 0 ise fiyat talebi etkilemez"),
		UCigPricingSystem::TalepCarpani(1.8f, 0.f, 1.f), 1.f, 0.001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPricingDirectionTest,
	"Cigkofte.Pricing.DearerMeansQuieter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigPricingDirectionTest::RunTest(const FString& /*Parameters*/)
{
	const float Pahali = UCigPricingSystem::TalepCarpani(1.3f, 1.4f, 1.f);
	const float Ucuz = UCigPricingSystem::TalepCarpani(0.8f, 1.4f, 1.f);

	TestTrue(TEXT("Zam talebi düşürmeli"), Pahali < 1.f);
	TestTrue(TEXT("İndirim talebi yükseltmeli"), Ucuz > 1.f);

	// A more elastic product has to lose more custom for the same hike, or the
	// per-product Esneklik column is doing nothing.
	const float EsnekUrun = UCigPricingSystem::TalepCarpani(1.3f, 1.8f, 1.f);
	TestTrue(TEXT("Esnek üründe aynı zam daha çok müşteri kaçırmalı"), EsnekUrun < Pahali);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPricingIncomeTest,
	"Cigkofte.Pricing.RicherStreetAbsorbsPrice",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigPricingIncomeTest::RunTest(const FString& /*Parameters*/)
{
	const float Yoksul = UCigPricingSystem::TalepCarpani(1.3f, 1.4f, 0.85f);
	const float Orta = UCigPricingSystem::TalepCarpani(1.3f, 1.4f, 1.f);
	const float Zengin = UCigPricingSystem::TalepCarpani(1.3f, 1.4f, 1.3f);

	TestTrue(TEXT("Gelir arttıkça aynı fiyat daha az müşteri kaçırmalı"), Yoksul < Orta && Orta < Zengin);

	// Income is meant to offset price exactly, so pricing up in step with the
	// neighbourhood should leave footfall where it was.
	TestEqual(TEXT("Fiyat ve gelir birlikte artarsa talep sabit kalmalı"),
		UCigPricingSystem::TalepCarpani(1.3f, 1.4f, 1.3f), 1.f, 0.001f);

	// A corrupt table must not produce an infinity.
	TestEqual(TEXT("Sıfır gelir nötr sokağa düşmeli"),
		UCigPricingSystem::TalepCarpani(1.f, 1.4f, 0.f), 1.f, 0.001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPricingClampTest,
	"Cigkofte.Pricing.ExtremesStayPlayable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigPricingClampTest::RunTest(const FString& /*Parameters*/)
{
	// Giving it away must not summon a queue nobody could serve, and pricing
	// absurdly must not empty the shop so hard the day cannot be played out.
	const float Bedava = UCigPricingSystem::TalepCarpani(0.1f, 2.f, 1.f);
	const float Fahis = UCigPricingSystem::TalepCarpani(10.f, 2.f, 1.f);

	TestTrue(TEXT("Talep üst sınırı aşmamalı"), Bedava <= 2.f);
	TestTrue(TEXT("Talep alt sınırın altına inmemeli"), Fahis >= 0.25f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPriceScoreTest,
	"Cigkofte.Pricing.PriceStarsFollowTheStreet",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigPriceScoreTest::RunTest(const FString& /*Parameters*/)
{
	// The price stars used to come from the cheap/normal/expensive toggle, so a
	// shop could charge double through its per-product markups and still be
	// rated on price as if nothing had changed. They follow the real ratio now.
	const float Ucuz = UCigPricingSystem::FiyatPuani(0.75f);
	const float Esit = UCigPricingSystem::FiyatPuani(1.00f);
	const float Pahali = UCigPricingSystem::FiyatPuani(1.25f);

	TestTrue(TEXT("Ucuzlukta puan artmalı"), Ucuz > Esit);
	TestTrue(TEXT("Pahalılıkta puan düşmeli"), Pahali < Esit);

	// Sokak seviyesinde fiyat, nötrün biraz üstünde olmalı: adil fiyat cezalandırılmaz.
	TestTrue(TEXT("Sokakla aynı fiyat nötrün altına düşmemeli"), Esit >= 3.f);

	// The thresholds the written comments use have to agree with the stars, or a
	// review calling the shop expensive lands next to a high price rating.
	TestTrue(TEXT("Pahalı eşiğinde puan belirgin biçimde kötü olmalı"), Pahali <= 2.5f);
	TestTrue(TEXT("Ucuz eşiğinde puan belirgin biçimde iyi olmalı"), Ucuz >= 4.5f);

	// A five-category score averages these, so the range has to stay inside it
	// however silly the pricing gets.
	for (const float Oran : { 0.f, 0.01f, 0.5f, 1.f, 3.f, 100.f })
	{
		const float Puan = UCigPricingSystem::FiyatPuani(Oran);
		TestTrue(FString::Printf(TEXT("Oran %.2f için puan 1-5 aralığında kalmalı"), Oran),
			Puan >= 1.f && Puan <= 5.f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
