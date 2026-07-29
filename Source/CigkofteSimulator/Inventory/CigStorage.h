#pragma once

#include "CoreMinimal.h"
#include "Core/CigkofteTypes.h"

// What the shop can hold, and where.
//
// Stock has been an unbounded integer per item: order fifty lettuces on day one
// and they all fit, because nothing said otherwise. That made the supplier tab a
// question of money and nothing else, and it made the big fridge - an upgrade
// the player pays for - worth only a slower spoilage curve.
//
// Two kinds of space, and they behave differently on purpose.
//
// Dry goods get a generous per-item shelf. It exists so a number cannot run away
// and rarely binds; bulgur is not the interesting constraint in a çiğköfte shop.
//
// Cold goods share one pool, because a fridge is one volume. That is the whole
// point of the rule: more lettuce means less ayran, and the choice is made at
// the moment of ordering rather than discovered at the counter. The big fridge
// upgrade more than doubles it, which is the first time that upgrade has bought
// anything a player can plan around.
//
// Pure, like the mix diagnosis and the topping table, and for the same reason:
// which goods are cold and how much room there is are balance decisions that
// will move. See Tests/CigStorageTests.cpp.

enum class ECigStorageClass : uint8
{
	Dry,
	Cold,
	// The tap. Water is not stored and not ordered, so it has no limit and no
	// place in either pool - excluding it is what stops the cold pool being
	// quietly filled by something that does not need refrigerating.
	Unlimited
};

namespace CigStorage
{
	inline constexpr int32 DryCapacityPerItem = 60;

	// Sized against the shop's own starting stock, which is 42 cold units across
	// seven perishables. Forty was tried first and the game began two over its
	// own limit with every order refused on day one - the rule failing before the
	// player had done anything. Sixty leaves room for roughly one delivery on
	// opening day, which is the pressure this is meant to create rather than a
	// wall. Tests/CigStorageTests.cpp pins the relationship so a change to
	// Stock.csv cannot quietly reintroduce it.
	inline constexpr int32 ColdCapacityBase = 60;
	inline constexpr int32 ColdCapacityBigFridge = 120;

	inline ECigStorageClass ClassOf(int32 Item)
	{
		switch (Item)
		{
		case (int32)ECigIngredient::Su:
			return ECigStorageClass::Unlimited;

		// Anything that wilts, sours or melts.
		case CigStockMarul:
		case CigStockAyran:
		case CigStockMaydanoz:
		case CigStockDomates:
		case CigStockTursu:
		case CigStockSogan:
		case CigStockLimon:
		case CigStockIcliKofte:
		case CigStockCorba:
		case CigStockKunefe:
			return ECigStorageClass::Cold;

		// Bulgur, isot, paste, spice, pomegranate molasses, flatbread, glasses.
		default:
			return ECigStorageClass::Dry;
		}
	}

	inline int32 ColdCapacity(bool bBigFridge)
	{
		return bBigFridge ? ColdCapacityBigFridge : ColdCapacityBase;
	}

	// How much of the cold pool is in use.
	inline int32 ColdUsed(const int32 Stock[CigStockCount])
	{
		int32 Used = 0;
		for (int32 i = 0; i < CigStockCount; ++i)
		{
			if (ClassOf(i) == ECigStorageClass::Cold)
			{
				Used += FMath::Max(0, Stock[i]);
			}
		}
		return Used;
	}

	// How many more of this item the shop has room for. Zero means an order
	// would have nowhere to go.
	//
	// Returned as a number rather than a bool so the caller can say "room for 6"
	// instead of "no": a delivery that half fits is the common case once the
	// fridge is tight, and refusing it outright would be the wrong answer as
	// well as an unhelpful message.
	inline int32 RoomFor(const int32 Stock[CigStockCount], int32 Item, bool bBigFridge)
	{
		if (Item < 0 || Item >= CigStockCount)
		{
			return 0;
		}
		switch (ClassOf(Item))
		{
		case ECigStorageClass::Unlimited:
			return MAX_int32;
		case ECigStorageClass::Cold:
			return FMath::Max(0, ColdCapacity(bBigFridge) - ColdUsed(Stock));
		default:
			return FMath::Max(0, DryCapacityPerItem - FMath::Max(0, Stock[Item]));
		}
	}
}
