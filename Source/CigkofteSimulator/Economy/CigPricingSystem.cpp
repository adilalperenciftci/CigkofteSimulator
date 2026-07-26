#include "Economy/CigPricingSystem.h"
#include "Game/CigkofteGameMode.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigRivalSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Core/CigBalance.h"
#include "Core/CigUnlocks.h"

namespace
{
	// Below this the queue is unservable and above it the shop is a ghost town;
	// either end stops being a pricing decision and starts being a broken day.
	constexpr float TalepAltSinir = 0.25f;
	constexpr float TalepUstSinir = 2.0f;

	// How far the wrap has to drift from the street before customers remark on
	// it. Narrow enough that deliberate pricing gets noticed, wide enough that
	// rounding does not.
	constexpr float PahaliEsigi = 1.25f;
	constexpr float UcuzEsigi = 0.75f;

	// Footfall weights across the menu. The wrap decides whether someone walks
	// in at all; the sides only decide what they add once they are inside.
	constexpr float UrunAgirliklari[CigUrunCount] =
	{
		0.45f, // Durum
		0.15f, // CiftPorsiyon
		0.05f, // Garnitur
		0.10f, // Ayran
		0.07f, // IcliKofte
		0.07f, // Corba
		0.06f, // Kunefe
		0.05f  // Cay
	};

	// Days a rival needs to notice a price move and answer it.
	constexpr float RakipTepkiGunu = 3.f;
}

UCigPricingSystem::UCigPricingSystem()
{
	for (float& C : Carpanlar)
	{
		C = 1.f;
	}
}

float UCigPricingSystem::TalepCarpani(float FiyatCarpani, float Esneklik, float GelirCarpani)
{
	// A zero or negative income multiplier would come from a corrupt table; fall
	// back to a neutral street rather than producing an infinity.
	const float Gelir = GelirCarpani > KINDA_SMALL_NUMBER ? GelirCarpani : 1.f;
	const float EtkinOran = FMath::Max(FiyatCarpani, KINDA_SMALL_NUMBER) / Gelir;
	return FMath::Clamp(FMath::Pow(EtkinOran, -Esneklik), TalepAltSinir, TalepUstSinir);
}

float UCigPricingSystem::Carpan(int32 Urun) const
{
	return Carpanlar[FMath::Clamp(Urun, 0, CigUrunCount - 1)];
}

float UCigPricingSystem::PolitikaCarpani() const
{
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	return Eco ? Eco->PolicyPriceMult() : 1.f;
}

float UCigPricingSystem::EtkinCarpan(int32 Urun) const
{
	return Carpan(Urun) * PolitikaCarpani();
}

int32 UCigPricingSystem::Fiyat(int32 Urun) const
{
	return FMath::RoundToInt(CigBalance::Pricing(Urun).TabanFiyat * EtkinCarpan(Urun));
}

float UCigPricingSystem::SokakOrani(int32 Urun) const
{
	const float Rakip = RakipOrtalamaCarpani();
	return EtkinCarpan(Urun) / (Rakip > KINDA_SMALL_NUMBER ? Rakip : 1.f);
}

float UCigPricingSystem::FiyatPuani(float Oran)
{
	// Level with the street is a little better than neutral: people forgive a
	// fair price. The slope is set so the thresholds the written comments use
	// land where the stars already agree with them - at PahaliEsigi the score is
	// clearly poor, at UcuzEsigi clearly good - because a five-star price
	// alongside a review calling the shop expensive reads as a bug.
	constexpr float NotrPuan = 3.5f;
	constexpr float Egim = 4.f;

	return FMath::Clamp(NotrPuan - (Oran - 1.f) * Egim, 1.f, 5.f);
}

bool UCigPricingSystem::UygunFiyatli() const
{
	const float Oran = SokakOrani(CigUrunDurum);
	return Oran <= 1.f && Oran > UcuzEsigi;
}

