#include "Economy/CigRivalSystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Progression/CigProgressionSystem.h"
#include "Economy/CigReviewSystem.h"
#include "Core/CigRandomSubsystem.h"

void UCigRivalSystem::OnInit()
{
	if (Rivals.Num() > 0)
	{
		return; // came from a save
	}

	FCigRival A;
	A.Name = TEXT("King Çiğköfte");
	A.Price = 1.2f; A.Quality = 70.f; A.Hygiene = 75.f; A.Popularity = 60.f; A.Capacity = 40; A.Attitude = -1;
	Rivals.Add(A);

	FCigRival B;
	B.Name = TEXT("Urfa Sultan");
	B.Price = 1.0f; B.Quality = 65.f; B.Hygiene = 60.f; B.Popularity = 50.f; B.Capacity = 30; B.Attitude = 1;
	Rivals.Add(B);

	FCigRival C;
	C.Name = TEXT("Eko Çiğ");
	C.Price = 0.7f; C.Quality = 40.f; C.Hygiene = 45.f; C.Popularity = 40.f; C.Capacity = 25; C.Attitude = 0;
	Rivals.Add(C);
}

int32 UCigRivalSystem::OpenRivalCount() const
{
	int32 N = 0;
	for (const FCigRival& R : Rivals)
	{
		if (R.bOpen)
		{
			N++;
		}
	}
	return N;
}

float UCigRivalSystem::PlayerPullMult() const
{
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	const UCigReviewSystem* Rev = GM ? GM->Reviews.Get() : nullptr;

	const float PlayerScore = (Prog ? Prog->Rep : 50.f) + (Rev ? Rev->ShopScore() * 10.f : 30.f);
	float RivalScore = 0.f;
	for (const FCigRival& R : Rivals)
	{
		if (R.bOpen)
		{
			RivalScore += R.Popularity + R.AdPower;
		}
	}
	RivalScore /= FMath::Max(1, OpenRivalCount());

	// A pull multiplier in the 0.7 - 1.3 range
	return FMath::Clamp(0.7f + 0.6f * (PlayerScore / FMath::Max(1.f, PlayerScore + RivalScore)), 0.7f, 1.3f);
}

void UCigRivalSystem::OnDayEnd(int32 Day)
{
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	const float PlayerRep = Prog ? Prog->Rep : 50.f;

	for (FCigRival& R : Rivals)
	{
		if (!R.bOpen)
		{
			continue;
		}

		// Campaign decay
		R.AdPower = FMath::Max(0.f, R.AdPower - 10.f);

		const float Roll = Rng().FRand();
		if (Roll < 0.15f)
		{
			// Starts a campaign
			R.AdPower = Rng().FRandRange(15.f, 30.f);
			R.Price *= 0.9f;
			GM->AddMessage(CigText::Format(TEXT("msg.rival.campaign"), *R.Name), FLinearColor(1.f, 0.8f, 0.4f));
		}
		else if (Roll < 0.25f)
		{
			// Makes a mistake
			R.Popularity = FMath::Max(0.f, R.Popularity - Rng().FRandRange(5.f, 12.f));
			R.Hygiene = FMath::Max(20.f, R.Hygiene - 8.f);
			GM->AddMessage(CigText::Format(TEXT("msg.rival.scandal"), *R.Name), FLinearColor(0.7f, 1.f, 0.7f));
		}
		else if (Roll < 0.35f && R.Popularity > 60.f)
		{
			// Grows
			R.Capacity += 5;
			GM->AddMessage(CigText::Format(TEXT("msg.rival.growing"), *R.Name), FLinearColor(1.f, 0.8f, 0.6f));
		}

		// Popularity balance: quality and advertising push up, the player's
		// reputation pulls down
		const float Target = R.Quality * 0.5f + R.AdPower - (PlayerRep - 50.f) * 0.3f;
		R.Popularity = FMath::Clamp(FMath::Lerp(R.Popularity, Target, 0.15f), 0.f, 100.f);

		if (R.Popularity < 12.f)
		{
			R.BadDays++;
			if (R.BadDays >= 3)
			{
				R.bOpen = false;
				GM->AddMessage(CigText::Format(TEXT("msg.rival.closed"), *R.Name), FLinearColor(0.5f, 1.f, 0.5f));
	Bus().RivalClosed.Broadcast();
			}
		}
		else
		{
			R.BadDays = 0;
		}
	}

	// Broadcast an event if the player has overtaken a rival on popularity
	if (Prog)
	{
		for (const FCigRival& R : Rivals)
		{
			if (R.bOpen && PlayerRep > R.Popularity + 10.f)
			{
				Bus().RivalBeaten.Broadcast();
				break;
			}
		}
	}
}
