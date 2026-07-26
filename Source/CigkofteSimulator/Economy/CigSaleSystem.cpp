#include "Economy/CigSaleSystem.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "Cooking/CigCookingSystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigPricingSystem.h"
#include "Economy/CigReviewSystem.h"
#include "Events/CigEventSystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigBalance.h"

int32 UCigSaleSystem::MenuFiyati(const UCigPricingSystem& Fiyatlar, const FCigWrapBuild& Wrap)
{
	const int32 UrunIndex = Wrap.Portions >= 2 ? CigUrunCiftPorsiyon : CigUrunDurum;
	int32 Fiyat = Fiyatlar.Fiyat(UrunIndex);

	int32 GarniturSayisi = 0;
	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		if (Wrap.HasTopping((ECigTopping)i))
		{
			GarniturSayisi++;
		}
	}
	Fiyat += GarniturSayisi * Fiyatlar.Fiyat(CigUrunGarnitur);

	if (Wrap.bAyran)
	{
		Fiyat += Fiyatlar.Fiyat(CigUrunAyran);
	}
	if (Wrap.Side != ECigSide::Yok)
	{
		Fiyat += Fiyatlar.Fiyat(CigSideUrunIndex(Wrap.Side));
	}

	return Fiyat;
}

void UCigSaleSystem::GeliriKaydet(int32 Tutar)
{
	if (!GM || !GM->Economy)
	{
		return;
	}

	GM->Economy->Earn(Tutar);

	if (GM->Days)
	{
		GM->Days->RegisterSale(Tutar);
		if (GM->Progression && GM->Days->DayEarnings > GM->Progression->BestDayEarnings)
		{
			GM->Progression->BestDayEarnings = GM->Days->DayEarnings;
		}
	}
}

float UCigSaleSystem::KaliteAccuracyCarpani(float Quality, float Accuracy)
{
	// Both terms are floors as much as multipliers. A bad wrap still sells,
	// because a customer who has already paid attention to the counter will
	// take it and complain rather than walk; the reputation hit is where that
	// costs. Sliding either to zero would make a single slip free money lost
	// and turn the accuracy readout into a pass/fail gate.
	const float KaliteTerimi = FMath::Clamp(Quality / 100.f, 0.3f, 1.2f);
	const float AccuracyTerimi = FMath::Lerp(0.35f, 1.f, FMath::Clamp(Accuracy, 0.f, 100.f) / 100.f);
	return KaliteTerimi * AccuracyTerimi;
}

float UCigSaleSystem::KomboCarpani(int32 Kombo)
{
	// The first perfect wrap sets the streak up rather than paying for it, so
	// the bonus starts at two and stops at six. Uncapped it would eventually
	// dwarf every other income source and make a bad day unrecoverable only
	// because the streak broke.
	if (Kombo < 2)
	{
		return 1.f;
	}
	return 1.f + 0.1f * (float)FMath::Min(Kombo - 1, 5);
}

int32 UCigSaleSystem::BahsisHesapla(const FCigSatisTalebi& Talep, float Fiyat) const
{
	if (Talep.Quality < 60.f)
	{
		return 0;
	}

	float Sans = 0.15f + (Talep.Accuracy - 60.f) / 200.f;
	float Carpan = 0.12f;

	// Trait effects are read from Config/Balance/Traits.csv.
	Sans += CigBalance::TraitTipChanceDelta((uint16)Talep.Traits);
	if (const float Override = CigBalance::TraitTipMultOverride((uint16)Talep.Traits); Override > 0.f)
	{
		Carpan = Override;
	}
	if (Talep.bSadik)
	{
		Sans += 0.2f;
	}
	if (GM && GM->Skills)
	{
		Carpan *= GM->Skills->TipRepMult(); // the Guleryuzlu skill
	}

	if (!Rng().Chance(Sans))
	{
		return 0;
	}
	return FMath::RoundToInt(Fiyat * Carpan * Rng().FRandRange(0.6f, 1.4f));
}

