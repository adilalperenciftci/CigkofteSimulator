#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "CigPricingSystem.generated.h"

// Per-product pricing and the demand it produces.
//
// The list prices used to sit as literals in the payment path
// (Customers/CigCustomerSystem.cpp). They live in Config/Balance/Pricing.csv
// now, and what the player owns on top of them is a markup per product. Keeping
// the markup separate from the list price means a balance change to the table
// does not silently undo the player's own pricing decisions in an old save.
UCLASS()
class UCigPricingSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnDayStart(int32 Day) override;

	// Footfall multiplier for a price ratio. Pulled out as a free function
	// because it is the whole model in one line and the only part worth testing
	// on its own (see Tests/CigPricingTests.cpp).
	//
	// Demand follows (price / income) ^ -Esneklik. Dividing by income is what
	// makes the same lira sting less in a wealthier street. The clamp keeps a
	// giveaway price from summoning an unservable queue, and a silly price from
	// emptying the shop so completely that the day cannot be played.
	static float TalepCarpani(float FiyatCarpani, float Esneklik, float GelirCarpani);

	// The player's markup on a product, 1.0 = list price.
	float Carpan(int32 Urun) const;

	// The shop-wide cheap/normal/expensive policy, as a multiplier.
	float PolitikaCarpani() const;

	// What a customer is actually charged, as a multiple of list price.
	//
	// Two knobs move the price: the per-product markup and the shop-wide policy.
	// Only the markup used to be visible to anything that judged the price, so
	// switching the shop to "expensive" raised every bill by a quarter while
	// demand, the reviews and the price shown on the tablet all carried on as if
	// nothing had changed. Everything that looks at price goes through here.
	float EtkinCarpan(int32 Urun) const;

	// Price actually charged, list price times the effective markup.
	int32 Fiyat(int32 Urun) const;

	// The effective markup measured against what the street charges. 1.0 means
	// priced level with the rivals.
	float SokakOrani(int32 Urun) const;

	// A one-to-five star opinion of the price, from that ratio. Pure, so the
	// curve can be checked without a shop (see Tests/CigPricingTests.cpp).
	static float FiyatPuani(float Oran);

	// Priced at or below the street without being cheap enough to look wrong.
	// This is what earns the small reputation bonus for being good value; it
	// used to be handed out for the "cheap" policy setting alone, regardless of
	// what the shop was really charging.
	bool UygunFiyatli() const;

	// Moves the markup by Delta, clamped to the product's own range. The tablet
	// steps in units of CarpanAdimi.
	void FiyatDegistir(int32 Urun, float Delta);
	static constexpr float CarpanAdimi = 0.05f;

	// Footfall multiplier across the menu, weighted towards the wrap because
	// that is what people come for. Read by the customer system.
	float GunlukTalepCarpani() const;

	// Average income of the streets that are open. Districts join as they
	// unlock, so the neighbourhood grows richer along with the map.
	float MahalleGeliri() const;

	// What the rivals charge, as a multiple of list price. Shown on the tablet
	// so the player can price against it.
	float RakipOrtalamaCarpani() const;

	// True while the wrap is priced far enough above the street to read as
	// expensive, or far enough below to look like corners are being cut. The
	// review system turns these into the comments people actually leave.
	bool PahaliGoruluyor() const;
	bool SuphelUcuzGoruluyor() const;

	// Markup per product, indexed by the CigUrun* constants.
	float Carpanlar[CigUrunCount];

	UCigPricingSystem();

private:
	// Rivals answer a price move, but only after they have seen it for a few
	// days - an instant reaction would make undercutting them pointless.
	float RakipTepkiGecikmesi = 0.f;
	float RakipHedefCarpani = 1.f;

	void RakipleriGuncelle();
};
