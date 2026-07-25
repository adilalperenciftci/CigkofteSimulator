#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigSocialSystem.generated.h"

// How the player answered a review. The choice is the mechanic: each one trades
// followers against reputation in a different direction, and none of them is
// free.
enum class ECigYanit : uint8
{
	Savun = 0,      // argue back
	OzurDile,       // apologise
	GormezdenGel    // let it stand
};

// The shop's online presence.
//
// Followers are a slow multiplier on footfall, so posting is worth doing on a
// quiet day and never worth doing instead of cooking. The loop closes through
// the review system: a bad review left unanswered costs followers, and how it
// is answered costs something too.
UCLASS()
class UCigSocialSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnDayStart(int32 Day) override;
	virtual void OnDayEnd(int32 Day) override;

	// Footfall multiplier from the follower count. Pure so the curve can be
	// checked without a shop (see Tests/CigSocialTests.cpp).
	//
	// Logarithmic on purpose: the first hundred followers should feel like
	// something, the ten-thousandth should not break the game. The ceiling is
	// what stops a viral run from turning into permanent free customers.
	static float TakipciCarpani(int32 Takipci);

	// Today's footfall multiplier: followers, plus a campaign if one was posted,
	// times the viral day if one landed.
	float GunlukCarpan() const;

	void UrunTanitimiPaylas();
	void KampanyaDuyurusuPaylas();

	// Answers the newest review that has not been answered yet.
	void YorumaYanitVer(ECigYanit Yanit);
	bool YanitBekleyenVar() const;

	// Called when an influencer leaves. A happy one is the single biggest
	// follower swing in the game; an unhappy one takes a share of what is there.
	void FenomenAyrildi(bool bMemnun);

	int32 Takipci = 0;
	int32 KalanGonderi = 2;
	bool bKampanyaAktif = false;
	bool bViralGun = false;

	// Index into the review list that the next reply will answer. -1 when there
	// is nothing waiting.
	int32 YanitlanacakYorum = -1;

private:
	void TakipciDegistir(int32 Delta);
	void ViralGunuDegerlendir();
};
