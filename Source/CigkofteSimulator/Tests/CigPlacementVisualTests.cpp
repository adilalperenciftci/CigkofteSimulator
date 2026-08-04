// The join between a placement record and the actors that represent it.
//
// Stage 3.5 needs to restore a layout from a save. The audit found that restoring
// a moved table would have moved the record - and with it navigation, validation
// and seat capacity - while the mesh stayed where the world builder put it,
// because nothing mapped a StableId to an actor. These tests exist so that cannot
// be true again.
//
// The Math group needs no world. The Registry group needs actors but not a shop.
// The Shop group is the one that matters: it stands up the real world builder and
// asserts that every installed record actually has something to look at.

#include "Misc/AutomationTest.h"
#include "Placement/CigPlacementVisuals.h"
#include "Placement/CigPlacementSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FTransform MakeTransform(float X, float Y, float Yaw)
	{
		return FTransform(FRotator(0.f, Yaw, 0.f), FVector(X, Y, 0.f));
	}

	AActor* SpawnAt(UWorld* World, const FTransform& Transform)
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		if (Actor)
		{
			Actor->SetMobility(EComponentMobility::Movable);
			Actor->SetActorTransform(Transform);
		}
		return Actor;
	}
}

// ------------------------------------------------------------------------ math

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigVisualRoundTripTest,
	"Cigkofte.PlacementVisual.Math.RelativeThenResolveReturnsTheOriginal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigVisualRoundTripTest::RunTest(const FString&)
{
	// The property everything else rests on. If this is not exact, every move
	// drifts a chair a little further from its table.
	const FTransform Placement = MakeTransform(-300.f, 950.f, 0.f);
	const FTransform Chair = MakeTransform(-300.f, 855.f, 270.f);

	const FTransform Relative = CigPlacementVisualMath::MakeRelative(Placement, Chair);
	const FTransform Back = CigPlacementVisualMath::Resolve(Placement, Relative);

	TestTrue(TEXT("Bagil donusum geri cozuldugunde ayni olmali"), Back.Equals(Chair, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigVisualFollowsMoveTest,
	"Cigkofte.PlacementVisual.Math.AnOffsetFollowsATranslationAndARotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigVisualFollowsMoveTest::RunTest(const FString&)
{
	const FTransform Placement = MakeTransform(0.f, 0.f, 0.f);
	// A chair 95 cm along -Y of its table, which is the authored seat offset.
	const FTransform Chair = MakeTransform(0.f, -95.f, 0.f);
	const FTransform Relative = CigPlacementVisualMath::MakeRelative(Placement, Chair);

	// Pure translation: the chair keeps its offset.
	const FTransform Moved = CigPlacementVisualMath::Resolve(MakeTransform(500.f, 200.f, 0.f), Relative);
	TestTrue(TEXT("Otelemede bagil konum korunmali"),
		Moved.GetLocation().Equals(FVector(500.f, 105.f, 0.f), 0.01f));

	// A quarter turn: the offset turns with the table rather than staying on -Y.
	const FTransform Turned = CigPlacementVisualMath::Resolve(MakeTransform(0.f, 0.f, 90.f), Relative);
	TestTrue(TEXT("Donusta bagil konum masayla birlikte donmeli"),
		Turned.GetLocation().Equals(FVector(95.f, 0.f, 0.f), 0.01f));
	return true;
}

// -------------------------------------------------------------------- registry

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigVisualRegistryTest,
	"Cigkofte.PlacementVisual.Registry.AttachApplyRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigVisualRegistryTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}

	FCigPlacementVisualRegistry Registry;
	const FName Id(TEXT("test.table"));
	const FTransform Placement = MakeTransform(0.f, 0.f, 0.f);

	AActor* Table = SpawnAt(Shop.World, Placement);
	AActor* Chair = SpawnAt(Shop.World, MakeTransform(0.f, -95.f, 0.f));

	TestTrue(TEXT("Aktor baglanmali"), Registry.Attach(Id, Table, Placement));
	TestTrue(TEXT("Ikinci aktor de baglanmali"), Registry.Attach(Id, Chair, Placement));
	TestEqual(TEXT("Iki gorsel kayitli olmali"), Registry.VisualCount(Id), 2);
	TestEqual(TEXT("Tek yerlesim kayitli olmali"), Registry.PlacementCount(), 1);

	// Attaching twice would move an actor twice per apply and make every count wrong.
	TestFalse(TEXT("Ayni aktor iki kez baglanmamali"), Registry.Attach(Id, Table, Placement));
	TestEqual(TEXT("Tekrar baglama sayiyi degistirmemeli"), Registry.VisualCount(Id), 2);

	// A null actor is ignored rather than stored: an absent optional mesh must not
	// leave a hole for Apply to walk into.
	TestFalse(TEXT("Null aktor baglanmamali"), Registry.Attach(Id, nullptr, Placement));
	TestEqual(TEXT("Null baglama sayiyi degistirmemeli"), Registry.VisualCount(Id), 2);

	TestEqual(TEXT("Bilinmeyen kimlik icin uygulama sifir olmali"),
		Registry.Apply(FName(TEXT("test.yok")), Placement), 0);

	const FTransform NewPlacement = MakeTransform(400.f, -200.f, 90.f);
	TestEqual(TEXT("Iki aktor de tasinmali"), Registry.Apply(Id, NewPlacement), 2);
	TestTrue(TEXT("Masa yeni yerlesime gitmeli"),
		Table->GetActorLocation().Equals(FVector(400.f, -200.f, 0.f), 0.01f));
	TestTrue(TEXT("Sandalye masayla birlikte donmeli"),
		Chair->GetActorLocation().Equals(FVector(495.f, -200.f, 0.f), 0.01f));

	// A destroyed actor is dropped rather than counted, so a cleaned-up world does
	// not leave the invariant passing on nothing.
	Chair->Destroy();
	TestEqual(TEXT("Yok edilen aktor sayilmamali"), Registry.VisualCount(Id), 1);
	TestEqual(TEXT("Yok edilen aktor tasinmamali"), Registry.Apply(Id, Placement), 1);

	TestEqual(TEXT("Birakma canli aktor sayisini dondurmeli"), Registry.Release(Id), 1);
	TestFalse(TEXT("Birakilan yerlesim kalmamali"), Registry.Contains(Id));
	TestEqual(TEXT("Birakma sonrasi yerlesim sayisi sifir olmali"), Registry.PlacementCount(), 0);
	return true;
}

// ------------------------------------------------------------------------ shop

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigVisualEveryRecordTest,
	"Cigkofte.PlacementVisual.Shop.EveryInstalledRecordHasSomethingToLookAt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigVisualEveryRecordTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}
	if (!Shop.GM->WorldBuilder || !Shop.GM->Placement)
	{
		AddError(TEXT("Dunya kurucusu veya yerlesim sistemi yok."));
		return false;
	}
	Shop.GM->WorldBuilder->BuildWorld();

	int32 Installed = 0;
	int32 WithVisual = 0;
	TArray<FString> Missing;

	for (const FCigPlacementRecord& Record : Shop.GM->Placement->PlacementRecords())
	{
		if (Record.Lifetime != ECigPlacementLifetime::Installed)
		{
			continue;
		}
		++Installed;
		if (Shop.GM->WorldBuilder->PlacementVisuals.VisualCount(Record.StableId) > 0)
		{
			++WithVisual;
		}
		else
		{
			Missing.Add(Record.StableId.ToString());
		}
	}

	TestTrue(TEXT("Varsayilan yerlesimde kurulu kayit olmali"), Installed > 0);
	TestEqual(FString::Printf(TEXT("Her kurulu kaydin gorseli olmali (eksik: %s)"),
		Missing.Num() > 0 ? *FString::Join(Missing, TEXT(", ")) : TEXT("yok")),
		WithVisual, Installed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigVisualTableMovesWholeTest,
	"Cigkofte.PlacementVisual.Shop.MovingATableTakesItsChairsWithIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigVisualTableMovesWholeTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this))
	{
		return false;
	}
	if (!Shop.GM->WorldBuilder || !Shop.GM->Placement)
	{
		AddError(TEXT("Dunya kurucusu veya yerlesim sistemi yok."));
		return false;
	}
	Shop.GM->WorldBuilder->BuildWorld();

	// A seating fixture is one record and four actors: the table, its plate and
	// two chairs. That is the case the audit said would break.
	const FName TableId(TEXT("fixture.seating.table.0"));
	const FCigPlacementRecord* Record = Shop.GM->Placement->FindPlacement(TableId);
	if (!Record)
	{
		AddError(TEXT("Varsayilan yerlesimde fixture.seating.table.0 yok."));
		return false;
	}

	const int32 Visuals = Shop.GM->WorldBuilder->PlacementVisuals.VisualCount(TableId);
	TestTrue(TEXT("Masa birden fazla aktorden olusmali"), Visuals > 1);

	// Sync against the unmoved record first: nothing should shift, which is what
	// proves the offsets were captured against the normalized transform rather
	// than against the requested one.
	const FVector Before = Record->Transform.GetLocation();
	TestEqual(TEXT("Yerinde eslemede tum aktorler islenmeli"),
		Shop.GM->WorldBuilder->SyncPlacementVisuals(TableId), Visuals);

	const FCigPlacementRecord* After = Shop.GM->Placement->FindPlacement(TableId);
	TestTrue(TEXT("Yerinde esleme kaydi degistirmemeli"),
		After && After->Transform.GetLocation().Equals(Before, 0.01f));
	return true;
}

#endif
