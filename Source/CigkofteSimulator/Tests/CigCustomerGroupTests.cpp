// People who arrive together, and what that costs the shop.
//
// The rule this file exists to hold onto is that a party is indivisible. It is
// the whole reason groups are interesting: the queue's capacity has always been
// a number nobody could see, and a party of four either fits or does not, in
// front of the player. Admitting whoever happens to fit would be cheaper and
// would read on screen as a bug - part of a group turning around for no visible
// reason - so the refusal is deliberate and worth failing on.
//
// Half of this is arithmetic and half is what the queue does with it, so the
// tests come in two kinds. The pure ones need no shop and pin the decision. The
// shop ones prove the decision is actually the one the queue acts on, and that
// a party leaving takes its own people out and nobody else's.

#include "Misc/AutomationTest.h"
#include "Customers/CigCustomerGroup.h"
#include "Customers/CigCustomerSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Core/CigRandomSubsystem.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "Orders/CigOrderSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Tests/CigTestShop.h"
#include "World/CigWorldBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Everybody still in the queue who belongs to this party. Named for this file
	// because separate .cpp files share a translation unit under unity builds and
	// a plain `PartyIn` would eventually collide with somebody else's helper.
	TArray<ACigkofteCustomer*> CigGroupTestsPartyInQueue(const UCigCustomerSystem& Cust, int32 GroupId)
	{
		TArray<ACigkofteCustomer*> Out;
		for (const TObjectPtr<ACigkofteCustomer>& C : Cust.Queue)
		{
			if (C && C->GroupId == GroupId) { Out.Add(C.Get()); }
		}
		return Out;
	}

	// Opens the shop and empties whatever the morning put in the queue.
	//
	// A day start rolls its own events, and two of them walk customers in before
	// the player has done anything - a student rush brings a pair, a VIP one. The
	// roll is not seeded here, so a test that assumed an empty counter passed or
	// failed depending on the clock. Sending them home first is what makes the
	// party under test the only party in the shop.
	//
	// They leave happy, which costs nothing: no reputation, no angry counter, no
	// missed order. Every baseline a test takes after this is therefore still the
	// baseline of a shop that has done nothing wrong.
	bool CigGroupTestsOpenEmptyShop(FAutomationTestBase& Test, UCigCustomerSystem& Cust, UCigDaySystem& Days)
	{
		Days.StartDay(false);
		Days.OpenShop();

		while (Cust.Queue.Num() > 0)
		{
			ACigkofteCustomer* C = Cust.Queue[0].Get();
			if (!C)
			{
				Cust.Queue.RemoveAt(0);
				continue;
			}
			Cust.RemoveCustomer(C, /*bAngry=*/false);
		}

		if (Cust.Queue.Num() != 0)
		{
			Test.AddError(TEXT("Acilis kuyrugu bosaltilamadi."));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupIndivisibleTest,
	"Cigkofte.Groups.APartyIsNeverSplitToMakeItFit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigGroupIndivisibleTest::RunTest(const FString&)
{
	// Three friends, a queue of five with two people already in it. There is room
	// for two of them. The tempting answer is to let two in.
	const FCigGroupIntake Tight = CigCustomerGroup::JudgeIntake(3, 3, 5);
	TestFalse(TEXT("Sigmayan grup kuyruga alinmamali"), Tight.IsAdmitted());
	TestEqual(TEXT("Kismi kabul olmamali"), Tight.Admitted, 0);
	TestEqual(TEXT("Sebep bolunme olmali"), Tight.Refusal, ECigGroupRefusal::WouldSplit);

	// Exactly enough room is enough room: the boundary is inclusive, and a party
	// that fills the last space is admitted rather than turned away by an
	// off-by-one nobody would ever notice in play.
	const FCigGroupIntake Exact = CigCustomerGroup::JudgeIntake(3, 2, 5);
	TestTrue(TEXT("Tam sigan grup alinmali"), Exact.IsAdmitted());
	TestEqual(TEXT("Grubun tamami alinmali"), Exact.Admitted, 3);

	// One short, and it is a refusal again. Said next to the case above so the
	// boundary is pinned from both sides rather than asserted once.
	const FCigGroupIntake OneShort = CigCustomerGroup::JudgeIntake(3, 3, 5);
	TestFalse(TEXT("Bir kisi eksik yer yetmemeli"), OneShort.IsAdmitted());

	// Whatever is admitted, it is all of them or none of them. Said as a property
	// over the whole space rather than at the three points above, because the
	// thing being defended is the absence of a partial answer anywhere.
	for (int32 Size = CigCustomerGroup::MinSize; Size <= CigCustomerGroup::MaxSize; ++Size)
	{
		for (int32 Used = 0; Used <= 7; ++Used)
		{
			for (int32 Cap = 1; Cap <= 7; ++Cap)
			{
				const FCigGroupIntake I = CigCustomerGroup::JudgeIntake(Size, Used, Cap);
				if (I.Admitted != 0 && I.Admitted != Size)
				{
					AddError(FString::Printf(
						TEXT("Grup bolundu: boyut %d, dolu %d, kapasite %d -> %d alindi."),
						Size, Used, Cap, I.Admitted));
					return false;
				}
				// And an admission never overfills the queue, which is the other
				// half of the same promise: nobody is let in past the limit to
				// keep a party whole.
				if (I.IsAdmitted() && Used + I.Admitted > Cap)
				{
					AddError(FString::Printf(
						TEXT("Grup kapasiteyi asti: %d + %d > %d."), Used, I.Admitted, Cap));
					return false;
				}
			}
		}
	}

	// A shop too small for this party gets its own answer even when empty. The
	// two refusals are different problems and only one of them goes away by
	// waiting, so the message the player sees must be able to tell them apart.
	const FCigGroupIntake TooBig = CigCustomerGroup::JudgeIntake(4, 0, 3);
	TestEqual(TEXT("Dukkan icin fazla buyuk grup ayri sebep almali"),
		TooBig.Refusal, ECigGroupRefusal::LargerThanShop);
	const FCigGroupIntake Full = CigCustomerGroup::JudgeIntake(2, 5, 5);
	TestEqual(TEXT("Dolu kuyruk ayri sebep almali"), Full.Refusal, ECigGroupRefusal::ShopFull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupSizeRollTest,
	"Cigkofte.Groups.PartySizeStaysInsideItsOwnBounds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigGroupSizeRollTest::RunTest(const FString&)
{
	// Every roll produces a real party. A size of one would be a lone customer
	// wearing group machinery, and a size of five fills the default queue in one
	// arrival - both are things the caller cannot check for and would not notice.
	TSet<int32> Seen;
	for (int32 i = 0; i <= 1000; ++i)
	{
		const float Roll = (float)i / 1000.f;
		const int32 Size = CigCustomerGroup::SizeFromRoll(Roll);
		if (Size < CigCustomerGroup::MinSize || Size > CigCustomerGroup::MaxSize)
		{
			AddError(FString::Printf(TEXT("Grup boyutu sinir disi: %.3f -> %d."), Roll, Size));
			return false;
		}
		Seen.Add(Size);
	}

	// Out-of-range rolls are clamped rather than trusted: a stream that ever
	// returns exactly 1.0 must not produce a fifth size nobody wrote a case for.
	TestTrue(TEXT("Negatif rulo da gecerli boyut vermeli"),
		CigCustomerGroup::SizeFromRoll(-1.f) >= CigCustomerGroup::MinSize);
	TestTrue(TEXT("Bir ve ustu rulo da gecerli boyut vermeli"),
		CigCustomerGroup::SizeFromRoll(2.f) <= CigCustomerGroup::MaxSize);

	// Each size is actually reachable. A weighting that quietly closed one off
	// would pass every bound check above and would mean a size that exists only
	// in the header.
	for (int32 Size = CigCustomerGroup::MinSize; Size <= CigCustomerGroup::MaxSize; ++Size)
	{
		TestTrue(FString::Printf(TEXT("%d kisilik grup mumkun olmali"), Size), Seen.Contains(Size));
	}

	// Small parties are the common case. A distribution weighted the other way
	// would make the four-person party - the one that fills the queue on its own -
	// the normal way the shop gets busy.
	int32 Pairs = 0, Fours = 0;
	for (int32 i = 0; i <= 1000; ++i)
	{
		const int32 Size = CigCustomerGroup::SizeFromRoll((float)i / 1000.f);
		if (Size == 2) { ++Pairs; }
		if (Size == CigCustomerGroup::MaxSize) { ++Fours; }
	}
	TestTrue(FString::Printf(TEXT("Ikili grup en yaygin olmali (%d > %d)"), Pairs, Fours),
		Pairs > Fours);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupWalkoutCostTest,
	"Cigkofte.Groups.AGroupWalkoutCostsMoreThanOneButNotFourTimes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigGroupWalkoutCostTest::RunTest(const FString&)
{
	// A lone customer costs exactly what they cost before groups existed. This is
	// what stops wiring parties in from silently repricing every ordinary
	// walkout in the game.
	TestEqual(TEXT("Tek musteri carpani degismemeli"), CigCustomerGroup::WalkoutRepMult(1), 1.f);
	TestEqual(TEXT("Sifir da tek gibi sayilmali"), CigCustomerGroup::WalkoutRepMult(0), 1.f);

	const float Two = CigCustomerGroup::WalkoutRepMult(2);
	const float Four = CigCustomerGroup::WalkoutRepMult(CigCustomerGroup::MaxSize);

	// More people leaving is worse. Monotonic across the whole range, not just at
	// the ends: a curve that dipped in the middle would make a party of three
	// cheaper to lose than a pair.
	float Prev = CigCustomerGroup::WalkoutRepMult(1);
	for (int32 Size = 2; Size <= CigCustomerGroup::MaxSize; ++Size)
	{
		const float Mult = CigCustomerGroup::WalkoutRepMult(Size);
		TestTrue(FString::Printf(TEXT("%d kisi %d kisiden pahali olmali (%.2f > %.2f)"),
			Size, Size - 1, Mult, Prev), Mult > Prev);
		Prev = Mult;
	}

	// And it is sub-linear. Four people leaving is one bad afternoon several
	// people saw, not four independent bad afternoons - charging the full
	// multiple would let a single unlucky party undo a good day, and reputation
	// is the slowest number in this game to earn back.
	TestTrue(FString::Printf(TEXT("Dortlu grup dort kat olmamali (%.2f < 4)"), Four), Four < 4.f);
	TestTrue(FString::Printf(TEXT("Ikili grup iki kat olmamali (%.2f < 2)"), Two), Two < 2.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupLeavesTogetherTest,
	"Cigkofte.Groups.OneMemberGivingUpTakesTheWholePartyOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigGroupLeavesTogetherTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	UCigProgressionSystem* Prog = Shop.GM->Progression.Get();
	if (!Cust || !Days || !Prog) { AddError(TEXT("Sistemler yok.")); return false; }

	if (!CigGroupTestsOpenEmptyShop(*this, *Cust, *Days)) { return false; }

	// A party of three, and one stranger who arrived after them. The stranger is
	// what makes this a test rather than a tautology: a walkout that emptied the
	// queue would satisfy "the party left" and would be catastrophically wrong.
	const int32 Admitted = Cust->SpawnGroup(3);
	if (Admitted != 3) { AddError(TEXT("Uc kisilik grup kuyruga girmedi.")); return false; }
	ACigkofteCustomer* Loner = Cust->SpawnCustomer();
	if (!Loner) { AddError(TEXT("Tek musteri spawn olmadi.")); return false; }

	TArray<ACigkofteCustomer*> Party;
	for (const TObjectPtr<ACigkofteCustomer>& C : Cust->Queue)
	{
		if (C && C.Get() != Loner) { Party.Add(C.Get()); }
	}
	if (Party.Num() != 3) { AddError(TEXT("Grup uyeleri bulunamadi.")); return false; }

	// They agree about who they are. A party whose members disagree about their
	// own size would charge the wrong walkout cost depending on which of them
	// happened to give up first.
	const int32 Id = Party[0]->GroupId;
	TestTrue(TEXT("Grup kimligi atanmali"), Id >= 0);
	for (ACigkofteCustomer* C : Party)
	{
		TestEqual(TEXT("Grup uyeleri ayni kimlikte olmali"), C->GroupId, Id);
		TestEqual(TEXT("Grup uyeleri ayni boyutta anlasmali"), C->GroupSize, 3);
	}
	TestEqual(TEXT("Yalniz musteri gruba ait olmamali"), Loner->GroupId, -1);
	TestEqual(TEXT("Yalniz musterinin boyutu bir olmali"), Loner->GroupSize, 1);

	// Traits are rolled, so a party can contain an influencer or a VIP - both of
	// which multiply the reputation hit for reasons that have nothing to do with
	// company. Cleared here so the number below is the group multiplier and only
	// the group multiplier; the trait multipliers are somebody else's test.
	for (ACigkofteCustomer* C : Party)
	{
		C->Traits = ECigTrait::None;
		C->bVIP = false;
	}

	const float RepBefore = Prog->Rep;
	const int32 AngryBefore = Prog->TotalAngryCustomers;

	// The middle one runs out of patience. Deliberately not the front one: if the
	// party only left together when its first member gave up, the rule would be
	// about queue position rather than about company.
	Cust->RemoveCustomer(Party[1], /*bAngry=*/true);

	for (ACigkofteCustomer* C : Party)
	{
		TestFalse(TEXT("Grubun tamami kuyruktan cikmali"), Cust->Queue.Contains(C));
	}
	TestTrue(TEXT("Grupla ilgisi olmayan musteri kalmali"), Cust->Queue.Contains(Loner));
	TestEqual(TEXT("Kuyrukta yalniz musteri kalmali"), Cust->Queue.Num(), 1);

	// Three people did leave, so three orders went unserved - the headcount moves
	// by three.
	TestEqual(TEXT("Uc kisi kizgin sayilmali"), Prog->TotalAngryCustomers - AngryBefore, 3);

	// But the reputation hit is charged once, with the group multiplier: worse
	// than one person walking out, and nowhere near three times worse.
	const float RepLost = RepBefore - Prog->Rep;
	const float Single = 6.f;
	TestTrue(FString::Printf(TEXT("Grup kaybi tek musteriden buyuk olmali (%.1f > %.1f)"),
		RepLost, Single), RepLost > Single);
	TestTrue(FString::Printf(TEXT("Grup kaybi uc kat olmamali (%.1f < %.1f)"),
		RepLost, Single * 3.f), RepLost < Single * 3.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupQueueRespectsCapacityTest,
	"Cigkofte.Groups.ArrivingAsAPartyStillCannotOverfillTheQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigGroupQueueRespectsCapacityTest::RunTest(const FString&)
{
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	if (!Cust || !Days) { AddError(TEXT("Sistemler yok.")); return false; }

	const int32 Cap = Cust->MaxQueue();
	Days->StartDay(false);
	Days->OpenShop();

	// A long open day, with the queue actually moving. Marking arrivals is what
	// makes this a day rather than a snapshot: nobody in the test harness walks,
	// so without it the queue fills to the limit in the first few ticks, the
	// spawn gate closes, and the arrival path is never asked another question -
	// which is exactly how the first version of this test passed while the group
	// path never ran once.
	//
	// Parties arrive on their own roll here rather than through SpawnGroup, so
	// what is under test is what the shop actually does.
	int32 WorstSeen = 0;
	for (int32 i = 0; i < 400; ++i)
	{
		for (const TObjectPtr<ACigkofteCustomer>& C : Cust->Queue)
		{
			if (C) { C->bArrived = true; }
		}
		Cust->UpdateSystem(1.f);

		WorstSeen = FMath::Max(WorstSeen, Cust->Queue.Num());
		if (Cust->Queue.Num() > Cap)
		{
			AddError(FString::Printf(TEXT("Kuyruk sinirini asti: %d > %d (tik %d)."),
				Cust->Queue.Num(), Cap, i));
			return false;
		}

		// Checked every tick rather than at the end, because the queue turns over
		// dozens of times across this day and a party admitted in halves would be
		// gone again long before the loop finished. This is the queue-side
		// statement of indivisibility: everyone here who belongs to a party has
		// the rest of their party standing with them.
		TMap<int32, int32> Present;
		for (const TObjectPtr<ACigkofteCustomer>& C : Cust->Queue)
		{
			if (C && C->GroupId >= 0) { Present.FindOrAdd(C->GroupId)++; }
		}
		for (const TObjectPtr<ACigkofteCustomer>& C : Cust->Queue)
		{
			if (!C || C->GroupId < 0) { continue; }
			if (Present[C->GroupId] != C->GroupSize)
			{
				AddError(FString::Printf(
					TEXT("%d numarali grup eksik bekliyor: %d/%d (tik %d)."),
					C->GroupId, Present[C->GroupId], C->GroupSize, i));
				return false;
			}
		}
	}

	// The limit was reached rather than merely never exceeded - a shop nobody
	// came to would satisfy every check above against any bug at all.
	TestEqual(TEXT("Kuyruk gun icinde tam siniri kadar dolmali"), WorstSeen, Cap);

	// And groups actually happened. Without this the whole test passes against a
	// build where the party roll never fires.
	TestTrue(FString::Printf(TEXT("Gun boyunca grup gelmeli (%d kabul, %d cevrildi)"),
		Cust->GroupsAdmitted, Cust->GroupsTurnedAway),
		Cust->GroupsAdmitted > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupHappyDepartureTest,
	"Cigkofte.Groups.AServedMemberLeavesAloneAndTheRestKeepWaiting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigGroupHappyDepartureTest::RunTest(const FString&)
{
	// The mirror image of the walkout test, and the one that is easy to get
	// wrong in the other direction. Everybody leaving together is the rule for
	// giving up; being served is personal. A single shared exit path would make
	// serving the first person in a party of four clear three unserved orders
	// off the counter and send three paying customers home.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	UCigProgressionSystem* Prog = Shop.GM->Progression.Get();
	if (!Cust || !Days || !Prog) { AddError(TEXT("Sistemler yok.")); return false; }

	if (!CigGroupTestsOpenEmptyShop(*this, *Cust, *Days)) { return false; }

	if (Cust->SpawnGroup(3) != 3) { AddError(TEXT("Uc kisilik grup kuyruga girmedi.")); return false; }
	ACigkofteCustomer* Loner = Cust->SpawnCustomer();
	if (!Loner) { AddError(TEXT("Tek musteri spawn olmadi.")); return false; }

	TArray<ACigkofteCustomer*> Party;
	for (const TObjectPtr<ACigkofteCustomer>& C : Cust->Queue)
	{
		if (C && C.Get() != Loner) { Party.Add(C.Get()); }
	}
	if (Party.Num() != 3) { AddError(TEXT("Grup uyeleri bulunamadi.")); return false; }
	const int32 Id = Party[0]->GroupId;

	const float RepBefore = Prog->Rep;
	const int32 AngryBefore = Prog->TotalAngryCustomers;
	const int32 MissedBefore = Days->DayMissed;

	// One of them is served and walks out with their food. Deliberately the
	// middle one again: a rule that only worked for the front of the queue would
	// be about position rather than about being served.
	Cust->RemoveCustomer(Party[1], /*bAngry=*/false);

	TestFalse(TEXT("Servis edilen uye kuyruktan cikmali"), Cust->Queue.Contains(Party[1]));
	TestTrue(TEXT("Birinci arkadas kuyrukta kalmali"), Cust->Queue.Contains(Party[0]));
	TestTrue(TEXT("Ucuncu arkadas kuyrukta kalmali"), Cust->Queue.Contains(Party[2]));
	TestTrue(TEXT("Ilgisiz musteri kuyrukta kalmali"), Cust->Queue.Contains(Loner));

	// The two left behind still belong to each other, and they agree that they
	// are now two. A headcount that stayed at three would price their eventual
	// walkout for a party that is no longer standing there.
	TArray<ACigkofteCustomer*> Remaining = CigGroupTestsPartyInQueue(*Cust, Id);
	TestEqual(TEXT("Geriye iki kisilik grup kalmali"), Remaining.Num(), 2);
	for (ACigkofteCustomer* C : Remaining)
	{
		TestEqual(TEXT("Kalan uyeler yeni boyutta anlasmali"), C->GroupSize, 2);
	}
	// And the one who left is nobody's companion any more, so a later removal
	// cannot reach back into the queue through them.
	TestEqual(TEXT("Ayrilan uye gruptan cikmali"), Party[1]->GroupId, -1);
	TestEqual(TEXT("Ayrilan uyenin boyutu bir olmali"), Party[1]->GroupSize, 1);

	// Being served is not a walkout. None of the punishment counters may move -
	// this is what would break if the happy path ever shared code with the angry
	// one and forgot to check which it was.
	TestEqual(TEXT("Mutlu ayrilis itibari degistirmemeli"), Prog->Rep, RepBefore);
	TestEqual(TEXT("Mutlu ayrilis kizgin sayacini artirmamali"),
		Prog->TotalAngryCustomers, AngryBefore);
	TestEqual(TEXT("Mutlu ayrilis kacan siparis saymamali"), Days->DayMissed, MissedBefore);

	// Serve the second one, and the last member stops being a party at all: one
	// person carrying group machinery would "leave together" with nobody and
	// would be charged a group multiplier for walking out alone.
	Cust->RemoveCustomer(Remaining[0], /*bAngry=*/false);
	ACigkofteCustomer* Last = Remaining[1];
	TestTrue(TEXT("Son uye kuyrukta kalmali"), Cust->Queue.Contains(Last));
	TestEqual(TEXT("Tek kalan uye siradan musteri olmali"), Last->GroupId, -1);
	TestEqual(TEXT("Tek kalan uyenin boyutu bir olmali"), Last->GroupSize, 1);
	TestTrue(TEXT("Ilgisiz musteri hala kuyrukta olmali"), Cust->Queue.Contains(Loner));
	TestEqual(TEXT("Kuyrukta iki musteri kalmali"), Cust->Queue.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupSeatedMemberTest,
	"Cigkofte.Groups.SittingDownAlsoLeavesTheParty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigGroupSeatedMemberTest::RunTest(const FString&)
{
	// A dine-in customer does not leave when they are served, they take a chair.
	// That is a second way out of the queue, and it went through its own code
	// path that removed them from the queue directly - so the party they had been
	// waiting with kept counting them from across the room.
	//
	// Driven through the real ServeFront rather than by calling the seating step,
	// because the bug lived in the branch a served customer actually takes.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	UCigOrderSystem* Orders = Shop.GM->Orders.Get();
	UCigWorldBuilder* WB = Shop.GM->WorldBuilder.Get();
	UCigRandomSubsystem* Rng = Shop.GI ? Shop.GI->GetSubsystem<UCigRandomSubsystem>() : nullptr;
	if (!Cust || !Days || !Orders || !WB || !Rng) { AddError(TEXT("Sistemler yok.")); return false; }

	WB->BuildWorld();
	if (WB->Seats.Num() == 0) { AddError(TEXT("Sandalye yok.")); return false; }

	if (!CigGroupTestsOpenEmptyShop(*this, *Cust, *Days)) { return false; }
	if (Cust->SpawnGroup(3) != 3) { AddError(TEXT("Uc kisilik grup kuyruga girmedi.")); return false; }

	ACigkofteCustomer* Front = Cust->FrontCustomer();
	if (!Front) { AddError(TEXT("On musteri yok.")); return false; }
	const int32 Id = Front->GroupId;
	if (Id < 0) { AddError(TEXT("Grup kimligi atanmadi.")); return false; }

	// The order and the wrap are pinned rather than rolled, because every branch
	// this test does not want to enter is guarded by a number: a tip is only
	// rolled above quality 60, a new regular only above accuracy 80, and a family
	// customer only with the trait. Each of those would take a draw from the
	// shared stream and move the seating roll below out from under us.
	Front->bArrived = true;
	Front->Traits = ECigTrait::None;
	Front->bVIP = false;
	Front->LoyalId = -1;
	Front->Spec = FCigOrderSpec();
	Front->Spec.Spice = ECigSpice::CokAci;
	Front->Spec.Portion = 1;
	Front->Spec.bPacked = false;   // dine-in, which is what makes a chair possible

	Orders->Wrap = FCigWrapBuild();
	Orders->Wrap.bActive = true;
	Orders->Wrap.bWrapped = true;
	Orders->Wrap.Portions = 1;
	Orders->Wrap.DoughQuality = 50.f;   // under 60: no tip roll
	Orders->Wrap.Spice = ECigSpice::AzAci;  // two steps off: accuracy stays under 80
	Orders->Wrap.bPacked = false;

	const FCigOrderScore Score = UCigOrderSystem::ScoreWrap(Orders->Wrap, Front->Spec, 0.f);
	if (Score.Accuracy >= 80.f)
	{
		AddError(FString::Printf(
			TEXT("Test kurulumu bozuk: dogruluk %.1f, sadakat cekimi engellenmiyor."), Score.Accuracy));
		return false;
	}

	// With those branches closed, the next draw from the stream is the seating
	// roll itself. Rather than hard-coding a magic seed, probe for one that seats
	// the customer and then rewind the stream to just before the probe.
	int32 Seed = 0;
	bool bFound = false;
	for (int32 Candidate = 1; Candidate <= 64 && !bFound; ++Candidate)
	{
		Rng->SeedWith(Candidate);
		if (Rng->FRand() < 0.7f) { Seed = Candidate; bFound = true; }
	}
	if (!bFound) { AddError(TEXT("Oturma cekimini gecen tohum bulunamadi.")); return false; }
	Rng->SeedWith(Seed);

	Cust->ServeFront();

	if (Cust->Seated.Num() != 1)
	{
		AddError(FString::Printf(TEXT("Musteri oturmadi (oturan %d)."), Cust->Seated.Num()));
		return false;
	}
	TestTrue(TEXT("Oturan kisi servis edilen uye olmali"), Cust->Seated[0].Customer.Get() == Front);
	TestFalse(TEXT("Oturan uye kuyrukta kalmamali"), Cust->Queue.Contains(Front));

	// The point of the test: a chair is as much an exit from the queue as the
	// door is.
	TestEqual(TEXT("Oturan uye gruptan cikmali"), Front->GroupId, -1);
	TestEqual(TEXT("Oturan uyenin boyutu bir olmali"), Front->GroupSize, 1);

	const TArray<ACigkofteCustomer*> Remaining = CigGroupTestsPartyInQueue(*Cust, Id);
	TestEqual(TEXT("Kuyrukta iki arkadas kalmali"), Remaining.Num(), 2);
	for (ACigkofteCustomer* C : Remaining)
	{
		TestEqual(TEXT("Kalan arkadaslar iki kisi oldugunda anlasmali"), C->GroupSize, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupPooledResetTest,
	"Cigkofte.Groups.ARecycledCustomerArrivesWithNoOldParty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigGroupPooledResetTest::RunTest(const FString&)
{
	// Customers are pooled, so the actor that walks in this afternoon is one that
	// already walked out this morning. A body that kept its old GroupId would
	// make a lone arrival "leave together" with three strangers who happen to
	// still be carrying the same number - and it would do it silently, hours
	// after the party that produced the id had gone home.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	if (!Cust || !Days) { AddError(TEXT("Sistemler yok.")); return false; }

	if (!CigGroupTestsOpenEmptyShop(*this, *Cust, *Days)) { return false; }

	if (Cust->SpawnGroup(2) != 2) { AddError(TEXT("Ikili grup kuyruga girmedi.")); return false; }
	ACigkofteCustomer* Member = Cust->FrontCustomer();
	if (!Member || Member->GroupId < 0) { AddError(TEXT("Grup uyesi yok.")); return false; }
	const int32 OldId = Member->GroupId;

	// Out through the real recycling path rather than by resetting the fields by
	// hand: what is under test is that the pool hands back a clean customer, and
	// asserting on an actor this test cleaned itself would prove nothing.
	Member->Deactivate();
	Cust->UpdateSystem(0.f);
	TestFalse(TEXT("Havuza giden uye kuyrukta kalmamali"), Cust->Queue.Contains(Member));

	ACigkofteCustomer* Reused = Cust->SpawnCustomer();
	if (!Reused) { AddError(TEXT("Musteri spawn olmadi.")); return false; }
	if (Reused != Member)
	{
		AddError(TEXT("Havuzdaki aktor yeniden kullanilmadi; test havuz yolunu olcemiyor."));
		return false;
	}

	TestEqual(TEXT("Yeniden kullanilan musteri gruba ait olmamali"), Reused->GroupId, -1);
	TestEqual(TEXT("Yeniden kullanilan musterinin boyutu bir olmali"), Reused->GroupSize, 1);
	TestNotEqual(TEXT("Eski grup kimligi tasinmamali"), Reused->GroupId, OldId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigGroupAtomicArrivalTest,
	"Cigkofte.Groups.APartyThatCannotAllArriveLeavesNobodyBehind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCigGroupAtomicArrivalTest::RunTest(const FString&)
{
	// Indivisibility has two enemies and only one of them is the queue. The other
	// is the machinery: acquiring a body can fail, and a party that spawned its
	// members one at a time left whoever had already arrived standing in the
	// queue with an order. That is the same broken half-party the capacity rule
	// exists to prevent, arriving through a door nobody was watching.
	FCigTestShop Shop;
	if (!Shop.Build(*this)) { return false; }
	UCigCustomerSystem* Cust = Shop.GM->Customers.Get();
	UCigDaySystem* Days = Shop.GM->Days.Get();
	UCigRandomSubsystem* Rng = Shop.GI ? Shop.GI->GetSubsystem<UCigRandomSubsystem>() : nullptr;
	if (!Cust || !Days || !Rng) { AddError(TEXT("Sistemler yok.")); return false; }

	if (!CigGroupTestsOpenEmptyShop(*this, *Cust, *Days)) { return false; }

	const int32 AdmittedBefore = Cust->GroupsAdmitted;
	const int64 DrawsBefore = Rng->DrawCount();

	// The second body is refused. One has already been taken by then, which is
	// exactly the state a naive implementation cannot undo.
	Cust->FailAcquireAfterForTest = 1;
	const int32 Arrived = Cust->SpawnGroup(3);
	Cust->FailAcquireAfterForTest = -1;

	TestEqual(TEXT("Eksik gelen grup hic gelmemis sayilmali"), Arrived, 0);
	TestEqual(TEXT("Yarim grup kuyrukta kalmamali"), Cust->Queue.Num(), 0);
	TestEqual(TEXT("Basarisiz grup kabul sayilmamali"), Cust->GroupsAdmitted, AdmittedBefore);

	// Nothing externally visible happened either. The draw count is the sharpest
	// statement of that: a member who got as far as having an order rolled would
	// have moved the shared stream, and every later roll in the run would land
	// somewhere else - a determinism bug produced by a failed spawn.
	TestEqual(TEXT("Basarisiz grup rastgele akisi ilerletmemeli"),
		(int32)(Rng->DrawCount() - DrawsBefore), 0);

	// And the bodies came back rather than leaking: the next party gets them.
	const int32 Second = Cust->SpawnGroup(3);
	TestEqual(TEXT("Sonraki grup normal gelmeli"), Second, 3);
	TestEqual(TEXT("Sonraki grup kuyrukta olmali"), Cust->Queue.Num(), 3);

	const int32 Id = Cust->Queue.Num() > 0 && Cust->Queue[0] ? Cust->Queue[0]->GroupId : -1;
	TestTrue(TEXT("Sonraki gruba kimlik atanmali"), Id >= 0);
	TestEqual(TEXT("Sonraki grubun tamami ayni kimlikte olmali"),
		CigGroupTestsPartyInQueue(*Cust, Id).Num(), 3);
	return true;
}

#endif
