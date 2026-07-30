// What the shop can hold.
//
// Stock was an unbounded integer per item, so the supplier tab was a question of
// money alone and the big fridge - a paid upgrade - bought only a slower
// spoilage curve. These tests pin the two rules that make it a decision: the
// cold pool is shared, and the fridge upgrade is the thing that widens it.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Storage; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Inventory/CigStorage.h"
#include "Core/CigBalance.h"
#include "Core/CigkofteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FKiler
	{
		int32 Stock[CigStockCount] = {};
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStorageColdIsShared,
	"Cigkofte.Storage.ColdGoodsCompeteForOneFridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStorageColdIsShared::RunTest(const FString&)
{
	FKiler K;

	// An empty fridge has its full capacity for any cold item.
	TestEqual(TEXT("Boş buzdolabı marul için tam kapasite vermeli"),
		CigStorage::RoomFor(K.Stock, CigStockMarul, false), CigStorage::ColdCapacityBase);

	// Filling it with tomatoes takes the room away from lettuce, which is the
	// entire point of the rule. A per-item limit would leave this untouched.
	K.Stock[CigStockDomates] = CigStorage::ColdCapacityBase;
	TestEqual(TEXT("Dolu buzdolabında marule yer kalmamalı"),
		CigStorage::RoomFor(K.Stock, CigStockMarul, false), 0);

	// And it does not touch the dry shelf.
	TestTrue(TEXT("Dolu buzdolabı bulgur rafını etkilememeli"),
		CigStorage::RoomFor(K.Stock, (int32)ECigIngredient::Bulgur, false) > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStorageBigFridgeBuysRoom,
	"Cigkofte.Storage.TheBigFridgeIsWhatWidensThePool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStorageBigFridgeBuysRoom::RunTest(const FString&)
{
	FKiler K;
	K.Stock[CigStockAyran] = CigStorage::ColdCapacityBase;

	TestEqual(TEXT("Küçük buzdolabı dolu olmalı"),
		CigStorage::RoomFor(K.Stock, CigStockMarul, false), 0);
	TestTrue(TEXT("Büyük buzdolabı aynı stokta hâlâ yer vermeli"),
		CigStorage::RoomFor(K.Stock, CigStockMarul, true) > 0);
	TestTrue(TEXT("Büyük buzdolabı havuzu genişletmeli"),
		CigStorage::ColdCapacity(true) > CigStorage::ColdCapacity(false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStorageClassesAreRight,
	"Cigkofte.Storage.WhatNeedsRefrigeratingAndWhatDoesNot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigStorageClassesAreRight::RunTest(const FString&)
{
	// The perishables. Getting one of these wrong would quietly let a shop hold
	// unlimited lettuce, which is the rule failing without failing loudly.
	for (int32 Item : { CigStockMarul, CigStockMaydanoz, CigStockDomates, CigStockTursu,
		CigStockSogan, CigStockLimon, CigStockAyran, CigStockIcliKofte, CigStockCorba, CigStockKunefe })
	{
		TestEqual(FString::Printf(TEXT("Kalem %d soğuk olmalı"), Item),
			CigStorage::ClassOf(Item), ECigStorageClass::Cold);
	}

	// The shelf-stable ones. Bulgur in a fridge would be the same rule failing
	// the other way, and it would eat the pool the perishables need.
	for (int32 Item : { (int32)ECigIngredient::Bulgur, (int32)ECigIngredient::Isot,
		(int32)ECigIngredient::Salca, (int32)ECigIngredient::Baharat,
		CigStockNarEksisi, CigStockLavas, CigStockCayBardak })
	{
		TestEqual(FString::Printf(TEXT("Kalem %d kuru olmalı"), Item),
			CigStorage::ClassOf(Item), ECigStorageClass::Dry);
	}

	// Water is a tap. It is neither stored nor ordered, and letting it into the
	// cold pool would fill a fridge with something that does not need one.
	TestEqual(TEXT("Su sınırsız olmalı"),
		CigStorage::ClassOf((int32)ECigIngredient::Su), ECigStorageClass::Unlimited);
	TestTrue(TEXT("Su için her zaman yer olmalı"),
		CigStorage::RoomFor(FKiler().Stock, (int32)ECigIngredient::Su, false) > 1000);

	// The shop must fit its own opening stock, with room for a delivery.
	//
	// This is the test that earned itself. Forty was picked as the cold capacity
	// without checking Stock.csv, whose starting perishables come to 42 - so the
	// game began two units over its own limit and refused every order on day
	// one. Pinned against the CSV rather than against a number, so raising a
	// starting amount fails here instead of in a player's first minute.
	int32 Baslangic[CigStockCount] = {};
	for (int32 i = 0; i < CigStockCount; ++i)
	{
		Baslangic[i] = CigBalance::Stock(i).StartAmount;
	}
	const int32 BaslangicSoguk = CigStorage::ColdUsed(Baslangic);
	TestTrue(FString::Printf(TEXT("Başlangıç soğuk stoğu (%d) kapasiteye (%d) sığmalı"),
		BaslangicSoguk, CigStorage::ColdCapacityBase),
		BaslangicSoguk <= CigStorage::ColdCapacityBase);

	// And leave room for at least one delivery, or the fridge is full before the
	// player has made a decision about it.
	TestTrue(TEXT("Açılış günü en az bir teslimatlık yer kalmalı"),
		CigStorage::ColdCapacityBase - BaslangicSoguk >= CigBalance::Stock(CigStockMarul).OrderAmount);

	// Dry goods start inside their shelf too.
	for (int32 i = 0; i < CigStockCount; ++i)
	{
		if (CigStorage::ClassOf(i) == ECigStorageClass::Dry)
		{
			TestTrue(FString::Printf(TEXT("Kalem %d başlangıçta rafa sığmalı"), i),
				Baslangic[i] <= CigStorage::DryCapacityPerItem);
		}
	}

	// Every item has a class, and an out-of-range index does not read past the
	// end of anything.
	TestEqual(TEXT("Aralık dışı kalem için yer olmamalı"),
		CigStorage::RoomFor(FKiler().Stock, CigStockCount + 5, false), 0);
	TestEqual(TEXT("Negatif kalem için yer olmamalı"),
		CigStorage::RoomFor(FKiler().Stock, -1, false), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
