// Deterministic shop-floor placement authority.
//
// These tests use the pure value type unless the name says Integration. No
// renderer, collision scene or actor scan is needed to answer floor geometry.

#include "Misc/AutomationTest.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "Placement/CigPlacementTypes.h"
#include "Placement/CigPlacementSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"
#include "World/CigkofteStation.h"
#include "Inventory/CigInventorySystem.h"
#include "Inventory/CigStockCrate.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigPlacementAuthority Authority(float HalfExtent = 1000.f)
	{
		FCigPlacementAuthority Result;
		FCigPlacementBounds Bounds;
		Bounds.HalfExtent = FVector2D(HalfExtent, HalfExtent);
		Result.Configure(Bounds, 10.f, 90.f, 1.f);
		return Result;
	}

	FCigPlacementRequest Request(const TCHAR* StableId, const FVector& Location,
		const FVector2D& Size = FVector2D(100.f, 100.f), float Yaw = 0.f)
	{
		FCigPlacementRequest Result;
		Result.StableId = FName(StableId);
		Result.Category = ECigPlacementCategory::Decoration;
		Result.Lifetime = ECigPlacementLifetime::Installed;
		Result.CandidateTransform = FTransform(FRotator(0.f, Yaw, 0.f), Location);
		Result.Footprint.Size = Size;
		Result.Context = ECigPlacementContext::BuildMode;
		return Result;
	}

	void DefineFunctionalConsequence(FCigPlacementRequest& Request,
		ECigPlacementCategory Category, int32 FunctionalCapacity = 1,
		const FVector2D& UseSize = FVector2D(120.f, 80.f),
		const FVector2D& UseOffset = FVector2D(100.f, 0.f), float UseYaw = 0.f)
	{
		Request.Category = Category;
		if (Category == ECigPlacementCategory::Decoration)
		{
			return;
		}
		Request.UseSpec.Size = UseSize;
		Request.UseSpec.CenterOffset = UseOffset;
		Request.UseSpec.YawOffsetDegrees = UseYaw;
		Request.UseSpec.FunctionalCapacity = FunctionalCapacity;
	}

	FCigProtectedZone Zone(const TCHAR* StableId, ECigPlacementFailure Failure)
	{
		FCigProtectedZone Result;
		Result.StableId = FName(StableId);
		Result.HalfExtent = FVector2D(100.f, 100.f);
		Result.Failure = Failure;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementKnownCategoriesTest,
	"Cigkofte.Placement.Classification.KnownCategoriesAccepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementKnownCategoriesTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	const ECigPlacementCategory Categories[] = {
		ECigPlacementCategory::Station,
		ECigPlacementCategory::Seating,
		ECigPlacementCategory::Storage,
		ECigPlacementCategory::Decoration
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Categories); ++Index)
	{
		FCigPlacementRequest R = Request(*FString::Printf(TEXT("category.%d"), Index), FVector::ZeroVector);
		DefineFunctionalConsequence(R, Categories[Index],
			Categories[Index] == ECigPlacementCategory::Seating ? 2 : 1);
		TestTrue(*FString::Printf(TEXT("Kategori %d kurulabilir olmalı"), Index), A.Validate(R).bAccepted);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementUnknownCategoryTest,
	"Cigkofte.Placement.Classification.UnknownCategoryRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementUnknownCategoryTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest R = Request(TEXT("category.unknown"), FVector::ZeroVector);
	R.Category = ECigPlacementCategory::Unknown;
	TestEqual(TEXT("Unknown kategori reddedilmeli"), A.Validate(R).Failure,
		ECigPlacementFailure::UnknownCategory);
	R.Category = static_cast<ECigPlacementCategory>(255);
	TestEqual(TEXT("Tanımsız kategori ordinali reddedilmeli"), A.Validate(R).Failure,
		ECigPlacementFailure::UnknownCategory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementUnknownLifetimeTest,
	"Cigkofte.Placement.Classification.UnknownLifetimeRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementUnknownLifetimeTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest R = Request(TEXT("lifetime.unknown"), FVector::ZeroVector);
	R.Lifetime = ECigPlacementLifetime::Unknown;
	TestEqual(TEXT("Unknown lifetime reddedilmeli"), A.Validate(R).Failure,
		ECigPlacementFailure::UnknownLifetime);
	R.Lifetime = static_cast<ECigPlacementLifetime>(255);
	TestEqual(TEXT("Tanımsız lifetime ordinali reddedilmeli"), A.Validate(R).Failure,
		ECigPlacementFailure::UnknownLifetime);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementUnknownContextTest,
	"Cigkofte.Placement.Classification.UnknownContextRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementUnknownContextTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest R = Request(TEXT("context.unknown"), FVector::ZeroVector);
	R.Context = ECigPlacementContext::Unknown;
	TestEqual(TEXT("Unknown bağlam reddedilmeli"), A.Validate(R).Failure,
		ECigPlacementFailure::UnknownContext);
	R.Context = static_cast<ECigPlacementContext>(255);
	TestEqual(TEXT("Tanımsız bağlam ordinali reddedilmeli"), A.Validate(R).Failure,
		ECigPlacementFailure::UnknownContext);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementClassificationMatrixTest,
	"Cigkofte.Placement.Classification.ContextMatrixIsEnforced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementClassificationMatrixTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();

	FCigPlacementRequest InstalledDelivery = Request(TEXT("installed.delivery"), FVector::ZeroVector);
	InstalledDelivery.Category = ECigPlacementCategory::Storage;
	InstalledDelivery.Context = ECigPlacementContext::Delivery;
	TestEqual(TEXT("Installed kayıt delivery bağlamına girememeli"), A.Validate(InstalledDelivery).Failure,
		ECigPlacementFailure::InvalidClassification);

	FCigPlacementRequest TransientBuild = Request(TEXT("transient.build"), FVector::ZeroVector);
	TransientBuild.Category = ECigPlacementCategory::Storage;
	TransientBuild.Lifetime = ECigPlacementLifetime::Transient;
	TestEqual(TEXT("Transient depolama build mode ile kurulamaz"), A.Validate(TransientBuild).Failure,
		ECigPlacementFailure::InvalidClassification);

	FCigPlacementRequest TransientDecoration = Request(TEXT("transient.decoration"), FVector::ZeroVector);
	TransientDecoration.Lifetime = ECigPlacementLifetime::Transient;
	TransientDecoration.Context = ECigPlacementContext::Delivery;
	TestEqual(TEXT("Transient dekorasyon delivery ile kurulamaz"), A.Validate(TransientDecoration).Failure,
		ECigPlacementFailure::InvalidClassification);

	FCigPlacementRequest Crate = Request(TEXT("transient.storage"), FVector::ZeroVector);
	DefineFunctionalConsequence(Crate, ECigPlacementCategory::Storage);
	Crate.Lifetime = ECigPlacementLifetime::Transient;
	Crate.Context = ECigPlacementContext::Delivery;
	TestTrue(TEXT("Transient storage delivery ile kabul edilmeli"), A.Validate(Crate).bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementMoveClassificationTest,
	"Cigkofte.Placement.Classification.MovePreservesClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementMoveClassificationTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	const FCigPlacementRequest Original = Request(TEXT("object.immutable"), FVector::ZeroVector);
	TestTrue(TEXT("Başlangıç kaydı kabul edilmeli"), A.TryRegister(Original).bAccepted);

	FCigPlacementRequest CategoryChange = Request(TEXT("object.immutable"), FVector(200.f, 0.f, 0.f));
	CategoryChange.Category = ECigPlacementCategory::Seating;
	CategoryChange.Context = ECigPlacementContext::MoveExisting;
	CategoryChange.IgnoreStableId = CategoryChange.StableId;
	TestEqual(TEXT("Taşırken kategori değişmemeli"), A.TryRegister(CategoryChange).Failure,
		ECigPlacementFailure::CategoryMismatch);

	FCigPlacementRequest LifetimeChange = Request(TEXT("object.immutable"), FVector(200.f, 0.f, 0.f));
	LifetimeChange.Lifetime = ECigPlacementLifetime::Transient;
	LifetimeChange.Context = ECigPlacementContext::MoveExisting;
	LifetimeChange.IgnoreStableId = LifetimeChange.StableId;
	TestEqual(TEXT("Taşırken lifetime değişmemeli"), A.TryRegister(LifetimeChange).Failure,
		ECigPlacementFailure::LifetimeMismatch);

	const FCigPlacementRecord* Kept = A.Find(Original.StableId);
	TestNotNull(TEXT("Reddedilen değişiklik kaydı silmemeli"), Kept);
	if (Kept)
	{
		TestEqual(TEXT("Kategori aynı kalmalı"), Kept->Category, ECigPlacementCategory::Decoration);
		TestEqual(TEXT("Lifetime aynı kalmalı"), Kept->Lifetime, ECigPlacementLifetime::Installed);
		TestTrue(TEXT("Konum aynı kalmalı"), Kept->Transform.GetLocation().IsNearlyZero());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementIgnoreContextTest,
	"Cigkofte.Placement.Classification.IgnoreIdOnlyAllowedForMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementIgnoreContextTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest Build = Request(TEXT("object.ignore"), FVector::ZeroVector);
	Build.IgnoreStableId = Build.StableId;
	TestEqual(TEXT("Build mode kendi kimliğini yok sayamamalı"), A.Validate(Build).Failure,
		ECigPlacementFailure::InvalidIgnoreStableId);

	FCigPlacementRequest Move = Request(TEXT("object.ignore"), FVector::ZeroVector);
	Move.Context = ECigPlacementContext::MoveExisting;
	TestEqual(TEXT("Move kendi kimliğini açıkça belirtmeli"), A.Validate(Move).Failure,
		ECigPlacementFailure::InvalidIgnoreStableId);

	Build.IgnoreStableId = NAME_None;
	const FCigPlacementRequest Other = Request(TEXT("object.other"), FVector(300.f, 0.f, 0.f));
	TestTrue(TEXT("Ignore sahipliği testi ilk kaydı eklemeli"), A.TryRegister(Build).bAccepted);
	TestTrue(TEXT("Ignore sahipliği testi ikinci kaydı eklemeli"), A.TryRegister(Other).bAccepted);
	Move.CandidateTransform.SetLocation(FVector(100.f, 0.f, 0.f));
	Move.IgnoreStableId = Other.StableId;
	TestEqual(TEXT("Move başka stable ID'yi yok sayamamalı"), A.TryRegister(Move).Failure,
		ECigPlacementFailure::InvalidIgnoreStableId);
	TestEqual(TEXT("Geçersiz ignore iki record'u da korumalı"), A.RecordCount(), 2);
	TestEqual(TEXT("Geçersiz ignore iki consequence'ı da korumalı"), A.ConsequenceCount(), 2);
	const FCigPlacementRecord* KeptSelf = A.Find(Build.StableId);
	const FCigPlacementRecord* KeptOther = A.Find(Other.StableId);
	TestTrue(TEXT("Geçersiz ignore ilk transformu değiştirmemeli"),
		KeptSelf && KeptSelf->Transform.GetLocation().IsNearlyZero());
	TestTrue(TEXT("Geçersiz ignore başka kaydı değiştirmemeli"),
		KeptOther && KeptOther->Transform.GetLocation().Equals(FVector(300.f, 0.f, 0.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementClassificationCountTest,
	"Cigkofte.Placement.Classification.CategoryAndLifetimeCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementClassificationCountTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	const ECigPlacementCategory Categories[] = {
		ECigPlacementCategory::Station,
		ECigPlacementCategory::Seating,
		ECigPlacementCategory::Storage,
		ECigPlacementCategory::Decoration
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Categories); ++Index)
	{
		FCigPlacementRequest R = Request(*FString::Printf(TEXT("installed.%d"), Index),
			FVector(-300.f + Index * 200.f, 0.f, 0.f));
		DefineFunctionalConsequence(R, Categories[Index],
			Categories[Index] == ECigPlacementCategory::Seating ? 2 : 1,
			FVector2D(80.f, 80.f), FVector2D::ZeroVector);
		TestTrue(TEXT("Installed sınıf kaydolmalı"), A.TryRegister(R).bAccepted);
	}
	FCigPlacementRequest Crate = Request(TEXT("transient.crate"), FVector(500.f, 0.f, 0.f));
	DefineFunctionalConsequence(Crate, ECigPlacementCategory::Storage);
	Crate.Lifetime = ECigPlacementLifetime::Transient;
	Crate.Context = ECigPlacementContext::Delivery;
	TestTrue(TEXT("Transient kasa kaydolmalı"), A.TryRegister(Crate).bAccepted);
	TestEqual(TEXT("Station sayısı"), A.CountByCategory(ECigPlacementCategory::Station), 1);
	TestEqual(TEXT("Seating sayısı"), A.CountByCategory(ECigPlacementCategory::Seating), 1);
	TestEqual(TEXT("Storage sayısı"), A.CountByCategory(ECigPlacementCategory::Storage), 2);
	TestEqual(TEXT("Decoration sayısı"), A.CountByCategory(ECigPlacementCategory::Decoration), 1);
	TestEqual(TEXT("Installed sayısı"), A.CountByLifetime(ECigPlacementLifetime::Installed), 4);
	TestEqual(TEXT("Transient sayısı"), A.CountByLifetime(ECigPlacementLifetime::Transient), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementValidFreeTest,
	"Cigkofte.Placement.ValidFreePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementValidFreeTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest R = Request(TEXT("object.free"), FVector(103.f, 207.f, 0.f));
	const FCigPlacementResult Result = A.TryRegister(R);
	TestTrue(TEXT("Boş yer kabul edilmeli"), Result.bAccepted);
	TestEqual(TEXT("X tek ayardan snap edilmeli"), Result.NormalizedTransform.GetLocation().X, 100.0);
	TestEqual(TEXT("Y tek ayardan snap edilmeli"), Result.NormalizedTransform.GetLocation().Y, 210.0);
	TestEqual(TEXT("Bir kez kaydedilmeli"), A.RecordCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementOutsideBoundsTest,
	"Cigkofte.Placement.OutsideBoundsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementOutsideBoundsTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority(200.f);
	const FCigPlacementResult Result = A.Validate(Request(TEXT("object.outside"), FVector(160.f, 0.f, 0.f)));
	TestFalse(TEXT("Ayak izi sınırı aşınca reddedilmeli"), Result.bAccepted);
	TestEqual(TEXT("Neden sınır olmalı"), Result.Failure, ECigPlacementFailure::OutsideShopBounds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementOverlapTest,
	"Cigkofte.Placement.OverlapRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementOverlapTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.TryRegister(Request(TEXT("fixture.alpha"), FVector::ZeroVector));
	const FCigPlacementResult Result = A.Validate(Request(TEXT("object.overlap"), FVector(40.f, 0.f, 0.f)));
	TestEqual(TEXT("Çakışma nedeni dönmeli"), Result.Failure, ECigPlacementFailure::Overlap);
	TestEqual(TEXT("Çakışan stabil kimlik dönmeli"), Result.ConflictingStableId, FName(TEXT("fixture.alpha")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementEdgeTouchTest,
	"Cigkofte.Placement.EdgeTouchIsAllowed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementEdgeTouchTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.TryRegister(Request(TEXT("object.left"), FVector::ZeroVector));
	const FCigPlacementResult Result = A.Validate(Request(TEXT("object.right"), FVector(100.f, 0.f, 0.f)));
	TestTrue(TEXT("Sıfır alanlı kenar teması kabul edilmeli"), Result.bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementQuarterTurnTest,
	"Cigkofte.Placement.QuarterTurnSwapsFootprintAxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementQuarterTurnTest::RunTest(const FString&)
{
	FCigPlacementRequest R = Request(TEXT("object.turn"), FVector::ZeroVector, FVector2D(120.f, 40.f), 90.f);
	R.Footprint.CenterOffset = FVector2D(20.f, 0.f);
	R.Footprint.ClearanceMargin = 5.f;
	const FCigPlacementRect Rect = FCigPlacementAuthority::EffectiveRect(R.CandidateTransform, R.Footprint);
	TestTrue(TEXT("90 derecede X yarıçapı Y boyundan gelmeli"), FMath::IsNearlyEqual(Rect.HalfExtent.X, 25.f, 0.01f));
	TestTrue(TEXT("90 derecede Y yarıçapı X boyundan gelmeli"), FMath::IsNearlyEqual(Rect.HalfExtent.Y, 65.f, 0.01f));
	TestTrue(TEXT("Merkez ofseti dönüşle Y eksenine geçmeli"), Rect.Center.Equals(FVector2D(0.f, 20.f), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementDuplicateIdTest,
	"Cigkofte.Placement.DuplicateStableIdRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementDuplicateIdTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.TryRegister(Request(TEXT("object.same"), FVector(-200.f, 0.f, 0.f)));
	const FCigPlacementResult Result = A.TryRegister(Request(TEXT("object.same"), FVector(200.f, 0.f, 0.f)));
	TestEqual(TEXT("Aynı kimlik ikinci kez reddedilmeli"), Result.Failure, ECigPlacementFailure::DuplicateStableId);
	TestEqual(TEXT("Kayıt sayısı artmamalı"), A.RecordCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementSelfIgnoreMoveTest,
	"Cigkofte.Placement.MoveIgnoresOnlyItsOwnRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementSelfIgnoreMoveTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.TryRegister(Request(TEXT("object.move"), FVector::ZeroVector));
	FCigPlacementRequest Move = Request(TEXT("object.move"), FVector(200.f, 0.f, 0.f));
	Move.Context = ECigPlacementContext::MoveExisting;
	Move.IgnoreStableId = Move.StableId;
	const FCigPlacementResult Result = A.TryRegister(Move);
	TestTrue(TEXT("Kendi eski izi yok sayılmalı"), Result.bAccepted);
	TestEqual(TEXT("Güncelleme yeni kayıt eklememeli"), A.RecordCount(), 1);
	TestEqual(TEXT("Kayıt yeni konuma taşınmalı"), A.Find(Move.StableId)->Transform.GetLocation().X, 200.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementEntranceTest,
	"Cigkofte.Placement.EntranceProtected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementEntranceTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.AddProtectedZone(Zone(TEXT("zone.entrance"), ECigPlacementFailure::BlocksEntrance));
	const FCigPlacementResult Result = A.Validate(Request(TEXT("object.entrance"), FVector::ZeroVector));
	TestEqual(TEXT("Giriş nedeni dönmeli"), Result.Failure, ECigPlacementFailure::BlocksEntrance);
	TestEqual(TEXT("Giriş zonu kimliği dönmeli"), Result.ProtectedZoneId, FName(TEXT("zone.entrance")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementQueueTest,
	"Cigkofte.Placement.QueueProtected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementQueueTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.AddProtectedZone(Zone(TEXT("zone.queue"), ECigPlacementFailure::BlocksQueue));
	TestEqual(TEXT("Kuyruk korunmalı"), A.Validate(Request(TEXT("object.queue"), FVector::ZeroVector)).Failure,
		ECigPlacementFailure::BlocksQueue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementServiceRouteTest,
	"Cigkofte.Placement.ServiceRouteProtected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementServiceRouteTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.AddProtectedZone(Zone(TEXT("zone.service"), ECigPlacementFailure::BlocksServiceRoute));
	TestEqual(TEXT("Servis rotası korunmalı"), A.Validate(Request(TEXT("object.service"), FVector::ZeroVector)).Failure,
		ECigPlacementFailure::BlocksServiceRoute);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementStationAccessTest,
	"Cigkofte.Placement.StationAccessProtected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementStationAccessTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.AddProtectedZone(Zone(TEXT("zone.station.yogurma.access"), ECigPlacementFailure::BlocksStationAccess));
	TestEqual(TEXT("İstasyon erişimi korunmalı"), A.Validate(Request(TEXT("object.station"), FVector::ZeroVector)).Failure,
		ECigPlacementFailure::BlocksStationAccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementCrateAlternativeTest,
	"Cigkofte.Placement.FirstFreeCrateAlternativeIsDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementCrateAlternativeTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority(2000.f);
	const TArray<FTransform>& Spots = CigPlacementLayout::DeliverySpots();
	FCigPlacementRequest Blocker = Request(TEXT("crate.blocker"), Spots[0].GetLocation(), FVector2D(60.f, 45.f));
	Blocker.Context = ECigPlacementContext::WorldRegistration;
	A.TryRegister(Blocker);
	FCigPlacementRequest Crate = Request(TEXT("crate.delivery.1"), FVector::ZeroVector, FVector2D(60.f, 45.f));
	DefineFunctionalConsequence(Crate, ECigPlacementCategory::Storage);
	Crate.Lifetime = ECigPlacementLifetime::Transient;
	Crate.Context = ECigPlacementContext::Delivery;
	const FCigPlacementResult Result = A.FindFirstValid(Crate, Spots);
	TestTrue(TEXT("Bir alternatif bulunmalı"), Result.bAccepted);
	TestTrue(TEXT("İlk doluysa ikinci seçilmeli"), Result.NormalizedTransform.GetLocation().Equals(Spots[1].GetLocation()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementAllCrateSpotsOccupiedTest,
	"Cigkofte.Placement.AllCrateSpotsOccupiedFailsClearly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementAllCrateSpotsOccupiedTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority(2000.f);
	const TArray<FTransform>& Spots = CigPlacementLayout::DeliverySpots();
	for (int32 i = 0; i < Spots.Num(); ++i)
	{
		FCigPlacementRequest Blocker = Request(*FString::Printf(TEXT("crate.blocker.%d"), i),
			Spots[i].GetLocation(), FVector2D(60.f, 45.f));
		Blocker.Context = ECigPlacementContext::WorldRegistration;
		A.TryRegister(Blocker);
	}
	FCigPlacementRequest Crate = Request(TEXT("crate.delivery.full"), FVector::ZeroVector, FVector2D(60.f, 45.f));
	DefineFunctionalConsequence(Crate, ECigPlacementCategory::Storage);
	Crate.Lifetime = ECigPlacementLifetime::Transient;
	Crate.Context = ECigPlacementContext::Delivery;
	const FCigPlacementResult Result = A.FindFirstValid(Crate, Spots);
	TestFalse(TEXT("Dolu sıra kabul edilmemeli"), Result.bAccepted);
	TestEqual(TEXT("Açık teslimat hatası dönmeli"), Result.Failure, ECigPlacementFailure::NoDeliverySpotAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementRegistrationOrderTest,
	"Cigkofte.Placement.RegistrationOrderDoesNotChooseConflict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementRegistrationOrderTest::RunTest(const FString&)
{
	auto Populate = [](FCigPlacementAuthority& A, bool bReverse)
	{
		const FCigPlacementRequest Alpha = Request(TEXT("fixture.alpha"), FVector(-80.f, 0.f, 0.f), FVector2D(80.f, 80.f));
		const FCigPlacementRequest Zeta = Request(TEXT("fixture.zeta"), FVector(80.f, 0.f, 0.f), FVector2D(80.f, 80.f));
		A.TryRegister(bReverse ? Zeta : Alpha);
		A.TryRegister(bReverse ? Alpha : Zeta);
	};
	FCigPlacementAuthority A = Authority();
	FCigPlacementAuthority B = Authority();
	Populate(A, false);
	Populate(B, true);
	const FCigPlacementRequest Wide = Request(TEXT("object.wide"), FVector::ZeroVector, FVector2D(240.f, 100.f));
	const FCigPlacementResult RA = A.Validate(Wide);
	const FCigPlacementResult RB = B.Validate(Wide);
	TestEqual(TEXT("İki sıra da aynı kimliği seçmeli"), RA.ConflictingStableId, RB.ConflictingStableId);
	TestEqual(TEXT("Leksik ilk çakışma seçilmeli"), RA.ConflictingStableId, FName(TEXT("fixture.alpha")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementFailurePrecedenceTest,
	"Cigkofte.Placement.FailurePrecedenceIsDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementFailurePrecedenceTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.AddProtectedZone(Zone(TEXT("zone.queue"), ECigPlacementFailure::BlocksQueue));
	A.AddProtectedZone(Zone(TEXT("zone.entrance"), ECigPlacementFailure::BlocksEntrance));
	FCigPlacementRequest Fixture = Request(TEXT("fixture.overlap"), FVector::ZeroVector);
	Fixture.Context = ECigPlacementContext::WorldRegistration;
	A.TryRegister(Fixture);
	const FCigPlacementResult Result = A.Validate(Request(TEXT("object.many-failures"), FVector::ZeroVector));
	TestEqual(TEXT("Korunan neden sırası kayıt sırasından bağımsız olmalı"), Result.Failure,
		ECigPlacementFailure::BlocksEntrance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementRejectedDoesNotMutateTest,
	"Cigkofte.Placement.RejectionDoesNotMutateState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementRejectedDoesNotMutateTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority(200.f);
	A.TryRegister(Request(TEXT("object.kept"), FVector::ZeroVector));
	const int32 Before = A.RecordCount();
	A.TryRegister(Request(TEXT("object.rejected"), FVector(500.f, 0.f, 0.f)));
	TestEqual(TEXT("Ret kaydı değiştirmemeli"), A.RecordCount(), Before);
	TestTrue(TEXT("Önceki kayıt kalmalı"), A.Find(TEXT("object.kept")) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementAcceptedExactlyOnceTest,
	"Cigkofte.Placement.AcceptedRegistrationOccursExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementAcceptedExactlyOnceTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest R = Request(TEXT("object.once"), FVector::ZeroVector);
	TestTrue(TEXT("İlk kayıt kabul edilmeli"), A.TryRegister(R).bAccepted);
	TestFalse(TEXT("İkinci kayıt reddedilmeli"), A.TryRegister(R).bAccepted);
	TestEqual(TEXT("Tek kayıt kalmalı"), A.RecordCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementRemoveFreesFootprintTest,
	"Cigkofte.Placement.RemovingRecordFreesFootprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementRemoveFreesFootprintTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	A.TryRegister(Request(TEXT("object.removed"), FVector::ZeroVector));
	TestTrue(TEXT("Kayıt silinmeli"), A.Remove(TEXT("object.removed")));
	TestTrue(TEXT("Aynı yer yeniden kullanılabilmeli"),
		A.Validate(Request(TEXT("object.replacement"), FVector::ZeroVector)).bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementInvalidRotationTest,
	"Cigkofte.Placement.UnsupportedRotationRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementInvalidRotationTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	const FCigPlacementResult Result = A.Validate(Request(TEXT("object.angled"), FVector::ZeroVector,
		FVector2D(100.f, 50.f), 45.f));
	TestEqual(TEXT("45 derece desteklenmemeli"), Result.Failure, ECigPlacementFailure::UnsupportedRotation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementFallbackFootprintTest,
	"Cigkofte.Placement.MissingOptionalMeshUsesFallbackFootprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementFallbackFootprintTest::RunTest(const FString&)
{
	const FCigPlacementFootprint Footprint = FCigPlacementFootprint::FromOptionalMeshSize(
		TOptional<FVector2D>(), FVector2D(60.f, 45.f), 10.f);
	TestTrue(TEXT("Fallback boyutu kullanılmalı"), Footprint.Size.Equals(FVector2D(60.f, 45.f)));
	TestEqual(TEXT("Clearance korunmalı"), Footprint.ClearanceMargin, 10.f);
	TestTrue(TEXT("Fallback geçerli olmalı"), Footprint.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementInvalidFloorTest,
	"Cigkofte.Placement.InvalidFloorRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementInvalidFloorTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	const FCigPlacementResult Result = A.Validate(Request(TEXT("object.airborne"), FVector(0.f, 0.f, 20.f)));
	TestEqual(TEXT("Dükkân zemini dışındaki Z reddedilmeli"), Result.Failure, ECigPlacementFailure::InvalidFloor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementConsequencePolicyTest,
	"Cigkofte.Placement.Consequences.Policy.CategorySemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementConsequencePolicyTest::RunTest(const FString&)
{
	FCigPlacementConsequence Consequence;
	FCigPlacementRequest Station = Request(TEXT("policy.station"), FVector::ZeroVector);
	DefineFunctionalConsequence(Station, ECigPlacementCategory::Station);
	TestEqual(TEXT("Station policy geçerli olmalı"),
		FCigPlacementConsequencePolicy::Derive(Station, Station.CandidateTransform, Consequence),
		ECigPlacementFailure::None);
	TestEqual(TEXT("Station bir çalışma birimi üretmeli"), Consequence.FunctionalCapacity, 1);
	TestTrue(TEXT("Station kullanım alanı üretmeli"), Consequence.bHasUseArea);
	TestTrue(TEXT("Installed station kalıcı layout parçası olmalı"), Consequence.bInstalledLayout);

	FCigPlacementRequest Seating = Request(TEXT("policy.seating"), FVector::ZeroVector);
	DefineFunctionalConsequence(Seating, ECigPlacementCategory::Seating, 2);
	TestEqual(TEXT("Seating policy geçerli olmalı"),
		FCigPlacementConsequencePolicy::Derive(Seating, Seating.CandidateTransform, Consequence),
		ECigPlacementFailure::None);
	TestEqual(TEXT("Masa grubu iki koltuk üretmeli"), Consequence.FunctionalCapacity, 2);

	FCigPlacementRequest Storage = Request(TEXT("policy.storage"), FVector::ZeroVector);
	DefineFunctionalConsequence(Storage, ECigPlacementCategory::Storage);
	Storage.Lifetime = ECigPlacementLifetime::Transient;
	TestEqual(TEXT("Transient storage policy geçerli olmalı"),
		FCigPlacementConsequencePolicy::Derive(Storage, Storage.CandidateTransform, Consequence),
		ECigPlacementFailure::None);
	TestEqual(TEXT("Storage bir unload birimi üretmeli"), Consequence.FunctionalCapacity, 1);
	TestFalse(TEXT("Transient storage kalıcı layout iddia etmemeli"), Consequence.bInstalledLayout);

	const FCigPlacementRequest Decoration = Request(TEXT("policy.decoration"), FVector::ZeroVector);
	TestEqual(TEXT("Decoration policy geçerli olmalı"),
		FCigPlacementConsequencePolicy::Derive(Decoration, Decoration.CandidateTransform, Consequence),
		ECigPlacementFailure::None);
	TestEqual(TEXT("Decoration kapasite üretmemeli"), Consequence.FunctionalCapacity, 0);
	TestFalse(TEXT("Decoration kullanım alanı üretmemeli"), Consequence.bHasUseArea);

	FCigPlacementRequest Unknown = Decoration;
	Unknown.Category = static_cast<ECigPlacementCategory>(255);
	TestEqual(TEXT("Tanımsız kategori policy tarafından reddedilmeli"),
		FCigPlacementConsequencePolicy::Derive(Unknown, Unknown.CandidateTransform, Consequence),
		ECigPlacementFailure::UnknownCategory);
	Unknown.Category = ECigPlacementCategory::Decoration;
	Unknown.Lifetime = static_cast<ECigPlacementLifetime>(255);
	TestEqual(TEXT("Tanımsız lifetime policy tarafından reddedilmeli"),
		FCigPlacementConsequencePolicy::Derive(Unknown, Unknown.CandidateTransform, Consequence),
		ECigPlacementFailure::UnknownLifetime);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementConsequenceGeometryTest,
	"Cigkofte.Placement.Consequences.Policy.RotationAndNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementConsequenceGeometryTest::RunTest(const FString&)
{
	FCigPlacementConsequence AtZero;
	FCigPlacementRequest R = Request(TEXT("geometry.zero"), FVector::ZeroVector, FVector2D(200.f, 80.f));
	DefineFunctionalConsequence(R, ECigPlacementCategory::Station, 1,
		FVector2D(120.f, 40.f), FVector2D(100.f, 0.f));
	TestEqual(TEXT("0 derece consequence türemeli"),
		FCigPlacementConsequencePolicy::Derive(R, R.CandidateTransform, AtZero), ECigPlacementFailure::None);
	TestTrue(TEXT("0 derece fiziksel non-square half extent"),
		AtZero.PhysicalRect.HalfExtent.Equals(FVector2D(100.f, 40.f), 0.01f));
	TestTrue(TEXT("0 derece kullanım merkezi"), AtZero.UseRect.Center.Equals(FVector2D(100.f, 0.f), 0.01f));
	TestTrue(TEXT("0 derece kullanım half extent"),
		AtZero.UseRect.HalfExtent.Equals(FVector2D(60.f, 20.f), 0.01f));

	FCigPlacementConsequence AtNinety;
	R.StableId = TEXT("geometry.ninety");
	R.CandidateTransform = FTransform(FRotator(0.f, 90.f, 0.f), FVector::ZeroVector,
		FVector(2.f, 3.f, 4.f));
	TestEqual(TEXT("90 derece consequence türemeli"),
		FCigPlacementConsequencePolicy::Derive(R, R.CandidateTransform, AtNinety), ECigPlacementFailure::None);
	TestTrue(TEXT("90 derece fiziksel non-square half extent"),
		AtNinety.PhysicalRect.HalfExtent.Equals(FVector2D(40.f, 100.f), 0.01f));
	TestTrue(TEXT("90 derece kullanım merkezi"), AtNinety.UseRect.Center.Equals(FVector2D(0.f, 100.f), 0.01f));
	TestTrue(TEXT("90 derece kullanım half extent"),
		AtNinety.UseRect.HalfExtent.Equals(FVector2D(20.f, 60.f), 0.01f));

	FCigPlacementAuthority A = Authority();
	const FCigPlacementResult Registered = A.TryRegister(R);
	TestTrue(TEXT("Ölçekli request normalize edilip kaydolmalı"), Registered.bAccepted);
	TestTrue(TEXT("Placement ölçeği authoritative geometri için bire normalize edilmeli"),
		Registered.NormalizedTransform.GetScale3D().Equals(FVector::OneVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementConsequenceDeterminismTest,
	"Cigkofte.Placement.Consequences.Policy.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementConsequenceDeterminismTest::RunTest(const FString&)
{
	FCigPlacementRequest R = Request(TEXT("policy.repeat"), FVector(120.f, -70.f, 0.f), FVector2D(90.f, 210.f), 90.f);
	DefineFunctionalConsequence(R, ECigPlacementCategory::Seating, 2,
		FVector2D(200.f, 300.f), FVector2D(-40.f, 15.f), 90.f);
	FCigPlacementConsequence First;
	FCigPlacementConsequence Second;
	TestEqual(TEXT("İlk türetim geçerli olmalı"),
		FCigPlacementConsequencePolicy::Derive(R, R.CandidateTransform, First), ECigPlacementFailure::None);
	TestEqual(TEXT("İkinci türetim geçerli olmalı"),
		FCigPlacementConsequencePolicy::Derive(R, R.CandidateTransform, Second), ECigPlacementFailure::None);
	TestTrue(TEXT("Fiziksel rect deterministik olmalı"),
		First.PhysicalRect.Center.Equals(Second.PhysicalRect.Center, 0.001f)
		&& First.PhysicalRect.HalfExtent.Equals(Second.PhysicalRect.HalfExtent, 0.001f));
	TestTrue(TEXT("Kullanım rect deterministik olmalı"),
		First.UseRect.Center.Equals(Second.UseRect.Center, 0.001f)
		&& First.UseRect.HalfExtent.Equals(Second.UseRect.HalfExtent, 0.001f));
	TestTrue(TEXT("Placement ve use yaw birleşimi local frame merkezini kesin döndürmeli"),
		First.UseRect.Center.Equals(FVector2D(160.f, -85.f), 0.001f));
	TestTrue(TEXT("Placement ve use yaw birleşimi non-square eksenleri kesin döndürmeli"),
		First.UseRect.HalfExtent.Equals(FVector2D(100.f, 150.f), 0.001f));
	TestEqual(TEXT("Kapasite deterministik olmalı"), First.FunctionalCapacity, Second.FunctionalCapacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementInvalidConsequenceTest,
	"Cigkofte.Placement.Consequences.Validation.InvalidDefinitionAndPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementInvalidConsequenceTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest Missing = Request(TEXT("invalid.station"), FVector::ZeroVector);
	Missing.Category = ECigPlacementCategory::Station;
	TestEqual(TEXT("Station kullanım alanı olmadan reddedilmeli"), A.Validate(Missing).Failure,
		ECigPlacementFailure::InvalidConsequence);

	FCigPlacementRequest Decor = Request(TEXT("invalid.decoration"), FVector::ZeroVector);
	Decor.UseSpec.Size = FVector2D(100.f, 100.f);
	Decor.UseSpec.FunctionalCapacity = 1;
	TestEqual(TEXT("Decoration fonksiyonel kapasite iddia edememeli"), A.Validate(Decor).Failure,
		ECigPlacementFailure::InvalidConsequence);

	FCigPlacementRequest Seating = Request(TEXT("invalid.seating"), FVector::ZeroVector);
	DefineFunctionalConsequence(Seating, ECigPlacementCategory::Seating, 0);
	TestEqual(TEXT("Seating sıfır kapasiteyle reddedilmeli"), A.Validate(Seating).Failure,
		ECigPlacementFailure::InvalidConsequence);

	Missing.Category = static_cast<ECigPlacementCategory>(255);
	TestEqual(TEXT("Unknown category consequence hatasından önce gelmeli"), A.Validate(Missing).Failure,
		ECigPlacementFailure::UnknownCategory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementConsequenceRouteTest,
	"Cigkofte.Placement.Consequences.Validation.BoundsRoutesAndEdgeTouch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementConsequenceRouteTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority(300.f);
	FCigPlacementRequest Outside = Request(TEXT("use.outside"), FVector(150.f, 0.f, 0.f), FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Outside, ECigPlacementCategory::Station, 1,
		FVector2D(100.f, 80.f), FVector2D(150.f, 0.f));
	TestEqual(TEXT("Kullanım alanı shop bounds dışına taşamamalı"), A.Validate(Outside).Failure,
		ECigPlacementFailure::FunctionalAreaOutsideShop);

	FCigProtectedZone Queue = Zone(TEXT("zone.use.queue"), ECigPlacementFailure::BlocksQueue);
	Queue.Center = FVector2D(200.f, 0.f);
	Queue.HalfExtent = FVector2D(50.f, 50.f);
	TestTrue(TEXT("Queue zone eklenmeli"), A.AddProtectedZone(Queue));

	FCigPlacementRequest Blocks = Request(TEXT("use.blocks"), FVector::ZeroVector, FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Blocks, ECigPlacementCategory::Station, 1,
		FVector2D(100.f, 80.f), FVector2D(120.f, 0.f));
	TestEqual(TEXT("Sadece consequence ile queue kesmek de reddedilmeli"), A.Validate(Blocks).Failure,
		ECigPlacementFailure::BlocksQueue);
	FCigPlacementRequest AuthoredBootstrap = Blocks;
	AuthoredBootstrap.StableId = TEXT("use.authored-bootstrap");
	AuthoredBootstrap.Context = ECigPlacementContext::WorldRegistration;
	TestTrue(TEXT("Authored world bootstrap protected route'u yeniden doğrulamamalı"),
		A.Validate(AuthoredBootstrap).bAccepted);

	Blocks.StableId = TEXT("use.edge");
	Blocks.UseSpec.CenterOffset = FVector2D(100.f, 0.f);
	TestTrue(TEXT("Kullanım alanının protected zone kenarına değmesi kabul edilmeli"),
		A.Validate(Blocks).bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementFunctionalClearanceTest,
	"Cigkofte.Placement.Consequences.Validation.FunctionalClearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementFunctionalClearanceTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest Station = Request(TEXT("station.clearance"), FVector::ZeroVector, FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Station, ECigPlacementCategory::Station, 1,
		FVector2D(100.f, 80.f), FVector2D(100.f, 0.f));
	TestTrue(TEXT("Station kaydolmalı"), A.TryRegister(Station).bAccepted);
	TestEqual(TEXT("Fiziksel nesne station çalışma alanını kapatamamalı"),
		A.Validate(Request(TEXT("decor.blocks.station"), FVector(100.f, 0.f, 0.f), FVector2D(40.f, 40.f))).Failure,
		ECigPlacementFailure::BlocksStationAccess);
	TestTrue(TEXT("Station çalışma alanına exact edge touch kabul edilmeli"),
		A.Validate(Request(TEXT("decor.edge.station"), FVector(170.f, 0.f, 0.f), FVector2D(40.f, 40.f))).bAccepted);

	FCigPlacementAuthority B = Authority();
	FCigPlacementRequest Seating = Request(TEXT("seating.clearance"), FVector::ZeroVector, FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Seating, ECigPlacementCategory::Seating, 2,
		FVector2D(100.f, 80.f), FVector2D(100.f, 0.f));
	TestTrue(TEXT("Seating kaydolmalı"), B.TryRegister(Seating).bAccepted);
	TestEqual(TEXT("Seating kullanım alanı generic clearance ile korunmalı"),
		B.Validate(Request(TEXT("decor.blocks.seating"), FVector(100.f, 0.f, 0.f), FVector2D(40.f, 40.f))).Failure,
		ECigPlacementFailure::BlocksFunctionalClearance);

	FCigPlacementAuthority C = Authority();
	FCigPlacementRequest Left = Request(TEXT("shared.left"), FVector::ZeroVector, FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Left, ECigPlacementCategory::Station, 1,
		FVector2D(160.f, 80.f), FVector2D(100.f, 0.f));
	FCigPlacementRequest Right = Request(TEXT("shared.right"), FVector(300.f, 0.f, 0.f), FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Right, ECigPlacementCategory::Station, 1,
		FVector2D(160.f, 80.f), FVector2D(-100.f, 0.f));
	TestTrue(TEXT("Paylaşılan aisle için ilk station kaydolmalı"), C.TryRegister(Left).bAccepted);
	TestTrue(TEXT("Use/use overlap fiziksel işgal olmadığından paylaşılabilmeli"), C.Validate(Right).bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementConsequenceQueriesTest,
	"Cigkofte.Placement.Consequences.Authority.RegisterAndQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementConsequenceQueriesTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest Station = Request(TEXT("query.station"), FVector(-500.f, 0.f, 0.f), FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Station, ECigPlacementCategory::Station, 1,
		FVector2D(80.f, 80.f), FVector2D::ZeroVector);
	FCigPlacementRequest Seating = Request(TEXT("query.seating"), FVector::ZeroVector, FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Seating, ECigPlacementCategory::Seating, 2,
		FVector2D(80.f, 80.f), FVector2D::ZeroVector);
	const FCigPlacementRequest Decoration = Request(TEXT("query.decoration"), FVector(500.f, 0.f, 0.f), FVector2D(40.f, 40.f));
	TestTrue(TEXT("Station kaydolmalı"), A.TryRegister(Station).bAccepted);
	TestTrue(TEXT("Seating kaydolmalı"), A.TryRegister(Seating).bAccepted);
	TestTrue(TEXT("Decoration kaydolmalı"), A.TryRegister(Decoration).bAccepted);
	TestEqual(TEXT("Record ve consequence sayısı bire bir olmalı"), A.ConsequenceCount(), A.RecordCount());
	TestEqual(TEXT("Station kapasite toplamı"),
		A.CountFunctionalCapacity(ECigPlacementCategory::Station), 1);
	TestEqual(TEXT("Seating kapasite toplamı"),
		A.CountFunctionalCapacity(ECigPlacementCategory::Seating), 2);
	TestEqual(TEXT("Decoration kapasite toplamı"),
		A.CountFunctionalCapacity(ECigPlacementCategory::Decoration), 0);
	TestEqual(TEXT("Üç installed layout consequence olmalı"), A.CountInstalledLayoutConsequences(), 3);
	const FCigPlacementRecord* Record = A.Find(Station.StableId);
	FCigPlacementConsequence Found;
	const bool bFound = A.TryGetConsequence(Station.StableId, Found);
	TestTrue(TEXT("Station consequence bulunmalı"), bFound);
	if (Record && bFound)
	{
		TestEqual(TEXT("Record/consequence stable ID invariantı"), Record->StableId, Found.StableId);
		TestEqual(TEXT("Record/consequence kategori invariantı"), Record->Category, Found.Category);
		TestEqual(TEXT("Record/consequence lifetime invariantı"), Record->Lifetime, Found.Lifetime);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementConsequenceMoveTest,
	"Cigkofte.Placement.Consequences.Authority.AtomicMoveAndRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementConsequenceMoveTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest Original = Request(TEXT("move.station"), FVector(-300.f, 0.f, 0.f), FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Original, ECigPlacementCategory::Station, 1,
		FVector2D(80.f, 80.f), FVector2D(80.f, 0.f));
	const FCigPlacementResult Added = A.TryRegister(Original);
	TestTrue(TEXT("İlk register state değiştirmeli"), Added.bAccepted && Added.bStateChanged);

	FCigPlacementRequest Move = Original;
	// The new physical footprint lies inside the old use area. A valid move must
	// ignore exactly its own old consequence while validating the candidate.
	Move.CandidateTransform.SetLocation(FVector(-220.f, 0.f, 0.f));
	Move.Context = ECigPlacementContext::MoveExisting;
	Move.IgnoreStableId = Move.StableId;
	const FCigPlacementResult Moved = A.TryRegister(Move);
	TestTrue(TEXT("Geçerli move state'i bir kez değiştirmeli"), Moved.bAccepted && Moved.bStateChanged);
	TestEqual(TEXT("Move duplicate consequence üretmemeli"), A.ConsequenceCount(), 1);
	FCigPlacementConsequence AfterMove;
	const bool bFoundAfterMove = A.TryGetConsequence(Move.StableId, AfterMove);
	TestTrue(TEXT("Move sonrası consequence bulunmalı"), bFoundAfterMove);
	if (bFoundAfterMove)
	{
		TestTrue(TEXT("Move kullanım alanını yeni transformla güncellemeli"),
			AfterMove.UseRect.Center.Equals(FVector2D(-140.f, 0.f), 0.01f));
	}

	const FCigPlacementResult NoOp = A.TryRegister(Move);
	TestTrue(TEXT("Aynı normalized move kabul edilmeli"), NoOp.bAccepted);
	TestFalse(TEXT("Aynı normalized move state change sayılmamalı"), NoOp.bStateChanged);

	FCigPlacementRequest ChangedSpec = Move;
	ChangedSpec.UseSpec.Size.X += 10.f;
	TestEqual(TEXT("Move consequence tanımını değiştirememeli"), A.TryRegister(ChangedSpec).Failure,
		ECigPlacementFailure::ConsequenceMismatch);

	const FCigPlacementRequest Blocker = Request(TEXT("move.blocker"),
		FVector(80.f, 0.f, 0.f), FVector2D(40.f, 40.f));
	TestTrue(TEXT("Rollback testi blocker kaydı eklemeli"), A.TryRegister(Blocker).bAccepted);
	FCigPlacementRequest Conflicting = Move;
	Conflicting.CandidateTransform.SetLocation(FVector::ZeroVector);
	TestEqual(TEXT("Başka stable ID yeni çalışma alanını kapatınca move reddedilmeli"),
		A.TryRegister(Conflicting).Failure, ECigPlacementFailure::BlocksStationAccess);
	FCigPlacementConsequence KeptConsequence;
	TestTrue(TEXT("Functional conflict eski consequence'ı korumalı"),
		A.TryGetConsequence(Move.StableId, KeptConsequence));
	TestTrue(TEXT("Functional conflict eski use rect'i değiştirmemeli"),
		KeptConsequence.UseRect.Center.Equals(FVector2D(-140.f, 0.f), 0.01f));

	FCigPlacementRequest Outside = Move;
	Outside.CandidateTransform.SetLocation(FVector(950.f, 0.f, 0.f));
	TestEqual(TEXT("Geçersiz move tamamen reddedilmeli"), A.TryRegister(Outside).Failure,
		ECigPlacementFailure::FunctionalAreaOutsideShop);
	const FCigPlacementRecord* Kept = A.Find(Move.StableId);
	TestNotNull(TEXT("Başarısız move eski kaydı korumalı"), Kept);
	if (Kept)
	{
		TestTrue(TEXT("Başarısız move eski transformu korumalı"),
			Kept->Transform.GetLocation().Equals(FVector(-220.f, 0.f, 0.f)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementConsequenceRemoveTest,
	"Cigkofte.Placement.Consequences.Authority.RemoveAndReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementConsequenceRemoveTest::RunTest(const FString&)
{
	FCigPlacementAuthority A = Authority();
	FCigPlacementRequest Station = Request(TEXT("reuse.station"), FVector(-300.f, 0.f, 0.f), FVector2D(40.f, 40.f));
	DefineFunctionalConsequence(Station, ECigPlacementCategory::Station, 1,
		FVector2D(80.f, 80.f), FVector2D::ZeroVector);
	const FCigPlacementRequest Other = Request(TEXT("reuse.other"), FVector(300.f, 0.f, 0.f), FVector2D(40.f, 40.f));
	TestTrue(TEXT("Station kaydolmalı"), A.TryRegister(Station).bAccepted);
	TestTrue(TEXT("Diğer kayıt kaydolmalı"), A.TryRegister(Other).bAccepted);
	TestTrue(TEXT("Remove record ve consequence'ı birlikte kaldırmalı"), A.Remove(Station.StableId));
	TestNull(TEXT("Record kaldırılmalı"), A.Find(Station.StableId));
	FCigPlacementConsequence RemovedConsequence;
	TestFalse(TEXT("Consequence kaldırılmalı"),
		A.TryGetConsequence(Station.StableId, RemovedConsequence));
	TestNotNull(TEXT("Başka kayıt etkilenmemeli"), A.Find(Other.StableId));
	TestFalse(TEXT("Olmayan kaydı kaldırmak state değiştirmemeli"), A.Remove(Station.StableId));
	TestTrue(TEXT("Remove sonrası stable ID tekrar kullanılabilmeli"), A.TryRegister(Station).bAccepted);
	TestEqual(TEXT("Reuse duplicate consequence üretmemeli"), A.ConsequenceCount(), 2);
	A.ResetRecords();
	TestEqual(TEXT("Reset record ve index'i birlikte boşaltmalı"), A.RecordCount(), 0);
	TestNull(TEXT("Reset ilk ID index'ini kaldırmalı"), A.Find(Station.StableId));
	TestNull(TEXT("Reset ikinci ID index'ini kaldırmalı"), A.Find(Other.StableId));
	TestFalse(TEXT("Reset consequence sorgusunu da boşaltmalı"),
		A.TryGetConsequence(Station.StableId, RemovedConsequence));
	TestTrue(TEXT("Reset sonrası stable ID yeniden kaydolabilmeli"), A.TryRegister(Station).bAccepted);
	TestTrue(TEXT("Reset sonrası consequence index'i yeniden kurulmalı"),
		A.TryGetConsequence(Station.StableId, RemovedConsequence));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementConsequenceEventTest,
	"Cigkofte.Placement.Consequences.Integration.EventOnlyOnStateChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementConsequenceEventTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->Bus)
	{
		AddError(TEXT("Placement event testi için sistemler yok."));
		return false;
	}

	TArray<FCigPlacementChange> Changes;
	const FDelegateHandle Handle = Shop.GM->Bus->PlacementChanged.AddLambda(
		[&Changes](const FCigPlacementChange& Change) { Changes.Add(Change); });
	FCigPlacementRequest R = Request(TEXT("event.decoration"), FVector(500.f, 1000.f, 0.f), FVector2D(40.f, 40.f));
	TestTrue(TEXT("Event register kabul edilmeli"), Shop.GM->Placement->RegisterPlacement(R).bAccepted);
	R.Context = ECigPlacementContext::MoveExisting;
	R.IgnoreStableId = R.StableId;
	R.CandidateTransform.SetLocation(FVector(600.f, 1000.f, 0.f));
	TestTrue(TEXT("Event move kabul edilmeli"), Shop.GM->Placement->RegisterPlacement(R).bAccepted);
	TestTrue(TEXT("No-op move kabul edilmeli"), Shop.GM->Placement->RegisterPlacement(R).bAccepted);
	TestTrue(TEXT("Event remove başarılı olmalı"), Shop.GM->Placement->RemovePlacement(R.StableId));
	TestFalse(TEXT("İkinci remove başarısız olmalı"), Shop.GM->Placement->RemovePlacement(R.StableId));
	Shop.GM->Bus->PlacementChanged.Remove(Handle);

	TestEqual(TEXT("Yalnız üç gerçek state change yayınlanmalı"), Changes.Num(), 3);
	if (Changes.Num() == 3)
	{
		TestEqual(TEXT("İlk event Added"), Changes[0].Mutation, ECigPlacementMutation::Added);
		TestEqual(TEXT("İkinci event Moved"), Changes[1].Mutation, ECigPlacementMutation::Moved);
		TestEqual(TEXT("Üçüncü event Removed"), Changes[2].Mutation, ECigPlacementMutation::Removed);
		TestEqual(TEXT("Event stable ID taşımalı"), Changes[2].StableId, R.StableId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigPlacementShopIntegrationTest,
	"Cigkofte.Placement.Integration.RealShopRegistersFixturesAndCrate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigPlacementShopIntegrationTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	if (!Shop.GM->Placement || !Shop.GM->WorldBuilder || !Shop.GM->Inventory)
	{
		AddError(TEXT("Yerleşim entegrasyonu için sistemler yok."));
		return false;
	}

	Shop.GM->WorldBuilder->BuildWorld();
	TestNotNull(TEXT("Servis fixture kaydı olmalı"), Shop.GM->Placement->FindPlacement(TEXT("fixture.station.servis")));
	TestNotNull(TEXT("Fallback boyutlu masa kaydı olmalı"), Shop.GM->Placement->FindPlacement(TEXT("fixture.seating.table.0")));
	const FCigPlacementRecord* Sofa = Shop.GM->Placement->FindPlacement(TEXT("fixture.seating.sofa"));
	TestNotNull(TEXT("Sofa fixture kaydı olmalı"), Sofa);
	if (Sofa)
	{
		TestEqual(TEXT("Sofa koltuk kapasitesi değil dekorasyon olmalı"), Sofa->Category,
			ECigPlacementCategory::Decoration);
		TestEqual(TEXT("Sofa installed lifetime taşımalı"), Sofa->Lifetime,
			ECigPlacementLifetime::Installed);
		TestTrue(TEXT("Sofa yerleşim izi görselin 90 derece yönünü korumalı"),
			FMath::IsNearlyEqual(FMath::Abs(FMath::UnwindDegrees(Sofa->Transform.Rotator().Yaw)), 90.f, 0.01f));
	}
	FCigPlacementConsequence SofaConsequence;
	const bool bHasSofaConsequence = Shop.GM->Placement->TryGetPlacementConsequence(
		TEXT("fixture.seating.sofa"), SofaConsequence);
	TestTrue(TEXT("Sofa consequence kaydı olmalı"), bHasSofaConsequence);
	if (bHasSofaConsequence)
	{
		TestEqual(TEXT("Sofa seating kapasitesi üretmemeli"), SofaConsequence.FunctionalCapacity, 0);
		TestFalse(TEXT("Sofa kullanım alanı üretmemeli"), SofaConsequence.bHasUseArea);
	}
	FCigPlacementConsequence TableConsequence;
	const bool bHasTableConsequence = Shop.GM->Placement->TryGetPlacementConsequence(
		TEXT("fixture.seating.table.0"), TableConsequence);
	TestTrue(TEXT("Masa consequence kaydı olmalı"), bHasTableConsequence);
	if (bHasTableConsequence)
	{
		TestEqual(TEXT("Her masa iki usable seat üretmeli"), TableConsequence.FunctionalCapacity, 2);
	}
	FCigPlacementConsequence ServiceConsequence;
	const bool bHasServiceConsequence = Shop.GM->Placement->TryGetPlacementConsequence(
		TEXT("fixture.station.servis"), ServiceConsequence);
	TestTrue(TEXT("Servis station consequence kaydı olmalı"), bHasServiceConsequence);
	if (bHasServiceConsequence)
	{
		TestEqual(TEXT("Servis bir station birimi üretmeli"), ServiceConsequence.FunctionalCapacity, 1);
		TestTrue(TEXT("Servis çalışma tarafı olmalı"), ServiceConsequence.bHasUseArea);
	}
	FCigPlacementConsequence BowlConsequence;
	FCigPlacementConsequence SideProductConsequence;
	const bool bHasBowlConsequence = Shop.GM->Placement->TryGetPlacementConsequence(
		TEXT("fixture.station.mama"), BowlConsequence);
	const bool bHasSideProductConsequence = Shop.GM->Placement->TryGetPlacementConsequence(
		TEXT("fixture.station.yanurun"), SideProductConsequence);
	TestTrue(TEXT("Mama kabının kullanım tarafı yanındaki masadan uzağa bakmalı"),
		bHasBowlConsequence && BowlConsequence.UseRect.Center.X > -200.f);
	TestTrue(TEXT("Yan ürün kullanım tarafı yanındaki masadan uzağa bakmalı"),
		bHasSideProductConsequence && SideProductConsequence.UseRect.Center.X > -350.f);
	TestEqual(TEXT("22 istasyon kaydolmalı"),
		Shop.GM->Placement->PlacementCountByCategory(ECigPlacementCategory::Station), 22);
	TestEqual(TEXT("4 masa grubu seating olarak kaydolmalı"),
		Shop.GM->Placement->PlacementCountByCategory(ECigPlacementCategory::Seating), 4);
	TestEqual(TEXT("Sofa tek dekorasyon kaydı olmalı"),
		Shop.GM->Placement->PlacementCountByCategory(ECigPlacementCategory::Decoration), 1);
	TestEqual(TEXT("Teslimattan önce storage kaydı olmamalı"),
		Shop.GM->Placement->PlacementCountByCategory(ECigPlacementCategory::Storage), 0);
	TestEqual(TEXT("Dünya kurulumunda 27 installed kayıt olmalı"),
		Shop.GM->Placement->PlacementCountByLifetime(ECigPlacementLifetime::Installed), 27);
	TestEqual(TEXT("Dünya kurulumunda transient kayıt olmamalı"),
		Shop.GM->Placement->PlacementCountByLifetime(ECigPlacementLifetime::Transient), 0);
	TestEqual(TEXT("Dünya kayıt toplamı kesin olmalı"), Shop.GM->Placement->PlacementCount(), 27);
	TestEqual(TEXT("Her record tam bir consequence taşımalı"),
		Shop.GM->Placement->PlacementConsequenceCount(), 27);
	TestEqual(TEXT("27 consequence installed layout parçası olmalı"),
		Shop.GM->Placement->InstalledLayoutConsequenceCount(), 27);
	TestEqual(TEXT("22 usable station birimi olmalı"),
		Shop.GM->Placement->FunctionalCapacityByCategory(ECigPlacementCategory::Station), 22);
	TestEqual(TEXT("4 masa toplam 8 usable seat üretmeli"),
		Shop.GM->Placement->FunctionalCapacityByCategory(ECigPlacementCategory::Seating), 8);
	TestEqual(TEXT("Decoration usable kapasite üretmemeli"),
		Shop.GM->Placement->FunctionalCapacityByCategory(ECigPlacementCategory::Decoration), 0);
	TestEqual(TEXT("Teslimat öncesi usable storage olmamalı"),
		Shop.GM->Placement->FunctionalCapacityByCategory(ECigPlacementCategory::Storage), 0);
	ACigkofteStation* ServiceStation = Shop.GM->WorldBuilder->FindStation(ECigStation::Servis);
	TestNotNull(TEXT("Station gameplay query consequence ile kullanılabilir olmalı"), ServiceStation);
	TestTrue(TEXT("Gerçek etkileşim kapısı consequence bulunan station'ı kabul etmeli"),
		Shop.GM->IsStationInteractionAvailable(ServiceStation));

	TArray<int32> ReservedSeats;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const int32 Seat = Shop.GM->WorldBuilder->ReserveSeat();
		TestTrue(TEXT("Sekiz authored seat consequence kapasitesi içinde reserve edilmeli"), Seat >= 0);
		ReservedSeats.Add(Seat);
	}
	TestEqual(TEXT("Dokuzuncu seat consequence kapasitesi dışında kalmalı"),
		Shop.GM->WorldBuilder->ReserveSeat(), -1);
	for (int32 Seat : ReservedSeats)
	{
		Shop.GM->WorldBuilder->ReleaseSeat(Seat);
	}

	FCigPendingOrder Order;
	Order.Item = (int32)ECigIngredient::Isot;
	Order.Amount = 5;
	Order.Quality = 1.f;
	Order.TimeLeft = 0.f;
	Order.PlacementSerial = 77;
	FCigPlacementRequest Probe;
	Probe.StableId = TEXT("crate.delivery.00000077");
	Probe.Category = ECigPlacementCategory::Storage;
	Probe.Lifetime = ECigPlacementLifetime::Transient;
	Probe.Footprint = UCigPlacementSystem::StockCrateFootprint();
	Probe.UseSpec = UCigPlacementSystem::StockCrateUseSpec();
	Probe.Context = ECigPlacementContext::Delivery;
	const FCigPlacementResult Expected = Shop.GM->Placement->FindFirstValidPlacement(
		Probe, CigPlacementLayout::DeliverySpots());
	if (!Expected.bAccepted)
	{
		AddError(TEXT("Gerçek dükkânda teslimat noktası bulunamadı."));
		return false;
	}

	Shop.GM->Inventory->PendingOrders.Add(Order);
	Shop.GM->Inventory->UpdateSystem(0.1f);
	TestEqual(TEXT("Bir fiziksel kasa doğmalı"), Shop.GM->Inventory->Crates.Num(), 1);
	ACigStockCrate* Crate = Shop.GM->Inventory->Crates.Num() ? Shop.GM->Inventory->Crates[0].Get() : nullptr;
	if (!Crate)
	{
		AddError(TEXT("Fiziksel kasa oluşmadı."));
		return false;
	}
	TestEqual(TEXT("Kasa stabil kimliği sipariş yaşam döngüsünden gelmeli"), Crate->PlacementId, Probe.StableId);
	TestTrue(TEXT("İlk geçerli declared nokta kullanılmalı"),
		Crate->GetActorLocation().Equals(Expected.NormalizedTransform.GetLocation()));
	const FCigPlacementRecord* CrateRecord = Shop.GM->Placement->FindPlacement(Crate->PlacementId);
	TestNotNull(TEXT("Kasa otoritede kayıtlı olmalı"), CrateRecord);
	if (CrateRecord)
	{
		TestEqual(TEXT("Kasa storage olmalı"), CrateRecord->Category, ECigPlacementCategory::Storage);
		TestEqual(TEXT("Kasa transient olmalı"), CrateRecord->Lifetime, ECigPlacementLifetime::Transient);
	}
	TestEqual(TEXT("Teslimat bir storage kaydı eklemeli"),
		Shop.GM->Placement->PlacementCountByCategory(ECigPlacementCategory::Storage), 1);
	TestEqual(TEXT("Teslimat bir transient kayıt eklemeli"),
		Shop.GM->Placement->PlacementCountByLifetime(ECigPlacementLifetime::Transient), 1);
	TestEqual(TEXT("Teslimat bir usable storage birimi eklemeli"),
		Shop.GM->Placement->FunctionalCapacityByCategory(ECigPlacementCategory::Storage), 1);
	FCigPlacementConsequence CrateConsequence;
	const bool bHasCrateConsequence = Shop.GM->Placement->TryGetPlacementConsequence(
		Crate->PlacementId, CrateConsequence);
	TestTrue(TEXT("Kasa consequence kaydı olmalı"), bHasCrateConsequence);
	if (bHasCrateConsequence)
	{
		TestFalse(TEXT("Transient kasa installed layout değildir"), CrateConsequence.bInstalledLayout);
		TestTrue(TEXT("Kasanın unload alanı olmalı"), CrateConsequence.bHasUseArea);
	}

	Shop.GM->Inventory->UnloadCrate(Crate);
	TestNull(TEXT("Boşaltılan kasanın izi kalkmalı"), Shop.GM->Placement->FindPlacement(Probe.StableId));
	TestFalse(TEXT("Boşaltılan kasanın consequence'ı kalkmalı"),
		Shop.GM->Placement->TryGetPlacementConsequence(Probe.StableId, CrateConsequence));
	TestEqual(TEXT("Boşaltma transient kaydı kaldırmalı"),
		Shop.GM->Placement->PlacementCountByLifetime(ECigPlacementLifetime::Transient), 0);
	TestEqual(TEXT("Boşaltma storage kapasitesini sıfırlamalı"),
		Shop.GM->Placement->FunctionalCapacityByCategory(ECigPlacementCategory::Storage), 0);

	FCigPendingOrder DestroyedOrder = Order;
	DestroyedOrder.Amount = 1;
	DestroyedOrder.PlacementSerial = 78;
	Shop.GM->Inventory->PendingOrders.Add(DestroyedOrder);
	Shop.GM->Inventory->UpdateSystem(0.1f);
	ACigStockCrate* DestroyedCrate = Shop.GM->Inventory->Crates.Num()
		? Shop.GM->Inventory->Crates[0].Get() : nullptr;
	TestNotNull(TEXT("Lifecycle testi için ikinci kasa doğmalı"), DestroyedCrate);
	if (DestroyedCrate)
	{
		const FName DestroyedPlacementId = DestroyedCrate->PlacementId;
		DestroyedCrate->Destroy();
		TestNull(TEXT("Beklenmedik actor destruction transient record bırakmamalı"),
			Shop.GM->Placement->FindPlacement(DestroyedPlacementId));
		TestFalse(TEXT("Beklenmedik actor destruction consequence bırakmamalı"),
			Shop.GM->Placement->TryGetPlacementConsequence(DestroyedPlacementId, CrateConsequence));
		TestEqual(TEXT("Destroyed kasa weak listeden event ile düşmeli"), Shop.GM->Inventory->Crates.Num(), 0);
	}

	TestTrue(TEXT("Bir seating record kaldırılabilmeli"),
		Shop.GM->Placement->RemovePlacement(TEXT("fixture.seating.table.0")));
	TArray<int32> RemainingSeats;
	for (;;)
	{
		const int32 Seat = Shop.GM->WorldBuilder->ReserveSeat();
		if (Seat < 0) { break; }
		RemainingSeats.Add(Seat);
	}
	TestEqual(TEXT("Kaldırılan masa iki seat'i de gameplay'den düşürmeli"), RemainingSeats.Num(), 6);
	for (int32 Seat : RemainingSeats)
	{
		Shop.GM->WorldBuilder->ReleaseSeat(Seat);
	}
	TestTrue(TEXT("Servis placement kaydı kaldırılabilmeli"),
		Shop.GM->Placement->RemovePlacement(TEXT("fixture.station.servis")));
	TestNull(TEXT("Consequence kalkınca station gameplay query kullanılamaz olmalı"),
		Shop.GM->WorldBuilder->FindStation(ECigStation::Servis));
	TestFalse(TEXT("Consequence kalkınca traced actor etkileşim kapısından geçmemeli"),
		Shop.GM->IsStationInteractionAvailable(ServiceStation));
	Shop.GM->Days->Phase = ECigPhase::Opening;
	Shop.GM->HandleInteract(ServiceStation);
	TestEqual(TEXT("Kaldırılmış station actor'ı dükkânı açamamalı"),
		Shop.GM->Days->Phase, ECigPhase::Opening);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
