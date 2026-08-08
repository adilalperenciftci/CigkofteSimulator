#pragma once

#include "CoreMinimal.h"
#include "Core/CigkofteTypes.h"

// The line above a customer's head.
//
// It used to carry spice, portion, ayran, takeaway and a VIP prefix - and not
// the toppings, which are worth 30 of the 100 points UCigOrderSystem::ScoreWrap
// awards. More than the spice (25), more than the portions (15). The single
// largest thing the player is graded on was the one thing the customer never
// said out loud.
//
// It was not invisible: DrawCustomerPanel lists the wanted toppings in full.
// But only for FrontCustomer(), so a queue of three has two whose orders cannot
// be read at all, and the panel sits bottom-right while the player is looking at
// a station. Reading it means looking away from the work.
//
// So the label becomes a real ticket. The problem that kept it short is width -
// seven toppings spelled out is a paragraph over somebody's head - and the
// answer is a code per topping.
//
// Which makes uniqueness the thing to get right. Two Turkish toppings start with
// M (Marul, Maydanoz) and two English ones with L (Lettuce, Lemon), so a
// first-letter scheme is ambiguous in both languages and differently ambiguous
// in each. The codes are therefore authored per language in Config/Text, and
// their uniqueness is a test rather than a hope: a collision would not crash
// anything, it would just quietly tell the player the wrong order.

namespace CigOrderTicket
{
	// The short code for a topping, in the current language. Two characters is
	// the target; the table decides, because a language may need otherwise.
	FString ToppingCode(ECigTopping Topping);

	// The full ticket line. Toppings appear as codes joined by "+", so the
	// player can see the whole order without the label growing past the body it
	// sits above.
	//
	// An order with no toppings says so rather than falling silent, because a
	// blank stretch where the toppings go reads as "not loaded yet" rather than
	// as "plain".
	FString Format(const FCigOrderSpec& Spec, bool bVIP);
}