FCigSatisSonucu UCigSaleSystem::SatisiIsle(const FCigSatisTalebi& Talep)
{
	FCigSatisSonucu Sonuc;
	if (!GM)
	{
		return Sonuc;
	}

	UCigEconomySystem* Eco = GM->Economy.Get();
	UCigProgressionSystem* Prog = GM->Progression.Get();
	if (!Eco || !Prog)
	{
		return Sonuc;
	}

	// --- Price ---
	float Fiyat = 0.f;
	if (const UCigPricingSystem* Fiyatlar = GM->Pricing.Get())
	{
		const FCigRecipe& R = UCigCookingSystem::Recipe(Talep.Wrap.Recipe);
		Fiyat = MenuFiyati(*Fiyatlar, Talep.Wrap) * R.PriceMult * Eco->PolicyPriceMult();
		Fiyat *= KaliteAccuracyCarpani(Talep.Quality, Talep.Accuracy);
		if (GM->Events)
		{
			Fiyat *= GM->Events->PriceMult();
		}
	}

	// --- Combo ---
	const bool bKusursuz = Talep.Accuracy >= 85.f && Talep.Quality >= 80.f;
	Sonuc.bKusursuz = bKusursuz;

	if (Talep.Kaynak == ECigSatisKaynagi::Oyuncu)
	{
		Kombo = bKusursuz ? Kombo + 1 : 0;
		Fiyat *= KomboCarpani(Kombo);
	}
	Sonuc.Kombo = Kombo;

	if (bKusursuz)
	{
		Prog->TotalPerfectOrders++;
	}

	// --- Tip ---
	// Read before the prestige bonus, so a long-running shop does not inflate
	// what its customers are seen to leave.
	Sonuc.Bahsis = BahsisHesapla(Talep, Fiyat);

	// Every shop handed over leaves a permanent income bonus behind.
	if (GM->Skills)
	{
		Fiyat *= GM->Skills->PrestigeEarnMult();
	}

	Sonuc.Brut = FMath::RoundToInt(Fiyat);
	if (Talep.bVIP)
	{
		Sonuc.Brut *= 3;
	}
	Sonuc.Toplam = Sonuc.Brut + Sonuc.Bahsis;

	// --- Book it ---
	GeliriKaydet(Sonuc.Toplam);

	// This counter is what a bulk order measures its progress against, so any
	// sale that fed a customer has to raise it whoever carried the plate. The
	// staff path used to skip it, which meant an apprentice could serve all day
	// while a wedding contract sat at zero and then failed.
	Prog->TotalServed++;
	if (UCigHygieneSystem* Hyg = GM->Hygiene.Get())
	{
		Hyg->OnServeMade(!Talep.Wrap.bPacked);
	}

	// --- Reputation ---
	const bool bFenomen = EnumHasAnyFlags(Talep.Traits, ECigTrait::Influencer);
	const float RepOlcek = bFenomen ? 3.f : (Talep.bVIP ? 2.f : 1.f);
	if (Talep.Accuracy >= 85.f && Talep.Quality >= 70.f)
	{
		Prog->AddRep(3.f * RepOlcek * (GM->Skills ? GM->Skills->TipRepMult() : 1.f));
	}
	else if (Talep.Accuracy < 50.f)
	{
		Prog->AddRep(-3.f * RepOlcek);
	}
	if (Talep.Quality < 40.f)
	{
		Prog->AddRep(-3.f);
	}
	if (Eco->PricePolicy == 0)
	{
		Prog->AddRep(0.5f);
	}

	// --- XP, quests and reviews ---
	Prog->AddXP(10 + FMath::RoundToInt(Talep.Quality / 10.f)
		+ (Talep.bVIP ? 10 : 0)
		+ (Talep.Accuracy >= 90.f ? 5 : 0));

	Bus().Served.Broadcast(Talep.Accuracy, Talep.Quality, Talep.Wrap.Portions);

	if (GM->Reviews)
	{
		const float Hijyen = GM->Hygiene ? GM->Hygiene->OverallHygiene() : 100.f;
		GM->Reviews->RecordServe(Talep.Quality, Talep.Accuracy, Talep.SabirKesri,
			Eco->PricePolicy, Hijyen, Talep.Traits);
	}

	return Sonuc;
}
