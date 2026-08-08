// What a customer's body says, and which signal wins when two are true.
//
// The interesting part is not the animation - it is the precedence. A customer
// can be leaving *and* out of patience, seated *and* holding a stale patience
// value, walking in *and* carrying whatever PatienceFrac they were constructed
// with. Each of those pairs has one right answer and the wrong one puts a lie on
// screen: a body chasing a sale that is already gone, or a body panicking about
// a clock that is not running.

#include "Misc/AutomationTest.h"
#include "Customers/CigCustomerReadout.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCigCustomerReadoutInput Queued(float Frac)
	{
		FCigCustomerReadoutInput In;
		In.PatienceFrac = Frac;
		In.bArrived = true;
		return In;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCustomerReadoutBandsTest,
	"Cigkofte.Customers.PatienceReadsAsAShapeBeforeItRunsOut",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigCustomerReadoutBandsTest::RunTest(const FString&)
{
	TestTrue(TEXT("Dolu sabir beklemeli"),
		CigCustomerReadout::Resolve(Queued(1.f)).Pose == ECigCustomerPose::Waiting);
	TestEqual(TEXT("Beklerken aciliyet olmamali"),
		CigCustomerReadout::Resolve(Queued(1.f)).Urgency, 0.f);

	// Just inside each band rather than in the middle of it: a threshold that
	// moved by a hair would still be caught.
	const float JustUnderRestless = CigCustomerReadout::RestlessBelow - 0.001f;
	TestTrue(TEXT("Yarinin altinda huzursuz olmali"),
		CigCustomerReadout::Resolve(Queued(JustUnderRestless)).Pose == ECigCustomerPose::Restless);
	TestTrue(TEXT("Tam esikte henuz huzursuz olmamali"),
		CigCustomerReadout::Resolve(Queued(CigCustomerReadout::RestlessBelow)).Pose == ECigCustomerPose::Waiting);

	const float JustUnderLeaving = CigCustomerReadout::AboutToLeaveBelow - 0.001f;
	TestTrue(TEXT("Son dilimde gitmek uzere olmali"),
		CigCustomerReadout::Resolve(Queued(JustUnderLeaving)).Pose == ECigCustomerPose::AboutToLeave);
	TestTrue(TEXT("Tam esikte henuz gitmek uzere olmamali"),
		CigCustomerReadout::Resolve(Queued(CigCustomerReadout::AboutToLeaveBelow)).Pose == ECigCustomerPose::Restless);

	// The bands hand over at full strength. If restless faded out towards its
	// boundary, the body would get calmer just as the situation got worse.
	const FCigCustomerReadout EndOfRestless =
		CigCustomerReadout::Resolve(Queued(CigCustomerReadout::AboutToLeaveBelow + 0.001f));
	TestTrue(TEXT("Huzursuzluk banti sonunda dolmali"), EndOfRestless.Urgency > 0.99f);

	// And urgency keeps climbing after the handover rather than restarting low.
	const FCigCustomerReadout Nearly = CigCustomerReadout::Resolve(Queued(0.f));
	TestTrue(TEXT("Sabir bitince aciliyet tepede olmali"), Nearly.Urgency > 0.99f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCustomerReadoutPrecedenceTest,
	"Cigkofte.Customers.ABodyNeverPlaysUrgencyItDoesNotHave",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigCustomerReadoutPrecedenceTest::RunTest(const FString&)
{
	// Leaving with no patience left - the state a customer is in the instant
	// they give up. They are walking out; nothing the player does reaches them,
	// and a body still straining would send somebody chasing a lost sale.
	FCigCustomerReadoutInput Gone = Queued(0.f);
	Gone.bLeaving = true;
	const FCigCustomerReadout GoneOut = CigCustomerReadout::Resolve(Gone);
	TestTrue(TEXT("Cikan musteri cikiyor okunmali"), GoneOut.Pose == ECigCustomerPose::Leaving);
	TestEqual(TEXT("Cikan musteride aciliyet olmamali"), GoneOut.Urgency, 0.f);

	// Leaving outranks seated too, not just the bands.
	FCigCustomerReadoutInput LeavingSeat = Queued(1.f);
	LeavingSeat.bSeated = true;
	LeavingSeat.bLeaving = true;
	TestTrue(TEXT("Cikmak oturmanin onunde gelmeli"),
		CigCustomerReadout::Resolve(LeavingSeat).Pose == ECigCustomerPose::Leaving);

	// Seated with a stale low patience. UCigCustomerSystem only counts patience
	// down for the queue, so this number is whatever it was when they sat - and
	// drawing panic from it would be reading a stopped clock.
	FCigCustomerReadoutInput Sitting = Queued(0.05f);
	Sitting.bSeated = true;
	const FCigCustomerReadout SittingOut = CigCustomerReadout::Resolve(Sitting);
	TestTrue(TEXT("Oturan musteri oturuyor okunmali"), SittingOut.Pose == ECigCustomerPose::Seated);
	TestEqual(TEXT("Oturan musteride aciliyet olmamali"), SittingOut.Urgency, 0.f);

	// Walking in with a low value for the same reason: the clock has not started.
	FCigCustomerReadoutInput Walking;
	Walking.PatienceFrac = 0.05f;
	Walking.bArrived = false;
	const FCigCustomerReadout WalkingOut = CigCustomerReadout::Resolve(Walking);
	TestTrue(TEXT("Varmamis musteri yaklasiyor okunmali"),
		WalkingOut.Pose == ECigCustomerPose::Approaching);
	TestEqual(TEXT("Varmamis musteride aciliyet olmamali"), WalkingOut.Urgency, 0.f);

	// A street passer-by is scenery: no order, no urgency, whatever else is set.
	FCigCustomerReadoutInput Passer = Queued(0.f);
	Passer.bAmbient = true;
	const FCigCustomerReadout PasserOut = CigCustomerReadout::Resolve(Passer);
	TestTrue(TEXT("Yoldan gecen ambient okunmali"), PasserOut.Pose == ECigCustomerPose::Ambient);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigCustomerReadoutTotalTest,
	"Cigkofte.Customers.EveryStateResolvesToExactlyOnePose",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigCustomerReadoutTotalTest::RunTest(const FString&)
{
	// Sixteen flag combinations across five patience values. The property is that
	// the function is total and never contradicts itself - urgency without a pose
	// that carries urgency would animate a body for no stated reason.
	const float Fracs[] = { 1.f, 0.6f, 0.35f, 0.1f, 0.f };
	for (float Frac : Fracs)
	{
		for (int32 i = 0; i < 16; ++i)
		{
			FCigCustomerReadoutInput In;
			In.PatienceFrac = Frac;
			In.bAmbient = (i & 1) != 0;
			In.bLeaving = (i & 2) != 0;
			In.bSeated = (i & 4) != 0;
			In.bArrived = (i & 8) != 0;

			const FCigCustomerReadout Out = CigCustomerReadout::Resolve(In);
			const FString Where = FString::Printf(TEXT("frac=%.2f bayrak=%d"), Frac, i);

			TestTrue(*(TEXT("Aciliyet 0..1 araliginda olmali - ") + Where),
				Out.Urgency >= 0.f && Out.Urgency <= 1.f);

			// Only the two urgent poses may carry urgency. Anything else with a
			// non-zero value would be a body worrying without a reason on screen.
			const bool bUrgentPose = Out.Pose == ECigCustomerPose::Restless
				|| Out.Pose == ECigCustomerPose::AboutToLeave;
			if (!bUrgentPose)
			{
				TestEqual(*(TEXT("Acil olmayan duruşta aciliyet sifir olmali - ") + Where),
					Out.Urgency, 0.f);
			}

		}
	}
	return true;
}

#endif
