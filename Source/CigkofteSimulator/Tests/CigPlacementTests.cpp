// Deterministic shop-floor placement authority.
//
// These tests use the pure value type unless the name says Integration. No
// renderer, collision scene or actor scan is needed to answer floor geometry.

#include "Misc/AutomationTest.h"
#include "Placement/CigPlacementTypes.h"
#include "Placement/CigPlacementSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"
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
		Result.Category = ECigPlacementCategory::ShopObject;
		Result.CandidateTransform = FTransform(FRotator(0.f, Yaw, 0.f), Location);
		Result.Footprint.Size = Size;
		return Result;
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
	Crate.Category = ECigPlacementCategory::StockCrate;
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
	Crate.Category = ECigPlacementCategory::StockCrate;
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
		TestTrue(TEXT("Sofa yerleşim izi görselin 90 derece yönünü korumalı"),
			FMath::IsNearlyEqual(FMath::Abs(FMath::UnwindDegrees(Sofa->Transform.Rotator().Yaw)), 90.f, 0.01f));
	}
	TestTrue(TEXT("22 istasyon ve 5 oturma fixture'ı kaydolmalı"), Shop.GM->Placement->PlacementCount() >= 27);

	FCigPendingOrder Order;
	Order.Item = (int32)ECigIngredient::Isot;
	Order.Amount = 5;
	Order.Quality = 1.f;
	Order.TimeLeft = 0.f;
	Order.PlacementSerial = 77;
	FCigPlacementRequest Probe;
	Probe.StableId = TEXT("crate.delivery.00000077");
	Probe.Category = ECigPlacementCategory::StockCrate;
	Probe.Footprint = UCigPlacementSystem::StockCrateFootprint();
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
	TestNotNull(TEXT("Kasa otoritede kayıtlı olmalı"), Shop.GM->Placement->FindPlacement(Crate->PlacementId));

	Shop.GM->Inventory->UnloadCrate(Crate);
	TestNull(TEXT("Boşaltılan kasanın izi kalkmalı"), Shop.GM->Placement->FindPlacement(Probe.StableId));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
