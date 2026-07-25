// The pure functions of the balance layer: effects derived from a trait mask,
// achievement condition resolution, and skill coefficients coming from the table.
//
// These tests do not care about the file system: they check the same values
// whether they came from Config/Balance/*.csv or from the C++ defaults. So they
// pass both in a clone without the CSVs and in a working tree with untouched
// balance. Change a number in a CSV and the matching test breaks - that is
// deliberate, so a balance change is visible.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Balance; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/CigBalance.h"
#include "Core/CigkofteTypes.h"
#include "Core/CigUpgrades.h"
#include "Progression/CigSkillSystem.h"
#include "Progression/CigAchievementSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigAchievementStats MakeEmptyStats()
	{
		FCigAchievementStats S;
		S.Level = 1;
		S.Day = 1;
		return S;
	}
}

// --- Table integrity ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBalanceTablesCoverEnumsTest,
	"Cigkofte.Balance.Tables.CoverAllEnumEntries",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBalanceTablesCoverEnumsTest::RunTest(const FString& /*Parameters*/)
{
	// Every enum entry needs a row, and no row may carry an empty name; a broken
	// CSV row is caught here rather than as a blank label in the HUD.
	for (int32 i = 0; i < (int32)ECigSkill::COUNT; ++i)
	{
		TestFalse(FString::Printf(TEXT("Yetenek %d adı boş olmamalı"), i), CigBalance::Skill(i).Name.IsEmpty());
		TestTrue(FString::Printf(TEXT("Yetenek %d azami rütbesi pozitif olmalı"), i), CigBalance::Skill(i).MaxRank > 0);
	}
	for (int32 i = 0; i < (int32)ECigUpgrade::COUNT; ++i)
	{
		TestFalse(FString::Printf(TEXT("Geliştirme %d adı boş olmamalı"), i), CigBalance::Upgrade(i).Name.IsEmpty());
	}
	for (int32 i = 0; i < (int32)ECigAchievement::COUNT; ++i)
	{
		TestFalse(FString::Printf(TEXT("Başarım %d adı boş olmamalı"), i), CigBalance::Achievement(i).Name.IsEmpty());
	}
	TestEqual(TEXT("Özellik tablosu ECigTrait ile aynı uzunlukta olmalı"), CigBalance::TraitCount(), CigTraitCount);
	for (int32 i = 0; i < CigStockCount; ++i)
	{
		TestTrue(FString::Printf(TEXT("Stok %d fiyatı pozitif olmalı"), i), CigBalance::Stock(i).BaseCost > 0);
		TestTrue(FString::Printf(TEXT("Stok %d sipariş miktarı pozitif olmalı"), i), CigBalance::Stock(i).OrderAmount > 0);
	}
	return true;
}

// --- Trait mask <-> index ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBalanceTraitMaskRoundTripTest,
	"Cigkofte.Balance.Traits.MaskIndexRoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBalanceTraitMaskRoundTripTest::RunTest(const FString& /*Parameters*/)
{
	for (int32 i = 0; i < CigBalance::TraitCount(); ++i)
	{
		const uint16 Mask = CigBalance::TraitMaskOfIndex(i);
		TestEqual(FString::Printf(TEXT("Özellik %d maske→indeks dönüşü"), i), CigBalance::TraitIndexOfMask(Mask), i);
	}

	// Zero and multi-bit masks do not map to a single trait.
	TestEqual(TEXT("Boş maske -1 dönmeli"), CigBalance::TraitIndexOfMask(0), -1);
	const uint16 TwoBits = (uint16)ECigTrait::Impatient | (uint16)ECigTrait::Generous;
	TestEqual(TEXT("İki bitli maske -1 dönmeli"), CigBalance::TraitIndexOfMask(TwoBits), -1);

	// The table row must line up with the enum's own bit.
	TestEqual(TEXT("Impatient maskesi tablonun 0. satırı"),
		CigBalance::TraitIndexOfMask((uint16)ECigTrait::Impatient), 0);
	TestEqual(TEXT("SecretCritic maskesi tablonun 13. satırı"),
		CigBalance::TraitIndexOfMask((uint16)ECigTrait::SecretCritic), 13);
	return true;
}

