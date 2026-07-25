#include "Economy/CigSocialSystem.h"
#include "Game/CigkofteGameMode.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigReviewSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Core/CigBalance.h"
#include "Core/CigText.h"
#include "Core/CigRandomSubsystem.h"

namespace
{
	// A review has to be poor before answering it is a decision worth making;
	// arguing with four stars is not a mechanic, it is a mistake.
	constexpr int32 KotuYorumEsigi = 2;
}

float UCigSocialSystem::TakipciCarpani(int32 Takipci)
{
	const float Etki = CigBalance::Social(TEXT("TakipciEtkisi"), 0.18f);
	const float Tavan = CigBalance::Social(TEXT("TakipciTavan"), 1.8f);

	if (Takipci <= 0)
	{
		return 1.f;
	}

	// log10 keeps each tenfold jump worth the same amount, so the climb from 100
	// to 1000 feels like the climb from 1000 to 10000 rather than dwarfing it.
	return FMath::Clamp(1.f + Etki * FMath::LogX(10.f, 1.f + (float)Takipci / 100.f), 1.f, Tavan);
}

float UCigSocialSystem::GunlukCarpan() const
{
	float C = TakipciCarpani(Takipci);
	if (bKampanyaAktif)
	{
		C *= 1.f + CigBalance::Social(TEXT("KampanyaSpawnEtkisi"), 0.25f);
	}
	if (bViralGun)
	{
		C *= CigBalance::Social(TEXT("ViralCarpan"), 3.f);
	}
	return C;
}

void UCigSocialSystem::TakipciDegistir(int32 Delta)
{
	Takipci = FMath::Max(0, Takipci + Delta);
}

void UCigSocialSystem::OnDayStart(int32 Day)
{
	KalanGonderi = FMath::RoundToInt(CigBalance::Social(TEXT("GunlukGonderi"), 2.f));
	bKampanyaAktif = false;

	// A viral day is drawn against the follower count: nobody goes viral from
	// nowhere, but a shop with a following can wake up to one.
	const float Sans = Takipci * CigBalance::Social(TEXT("ViralSans"), 0.00004f);
	bViralGun = Rng().Chance(FMath::Min(Sans, 0.25f));
	if (bViralGun)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.social.viral")), FLinearColor(1.f, 0.8f, 0.3f));
	}
}

void UCigSocialSystem::OnDayEnd(int32 Day)
{
	ViralGunuDegerlendir();

	// A bad review nobody answered is read as the shop having nothing to say.
	if (YanitBekleyenVar())
	{
		TakipciDegistir(FMath::RoundToInt(CigBalance::Social(TEXT("GormezdenTakipci"), -8.f)));
	}
	YanitlanacakYorum = -1;
	bViralGun = false;
}

void UCigSocialSystem::ViralGunuDegerlendir()
{
	if (!bViralGun)
	{
		return;
	}

	const UCigInventorySystem* Inv = GM ? GM->Inventory.Get() : nullptr;
	UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;

	// Going viral with empty shelves is worse than not going viral: a crowd
	// turns up, finds nothing, and says so.
	int32 ToplamStok = 0;
	if (Inv)
	{
		for (int32 i = 0; i < CigStockCount; ++i)
		{
			ToplamStok += Inv->Stock[i];
		}
	}

	if (ToplamStok < FMath::RoundToInt(CigBalance::Social(TEXT("ViralStokEsigi"), 25.f)))
	{
		if (Prog)
		{
			Prog->AddRep(CigBalance::Social(TEXT("ViralTersTepmeItibar"), -15.f));
		}
		TakipciDegistir(-Takipci / 5);
		GM->AddMessage(CigText::Get(TEXT("msg.social.viralbackfire")), FLinearColor(1.f, 0.3f, 0.3f));
		return;
	}

	TakipciDegistir(Takipci / 4 + 50);
	GM->AddMessage(CigText::Get(TEXT("msg.social.viralgood")), FLinearColor(0.5f, 1.f, 0.5f));
}

void UCigSocialSystem::UrunTanitimiPaylas()
{
	if (KalanGonderi <= 0)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.social.noposts")), FLinearColor(1.f, 0.7f, 0.4f));
		return;
	}

	KalanGonderi--;
	const int32 Kazanc = FMath::RoundToInt(CigBalance::Social(TEXT("TanitimTakipci"), 40.f));
	TakipciDegistir(Kazanc);
	GM->AddMessage(CigText::Format(TEXT("msg.social.posted"), Kazanc, Takipci), FLinearColor(0.6f, 0.9f, 1.f));
}

