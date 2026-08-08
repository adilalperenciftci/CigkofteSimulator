#include "Orders/CigOrderTicket.h"

#include "Core/CigText.h"

namespace CigOrderTicket
{
	FString ToppingCode(ECigTopping Topping)
	{
		switch (Topping)
		{
		case ECigTopping::Marul:     return CigText::Get(TEXT("ticket.topping.marul"));
		case ECigTopping::Maydanoz:  return CigText::Get(TEXT("ticket.topping.maydanoz"));
		case ECigTopping::Domates:   return CigText::Get(TEXT("ticket.topping.domates"));
		case ECigTopping::Tursu:     return CigText::Get(TEXT("ticket.topping.tursu"));
		case ECigTopping::Sogan:     return CigText::Get(TEXT("ticket.topping.sogan"));
		case ECigTopping::Limon:     return CigText::Get(TEXT("ticket.topping.limon"));
		case ECigTopping::NarEksisi: return CigText::Get(TEXT("ticket.topping.nareksisi"));
		default:                     return FString();
		}
	}

	FString Format(const FCigOrderSpec& Spec, bool bVIP)
	{
		FString Line;

		if (bVIP)
		{
			Line += CigText::Get(TEXT("customer.tag.vip"));
		}

		Line += CigSpiceNameAscii(Spec.Spice);
		Line += Spec.Portion >= 2
			? *CigText::Get(TEXT("customer.tag.portion"))
			: *CigText::Get(TEXT("customer.tag.wrap"));

		// The part that was missing. Codes rather than names, because seven
		// spelled-out toppings is a paragraph over somebody's head.
		TArray<FString> Codes;
		for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
		{
			const ECigTopping T = (ECigTopping)i;
			if (Spec.WantsTopping(T))
			{
				const FString Code = ToppingCode(T);
				if (!Code.IsEmpty())
				{
					Codes.Add(Code);
				}
			}
		}

		Line += TEXT(" ");
		if (Codes.Num() > 0)
		{
			Line += FString::Join(Codes, TEXT("+"));
		}
		else
		{
			// Said rather than left blank. An empty stretch where the toppings
			// belong reads as a label that has not finished loading; "plain" is
			// an order.
			Line += CigText::Get(TEXT("ticket.plain"));
		}

		if (Spec.Side != ECigSide::Yok)
		{
			Line += TEXT(" ") + CigSideName(Spec.Side);
		}
		if (Spec.bWantsAyran)
		{
			Line += CigText::Get(TEXT("customer.tag.ayran"));
		}
		if (Spec.bPacked)
		{
			Line += CigText::Get(TEXT("customer.tag.packed"));
		}

		return Line;
	}
}
