// The event bus.
//
// The point of the refactor: a publishing system does not know its listeners.
// These tests verify the mechanism works - a broadcast reaches a subscriber,
// multiple subscribers are supported, and broadcasting with none does not crash.
//
// The stronger guarantee, "no publisher calls the quest system directly", is
// enforced statically in Tools/check_sources.py - write GM->Quests->Notify...
// again anywhere and CI fails.

#include "Misc/AutomationTest.h"
#include "Game/CigEventBus.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigEventBusDeliversTest,
	"Cigkofte.EventBus.BroadcastReachesSubscribers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigEventBusDeliversTest::RunTest(const FString& /*Parameters*/)
{
	TStrongObjectPtr<UCigEventBus> Bus(NewObject<UCigEventBus>());

	int32 Wraps = 0;
	Bus->WrapStarted.AddLambda([&Wraps]() { ++Wraps; });

	Bus->WrapStarted.Broadcast();
	Bus->WrapStarted.Broadcast();
	TestEqual(TEXT("İki yayın aboneye iki kez ulaşmalı"), Wraps, 2);

	// Event carrying a value
	float LastQuality = -1.f;
	Bus->DoughPrepared.AddLambda([&LastQuality](float Q) { LastQuality = Q; });
	Bus->DoughPrepared.Broadcast(87.5f);
	TestTrue(TEXT("Değer olduğu gibi taşınmalı"), FMath::IsNearlyEqual(LastQuality, 87.5f));

	// Three-parameter serving event
	float A = 0.f, Q = 0.f;
	int32 P = 0;
	Bus->Served.AddLambda([&A, &Q, &P](float InA, float InQ, int32 InP) { A = InA; Q = InQ; P = InP; });
	Bus->Served.Broadcast(91.f, 80.f, 2);
	TestTrue(TEXT("Doğruluk taşınmalı"), FMath::IsNearlyEqual(A, 91.f));
	TestTrue(TEXT("Kalite taşınmalı"), FMath::IsNearlyEqual(Q, 80.f));
	TestEqual(TEXT("Porsiyon taşınmalı"), P, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigEventBusMultipleSubscribersTest,
	"Cigkofte.EventBus.SupportsMultipleSubscribers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigEventBusMultipleSubscribersTest::RunTest(const FString& /*Parameters*/)
{
	// This is what the bus actually buys: today only the quest system listens;
	// if achievements listen tomorrow, not one line changes on the publisher side.
	TStrongObjectPtr<UCigEventBus> Bus(NewObject<UCigEventBus>());

	int32 First = 0, Second = 0;
	Bus->Delivered.AddLambda([&First]() { ++First; });
	Bus->Delivered.AddLambda([&Second]() { ++Second; });

	Bus->Delivered.Broadcast();
	TestEqual(TEXT("Birinci abone almalı"), First, 1);
	TestEqual(TEXT("İkinci abone de almalı"), Second, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigEventBusNoSubscriberIsSafeTest,
	"Cigkofte.EventBus.BroadcastWithoutSubscriberIsSafe",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigEventBusNoSubscriberIsSafeTest::RunTest(const FString& /*Parameters*/)
{
	// Publishers no longer check whether anyone is listening (there used to be
	// an if (GM->Quests)). A broadcast with no subscriber must quietly do nothing.
	TStrongObjectPtr<UCigEventBus> Bus(NewObject<UCigEventBus>());
	Bus->RivalClosed.Broadcast();
	Bus->ShopScoreChanged.Broadcast(4.2f);
	Bus->Served.Broadcast(50.f, 50.f, 1);
	TestTrue(TEXT("Abonesiz yayın çökmemeli"), true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
