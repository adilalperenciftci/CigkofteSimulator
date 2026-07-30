#include "Economy/CigReviewSystem.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigPricingSystem.h"
#include "Economy/CigSocialSystem.h"
#include "Cat/CigCatSystem.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigText.h"


namespace
{
	const TCHAR* GReviewAuthors[] = {
		TEXT("acibiber42"), TEXT("gurme_mehmet"), TEXT("urfali_kral"), TEXT("ogrenci_hali"),
		TEXT("lezzet_avcisi"), TEXT("mahalleli"), TEXT("doyamayan"), TEXT("kofte_sever")
	};
}

void UCigReviewSystem::BlendCategory(float& Cat, float Sample, float Weight)
{
	Cat = FMath::Clamp(FMath::Lerp(Cat, Sample, Weight), 0.f, 5.f);
}

float UCigReviewSystem::ComputeAtmosphere() const
{
	float A = 2.5f;
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (Eco)
	{
		if (Eco->HasUpgrade(ECigUpgrade::MuzikSistemi)) { A += 1.f; }
		if (Eco->HasUpgrade(ECigUpgrade::Dekorasyon))   { A += 1.f; }
		if (Eco->HasUpgrade(ECigUpgrade::DisOturma))    { A += 0.5f; }
	}
	const UCigCatSystem* CatSys = GM ? GM->CatSys.Get() : nullptr;
	if (CatSys && CatSys->Happiness() > 70.f)
	{
		A += 0.5f; // a happy cat is part of the atmosphere
	}
	return FMath::Clamp(A, 0.f, 5.f);
}

void UCigReviewSystem::RecordServe(float Quality, float Accuracy, float PatienceFrac, float Hygiene, ECigTrait Traits)
{
	FDayServeData D;
	D.Quality = Quality;
	D.Accuracy = Accuracy;
	D.PatienceFrac = PatienceFrac;
	D.Hygiene = Hygiene;
	D.Traits = (uint16)Traits;
	DayServes.Add(D);

	// What the customer was actually charged, against what the street charges.
	// The written comments below already judge the price this way, so the stars
	// had to stop disagreeing with them.
	const UCigPricingSystem* Fiyatlar = GM ? GM->Pricing.Get() : nullptr;
	const float Fiyat = Fiyatlar
		? UCigPricingSystem::FiyatPuani(Fiyatlar->SokakOrani(CigUrunDurum))
		: 3.5f;

	BlendCategory(FoodScore, Quality / 20.f);
	BlendCategory(ServiceScore, (Accuracy / 100.f) * 2.5f + PatienceFrac * 2.5f);
	BlendCategory(PriceScore, Fiyat, 0.1f);
	BlendCategory(HygieneScore, Hygiene / 20.f, 0.1f);
	AtmosphereScore = ComputeAtmosphere();
}

void UCigReviewSystem::RecordAngryLeave(bool bInfluencer)
{
	DayAngry++;
	if (bInfluencer)
	{
		DayInfluencerAngry++;
	}
	BlendCategory(ServiceScore, 0.5f, 0.2f);
}

void UCigReviewSystem::RecordDelivery(float Score)
{
	BlendCategory(ServiceScore, Score / 20.f, 0.1f);
}

float UCigReviewSystem::ShopScore() const
{
	return (FoodScore + ServiceScore + PriceScore + HygieneScore + AtmosphereScore) / 5.f;
}

float UCigReviewSystem::SpawnRateMult() const
{
	// 3 stars is neutral; 5 stars brings 30% more customers, 1 star 30% fewer.
	return 1.f + (ShopScore() - 3.f) * 0.15f;
}

const FCigReview* UCigReviewSystem::YorumBul(int32 Id) const
{
	return Id > 0 ? Reviews.FindByPredicate([Id](const FCigReview& R) { return R.Id == Id; }) : nullptr;
}

void UCigReviewSystem::PushReview(const FString& Author, const FString& Text, int32 Stars, int32 Day)
{
	FCigReview R;
	R.Id = NextReviewId++;
	R.Author = Author;
	R.Text = Text;
	R.Stars = FMath::Clamp(Stars, 1, 5);
	R.Day = Day;
	Reviews.Insert(R, 0);
	if (Reviews.Num() > 12)
	{
		Reviews.SetNum(12);
	}
}

