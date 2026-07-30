// What the batch on the counter looks like, and why.
//
// The station used to be handed two numbers and lerp between two colours, so
// none of this could be checked and none of it was visible: isot, mix quality
// and freshness all fed the price, the customer's reaction and the review while
// the thing the player was looking at ignored them. These tests pin the
// derivation - not the exact colours, which are a design decision and will move,
// but the direction each input pushes it, which is what makes the counter
// readable at all.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.DoughVisual; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Cooking/CigDoughVisual.h"
#include "Cooking/CigCookingSystem.h"
#include "Game/CigDaySystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Tests/CigTestShop.h"
#include "Core/CigkofteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Perceived lightness, near enough for "is this darker than that".
	float Parlaklik(const FLinearColor& C)
	{
		return 0.299f * C.R + 0.587f * C.G + 0.114f * C.B;
	}

	// How far the colour is from grey. Falls as a mix gets worse or a batch stales.
	float Doygunluk(const FLinearColor& C)
	{
		const float Enb = FMath::Max3(C.R, C.G, C.B);
		const float Enk = FMath::Min3(C.R, C.G, C.B);
		return Enb > 0.f ? (Enb - Enk) / Enb : 0.f;
	}

	FCigDoughVisual Yogrulmus()
	{
		FCigDoughVisual V;
		V.Fill01 = 1.f;
		V.Knead01 = 1.f;
		V.Spice01 = 0.5f;
		V.Quality01 = 1.f;
		V.Freshness01 = 1.f;
		return V;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDoughVisualDerivationTest,
	"Cigkofte.DoughVisual.EveryInputMovesTheColour",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigDoughVisualDerivationTest::RunTest(const FString& /*Parameters*/)
{
	// Nothing in the bowl, nothing on the counter.
	FCigDoughVisual Bos;
	TestFalse(TEXT("Boş kase görünmemeli"), Bos.IsVisible());
	Bos.Fill01 = 0.1f;
	TestTrue(TEXT("İçinde bir şey olan kase görünmeli"), Bos.IsVisible());

	// Fill drives the ball's size, and a full bowl has to read as bigger than a
	// nearly empty one from across the room.
	FCigDoughVisual Az = Yogrulmus();
	Az.Fill01 = 0.1f;
	FCigDoughVisual Cok = Yogrulmus();
	Cok.Fill01 = 1.f;
	TestTrue(TEXT("Dolu kase daha büyük görünmeli"), Cok.Scale() > Az.Scale());

	// Kneading: pale dry bulgur turns into worked, dark dough.
	FCigDoughVisual Ham = Yogrulmus();
	Ham.Knead01 = 0.f;
	TestTrue(TEXT("Yoğrulmuş hamur ham kaseden koyu olmalı"),
		Parlaklik(Yogrulmus().Color()) < Parlaklik(Ham.Color()));

	// Isot: the same batch with more isot has to read redder. Measured as the
	// red channel's lead over the others, so it cannot be satisfied by the
	// colour simply getting brighter.
	FCigDoughVisual Acisiz = Yogrulmus();
	Acisiz.Spice01 = 0.f;
	FCigDoughVisual CokAci = Yogrulmus();
	CokAci.Spice01 = 1.f;
	const FLinearColor A = Acisiz.Color();
	const FLinearColor B = CokAci.Color();
	TestTrue(TEXT("İsotlu hamurda kırmızı baskınlığı artmalı"),
		(B.R - FMath::Max(B.G, B.B)) > (A.R - FMath::Max(A.G, A.B)));

	// Spice is only visible once the batch has been worked: isot stirred into a
	// loose bowl has not coloured anything yet, and showing it before then would
	// tell the player the mix is further along than it is.
	FCigDoughVisual HamAcisiz = Acisiz;
	HamAcisiz.Knead01 = 0.f;
	FCigDoughVisual HamAcili = CokAci;
	HamAcili.Knead01 = 0.f;
	TestTrue(TEXT("Yoğrulmamış kasede isot rengi henüz görünmemeli"),
		HamAcisiz.Color().Equals(HamAcili.Color(), 0.01f));

	// A careless mix is duller rather than a different colour.
	FCigDoughVisual Kotu = Yogrulmus();
	Kotu.Quality01 = 0.f;
	TestTrue(TEXT("Kötü karışım daha mat olmalı"),
		Doygunluk(Kotu.Color()) < Doygunluk(Yogrulmus().Color()));

	// Staling greys and darkens it, but a dead batch is still sellable and must
	// not look like ash.
	FCigDoughVisual Bayat = Yogrulmus();
	Bayat.Freshness01 = 0.f;
	TestTrue(TEXT("Bayat hamur solmalı"),
		Doygunluk(Bayat.Color()) < Doygunluk(Yogrulmus().Color()));
	TestTrue(TEXT("Bayat hamur tamamen griye dönmemeli"), Doygunluk(Bayat.Color()) > 0.05f);

	// The station skips material writes on an unchanged state, so "unchanged"
	// has to mean unchanged in every field.
	FCigDoughVisual X = Yogrulmus();
	TestTrue(TEXT("Aynı durum eşit sayılmalı"), X.NearlyEqual(Yogrulmus()));
	FCigDoughVisual Y = Yogrulmus();
	Y.Freshness01 = 0.5f;
	TestFalse(TEXT("Tazeliği değişen durum eşit sayılmamalı"), X.NearlyEqual(Y));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDoughVisualFromFoodStateTest,
	"Cigkofte.DoughVisual.ComesFromTheFoodOnTheCounter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigDoughVisualFromFoodStateTest::RunTest(const FString& /*Parameters*/)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}

	UCigCookingSystem* Cook = Shop.GM->Cooking.Get();
	if (!Cook)
	{
		AddError(TEXT("Pişirme sistemi kurulmadı."));
		return false;
	}

	// An empty bowl shows nothing at all.
	TestFalse(TEXT("Boş tezgâhta gösterilecek bir şey olmamalı"), Cook->CurrentVisual().IsVisible());

	// Ingredients go in: the bowl fills and the isot reads through.
	Shop.GM->Days->StartDay(false);
	for (int32 i = 0; i < 5; ++i) { Cook->AddIngredient(ECigIngredient::Bulgur); }
	const FCigDoughVisual Sade = Cook->CurrentVisual();
	TestTrue(TEXT("Malzeme girince kase görünür olmalı"), Sade.IsVisible());
	TestEqual(TEXT("Isotsuz kasede acılık sıfır olmalı"), Sade.Spice01, 0.f, 0.001f);

	for (int32 i = 0; i < 4; ++i) { Cook->AddIngredient(ECigIngredient::Isot); }
	const FCigDoughVisual Acili = Cook->CurrentVisual();
	TestTrue(TEXT("İsot eklenince acılık artmalı"), Acili.Spice01 > Sade.Spice01);
	TestTrue(TEXT("Kase doluluğu artmalı"), Acili.Fill01 > Sade.Fill01);
	TestEqual(TEXT("Yoğrulmamış kasede yoğurma sıfır olmalı"), Acili.Knead01, 0.f, 0.001f);

	// A finished batch reports itself as fully kneaded and fully fresh, and its
	// fill tracks the servings left rather than what went into the bowl.
	Cook->Dough.Servings = CigDoughServings;
	Cook->Dough.Quality = 80.f;
	Cook->Dough.Spice = ECigSpice::CokAci;
	Cook->Dough.Freshness = 100.f;

	const FCigDoughVisual Tam = Cook->CurrentVisual();
	TestEqual(TEXT("Hazır hamur tamamen yoğrulmuş görünmeli"), Tam.Knead01, 1.f, 0.001f);
	TestEqual(TEXT("Dolu hamur tam dolulukta olmalı"), Tam.Fill01, 1.f, 0.001f);
	TestEqual(TEXT("Çok acılı hamur en yüksek acılıkta olmalı"), Tam.Spice01, 1.f, 0.001f);

	Cook->Dough.Servings = CigDoughServings / 2;
	TestEqual(TEXT("Yarısı kullanılan hamur yarı dolulukta olmalı"),
		Cook->CurrentVisual().Fill01, 0.5f, 0.001f);

	// Freshness falls while the batch sits, and the visual has to follow it -
	// this is the one input a player cannot see any other way on the counter.
	Cook->Dough.Freshness = 20.f;
	const FCigDoughVisual Bayat = Cook->CurrentVisual();
	TestEqual(TEXT("Tazelik görsele geçmeli"), Bayat.Freshness01, 0.2f, 0.001f);
	TestTrue(TEXT("Bayatlayan hamurun rengi değişmeli"),
		!Bayat.Color().Equals(Tam.Color(), 0.01f));

	// Every field stays inside 0-1 whatever the state throws at it, because the
	// colour maths assumes it.
	Cook->Dough.Quality = 200.f;
	Cook->Dough.Servings = CigDoughServings * 3;
	const FCigDoughVisual Asiri = Cook->CurrentVisual();
	TestTrue(TEXT("Doluluk 0-1 aralığında kalmalı"), Asiri.Fill01 >= 0.f && Asiri.Fill01 <= 1.f);
	TestTrue(TEXT("Kalite 0-1 aralığında kalmalı"), Asiri.Quality01 >= 0.f && Asiri.Quality01 <= 1.f);

	return true;
}

// Stage 1.3: a loose mix slumps and a worked one gathers.
//
// The shape is the only thing on the counter that separates a bowl stirred twice
// from one that is ready - the colour moves too, but slowly and under four other
// inputs. Pinned as directions rather than numbers: wider and flatter when
// loose, and roughly the same volume throughout, so the batch gathers instead of
// appearing to grow.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigDoughCohesionShape,
	"Cigkofte.DoughVisual.LooseDoughSlumpsAndWorkedDoughGathers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigDoughCohesionShape::RunTest(const FString&)
{
	FCigDoughVisual Gevsek;
	Gevsek.Fill01 = 0.8f;
	Gevsek.Knead01 = 0.f;

	FCigDoughVisual Yogrulmus = Gevsek;
	Yogrulmus.Knead01 = 1.f;

	const FVector G = Gevsek.Scale3D();
	const FVector Y = Yogrulmus.Scale3D();

	TestTrue(TEXT("Gevşek karışım daha geniş yayılmalı"), G.X > Y.X);
	TestTrue(TEXT("Gevşek karışım daha alçak durmalı"), G.Z < Y.Z);
	TestTrue(TEXT("Yoğrulmuş hamur küresel olmalı"),
		FMath::IsNearlyEqual(Y.X, Y.Z, 0.001f) && FMath::IsNearlyEqual(Y.X, Y.Y, 0.001f));

	// Volume is only roughly held - the point is that the batch does not read as
	// growing while it is being worked, not that the maths conserves anything.
	const float HacimG = G.X * G.Y * G.Z;
	const float HacimY = Y.X * Y.Y * Y.Z;
	TestTrue(TEXT("Toparlanırken hacim büyümemeli"), HacimY <= HacimG * 1.05f);
	TestTrue(TEXT("Toparlanırken hacim çökmemeli"), HacimY >= HacimG * 0.65f);

	// Cohesion is Knead01 today and is named separately because it will not
	// always be. If they are ever wired apart, this is what says so.
	TestEqual(TEXT("Tutunma yoğurma ilerlemesini izlemeli"), Yogrulmus.Cohesion01(), 1.f, 0.001f);

	// An empty bowl has no shape to report; Scale3D must not be asked to divide
	// by anything it was not given.
	FCigDoughVisual Bos;
	TestFalse(TEXT("Boş kase görünmemeli"), Bos.IsVisible());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
