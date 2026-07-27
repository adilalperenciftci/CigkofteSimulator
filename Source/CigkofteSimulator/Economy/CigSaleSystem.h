#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "Orders/CigOrderSystem.h"
#include "CigSaleSystem.generated.h"

// Who handed the food over. Both are priced and booked identically; the source
// exists because a combo is the player's streak, and an apprentice serving in
// the background should neither build it nor break it.
UENUM()
enum class ECigSatisKaynagi : uint8
{
	Oyuncu,
	Personel
};

// Everything one sale needs, so the pipeline never has to reach back into the
// caller for a missing piece. A courier or a contract fills in what it knows
// and leaves the rest at its default.
struct FCigSatisTalebi
{
	ECigSatisKaynagi Kaynak = ECigSatisKaynagi::Oyuncu;

	// What was handed over and how well it matched the order.
	FCigWrapBuild Wrap;
	float Accuracy = 100.f;
	float Quality = 100.f;

	// Who took it.
	ECigTrait Traits = ECigTrait::None;
	bool bVIP = false;
	bool bSadik = false;         // a regular: tips more readily
	float SabirKesri = 1.f;      // patience left, 0-1, for the review
};

// What the sale came to, so the caller can say it out loud without recomputing
// any of it.
struct FCigSatisSonucu
{
	int32 Brut = 0;
	int32 Bahsis = 0;
	int32 Toplam = 0;
	bool bKusursuz = false;
	int32 Kombo = 0;
};

// The one place a sale is priced and booked.
//
// Before this existed the player's counter and the apprentice each did their
// own version. The apprentice's was a single hardcoded 55 lira that ignored the
// price list, hygiene, events, policy and skills, and - because it never
// touched TotalServed - a bulk order could not be advanced by staff at all. Two
// implementations of the same idea is one too many; every route now ends here.
UCLASS()
class UCigSaleSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	// Prices, books and reports one sale. Money, statistics, hygiene wear,
	// reputation, XP, reviews and the served event all happen here; what the
	// caller keeps is presentation and anything specific to who it is serving.
	FCigSatisSonucu SatisiIsle(const FCigSatisTalebi& Talep);

	// Books money the shop took in: the balance, the day's tally and the best-day
	// record, which used to be updated in one place and forgotten in the others.
	//
	// A delivery is not a counter sale - it has its own score, its own review and
	// its own progression counters - so it uses this rather than being pushed
	// through SatisiIsle behind a row of opt-out flags. Sharing the bookkeeping is
	// the part that was actually duplicated.
	void GeliriKaydet(int32 Tutar);

	// The menu price of a wrap before quality, accuracy and the multipliers.
	// Split out so the price list can be checked without a shop around it.
	static int32 MenuFiyati(const class UCigPricingSystem& Fiyatlar, const FCigWrapBuild& Wrap);

	// How much of the list price a wrap actually earns. Pure: quality and
	// accuracy both scale it, and neither may take it to zero or above list.
	static float KaliteAccuracyCarpani(float Quality, float Accuracy);

	// The combo multiplier for a streak of perfect wraps. Streak 0 and 1 pay
	// list price; beyond that it climbs and then stops.
	static float KomboCarpani(int32 Kombo);

	// Consecutive perfect wraps from the player's own hands. Staff sales do not
	// build it and do not break it: it is the player's streak, and an
	// apprentice serving in the background should neither earn nor spend it.
	int32 Kombo = 0;

private:
	int32 BahsisHesapla(const FCigSatisTalebi& Talep, float Fiyat) const;
};
