// What build mode is willing to let a player pick up.
//
// Selection reads like a rendering detail and is not one: whether a thing can be
// moved is a question about the placement authority, and the answer must not
// depend on how the thing was reached. So the decision is a pure function over a
// record, testable with no world, and the trace that found the actor is somebody
// else's problem.
//
// The reverse lookup is tested with real actors, because that is the only part
// that genuinely needs them.

#include "Misc/AutomationTest.h"
#include "Core/CigInput.h"
#include "Placement/CigBuildSelection.h"
#include "Placement/CigPlacementVisuals.h"
#include "Tests/CigTestShop.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Named for this file rather than generically. Two test files each had an
	// anonymous MakeRecord with a different signature, which was invisible until
	// a new test file shifted the unity grouping and put them in one translation
	// unit - at which point the build broke somewhere neither had been touched.
	FCigPlacementRecord MakeSelectionRecord(const TCHAR* Id, ECigPlacementCategory Category,
		ECigPlacementLifetime Lifetime)
	{
		FCigPlacementRecord R;
		R.StableId = FName(Id);
		R.Category = Category;
		R.Lifetime = Lifetime;
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildSelectionInstalledTest,
	"Cigkofte.BuildMode.AnInstalledPlacementIsSelectable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildSelectionInstalledTest::RunTest(const FString&)
{
	const FCigPlacementRecord Table = MakeSelectionRecord(TEXT("masa.01"),
		ECigPlacementCategory::Seating, ECigPlacementLifetime::Installed);

	const FCigBuildSelection S = CigBuildSelection::Resolve(Table.StableId, &Table);

	TestTrue(TEXT("Kurulu yerlesim secilebilmeli"), S.IsValid());
	TestTrue(TEXT("Hata olmamali"), S.Fault == ECigBuildSelectionFault::None);
	TestEqual(TEXT("Kimlik tasinmali"), S.StableId, Table.StableId);
	TestTrue(TEXT("Kategori tasinmali"), S.Category == ECigPlacementCategory::Seating);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildSelectionRefusalsTest,
	"Cigkofte.BuildMode.EachRefusalIsNamedRatherThanCollapsedIntoNo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildSelectionRefusalsTest::RunTest(const FString&)
{
	// Looking at a wall. The overwhelmingly common case and not an error, which is
	// why it has to be distinguishable from the two that are.
	const FCigBuildSelection Nothing = CigBuildSelection::Resolve(NAME_None, nullptr);
	TestFalse(TEXT("Bos iz secim vermemeli"), Nothing.IsValid());
	TestTrue(TEXT("Bos iz NotAPlacement olmali"),
		Nothing.Fault == ECigBuildSelectionFault::NotAPlacement);
	TestTrue(TEXT("Bos iz kimlik tasimamali"), Nothing.StableId.IsNone());

	// The registry knew the actor, the authority did not know the record. Nothing
	// the player did causes this, so it must not read as bad aim.
	const FCigBuildSelection Orphan = CigBuildSelection::Resolve(FName(TEXT("hayalet.01")), nullptr);
	TestFalse(TEXT("Kayitsiz yerlesim secilememeli"), Orphan.IsValid());
	TestTrue(TEXT("Kayitsiz yerlesim Orphaned olmali"),
		Orphan.Fault == ECigBuildSelectionFault::Orphaned);
	TestEqual(TEXT("Kayitsiz yerlesim kimligini bildirmeli"), Orphan.StableId, FName(TEXT("hayalet.01")));

	// A delivery crate: real, present, and not the shop's layout. The save keeps
	// only Installed records, so letting a player move this would promise to
	// remember something that will not survive the evening.
	const FCigPlacementRecord Crate = MakeSelectionRecord(TEXT("kasa.01"),
		ECigPlacementCategory::Storage, ECigPlacementLifetime::Transient);
	const FCigBuildSelection Transient = CigBuildSelection::Resolve(Crate.StableId, &Crate);
	TestFalse(TEXT("Teslimat kasasi secilememeli"), Transient.IsValid());
	TestTrue(TEXT("Teslimat kasasi NotInstalled olmali"),
		Transient.Fault == ECigBuildSelectionFault::NotInstalled);
	// It still reports what it is, so the refusal can explain itself.
	TestEqual(TEXT("Reddedilen yerlesim kimligini bildirmeli"), Transient.StableId, Crate.StableId);

	// Three refusals, three different answers.
	TestTrue(TEXT("Uc ret birbirinden ayirt edilebilmeli"),
		Nothing.Fault != Orphan.Fault && Orphan.Fault != Transient.Fault
			&& Nothing.Fault != Transient.Fault);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildSelectionDescribeTest,
	"Cigkofte.BuildMode.LookingAtNothingSaysNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildSelectionDescribeTest::RunTest(const FString&)
{
	// The HUD draws whatever this returns, so an empty answer is the only thing
	// that keeps a message off the screen while the player looks at the floor.
	const FCigBuildSelection Nothing = CigBuildSelection::Resolve(NAME_None, nullptr);
	TestTrue(TEXT("Duvara bakmak ekrana yazi koymamali"),
		CigBuildSelection::Describe(Nothing).IsEmpty());

	// Whereas both real refusals have something to say.
	const FCigPlacementRecord Crate = MakeSelectionRecord(TEXT("kasa.01"),
		ECigPlacementCategory::Storage, ECigPlacementLifetime::Transient);
	TestFalse(TEXT("Teslimat kasasi kendini aciklamali"),
		CigBuildSelection::Describe(CigBuildSelection::Resolve(Crate.StableId, &Crate)).IsEmpty());
	TestFalse(TEXT("Kayitsiz yerlesim kendini aciklamali"),
		CigBuildSelection::Describe(CigBuildSelection::Resolve(FName(TEXT("hayalet.01")), nullptr)).IsEmpty());

	// A selectable one names itself, so the player can say which table they mean.
	const FCigPlacementRecord Table = MakeSelectionRecord(TEXT("masa.01"),
		ECigPlacementCategory::Seating, ECigPlacementLifetime::Installed);
	const FString Line = CigBuildSelection::Describe(CigBuildSelection::Resolve(Table.StableId, &Table));
	TestTrue(TEXT("Secili yerlesim kimligini gostermeli"), Line.Contains(TEXT("masa.01")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildSelectionScopeTest,
	"Cigkofte.BuildMode.BuildModeIsItsOwnInputLayer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigBuildSelectionScopeTest::RunTest(const FString&)
{
	TestTrue(TEXT("Yalniz insa kipi aciksa insa kipi"),
		CigInput::Scope(false, false, true) == ECigInputScope::BuildMode);

	// The panels win. They are drawn over the screen; build mode is a way of
	// standing in the world, and a mode that kept taking keys behind an open
	// tablet would fire selection on the tablet's own number rows.
	TestTrue(TEXT("Tablet insa kipinin onunde gelmeli"),
		CigInput::Scope(false, true, true) == ECigInputScope::Tablet);
	TestTrue(TEXT("Metin girisi insa kipinin onunde gelmeli"),
		CigInput::Scope(true, false, true) == ECigInputScope::TextEntry);

	// That every combination of the three resolves to exactly one layer is pinned
	// where it has always been pinned, in Cigkofte.Input.Scope.ExactlyOneLayerIsEverActive,
	// which grew from four cases to eight when this flag was added. Repeating it
	// here would give two tests one owner between them.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigBuildSelectionReverseLookupTest,
	"Cigkofte.BuildMode.AnyAttachedActorResolvesToItsPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigBuildSelectionReverseLookupTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }

	UWorld* World = Shop.GM->GetWorld();
	if (!World) { AddError(TEXT("Dunya yok.")); return false; }

	AActor* Table = World->SpawnActor<AActor>();
	AActor* Chair = World->SpawnActor<AActor>();
	AActor* Wall = World->SpawnActor<AActor>();
	if (!Table || !Chair || !Wall) { AddError(TEXT("Aktor olusturulamadi.")); return false; }

	FCigPlacementVisualRegistry Registry;
	const FName Id(TEXT("masa.01"));
	Registry.Attach(Id, Table, FTransform::Identity);
	Registry.Attach(Id, Chair, FTransform::Identity);

	TestEqual(TEXT("Masanin kendisi masayi vermeli"), Registry.FindByActor(Table), Id);
	// The one that matters: a chair is not a placement, it belongs to one. Looking
	// at it has to select the table, because the table is what the authority holds
	// a record for and what a move would actually move.
	TestEqual(TEXT("Sandalyeye bakmak masayi secmeli"), Registry.FindByActor(Chair), Id);

	TestTrue(TEXT("Kayitsiz aktor hicbir sey vermemeli"), Registry.FindByActor(Wall).IsNone());
	TestTrue(TEXT("Null aktor hicbir sey vermemeli"), Registry.FindByActor(nullptr).IsNone());

	// Released actors must stop answering, or a removed placement would keep
	// selecting itself through a stale entry.
	Registry.Release(Id);
	TestTrue(TEXT("Birakilan yerlesim artik eslesmemeli"), Registry.FindByActor(Table).IsNone());
	return true;
}

#endif