// --- Trait effects ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBalanceTraitPatienceTest,
	"Cigkofte.Balance.Traits.PatienceMultiplies",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBalanceTraitPatienceTest::RunTest(const FString& /*Parameters*/)
{
	TestTrue(TEXT("Özelliksiz müşteri sabrı değişmez"),
		FMath::IsNearlyEqual(CigBalance::TraitPatienceMult(0), 1.f, KINDA_SMALL_NUMBER));

	const float Patient = CigBalance::TraitPatienceMult((uint16)ECigTrait::Patient);
	const float Impatient = CigBalance::TraitPatienceMult((uint16)ECigTrait::Impatient);
	TestTrue(TEXT("Sakin müşteri daha sabırlı olmalı"), Patient > 1.f);
	TestTrue(TEXT("Sabırsız müşteri daha az sabırlı olmalı"), Impatient < 1.f);

	// A trait with no effect must not disturb the product.
	const uint16 PatientAndTourist = (uint16)ECigTrait::Patient | (uint16)ECigTrait::Tourist;
	TestTrue(TEXT("Etkisiz özellik sabır çarpanını değiştirmemeli"),
		FMath::IsNearlyEqual(CigBalance::TraitPatienceMult(PatientAndTourist), Patient, KINDA_SMALL_NUMBER));

	// Several effective traits accumulate multiplicatively.
	const uint16 Both = (uint16)ECigTrait::Patient | (uint16)ECigTrait::Impatient;
	TestTrue(TEXT("İki sabır etkisi çarpılarak birikmeli"),
		FMath::IsNearlyEqual(CigBalance::TraitPatienceMult(Both), Patient * Impatient, KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBalanceTraitTipTest,
	"Cigkofte.Balance.Traits.TipEffectsAccumulate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBalanceTraitTipTest::RunTest(const FString& /*Parameters*/)
{
	TestTrue(TEXT("Özelliksiz müşteride bahşiş farkı yok"),
		FMath::IsNearlyEqual(CigBalance::TraitTipChanceDelta(0), 0.f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Özelliksiz müşteride bahşiş oranı dayatması yok"),
		FMath::IsNearlyEqual(CigBalance::TraitTipMultOverride(0), 0.f, KINDA_SMALL_NUMBER));

	const float Generous = CigBalance::TraitTipChanceDelta((uint16)ECigTrait::Generous);
	const float Student = CigBalance::TraitTipChanceDelta((uint16)ECigTrait::Student);
	TestTrue(TEXT("Eli açık müşteri bahşiş şansını artırmalı"), Generous > 0.f);
	TestTrue(TEXT("Öğrenci bahşiş şansını azaltmalı"), Student < 0.f);

	// Deltas add up: two opposing traits partly cancel out.
	const uint16 Both = (uint16)ECigTrait::Generous | (uint16)ECigTrait::Student;
	TestTrue(TEXT("Bahşiş farkları toplanmalı"),
		FMath::IsNearlyEqual(CigBalance::TraitTipChanceDelta(Both), Generous + Student, KINDA_SMALL_NUMBER));

	// A generous customer forces the tip rate outright; adding another trait cannot lower it.
	const float Override = CigBalance::TraitTipMultOverride((uint16)ECigTrait::Generous);
	TestTrue(TEXT("Eli açık müşteri bahşiş oranını dayatmalı"), Override > 0.f);
	TestTrue(TEXT("Dayatmayan özellik eklenince oran düşmemeli"),
		FMath::IsNearlyEqual(CigBalance::TraitTipMultOverride(Both), Override, KINDA_SMALL_NUMBER));
	return true;
}

// --- Skill coefficients ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBalanceSkillEffectsTest,
	"Cigkofte.Balance.Skills.EffectDirections",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBalanceSkillEffectsTest::RunTest(const FString& /*Parameters*/)
{
	// Additive skills need a coefficient above 0 (the multiplier grows with rank).
	TestTrue(TEXT("Hızlı El katsayısı pozitif olmalı"), CigSkillEffect(ECigSkill::HizliEl) > 0.f);
	TestTrue(TEXT("Güler Yüz katsayısı pozitif olmalı"), CigSkillEffect(ECigSkill::Guleryuzlu) > 0.f);

	// Damping skills must sit between 0 and 1; above 1 the effect inverts and a
	// skill meant to slow dirt build-up would speed it up instead.
	for (const ECigSkill S : { ECigSkill::TemizIsci, ECigSkill::DayanikliBunye, ECigSkill::PazarlikUstasi })
	{
		const float K = CigSkillEffect(S);
		TestTrue(FString::Printf(TEXT("Sönümlü yetenek katsayısı (0,1) aralığında olmalı: %s"), *CigSkillDef(S).Name),
			K > 0.f && K < 1.f);
	}
	return true;
}

// --- Achievement conditions ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigAchievementThresholdTest,
	"Cigkofte.Balance.Achievements.ThresholdIsInclusive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigAchievementThresholdTest::RunTest(const FString& /*Parameters*/)
{
	const FCigAchievementRow& YuzDurum = CigAchievementDef(ECigAchievement::YuzDurum);

	FCigAchievementStats S = MakeEmptyStats();
	S.TotalServed = FMath::FloorToInt(YuzDurum.Threshold) - 1;
	TestFalse(TEXT("Eşiğin bir altında açılmamalı"), CigAchievementMet(YuzDurum, S));

	S.TotalServed = FMath::FloorToInt(YuzDurum.Threshold);
	TestTrue(TEXT("Eşiğe ulaşınca açılmalı"), CigAchievementMet(YuzDurum, S));

	S.TotalServed = FMath::FloorToInt(YuzDurum.Threshold) + 1000;
	TestTrue(TEXT("Eşiğin üstünde açık kalmalı"), CigAchievementMet(YuzDurum, S));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigAchievementBooleanStatsTest,
	"Cigkofte.Balance.Achievements.BooleanStats",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigAchievementBooleanStatsTest::RunTest(const FString& /*Parameters*/)
{
	const FCigAchievementRow& EvSahibi = CigAchievementDef(ECigAchievement::EvSahibi);

	FCigAchievementStats S = MakeEmptyStats();
	TestFalse(TEXT("Ev alınmadan açılmamalı"), CigAchievementMet(EvSahibi, S));

	S.bOwnHouse = true;
	TestTrue(TEXT("Ev alınınca açılmalı"), CigAchievementMet(EvSahibi, S));

	// The cat achievement takes the lower of two values: feeding alone is not enough.
	const FCigAchievementRow& KediDostu = CigAchievementDef(ECigAchievement::KediDostu);
	FCigAchievementStats Cat = MakeEmptyStats();
	Cat.CatHappy = KediDostu.Threshold - 1.f;
	TestFalse(TEXT("Kedi yeterince mutlu değilse açılmamalı"), CigAchievementMet(KediDostu, Cat));
	Cat.CatHappy = KediDostu.Threshold;
	TestTrue(TEXT("Kedi mutluluğu eşiğe ulaşınca açılmalı"), CigAchievementMet(KediDostu, Cat));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigAchievementUnknownStatTest,
	"Cigkofte.Balance.Achievements.UnknownStatIsReported",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigAchievementUnknownStatTest::RunTest(const FString& /*Parameters*/)
{
	// A typo in the CSV must not quietly turn an achievement into one that never unlocks.
	FCigAchievementRow Bad;
	Bad.Key = TEXT("Test");
	Bad.Stat = TEXT("BoyleBirIstatistikYok");
	Bad.Threshold = 1.f;

	bool bUnknown = false;
	const FCigAchievementStats S = MakeEmptyStats();
	TestFalse(TEXT("Bilinmeyen istatistik başarımı açmamalı"), CigAchievementMet(Bad, S, &bUnknown));
	TestTrue(TEXT("Bilinmeyen istatistik bildirilmeli"), bUnknown);

	// For a known stat the flag must stay clear.
	bool bKnownFlag = true;
	CigAchievementMet(CigAchievementDef(ECigAchievement::IlkDurum), S, &bKnownFlag);
	TestFalse(TEXT("Bilinen istatistikte bayrak temizlenmeli"), bKnownFlag);

	// Every row in the default table must use a recognised stat.
	for (int32 i = 0; i < (int32)ECigAchievement::COUNT; ++i)
	{
		bool bRowUnknown = false;
		CigAchievementMet(CigBalance::Achievement(i), S, &bRowUnknown);
		TestFalse(FString::Printf(TEXT("Başarım '%s' tanınan bir Stat kullanmalı"), *CigBalance::Achievement(i).Key), bRowUnknown);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
