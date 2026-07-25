#include "Economy/CigInspectionSystem.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigRivalSystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Core/CigBalance.h"
#include "Core/CigText.h"
#include "Core/CigRandomSubsystem.h"

namespace
{
	// Being caught taking a bribe is not a bigger fine, it is a different event:
	// the shop pays twice and the reputation goes with it.
	constexpr float RusvetYakalanmaCezaCarpani = 2.5f;
	constexpr float RusvetYakalanmaItibar = -25.f;

	// How long the council shuts the shop for once the strikes run out.
	constexpr int32 KapatmaGunSayisi = 1;

	// A failed inspection costs this much reputation before the fine is counted;
	// the multiplier scales it with how far under the pass mark the shop was.
	constexpr float BasarisizItibarTabani = -6.f;
	constexpr float BasariliItibar = 5.f;
}

void UCigInspectionSystem::OnInit()
{
	// A new shop starts with a valid licence rather than an expired one; being
	// fined on day one for paperwork nobody mentioned would just read as a bug.
	RuhsatBitisGunu = FMath::RoundToInt(Param(TEXT("RuhsatSuresi"), 14.f));
}

float UCigInspectionSystem::Param(const TCHAR* Key, float Fallback) const
{
	return CigBalance::Inspection(Key, Fallback);
}

float UCigInspectionSystem::DenetimPuani(float Hijyen, float StokTazeligi, bool bRuhsatGecerli)
{
	const float HijyenAgirlik = CigBalance::Inspection(TEXT("HijyenAgirligi"), 0.5f);
	const float TazelikAgirlik = CigBalance::Inspection(TEXT("TazelikAgirligi"), 0.3f);
	const float RuhsatAgirlik = CigBalance::Inspection(TEXT("RuhsatAgirligi"), 0.2f);

	const float H = FMath::Clamp(Hijyen, 0.f, 100.f);
	const float T = FMath::Clamp(StokTazeligi, 0.f, 100.f);

	const float Puan = H * HijyenAgirlik + T * TazelikAgirlik + (bRuhsatGecerli ? 100.f : 0.f) * RuhsatAgirlik;
	if (bRuhsatGecerli)
	{
		return Puan;
	}

	// Without a licence the score is capped below the pass mark rather than
	// merely docked. Left as a plain weight, a spotless counter could carry an
	// unlicensed shop past the inspector and the paperwork would never once cost
	// anything - which is not what a heavy penalty is supposed to mean.
	return FMath::Min(Puan, CigBalance::Inspection(TEXT("RuhsatsizTavan"), 45.f));
}

bool UCigInspectionSystem::RuhsatGecerli() const
{
	return BugununGunu <= RuhsatBitisGunu;
}

int32 UCigInspectionSystem::RuhsatKalanGun() const
{
	return FMath::Max(0, RuhsatBitisGunu - BugununGunu);
}

int32 UCigInspectionSystem::RuhsatUcreti() const
{
	return FMath::RoundToInt(Param(TEXT("RuhsatUcreti"), 250.f));
}

float UCigInspectionSystem::DenetimSansi(float TabanSans, float RiskCarpani, float Tavan)
{
	return FMath::Clamp(FMath::Max(TabanSans, 0.f) * FMath::Max(RiskCarpani, 0.f), 0.f, FMath::Clamp(Tavan, 0.f, 1.f));
}

float UCigInspectionSystem::BugunkuDenetimSansi() const
{
	return DenetimSansi(Param(TEXT("GunlukDenetimSansi"), 0.30f),
		DenetimRiskCarpani(),
		Param(TEXT("DenetimSansTavani"), 0.65f));
}

float UCigInspectionSystem::DenetimRiskCarpani() const
{
	return 1.f + (bSikayetVar ? Param(TEXT("SikayetRiskArtisi"), 0.25f) : 0.f);
}

void UCigInspectionSystem::OnDayStart(int32 Day)
{
	BugununGunu = Day;
	BekleyenCeza = 0;

	if (KalanKapaliGun > 0)
	{
		KalanKapaliGun--;
		if (KalanKapaliGun == 0)
		{
			GM->AddMessage(CigText::Get(TEXT("msg.inspect.reopened")), FLinearColor(0.5f, 1.f, 0.5f));
		}
	}

	if (!RuhsatGecerli())
	{
		GM->AddMessage(CigText::Get(TEXT("msg.inspect.licenceexpired")), FLinearColor(1.f, 0.5f, 0.3f));
	}
	else if (RuhsatKalanGun() <= 2)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.inspect.licencesoon"), RuhsatKalanGun()),
			FLinearColor(1.f, 0.85f, 0.4f));
	}

	// A rival only files a complaint against a shop doing well enough to be
	// worth complaining about.
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	const UCigRivalSystem* Riv = GM ? GM->Rivals.Get() : nullptr;
	bSikayetVar = false;
	if (Prog && Riv && Prog->Rep > 60.f && Riv->OpenRivalCount() > 0 && Rng().Chance(0.15f))
	{
		bSikayetVar = true;
		GM->AddMessage(CigText::Get(TEXT("msg.inspect.complaint")), FLinearColor(1.f, 0.7f, 0.4f));
	}
}

