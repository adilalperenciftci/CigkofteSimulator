// The ticket over a customer's head.
//
// The rule worth testing is not the wording, it is that the topping codes are
// unambiguous - in every language. Two Turkish toppings start with M (Marul,
// Maydanoz) and two English ones with L (Lettuce, Lemon), so the obvious scheme
// is broken in both and broken differently in each. A collision would not crash
// anything; it would quietly tell the player the wrong order, and they would
// lose 30 of the 100 points ScoreWrap awards without ever seeing why.

#include "Misc/AutomationTest.h"
#include "Orders/CigOrderTicket.h"
#include "Core/CigText.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigOrderTicketCodesTest,
	"Cigkofte.Orders.ToppingCodesAreUnambiguousInEveryLanguage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigOrderTicketCodesTest::RunTest(const FString&)
{
	const int32 Restore = CigText::GetLanguage();

	for (int32 Lang = 0; Lang < CigText::LanguageCount(); ++Lang)
	{
		CigText::SetLanguage(Lang);
		const FString Where = FString::Printf(TEXT("dil=%d"), Lang);

		TSet<FString> Seen;
		for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
		{
			const FString Code = CigOrderTicket::ToppingCode((ECigTopping)i);

			// A missing code would leave a topping off the ticket entirely,
			// which is the same failure as an ambiguous one but quieter.
			TestFalse(*(TEXT("Kod bos olmamali - ") + Where + FString::Printf(TEXT(" sos=%d"), i)),
				Code.IsEmpty());

			// The whole point.
			TestFalse(*(TEXT("Kod benzersiz olmali - ") + Where + TEXT(" kod=") + Code),
				Seen.Contains(Code));
			Seen.Add(Code);

			// Short enough to sit above a head. Not a style rule: the label is
			// one line of world text and seven long codes would overrun the body.
			TestTrue(*(TEXT("Kod kisa olmali - ") + Where + TEXT(" kod=") + Code),
				Code.Len() <= 4);
		}
	}

	CigText::SetLanguage(Restore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigOrderTicketContentTest,
	"Cigkofte.Orders.TheTicketCarriesWhatThePlayerIsScoredOn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigOrderTicketContentTest::RunTest(const FString&)
{
	const int32 Restore = CigText::GetLanguage();
	CigText::SetLanguage(0);

	FCigOrderSpec Spec;
	Spec.Spice = ECigSpice::CokAci;
	Spec.Portion = 2;
	Spec.SetTopping(ECigTopping::Domates, true);
	Spec.SetTopping(ECigTopping::Sogan, true);
	Spec.bWantsAyran = true;
	Spec.Side = ECigSide::Cay;

	const FString Line = CigOrderTicket::Format(Spec, /*bVIP=*/false);

	// Toppings are 30 of the 100 points ScoreWrap awards - the largest single
	// component - and they were the thing the label left out.
	TestTrue(TEXT("Istenen sos fiste olmali"),
		Line.Contains(CigOrderTicket::ToppingCode(ECigTopping::Domates)));
	TestTrue(TEXT("Ikinci istenen sos da fiste olmali"),
		Line.Contains(CigOrderTicket::ToppingCode(ECigTopping::Sogan)));

	// And an unwanted one must not appear, or the player adds what nobody asked
	// for and loses points for the extra.
	TestFalse(TEXT("Istenmeyen sos fiste olmamali"),
		Line.Contains(CigOrderTicket::ToppingCode(ECigTopping::Tursu)));

	// A plain order says so rather than leaving a gap that reads as a label
	// which has not finished loading.
	FCigOrderSpec Plain;
	Plain.Spice = ECigSpice::Orta;
	const FString PlainLine = CigOrderTicket::Format(Plain, false);
	TestTrue(TEXT("Sossuz siparis kendini soylemeli"),
		PlainLine.Contains(CigText::Get(TEXT("ticket.plain"))));
	TestFalse(TEXT("Sossuz siparis bos olmamali"), PlainLine.IsEmpty());

	// VIP still leads, because it changes what the order is worth.
	const FString VipLine = CigOrderTicket::Format(Spec, /*bVIP=*/true);
	TestTrue(TEXT("VIP fisin basinda olmali"),
		VipLine.StartsWith(CigText::Get(TEXT("customer.tag.vip"))));

	CigText::SetLanguage(Restore);
	return true;
}

#endif