void UCigReviewSystem::OnDayEnd(int32 Day)
{
	// The wording is resolved here, when the review is written, and the resulting
	// string is what goes in the save file. A review already on the board keeps the
	// language it was posted in after the player switches - which is the honest
	// reading of it: these are things people said, not labels on a button.
	//
	// Produce 0-3 reviews from the day's services
	const int32 ReviewCount = FMath::Min(DayServes.Num(), Rng().RandRange(0, 3));
	for (int32 i = 0; i < ReviewCount; ++i)
	{
		const FDayServeData& D = DayServes[Rng().PickIndex(DayServes.Num())];
		const bool bCritic = (D.Traits & (uint16)ECigTrait::SecretCritic) != 0;

		int32 Stars;
		FString Text;
		const float Overall = (D.Quality + D.Accuracy) * 0.5f;
		if (Overall >= 85.f)
		{
			Stars = 5;
			static const TCHAR* Goods[] = {
				TEXT("review.good.1"), TEXT("review.good.2"), TEXT("review.good.3"), TEXT("review.good.4")
			};
			Text = CigText::Get(Goods[Rng().PickIndex(UE_ARRAY_COUNT(Goods))]);
		}
		else if (Overall >= 60.f)
		{
			Stars = Rng().RandRange(3, 4);
			static const TCHAR* Mids[] = {
				TEXT("review.mid.1"), TEXT("review.mid.2"), TEXT("review.mid.3"), TEXT("review.mid.4")
			};
			Text = CigText::Get(Mids[Rng().PickIndex(UE_ARRAY_COUNT(Mids))]);
		}
		else
		{
			Stars = Rng().RandRange(1, 2);
			static const TCHAR* Bads[] = {
				TEXT("review.bad.1"), TEXT("review.bad.2"), TEXT("review.bad.3"), TEXT("review.bad.4")
			};
			Text = CigText::Get(Bads[Rng().PickIndex(UE_ARRAY_COUNT(Bads))]);
		}

		if (D.Hygiene < 40.f && Stars > 2)
		{
			Stars = 2;
			Text = CigText::Get(TEXT("review.dirty"));
		}

		// Price reaches the reviews before it reaches the ledger: an expensive
		// shop gets called expensive even when the food is good, and one that
		// undercuts the street too far gets doubted for the same reason.
		const UCigPricingSystem* Fiyatlar = GM ? GM->Pricing.Get() : nullptr;
		if (Fiyatlar && Fiyatlar->PahaliGoruluyor() && Stars > 2)
		{
			Stars--;
			static const TCHAR* Pahali[] = {
				TEXT("review.expensive.1"), TEXT("review.expensive.2"), TEXT("review.expensive.3")
			};
			Text = CigText::Get(Pahali[Rng().PickIndex(UE_ARRAY_COUNT(Pahali))]);
		}
		else if (Fiyatlar && Fiyatlar->SuphelUcuzGoruluyor() && Stars >= 4)
		{
			Stars--;
			static const TCHAR* Ucuz[] = {
				TEXT("review.cheap.1"), TEXT("review.cheap.2"), TEXT("review.cheap.3")
			};
			Text = CigText::Get(Ucuz[Rng().PickIndex(UE_ARRAY_COUNT(Ucuz))]);
		}

		FString Author = GReviewAuthors[Rng().PickIndex(UE_ARRAY_COUNT(GReviewAuthors))];
		if (bCritic)
		{
			Author = CigText::Get(TEXT("review.critic.author"));
			Stars = FMath::Clamp(Stars + (Overall >= 80.f ? 0 : -1), 1, 5);
			if (GM)
			{
				GM->AddMessage(CigText::Get(TEXT("msg.review.criticvisited")), FLinearColor(0.9f, 0.7f, 1.f));
			}
		}
		PushReview(Author, Text, Stars, Day);
		// A poor review is the one the social system offers a reply to; it always
		// lands at index 0 because PushReview inserts at the front.
		if (Stars <= 2 && GM->Social)
		{
			GM->Social->YanitlanacakYorumId = Reviews[0].Id;
		}
	}

	if (DayAngry >= 3)
	{
		PushReview(TEXT("sinirli_musteri"), CigText::Get(TEXT("review.queue")), 1, Day);
	}

	Bus().ShopScoreChanged.Broadcast(ShopScore());

	DayServes.Empty();
	DayAngry = 0;
	DayInfluencerAngry = 0;
}

