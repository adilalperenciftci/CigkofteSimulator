#pragma once

#include "CoreMinimal.h"

// People who arrive together.
//
// Until now every customer was a stranger who happened to be standing behind
// another stranger. The one thing in the shop that gestured at company was the
// Family trait, and what it actually did was shorten the timer before the next
// unrelated person spawned - a queue that fills faster, not a party.
//
// A group is worth having because it makes the queue's capacity mean something.
// A single customer asks "is there room for me", and the answer is a comparison
// against a number nobody notices. A party of four asks the same question and
// the answer is visible: either four people walk in, or four people walk away
// together. That is the same limit, finally legible.
//
// The rule this file exists to hold is that a group is not divisible. Three
// friends do not become two friends in the queue and one on the pavement. The
// alternative - admit whoever fits - is cheaper to implement and reads as a bug
// in every screenshot: a group animation walks up, and then part of it turns
// around for no reason the player can see.
//
// Nothing here touches a world, a queue or an actor. It answers questions about
// integers, which is what makes the answers arguable.

// Why a party did not come in. Named rather than a bool, because the shop tells
// the player something different for each, and "the queue is full" when the
// queue has three free spaces and a party of four is the kind of message that
// teaches players to stop reading them.
enum class ECigGroupRefusal : uint8
{
	None = 0,
	// Not one free space. The same answer a lone customer would get.
	ShopFull,
	// There is room, just not enough of it. The party stays together and leaves.
	WouldSplit,
	// The party is bigger than the queue could ever hold, empty or not. This is
	// not a busy shop turning somebody away; it is a shop that cannot serve this
	// party at all, and if it ever fires it is a balance problem rather than a
	// busy afternoon.
	LargerThanShop
};

struct FCigGroupIntake
{
	// How many actually join the queue. Always either the whole party or none of
	// it - there is no partial admission, on purpose.
	int32 Admitted = 0;
	ECigGroupRefusal Refusal = ECigGroupRefusal::ShopFull;

	bool IsAdmitted() const { return Refusal == ECigGroupRefusal::None && Admitted > 0; }
};

namespace CigCustomerGroup
{
	// A party is two to four. Four is already most of a five-deep queue, which is
	// the point: the size that makes capacity legible is also the size that must
	// not be casually exceeded, or a single party locks the counter for a day.
	inline constexpr int32 MinSize = 2;
	inline constexpr int32 MaxSize = 4;

	// How often an arrival is a party rather than one person. Low, because the
	// queue holds five: a shop where half the arrivals are parties is a shop with
	// two customers in it.
	inline constexpr float GroupChance = 0.18f;

	// Party size from one roll in [0, 1). Weighted small - a pair is the common
	// case and a four is an event.
	//
	// Takes the roll rather than a stream, so the same numbers can be asked for
	// twice and the distribution can be asserted rather than believed.
	int32 SizeFromRoll(float Roll01);

	// Whether this party comes in.
	//
	// QueueUsed is how many are standing in the queue now; QueueCapacity is what
	// the queue can hold, which moves with the İkinciKasa upgrade. Both are passed
	// in rather than looked up, so this is callable with no shop.
	FCigGroupIntake JudgeIntake(int32 GroupSize, int32 QueueUsed, int32 QueueCapacity);

	// A party leaves together when one of them gives up. What that costs.
	//
	// Not linear. Four people walking out is worse than one person walking out,
	// and it is not four times worse: it is one bad afternoon that several people
	// witnessed, not four independent bad afternoons. Charging the full multiple
	// would make a single unlucky party undo a good day, and reputation is the
	// slowest number in the game to earn back.
	//
	// A party of one - a normal customer - costs exactly what it costs today, so
	// wiring groups in cannot silently reprice ordinary walkouts.
	float WalkoutRepMult(int32 GroupSize);
}