void UCigPricingSystem::FiyatDegistir(int32 Urun, float Delta)
{
	if (Urun < 0 || Urun >= CigUrunCount)
	{
		return;
	}

	const FCigPricingRow& Row = CigBalance::Pricing(Urun);
	Carpanlar[Urun] = FMath::Clamp(Carpanlar[Urun] + Delta, Row.MinCarpan, Row.MaxCarpan);
}

float UCigPricingSystem::MahalleGeliri() const
{
	// Row 0 is the shop's own street and always counts; a district joins the
	// average once the player has reached the level that opens it.
	const int32 Seviye = GM->Progression ? GM->Progression->Level : 1;

	float Toplam = CigBalance::Mahalle(0).GelirCarpani;
	int32 Sayi = 1;

	for (int32 i = 0; i < (int32)ECigDistrict::COUNT; ++i)
	{
		if (Seviye >= CigDistrictDef((ECigDistrict)i).Level)
		{
			Toplam += CigBalance::Mahalle(i + 1).GelirCarpani;
			Sayi++;
		}
	}

	return Toplam / Sayi;
}

float UCigPricingSystem::GunlukTalepCarpani() const
{
	const float Gelir = MahalleGeliri();

	float Toplam = 0.f;
	for (int32 i = 0; i < CigUrunCount; ++i)
	{
		// The effective markup, not the raw one: a shop on the expensive policy
		// charges a quarter more and footfall has to feel that.
		Toplam += UrunAgirliklari[i] * TalepCarpani(EtkinCarpan(i), CigBalance::Pricing(i).Esneklik, Gelir);
	}

	// The weights are authored to sum to 1, so the weighted mean needs no
	// normalisation pass.
	return Toplam;
}

float UCigPricingSystem::RakipOrtalamaCarpani() const
{
	const UCigRivalSystem* Riv = GM ? GM->Rivals.Get() : nullptr;
	if (!Riv)
	{
		return 1.f;
	}

	float Toplam = 0.f;
	int32 Acik = 0;
	for (const FCigRival& R : Riv->Rivals)
	{
		if (R.bOpen)
		{
			Toplam += R.Price;
			Acik++;
		}
	}

	// With every rival shut there is nothing to price against, so the player's
	// own list price becomes the reference.
	return Acik > 0 ? Toplam / Acik : 1.f;
}

bool UCigPricingSystem::PahaliGoruluyor() const
{
	return SokakOrani(CigUrunDurum) >= PahaliEsigi;
}

bool UCigPricingSystem::SuphelUcuzGoruluyor() const
{
	return SokakOrani(CigUrunDurum) <= UcuzEsigi;
}

void UCigPricingSystem::OnDayStart(int32 Day)
{
	RakipleriGuncelle();
}

void UCigPricingSystem::RakipleriGuncelle()
{
	UCigRivalSystem* Riv = GM ? GM->Rivals.Get() : nullptr;
	if (!Riv)
	{
		return;
	}

	// Rivals aim at the player's wrap price but only start moving once they have
	// watched it for a few days. Undercutting therefore buys a real window
	// instead of being matched the next morning.
	const float OyuncuCarpani = Carpanlar[CigUrunDurum];
	if (!FMath::IsNearlyEqual(OyuncuCarpani, RakipHedefCarpani, CarpanAdimi))
	{
		RakipHedefCarpani = OyuncuCarpani;
		RakipTepkiGecikmesi = RakipTepkiGunu;
		return;
	}

	if (RakipTepkiGecikmesi > 0.f)
	{
		RakipTepkiGecikmesi -= 1.f;
		return;
	}

	// Undercut by a hair: matching exactly would leave the player no reason to
	// ever move again.
	const float Hedef = RakipHedefCarpani * 0.95f;
	for (FCigRival& R : Riv->Rivals)
	{
		if (R.bOpen)
		{
			R.Price = FMath::FInterpTo(R.Price, Hedef, 1.f, 0.35f);
		}
	}
}
