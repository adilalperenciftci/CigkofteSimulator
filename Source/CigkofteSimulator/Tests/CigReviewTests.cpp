// Review identity.
//
// The social system holds the review it is waiting to answer. Reviews are
// inserted at the front of the list, so holding an index meant a second poor
// review on the same day silently redirected the reply to a different one.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Reviews; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Economy/CigReviewSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigReviewIdStableTest,
	"Cigkofte.Reviews.IdSurvivesNewerReviews",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigReviewIdStableTest::RunTest(const FString& /*Parameters*/)
{
	UCigReviewSystem* Sys = NewObject<UCigReviewSystem>();
	Sys->AddToRoot();

	// Three reviews on the same day, the first of them the poor one the player
	// would be answering.
	Sys->PushReview(TEXT("a"), TEXT("kotu"), 1, 3);
	const int32 BeklenenId = Sys->Reviews[0].Id;

	Sys->PushReview(TEXT("b"), TEXT("iyi"), 5, 3);
	Sys->PushReview(TEXT("c"), TEXT("orta"), 3, 3);

	TestEqual(TEXT("Yeni yorumlar başa eklenmeli"), Sys->Reviews.Num(), 3);
	TestNotEqual(TEXT("Beklenen yorum artık 0. indekste olmamalı"), Sys->Reviews[0].Id, BeklenenId);

	const FCigReview* Bulunan = Sys->YorumBul(BeklenenId);
	TestNotNull(TEXT("Yorum ID ile hâlâ bulunmalı"), Bulunan);
	if (Bulunan)
	{
		TestEqual(TEXT("Bulunan yorum doğru olmalı"), Bulunan->Author, FString(TEXT("a")));
		TestEqual(TEXT("Yıldızı korunmalı"), Bulunan->Stars, 1);
	}

	Sys->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigReviewIdUniqueTest,
	"Cigkofte.Reviews.IdsAreUniqueAndTrimSafe",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigReviewIdUniqueTest::RunTest(const FString& /*Parameters*/)
{
	UCigReviewSystem* Sys = NewObject<UCigReviewSystem>();
	Sys->AddToRoot();

	// The list is capped at twelve, so a long-running shop trims the oldest.
	TSet<int32> Gorulen;
	for (int32 i = 0; i < 20; ++i)
	{
		Sys->PushReview(FString::Printf(TEXT("y%d"), i), TEXT("metin"), 3, 1);
		Gorulen.Add(Sys->Reviews[0].Id);
	}

	TestEqual(TEXT("Yirmi yorumun yirmi farklı kimliği olmalı"), Gorulen.Num(), 20);
	TestEqual(TEXT("Liste on ikide kalmalı"), Sys->Reviews.Num(), 12);

	// A trimmed review must report as gone rather than resolving to a neighbour.
	TestNull(TEXT("Listeden düşen yorum bulunmamalı"), Sys->YorumBul(1));
	TestNull(TEXT("Sıfır kimlik hiçbir yorumu göstermemeli"), Sys->YorumBul(0));

	Sys->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