FCigDenetimSonucu UCigInspectionSystem::Denetle()
{
	const UCigHygieneSystem* Hyg = GM ? GM->Hygiene.Get() : nullptr;
	const UCigInventorySystem* Inv = GM ? GM->Inventory.Get() : nullptr;

	FCigDenetimSonucu S;
	const float Hijyen = Hyg ? Hyg->OverallHygiene() : 100.f;

	// Ingredient quality doubles as stock freshness here: the same number the
	// gourmet customer tastes is what the inspector writes down.
	const float Tazelik = Inv ? FMath::Clamp(Inv->AverageIngredientQuality() * 100.f, 0.f, 100.f) : 100.f;
	const bool bRuhsat = RuhsatGecerli();

	S.Puan = DenetimPuani(Hijyen, Tazelik, bRuhsat);
	S.bGecti = S.Puan >= Param(TEXT("GecerNotu"), 60.f);

	if (S.bGecti)
	{
		BasarisizDenetim = 0;
		S.ItibarFarki = BasariliItibar;
		CezaUygula(0, S.ItibarFarki);
		GM->AddMessage(CigText::Format(TEXT("msg.inspect.passed"), FMath::RoundToInt(S.Puan)),
			FLinearColor(0.4f, 1.f, 0.4f));
		return S;
	}

	// The fine scales with the shortfall so a near miss is not billed like a
	// disaster, and an invalid licence multiplies whatever it came to.
	const float GecerNotu = Param(TEXT("GecerNotu"), 60.f);
	const float Eksiklik = GecerNotu > 0.f ? FMath::Clamp((GecerNotu - S.Puan) / GecerNotu, 0.f, 1.f) : 1.f;
	float Ceza = Param(TEXT("CezaTabani"), 300.f) * (0.5f + Eksiklik);
	if (!bRuhsat)
	{
		Ceza *= Param(TEXT("RuhsatsizCezaCarpani"), 2.f);
	}

	S.Ceza = FMath::RoundToInt(Ceza);
	S.ItibarFarki = BasarisizItibarTabani * (0.5f + Eksiklik);
	BasarisizDenetim++;
	BekleyenCeza = S.Ceza;

	CezaUygula(S.Ceza, S.ItibarFarki);
	GM->AddMessage(CigText::Format(TEXT("msg.inspect.failed"), FMath::RoundToInt(S.Puan), S.Ceza),
		FLinearColor(1.f, 0.35f, 0.3f));

	if (Hyg && Hyg->WorstProblem().Len() > 0)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.inspect.report"), *Hyg->WorstProblem()),
			FLinearColor(1.f, 0.6f, 0.4f));
	}

	if (BasarisizDenetim >= FMath::RoundToInt(Param(TEXT("KapatmaEsigi"), 3.f)))
	{
		BasarisizDenetim = 0;
		KalanKapaliGun = KapatmaGunSayisi;
		GM->AddMessage(CigText::Get(TEXT("msg.inspect.closed")), FLinearColor(1.f, 0.2f, 0.2f));
	}

	return S;
}

void UCigInspectionSystem::RusvetVer()
{
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (!Eco || BekleyenCeza <= 0)
	{
		return;
	}

	const int32 Tutar = FMath::RoundToInt(BekleyenCeza * Param(TEXT("RusvetOrani"), 0.6f));
	if (!Eco->TrySpend(Tutar))
	{
		GM->AddMessage(CigText::Get(TEXT("msg.inspect.bribenomoney")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	// The risk is carried, not rolled fresh each time: it is the pattern of
	// bribes that gets a shop caught, not any single envelope.
	const float Risk = RusvetSayisi * Param(TEXT("RusvetRiskArtisi"), 0.15f);
	RusvetSayisi++;

	if (Rng().Chance(Risk))
	{
		const int32 AgirCeza = FMath::RoundToInt(BekleyenCeza * RusvetYakalanmaCezaCarpani);
		Eco->Money -= AgirCeza;
		if (Prog)
		{
			Prog->AddRep(RusvetYakalanmaItibar);
		}
		KalanKapaliGun = KapatmaGunSayisi;
		BekleyenCeza = 0;
		GM->AddMessage(CigText::Format(TEXT("msg.inspect.bribecaught"), AgirCeza), FLinearColor(1.f, 0.2f, 0.2f));
		return;
	}

	// The fine that was on the table is written off along with the strike.
	Eco->Earn(BekleyenCeza);
	BasarisizDenetim = FMath::Max(0, BasarisizDenetim - 1);
	BekleyenCeza = 0;
	GM->AddMessage(CigText::Format(TEXT("msg.inspect.bribed"), Tutar), FLinearColor(0.8f, 0.8f, 0.5f));
}

void UCigInspectionSystem::RuhsatYenile()
{
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (!Eco)
	{
		return;
	}
	if (!Eco->TrySpend(RuhsatUcreti()))
	{
		GM->AddMessage(CigText::Get(TEXT("msg.inspect.licencenomoney")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	// Renewing early extends from today rather than from the old expiry, so
	// there is no reason to leave it to the last day.
	RuhsatBitisGunu = BugununGunu + FMath::RoundToInt(Param(TEXT("RuhsatSuresi"), 14.f));
	GM->AddMessage(CigText::Format(TEXT("msg.inspect.licencerenewed"), RuhsatKalanGun()),
		FLinearColor(0.5f, 1.f, 0.5f));
}

void UCigInspectionSystem::CezaUygula(int32 Ceza, float ItibarFarki)
{
	if (Ceza > 0 && GM->Economy)
	{
		GM->Economy->Money -= Ceza;
	}
	if (GM->Progression)
	{
		GM->Progression->AddRep(ItibarFarki);
	}
}
