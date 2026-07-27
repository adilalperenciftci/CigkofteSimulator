#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigInspectionSystem.generated.h"

// What an inspection concluded, and what it costs.
struct FCigDenetimSonucu
{
	float Puan = 100.f;
	bool bGecti = false;
	int32 Ceza = 0;
	float ItibarFarki = 0.f;
};

// Municipal inspection: the licence, the verdict and what follows from it.
//
// The inspector already existed as a customer who walks in and looks at the
// counter (Customers/CigCustomerSystem.cpp). What it did not have was anything
// to rule on beyond hygiene, and the thresholds were literals in the middle of
// the visit. The visit stays where it is - the inspector is a person in the
// queue, not a system - and the ruling moves here, where the licence, the fines
// and the record of past failures can live together.
UCLASS()
class UCigInspectionSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;
	virtual void OnDayStart(int32 Day) override;

	// Scores an inspection out of 100. Pure so the weighting can be checked
	// without a shop (see Tests/CigInspectionTests.cpp). An invalid licence is
	// not a deduction like the others - it zeroes its whole share, because a
	// spotless counter does not make an unlicensed shop legal.
	static float DenetimPuani(float Hijyen, float StokTazeligi, bool bRuhsatGecerli);

	// Runs the inspection the walking inspector triggered and applies the
	// outcome: fine, reputation, and the strike that leads to closure.
	FCigDenetimSonucu Denetle();

	// Pays the inspector off instead. Passes today, but every bribe makes the
	// next one likelier to be caught.
	void RusvetVer();

	void RuhsatYenile();

	bool RuhsatGecerli() const;
	int32 RuhsatKalanGun() const;
	int32 RuhsatUcreti() const;

	// True while the shop is shut by the council. The day system reads this.
	bool KapaliMi() const { return KalanKapaliGun > 0; }

	// A rival complaint makes an inspection likelier; the customer system reads
	// this when deciding whether the inspector turns up at all.
	float DenetimRiskCarpani() const;

	// Probability that an inspector arrives today. Pure so the complaint
	// multiplier can be checked without a shop (see Tests/CigInspectionTests.cpp).
	// Capped below certainty: a complaint raises the odds, it does not summon an
	// inspector, and a guaranteed visit would make the warning pointless.
	static float DenetimSansi(float TabanSans, float RiskCarpani, float Tavan);

	// Today's odds with the current complaint state applied.
	float BugunkuDenetimSansi() const;

	int32 RuhsatBitisGunu = 0;
	int32 BasarisizDenetim = 0;
	int32 RusvetSayisi = 0;
	int32 KalanKapaliGun = 0;
	bool bSikayetVar = false;

	// The fine standing on the table while the player decides whether to pay it
	// or bribe their way out. 0 when no inspection is in progress.
	int32 BekleyenCeza = 0;

private:
	int32 BugununGunu = 1;

	float Param(const TCHAR* Key, float Fallback) const;
	void CezaUygula(int32 Ceza, float ItibarFarki);
};
