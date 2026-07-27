// The dialogue bucket space and the line table.
//
// What actually matters here is that the bucket key is produced IDENTICALLY by
// the generation pipeline (CigDialogueGenerator) and at runtime
// (CigDialogueContext::CacheKey). If they drift apart, none of the 9,600
// generated lines can ever be found, and it happens silently - the game falls
// back to canned sentences and nobody notices.
//
// Running:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Dialogue; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "AI/CigDialogueContext.h"
#include "AI/CigDialogueTable.h"
#include "AI/CigOfflineDialogueProvider.h"
#include "Core/CigText.h"
#include "Core/CigkofteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDialogueBucketSpaceTest,
	"Cigkofte.Dialogue.BucketSpaceIsFiniteAndUnique",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigDialogueBucketSpaceTest::RunTest(const FString& /*Parameters*/)
{
	// The same loop as the generator: every combination must yield a unique key
	// and the total must match BucketCount.
	TSet<FString> Keys;
	for (int32 Mood = 0; Mood < 5; ++Mood)
	{
		for (int32 TraitIdx = 0; TraitIdx <= FCigDialogueContext::NoTraitIndex; ++TraitIdx)
		{
			for (int32 Flags = 0; Flags < 32; ++Flags)
			{
				Keys.Add(FCigDialogueContext::BucketKey(Mood, TraitIdx,
					(Flags & 1) != 0, (Flags & 2) != 0, (Flags & 4) != 0,
					(Flags & 8) != 0, (Flags & 16) != 0));
			}
		}
	}

	TestEqual(TEXT("Kova sayısı BucketCount ile aynı olmalı"), Keys.Num(), FCigDialogueContext::BucketCount);
	TestEqual(TEXT("Kova sayısı 2400 olmalı"), Keys.Num(), 2400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDialogueContextKeyMatchesGeneratorTest,
	"Cigkofte.Dialogue.ContextKeyMatchesGeneratorKey",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigDialogueContextKeyMatchesGeneratorTest::RunTest(const FString& /*Parameters*/)
{
	// Build a real serving context and verify its key matches what the generator
	// would write for the same situation.
	FCigDialogueContext Ctx;
	Ctx.Accuracy = 95.f;             // Delighted
	Ctx.Quality = 90.f;
	Ctx.Traits = (uint16)ECigTrait::HygieneSensitive;
	Ctx.bVIP = false;
	Ctx.bRegular = false;
	Ctx.bWantedAyran = true;
	Ctx.bGotAyran = true;            // ayran correct -> a1
	Ctx.Hygiene = 30.f;              // low -> h1
	Ctx.PatienceFrac = 1.f;          // plenty of patience -> p0

	const int32 TraitIdx = FCigDialogueContext::DominantTraitIndex((uint16)ECigTrait::HygieneSensitive);
	const FString Expected = FCigDialogueContext::BucketKey(
		(int32)FCigDialogueContext::EMood::Delighted, TraitIdx, false, false, true, true, false);

	TestEqual(TEXT("Bağlam anahtarı üreteç anahtarıyla aynı olmalı"), Ctx.CacheKey(), Expected);
	TestEqual(TEXT("Titiz müşterinin özellik indeksi 4 olmalı"), TraitIdx, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDialogueWrongOrderTest,
	"Cigkofte.Dialogue.WrongOrderReachesItsOwnBucket",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigDialogueWrongOrderTest::RunTest(const FString& /*Parameters*/)
{
	// Regression. The caller filled the delivered side of the context from the
	// requested side, so the a-flag was pinned to 1 and the 1200 buckets written
	// for a wrong order could never be addressed. What proves the fix is not
	// that a correct order still keys to a1, but that a wrong one keys to a0.
	FCigDialogueContext Dogru;
	Dogru.Accuracy = 95.f;
	Dogru.Quality = 90.f;
	Dogru.RequestedSpice = ECigSpice::CokAci;
	Dogru.ServedSpice = ECigSpice::CokAci;
	Dogru.bWantedAyran = true;
	Dogru.bGotAyran = true;

	TestTrue(TEXT("Doğru sipariş eşleşmiş sayılmalı"), Dogru.OrderMatched());
	TestTrue(TEXT("Doğru siparişin anahtarı a1 olmalı"), Dogru.CacheKey().Contains(TEXT("_a1_")));

	// Missing ayran.
	FCigDialogueContext AyranYok = Dogru;
	AyranYok.bGotAyran = false;
	TestFalse(TEXT("Eksik ayran eşleşme sayılmamalı"), AyranYok.OrderMatched());
	TestTrue(TEXT("Eksik ayran a0 kovasına düşmeli"), AyranYok.CacheKey().Contains(TEXT("_a0_")));

	// Ayran nobody asked for is also a mistake, in the other direction.
	FCigDialogueContext FazlaAyran = Dogru;
	FazlaAyran.bWantedAyran = false;
	TestFalse(TEXT("İstenmeyen ayran da hata sayılmalı"), FazlaAyran.OrderMatched());

	// Wrong spice, which the context could not express at all before.
	FCigDialogueContext YanlisAci = Dogru;
	YanlisAci.ServedSpice = ECigSpice::AzAci;
	TestFalse(TEXT("Yanlış acılık eşleşme sayılmamalı"), YanlisAci.OrderMatched());
	TestTrue(TEXT("Yanlış acılık a0 kovasına düşmeli"), YanlisAci.CacheKey().Contains(TEXT("_a0_")));

	// The prompt has to be able to name the mistake, not just score it.
	TestTrue(TEXT("Doğru siparişte hata metni boş olmalı"), Dogru.MistakeSummary().IsEmpty());
	TestFalse(TEXT("Eksik ayran hata metninde geçmeli"), AyranYok.MistakeSummary().IsEmpty());
	TestFalse(TEXT("Yanlış acılık hata metninde geçmeli"), YanlisAci.MistakeSummary().IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDialogueDominantTraitTest,
	"Cigkofte.Dialogue.DominantTraitFollowsPriority",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigDialogueDominantTraitTest::RunTest(const FString& /*Parameters*/)
{
	TestEqual(TEXT("Özelliksiz müşteri 'yok' kovasına düşmeli"),
		FCigDialogueContext::DominantTraitIndex(0), FCigDialogueContext::NoTraitIndex);

	// A single trait: its own bit position.
	TestEqual(TEXT("Sabırsız → 0"),
		FCigDialogueContext::DominantTraitIndex((uint16)ECigTrait::Impatient), 0);

	// Multiple traits: the most narratively decisive one wins.
	// The undercover critic beats everything.
	const uint16 CriticAndStudent = (uint16)ECigTrait::SecretCritic | (uint16)ECigTrait::Student;
	TestEqual(TEXT("Gizli eleştirmen öğrenciyi yenmeli"),
		FCigDialogueContext::DominantTraitIndex(CriticAndStudent), 13);

	// The influencer beats the regular.
	const uint16 InfluencerAndRegular = (uint16)ECigTrait::Influencer | (uint16)ECigTrait::Regular;
	TestEqual(TEXT("Fenomen müdavimi yenmeli"),
		FCigDialogueContext::DominantTraitIndex(InfluencerAndRegular), 6);

	// The result must always be a valid bucket index.
	for (uint16 Mask = 0; Mask < 512; ++Mask)
	{
		const int32 Idx = FCigDialogueContext::DominantTraitIndex(Mask);
		if (Idx < 0 || Idx > FCigDialogueContext::NoTraitIndex)
		{
			AddError(FString::Printf(TEXT("Maske %u geçersiz indeks verdi: %d"), Mask, Idx));
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDialogueTableLoadsTest,
	"Cigkofte.Dialogue.SeedTableLoadsAndMatchesKeys",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigDialogueTableLoadsTest::RunTest(const FString& /*Parameters*/)
{
	// Config/Dialogue/Lines.csv carries the seed lines in the repo. Without it
	// the test would be meaningless, so its presence is asserted too.
	if (CigDialogue::TotalLines() == 0)
	{
		AddError(TEXT("Diyalog tablosu boş — Config/Dialogue/Lines.csv okunamadı."));
		return false;
	}

	// A bucket in the seed table must actually be findable.
	const FString Key = FCigDialogueContext::BucketKey(
		(int32)FCigDialogueContext::EMood::Delighted, FCigDialogueContext::NoTraitIndex,
		false, false, true, false, false);

	const TArray<FCigDialogueRow>& Rows = CigDialogue::LinesFor(Key);
	TestTrue(FString::Printf(TEXT("'%s' kovasında replik olmalı"), *Key), Rows.Num() > 0);
	for (const FCigDialogueRow& R : Rows)
	{
		TestEqual(TEXT("Satırın anahtarı aranan kova olmalı"), R.Key, Key);
		TestFalse(TEXT("Türkçe replik boş olmamalı"), R.TR.IsEmpty());
		TestFalse(TEXT("İngilizce replik boş olmamalı"), R.EN.IsEmpty());
	}

	// A missing bucket returns empty quietly (the game falls back to canned lines).
	TestEqual(TEXT("Bilinmeyen kova boş dönmeli"),
		CigDialogue::LinesFor(TEXT("m9_t99_v0_r0_a0_h0_p0")).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDialogueFollowsGameLanguageTest,
	"Cigkofte.Dialogue.CustomersSpeakTheGamesLanguage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigDialogueFollowsGameLanguageTest::RunTest(const FString& /*Parameters*/)
{
	// The provider used to ask UE's culture which language to answer in, while
	// every other string in the game asks CigText. On an English Windows the
	// menus were Turkish and the customers replied in English, in the same
	// breath. Both now read one setting.
	//
	// There are two paths out of PickLine and both were reading the wrong
	// setting, so both are checked here.
	const int32 Onceki = CigText::GetLanguage();

	// --- Path 1: the generated table -------------------------------------
	// A low-hygiene angry customer is a bucket the seed table covers, so this
	// exercises the bilingual row lookup.
	FCigDialogueContext Tabloda;
	Tabloda.Traits = (uint16)ECigTrait::HygieneSensitive;
	Tabloda.Hygiene = 10.f;
	Tabloda.Accuracy = 30.f;
	Tabloda.Quality = 30.f;

	const TArray<FCigDialogueRow>& TabloSatirlari = CigDialogue::LinesFor(Tabloda.CacheKey());
	if (TabloSatirlari.Num() == 0)
	{
		AddError(FString::Printf(TEXT("'%s' kovası tabloda yok; test yanlış yolu ölçüyor."),
			*Tabloda.CacheKey()));
		return false;
	}

	CigText::SetLanguage(0);
	const FString TabloTr = FCigOfflineDialogueProvider::PickLine(Tabloda);
	CigText::SetLanguage(1);
	const FString TabloEn = FCigOfflineDialogueProvider::PickLine(Tabloda);
	CigText::SetLanguage(Onceki);

	bool bTrSutunundan = false;
	bool bEnSutunundan = false;
	for (const FCigDialogueRow& R : TabloSatirlari)
	{
		bTrSutunundan |= (R.TR == TabloTr);
		bEnSutunundan |= (R.EN == TabloEn);
	}
	TestTrue(TEXT("Türkçe oyunda tablo repliği TR sütunundan gelmeli"), bTrSutunundan);
	TestTrue(TEXT("İngilizce oyunda tablo repliği EN sütunundan gelmeli"), bEnSutunundan);

	// --- Path 2: the canned mood pool -------------------------------------
	// No VIP bucket was ever generated, so a VIP falls through to the pool -
	// the lines that used to live in TEXT() in Turkish only.
	FCigDialogueContext Havuzda;
	Havuzda.bVIP = true;
	Havuzda.Accuracy = 20.f;
	Havuzda.Quality = 20.f;   // angry, no dominant trait
	if (CigDialogue::LinesFor(Havuzda.CacheKey()).Num() != 0)
	{
		AddError(FString::Printf(TEXT("'%s' artık tabloda; yedek havuz yolu ölçülemiyor."),
			*Havuzda.CacheKey()));
		return false;
	}

	static const TCHAR* AngryKeys[] = {
		TEXT("dlg.mood.angry.0"), TEXT("dlg.mood.angry.1"), TEXT("dlg.mood.angry.2")
	};

	CigText::SetLanguage(0);
	const FString HavuzTr = FCigOfflineDialogueProvider::PickLine(Havuzda);
	TArray<FString> BeklenenTr;
	for (const TCHAR* K : AngryKeys) { BeklenenTr.Add(CigText::Get(K)); }

	CigText::SetLanguage(1);
	const FString HavuzEn = FCigOfflineDialogueProvider::PickLine(Havuzda);
	TArray<FString> BeklenenEn;
	for (const TCHAR* K : AngryKeys) { BeklenenEn.Add(CigText::Get(K)); }

	CigText::SetLanguage(Onceki);

	TestTrue(TEXT("Türkçe oyunda yedek replik TR havuzundan gelmeli"),
		BeklenenTr.Contains(HavuzTr));
	TestTrue(TEXT("İngilizce oyunda yedek replik EN havuzundan gelmeli"),
		BeklenenEn.Contains(HavuzEn));
	// The two pools must not be the same text, or the English column is a copy
	// of the Turkish one and nothing above would notice.
	TestFalse(TEXT("İki dilin havuzları aynı olmamalı"), BeklenenEn.Contains(BeklenenTr[0]));

	TestEqual(TEXT("Test dilin ayarını geri bırakmalı"), CigText::GetLanguage(), Onceki);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