void UCigSocialSystem::KampanyaDuyurusuPaylas()
{
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (KalanGonderi <= 0)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.social.noposts")), FLinearColor(1.f, 0.7f, 0.4f));
		return;
	}
	if (bKampanyaAktif)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.social.campaignactive")), FLinearColor(1.f, 0.7f, 0.4f));
		return;
	}
	if (!Eco || !Eco->TrySpend(FMath::RoundToInt(CigBalance::Social(TEXT("KampanyaUcreti"), 150.f))))
	{
		GM->AddMessage(CigText::Get(TEXT("msg.social.nomoney")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	KalanGonderi--;
	bKampanyaAktif = true;
	TakipciDegistir(FMath::RoundToInt(CigBalance::Social(TEXT("KampanyaTakipci"), 15.f)));
	GM->AddMessage(CigText::Get(TEXT("msg.social.campaign")), FLinearColor(0.6f, 0.9f, 1.f));
}

bool UCigSocialSystem::YanitBekleyenVar() const
{
	const UCigReviewSystem* Rev = GM ? GM->Reviews.Get() : nullptr;
	if (!Rev || !Rev->Reviews.IsValidIndex(YanitlanacakYorum))
	{
		return false;
	}
	return Rev->Reviews[YanitlanacakYorum].Stars <= KotuYorumEsigi;
}

void UCigSocialSystem::YorumaYanitVer(ECigYanit Yanit)
{
	UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (!YanitBekleyenVar())
	{
		return;
	}

	int32 TakipciFarki = 0;
	float ItibarFarki = 0.f;
	const TCHAR* MesajAnahtari = TEXT("msg.social.replied.ignore");

	switch (Yanit)
	{
	case ECigYanit::Savun:
		// Arguing wins nothing back and reads badly in public, but it is the
		// only answer that does not concede the complaint.
		TakipciFarki = FMath::RoundToInt(CigBalance::Social(TEXT("SavunTakipci"), -25.f));
		ItibarFarki = CigBalance::Social(TEXT("SavunItibar"), -2.f);
		MesajAnahtari = TEXT("msg.social.replied.defend");
		break;

	case ECigYanit::OzurDile:
		TakipciFarki = FMath::RoundToInt(CigBalance::Social(TEXT("OzurTakipci"), 10.f));
		ItibarFarki = CigBalance::Social(TEXT("OzurItibar"), 3.f);
		MesajAnahtari = TEXT("msg.social.replied.apologise");
		break;

	case ECigYanit::GormezdenGel:
		TakipciFarki = FMath::RoundToInt(CigBalance::Social(TEXT("GormezdenTakipci"), -8.f));
		MesajAnahtari = TEXT("msg.social.replied.ignore");
		break;
	}

	TakipciDegistir(TakipciFarki);
	if (Prog && !FMath::IsNearlyZero(ItibarFarki))
	{
		Prog->AddRep(ItibarFarki);
	}

	// Answered either way, so the end-of-day silence penalty does not also land.
	YanitlanacakYorum = -1;
	GM->AddMessage(CigText::Format(MesajAnahtari, Takipci), FLinearColor(0.7f, 0.85f, 1.f));
}

void UCigSocialSystem::FenomenAyrildi(bool bMemnun)
{
	if (bMemnun)
	{
		const int32 Kazanc = FMath::RoundToInt(CigBalance::Social(TEXT("FenomenKazanc"), 350.f));
		TakipciDegistir(Kazanc);
		GM->AddMessage(CigText::Format(TEXT("msg.social.influencergood"), Kazanc, Takipci),
			FLinearColor(0.5f, 1.f, 0.5f));
		return;
	}

	// The loss is a share rather than a number: an influencer can only take away
	// an audience the shop actually had.
	const int32 Kayip = FMath::RoundToInt(Takipci * CigBalance::Social(TEXT("FenomenKayip"), 0.35f));
	TakipciDegistir(-Kayip);
	GM->AddMessage(CigText::Format(TEXT("msg.social.influencerbad"), Kayip, Takipci),
		FLinearColor(1.f, 0.3f, 0.3f));
}
