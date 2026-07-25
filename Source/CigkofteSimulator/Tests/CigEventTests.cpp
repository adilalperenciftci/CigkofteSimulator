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

#endif // WITH_DEV_AUTOMATION_TESTS
