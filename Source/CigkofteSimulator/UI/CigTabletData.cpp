#include "UI/CigTabletData.h"
#include "UI/CigUIColors.h"
#include "Core/CigText.h"
#include "Core/CigUpgrades.h"
#include "Core/CigkofteTypes.h"
#include "Inventory/CigInventorySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigPricingSystem.h"
#include "Economy/CigInspectionSystem.h"
#include "Core/CigBalance.h"
#include "Progression/CigSkillSystem.h"
#include "Progression/CigAchievementSystem.h"
#include "Cooking/CigCookingSystem.h"
#include "Economy/CigRivalSystem.h"
#include "Economy/CigReviewSystem.h"
#include "Quests/CigQuestSystem.h"
#include "Events/CigEventSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Staff/CigStaffSystem.h"
#include "Cat/CigCatSystem.h"
#include "Progression/CigProgressionSystem.h"

#define LOCTEXT_NAMESPACE "CigTablet"

namespace
{
	const TCHAR* TabTextKey(ECigTabletTab Tab)
	{
		switch (Tab)
		{
		case ECigTabletTab::Stok:       return TEXT("tab.stok");
		case ECigTabletTab::Tarifler:   return TEXT("tab.tarifler");
		case ECigTabletTab::Fiyatlar:   return TEXT("tab.fiyatlar");
		case ECigTabletTab::Dukkan:     return TEXT("tab.dukkan");
		case ECigTabletTab::Tedarikci:  return TEXT("tab.tedarikci");
		case ECigTabletTab::Rakipler:   return TEXT("tab.rakipler");
		case ECigTabletTab::Yorumlar:   return TEXT("tab.yorumlar");
		case ECigTabletTab::Gorevler:   return TEXT("tab.gorevler");
		case ECigTabletTab::Personel:   return TEXT("tab.personel");
		case ECigTabletTab::Yetenekler: return TEXT("tab.yetenekler");
		default:                        return TEXT("tab.basarimlar");
		}
	}

	FCigTabletRow MakeRow(const FString& Left, const FString& Right, const FLinearColor& C, bool bSelectable = false)
	{
		FCigTabletRow R;
		R.Left = Left;
		R.Right = Right;
		R.Color = C;
		R.bSelectable = bSelectable;
		return R;
	}

	FCigTabletRow MakeHeader(const FString& Title)
	{
		FCigTabletRow R;
		R.Left = Title;
		R.Color = CigUI::Gold;
		R.bHeader = true;
		return R;
	}

	FCigTabletRow MakeBarRow(const FString& Left, float Frac, const FLinearColor& BarColor, const FLinearColor& TextColor)
	{
		FCigTabletRow R;
		R.Left = Left;
		R.Color = TextColor;
		R.BarFrac = FMath::Clamp(Frac, 0.f, 1.f);
		R.BarColor = BarColor;
		return R;
	}

	// Must match the star display the Canvas version used.
	FString StarText(float Value)
	{
		const int32 Full = FMath::Clamp(FMath::RoundToInt(Value), 0, 5);
		return FString::ChrN(Full, TEXT('*')) + FString::ChrN(5 - Full, TEXT('-'));
	}
}

namespace CigTablet
{
	FString TabName(ECigTabletTab Tab)
	{
		return CigText::Get(TabTextKey(Tab));
	}

	TArray<FCigTabletRow> BuildRows(ACigkofteGameMode* GM, ECigTabletTab Tab)
	{
		TArray<FCigTabletRow> Rows;
		if (!GM)
		{
			return Rows;
		}

		switch (Tab)
		{
		case ECigTabletTab::Stok:
		{
			const UCigInventorySystem* Inv = GM->Inventory.Get();
			if (!Inv)
			{
				break;
			}
			for (int32 i = 0; i < CigStockCount; ++i)
			{
				const int32 Have = Inv->Stock[i];
				// Low stock shifts to red so the player sees it without reading the list.
				const FLinearColor C = Have <= 0 ? FLinearColor(1.f, 0.4f, 0.35f)
					: (Have < 5 ? FLinearColor(1.f, 0.8f, 0.35f) : CigUI::White);
				Rows.Add(MakeRow(
					FString::Printf(TEXT("%d) %s"), i + 1, *CigStockName(i)),
					FString::Printf(TEXT("%d  ·  %d TL"), Have, Inv->OrderCost(i)),
					C, true));
			}
			break;
		}

		case ECigTabletTab::Dukkan:
		{
			// Paperwork first: an expired licence turns the next inspection into a
			// double fine, which matters more than any upgrade on the list below.
			if (const UCigInspectionSystem* Den = GM->Inspection.Get())
			{
				const bool bGecerli = Den->RuhsatGecerli();
				Rows.Add(MakeRow(
					LOCTEXT("RuhsatSatiri", "0) Ruhsat ve vergi levhası").ToString(),
					bGecerli
						? FText::Format(LOCTEXT("RuhsatGecerli", "{0} gün geçerli  ·  yenile {1} TL"),
							FText::AsNumber(Den->RuhsatKalanGun()), FText::AsNumber(Den->RuhsatUcreti())).ToString()
						: FText::Format(LOCTEXT("RuhsatSuresiz", "SÜRESİ DOLDU  ·  yenile {0} TL"),
							FText::AsNumber(Den->RuhsatUcreti())).ToString(),
					bGecerli ? CigUI::White : CigUI::Bad, true));

				if (Den->BekleyenCeza > 0)
				{
					Rows.Add(MakeRow(
						LOCTEXT("RusvetSatiri", "9) Müfettişe rüşvet ver").ToString(),
						FText::Format(LOCTEXT("RusvetTutar", "{0} TL  ·  riskli"),
							FText::AsNumber(FMath::RoundToInt(Den->BekleyenCeza * 0.6f))).ToString(),
						CigUI::Bad, true));
				}
				if (Den->KapaliMi())
				{
					Rows.Add(MakeRow(LOCTEXT("KapaliSatiri", "Dükkân belediye kararıyla kapalı").ToString(),
						FString(), CigUI::Bad));
				}
			}

			const UCigEconomySystem* Eco = GM->Economy.Get();
			if (!Eco)
			{
				break;
			}
			for (int32 i = 0; i < (int32)ECigUpgrade::COUNT; ++i)
			{
				const FCigUpgradeRow& D = CigUpgradeDef((ECigUpgrade)i);
				const bool bOwned = Eco->HasUpgrade((ECigUpgrade)i);
				Rows.Add(MakeRow(
					FString::Printf(TEXT("%d) %s"), i + 1, *CigBalance::UpgradeName(i)),
					bOwned ? CigText::Get(TEXT("tablet.owned"))
					       : FString::Printf(TEXT("%d TL  (Sv.%d)"), D.Cost, D.MinLevel),
					bOwned ? CigUI::Good : CigUI::White, !bOwned));
			}
			break;
		}

		case ECigTabletTab::Yetenekler:
		{
			const UCigSkillSystem* Sk = GM->Skills.Get();
			if (!Sk)
			{
				break;
			}
			for (int32 i = 0; i < (int32)ECigSkill::COUNT; ++i)
			{
				const ECigSkill S = (ECigSkill)i;
				const FCigSkillRow& D = CigSkillDef(S);
				const int32 R = Sk->RankOf(S);
				Rows.Add(MakeRow(
					FString::Printf(TEXT("%d) %s"), i + 1, *CigBalance::SkillName(i)),
					FString::Printf(TEXT("%d/%d"), R, D.MaxRank),
					R >= D.MaxRank ? CigUI::Good : (Sk->CanUpgrade(S) ? CigUI::White : CigUI::Dim),
					Sk->CanUpgrade(S)));
			}
			break;
		}

		case ECigTabletTab::Basarimlar:
		{
			const UCigAchievementSystem* Ach = GM->Achievements.Get();
			if (!Ach)
			{
				break;
			}
			for (int32 i = 0; i < (int32)ECigAchievement::COUNT; ++i)
			{
				const bool bOn = Ach->IsUnlocked((ECigAchievement)i);
				// A locked achievement shows its condition, not its name, so the goal is visible.
				Rows.Add(MakeRow(bOn ? *CigBalance::AchievementName(i) : *CigBalance::AchievementDesc(i),
					bOn ? CigText::Get(TEXT("tablet.unlocked")) : FString(),
					bOn ? CigUI::Gold : CigUI::Dim));
			}
			break;
		}

		case ECigTabletTab::Tarifler:
		{
			const UCigCookingSystem* Cook = GM->Cooking.Get();
			if (!Cook)
			{
				break;
			}
			for (int32 i = 0; i < CigRecipeCount; ++i)
			{
				const FCigRecipe& R = UCigCookingSystem::Recipe(i);
				const bool bUnlocked = Cook->IsRecipeUnlocked(i);
				const bool bCurrent = Cook->CurrentRecipe == i;

				Rows.Add(MakeRow(
					FString::Printf(TEXT("%d) %s%s"), i + 1, bCurrent ? TEXT("> ") : TEXT("  "), R.Name),
					bUnlocked ? CigText::Format(TEXT("tablet.salesmult"), R.PriceMult)
					          : (R.UnlockLevel >= 99 ? CigText::Get(TEXT("tablet.locked.story"))
					                                 : CigText::Format(TEXT("tablet.levelshort"), R.UnlockLevel)),
					bCurrent ? CigUI::Gold : (bUnlocked ? CigUI::White : CigUI::Dim), bUnlocked));

				// An unlocked recipe puts its mix/isot detail on a second line; a locked
				// one should not take up the space.
				if (bUnlocked)
				{
					Rows.Add(MakeRow(
						CigText::Format(TEXT("tablet.recipemix"),
							*UCigCookingSystem::HumanRecipeMix(R), *UCigCookingSystem::HumanIsotLevel(R)),
						FString(), FLinearColor(0.7f, 0.85f, 0.7f)));
				}
				else
				{
					Rows.Add(MakeRow(FString::Printf(TEXT("     %s"), R.Desc), FString(), CigUI::Dim));
				}
			}
			break;
		}

		case ECigTabletTab::Fiyatlar:
		{
			const UCigPricingSystem* Fiyat = GM->Pricing.Get();
			if (!Fiyat)
			{
				break;
			}

			const float RakipCarpan = Fiyat->RakipOrtalamaCarpani();
			Rows.Add(MakeRow(
				LOCTEXT("FiyatRakipBasligi", "Rakip ortalaması").ToString(),
				FText::Format(LOCTEXT("FiyatRakipDeger", "liste x{0}"),
					FText::AsNumber(RakipCarpan)).ToString(),
				CigUI::Dim));
			Rows.Add(MakeRow(
				LOCTEXT("FiyatMahalleBasligi", "Mahalle gelir düzeyi").ToString(),
				FText::Format(LOCTEXT("FiyatMahalleDeger", "x{0}"),
					FText::AsNumber(Fiyat->MahalleGeliri())).ToString(),
				CigUI::Dim));

			for (int32 i = 0; i < CigUrunCount; ++i)
			{
				const FCigPricingRow& Row = CigBalance::Pricing(i);
				const float Carpan = Fiyat->Carpan(i);

				// Colour carries the pricing decision at a glance: dear in red,
				// cheap in green, list price plain.
				const FLinearColor Renk = Carpan > 1.02f ? CigUI::Bad
				                        : (Carpan < 0.98f ? CigUI::Good : CigUI::White);

				Rows.Add(MakeRow(
					FString::Printf(TEXT("%d) %s"), i + 1, *Row.Label),
					FText::Format(LOCTEXT("FiyatSatiri", "{0} TL  (liste {1} - x{2})"),
						FText::AsNumber(Fiyat->Fiyat(i)),
						FText::AsNumber(Row.TabanFiyat),
						FText::AsNumber(Carpan)).ToString(),
					Renk, true));
			}

			Rows.Add(MakeRow(
				LOCTEXT("FiyatIpucu", "Rakam tuşu fiyatı artırır, Shift ile düşürür.").ToString(),
				FString(), CigUI::Dim));
			break;
		}

		case ECigTabletTab::Tedarikci:
		{
			const UCigEconomySystem* Eco = GM->Economy.Get();
			if (!Eco)
			{
				break;
			}
			for (int32 i = 0; i < CigSupplierCount; ++i)
			{
				const FCigSupplier& S = UCigEconomySystem::Supplier(i);
				const bool bCurrent = Eco->CurrentSupplier == i;
				Rows.Add(MakeRow(
					FString::Printf(TEXT("%d) %s%s"), i + 1, bCurrent ? TEXT("> ") : TEXT("  "), S.Name),
					CigText::Format(TEXT("tablet.pricemult"), S.PriceMult * (1.f - Eco->RelationDiscount(i))),
					bCurrent ? CigUI::Gold : CigUI::White, true));

				const FString QualityWord = CigText::Get(S.Quality >= 1.15f ? TEXT("common.high") : (S.Quality <= 0.85f ? TEXT("common.low") : TEXT("common.normal")));
				const FString SpeedWord = CigText::Get(S.DeliverTime <= 15.f ? TEXT("common.veryfast") : (S.DeliverTime <= 30.f ? TEXT("common.normal") : TEXT("common.slow")));
				Rows.Add(MakeBarRow(
					CigText::Format(TEXT("tablet.supplierdetail"), *QualityWord, *SpeedWord),
					Eco->SupplierRelation[i] / 100.f, FLinearColor(0.4f, 0.8f, 0.5f), FLinearColor(0.75f, 0.85f, 0.75f)));
			}
			Rows.Add(MakeRow(
				CigText::Format(TEXT("tablet.ingredienttier"), CigSupplierCount + 1, *Eco->IngredientTierName()),
				CigText::Format(TEXT("tablet.costquality"), Eco->IngredientTierCostMult(), Eco->IngredientTierQualityMult()),
				CigUI::Gold, true));
			break;
		}

		case ECigTabletTab::Rakipler:
		{
			const UCigRivalSystem* Riv = GM->Rivals.Get();
			if (!Riv)
			{
				break;
			}
			if (const UCigProgressionSystem* Prog = GM->Progression.Get())
			{
				Rows.Add(MakeHeader(CigText::Format(TEXT("tablet.yourpopularity"),
					Prog->Rep, *Prog->PopularityTitle())));
			}
			for (const FCigRival& R : Riv->Rivals)
			{
				if (!R.bOpen)
				{
					Rows.Add(MakeRow(R.Name, CigText::Get(TEXT("tablet.closed")), CigUI::Dim));
					continue;
				}
				const FString PriceWord = CigText::Get(R.Price >= 1.15f ? TEXT("common.expensive") : (R.Price <= 0.85f ? TEXT("common.cheap") : TEXT("common.normal")));
				Rows.Add(MakeRow(R.Name, R.AdPower > 5.f ? CigText::Get(TEXT("tablet.campaign")) : FString(),
					R.AdPower > 5.f ? CigUI::Bad : CigUI::White));
				Rows.Add(MakeBarRow(
					CigText::Format(TEXT("tablet.rivaldetail"), *PriceWord, R.Quality, R.Hygiene),
					R.Popularity / 100.f, FLinearColor(0.9f, 0.5f, 0.3f), CigUI::Dim));
			}
			break;
		}

		case ECigTabletTab::Yorumlar:
		{
			const UCigReviewSystem* Rev = GM->Reviews.Get();
			if (!Rev)
			{
				break;
			}
			Rows.Add(MakeHeader(CigText::Format(TEXT("tablet.overallrating"),
				*StarText(Rev->ShopScore()), Rev->ShopScore())));
			Rows.Add(MakeRow(
				CigText::Format(TEXT("tablet.ratings"),
					*StarText(Rev->FoodScore), *StarText(Rev->ServiceScore), *StarText(Rev->PriceScore),
					*StarText(Rev->HygieneScore), *StarText(Rev->AtmosphereScore)),
				FString(), CigUI::Dim));

			if (Rev->Reviews.Num() == 0)
			{
				Rows.Add(MakeRow(CigText::Get(TEXT("tablet.noreviews")), FString(), CigUI::Dim));
				break;
			}
			// UScrollBox handles the scrolling, so there is no need to truncate the
			// list to fit the screen the way the Canvas version did.
			for (const FCigReview& R : Rev->Reviews)
			{
				Rows.Add(MakeRow(
					FString::Printf(TEXT("%s  %s"), *StarText((float)R.Stars), *R.Author),
					CigText::Format(TEXT("tablet.reviewday"), R.Day),
					R.Stars >= 4 ? CigUI::Good : (R.Stars <= 2 ? CigUI::Bad : CigUI::White)));
				Rows.Add(MakeRow(FString::Printf(TEXT("   \"%s\""), *R.Text), FString(), CigUI::Dim));
			}
			break;
		}

		case ECigTabletTab::Gorevler:
		{
			// The bulk order sits above the daily quest: it is the only thing here
			// with a deadline the player can still act on.
			if (const UCigEventSystem* Ev = GM->Events.Get())
			{
				const FCigTopluSiparis& T = Ev->TopluSiparis;
				if (T.bTeklifVar)
				{
					Rows.Add(MakeHeader(LOCTEXT("TopluBaslik", "Toplu sipariş teklifi").ToString()));
					Rows.Add(MakeRow(
						FText::Format(LOCTEXT("TopluTeklif", "{0}. güne {1} dürüm"),
							FText::AsNumber(T.TeslimGunu), FText::AsNumber(T.IstenenAdet)).ToString(),
						FText::Format(LOCTEXT("TopluOdul", "{0} TL"), FText::AsNumber(T.Odul)).ToString(),
						CigUI::Gold));
					Rows.Add(MakeRow(LOCTEXT("TopluKabul", "1) Kabul et").ToString(), FString(), CigUI::Good, true));
					Rows.Add(MakeRow(LOCTEXT("TopluRet", "2) Reddet").ToString(), FString(), CigUI::Dim, true));
				}
				else if (T.bKabulEdildi)
				{
					const UCigProgressionSystem* Pr = GM->Progression.Get();
					const int32 Yapilan = Pr ? Pr->TotalServed - T.BaslangicServis : 0;
					Rows.Add(MakeHeader(LOCTEXT("TopluAktifBaslik", "Toplu sipariş").ToString()));
					Rows.Add(MakeBarRow(
						FText::Format(LOCTEXT("TopluIlerleme", "{0}/{1} dürüm  ·  teslim {2}. gün"),
							FText::AsNumber(Yapilan), FText::AsNumber(T.IstenenAdet),
							FText::AsNumber(T.TeslimGunu)).ToString(),
						T.IstenenAdet > 0 ? (float)Yapilan / (float)T.IstenenAdet : 0.f,
						CigUI::Gold, CigUI::White));
				}
			}
			const UCigQuestSystem* Q = GM->Quests.Get();
			if (!Q)
			{
				break;
			}
			Rows.Add(MakeHeader(CigText::Get(TEXT("tablet.dailyquest"))));
			Rows.Add(MakeRow(Q->bQuestDone
				? CigText::Get(TEXT("tablet.questdone"))
				: FString::Printf(TEXT("%s  (%d/%d)"), *Q->QuestDesc, Q->QuestProgress, Q->QuestTarget),
				Q->bQuestDone ? FString() : FString::Printf(TEXT("%d TL"), Q->QuestReward),
				Q->bQuestDone ? CigUI::Good : CigUI::White));

			Rows.Add(MakeHeader(CigText::Get(TEXT("tablet.storygoal"))));
			Rows.Add(MakeRow(Q->StoryGoalText(), FString(), CigUI::White));

			if (const UCigProgressionSystem* Prog = GM->Progression.Get())
			{
				Rows.Add(MakeHeader(CigText::Get(TEXT("tablet.stats"))));
				Rows.Add(MakeRow(
					CigText::Format(TEXT("tablet.stats.service"),
						Prog->TotalServed, Prog->TotalDeliveries, Prog->TotalPerfectOrders, Prog->TotalAngryCustomers),
					FString(), CigUI::Dim));
				Rows.Add(MakeRow(
					CigText::Format(TEXT("tablet.stats.earnings"),
						Prog->TotalEarned, Prog->BestDayEarnings),
					FString(), CigUI::Dim));
			}

			if (const UCigCustomerSystem* Cust = GM->Customers.Get())
			{
				Rows.Add(MakeHeader(CigText::Get(TEXT("tablet.regulars"))));
				if (Cust->Loyals.Num() == 0)
				{
					Rows.Add(MakeRow(CigText::Get(TEXT("tablet.noregulars")), FString(), CigUI::Dim));
				}
				for (const FCigLoyalCustomer& L : Cust->Loyals)
				{
					Rows.Add(MakeRow(L.Name,
						CigText::Format(TEXT("tablet.regularstats"), L.Visits, L.Trust), CigUI::White));
				}
			}
			break;
		}

		case ECigTabletTab::Personel:
		{
			Rows.Add(MakeHeader(CigText::Get(TEXT("tablet.apprentice"))));
			const UCigStaffSystem* Staff = GM->Staff.Get();
			if (Staff && Staff->Apprentice.bHired)
			{
				const FCigApprentice& Ap = Staff->Apprentice;
				Rows.Add(MakeRow(
					CigText::Format(TEXT("tablet.staffline"), *Ap.Name, Ap.Level, *UCigStaffSystem::SpecName(Ap.Spec)),
					CigText::Format(TEXT("tablet.salary"), Ap.Salary), CigUI::White));
				Rows.Add(MakeRow(
					CigText::Format(TEXT("tablet.task"), *UCigStaffSystem::TaskName(Ap.Task)),
					CigText::Get(TEXT("tablet.change")), CigUI::Info, true));
				Rows.Add(MakeBarRow(CigText::Get(TEXT("tablet.energy")), Ap.Energy / 100.f, FLinearColor(0.4f, 0.9f, 0.5f), CigUI::Dim));
				Rows.Add(MakeBarRow(CigText::Get(TEXT("tablet.morale")), Ap.Morale / 100.f,
					Ap.Morale < 30.f ? CigUI::Bad : FLinearColor(0.9f, 0.7f, 0.2f), CigUI::Dim));
				Rows.Add(MakeRow(
					FText::Format(LOCTEXT("PersonelYetenek", "Hız x{0}  ·  Titizlik x{1}  ·  Güler yüz x{2}"),
						FText::AsNumber(Ap.Hiz), FText::AsNumber(Ap.Titizlik), FText::AsNumber(Ap.GulerYuz)).ToString(),
					FString(), CigUI::Dim));

				if (Ap.OdenmemisGun > 0)
				{
					Rows.Add(MakeRow(
						FText::Format(LOCTEXT("PersonelOdenmemis", "{0} gündür maaşı ödenmedi"),
							FText::AsNumber(Ap.OdenmemisGun)).ToString(),
						FString(), CigUI::Bad));
				}

				// The offer sits above the raise nag: losing someone outright is
				// the more urgent of the two.
				if (Staff->TransferTeklifi > 0)
				{
					Rows.Add(MakeRow(
						FText::Format(LOCTEXT("PersonelTransfer", "Rakip {0} TL teklif etti"),
							FText::AsNumber(Staff->TransferTeklifi)).ToString(),
						LOCTEXT("PersonelKarsiTeklif", "karşı teklif").ToString(), CigUI::Bad, true));
				}

				if (Ap.bWantsRaise)
				{
					Rows.Add(MakeRow(CigText::Get(TEXT("tablet.wantsraise")), CigText::Get(TEXT("tablet.raise")), CigUI::Bad, true));
				}
			}
			else if (Staff)
			{
				Rows.Add(MakeRow(CigText::Get(TEXT("tablet.nostaff")), FString(), CigUI::Dim));
				for (int32 i = 0; i < Staff->Adaylar.Num(); ++i)
				{
					const FCigStaffAday& A = Staff->Adaylar[i];
					Rows.Add(MakeRow(
						FString::Printf(TEXT("%d) %s — %s"), i + 1, *A.Name, *CigBalance::Staff(A.Arketip).Label),
						FText::Format(LOCTEXT("AdayMaas", "{0} TL/gün"), FText::AsNumber(A.MaasBeklentisi)).ToString(),
						CigUI::White, true));
					Rows.Add(MakeRow(
						FText::Format(LOCTEXT("AdayYetenek", "     hız x{0}  ·  titizlik x{1}  ·  güler yüz x{2}"),
							FText::AsNumber(A.Hiz), FText::AsNumber(A.Titizlik), FText::AsNumber(A.GulerYuz)).ToString(),
						FString(), CigUI::Dim));
				}
			}

			Rows.Add(MakeHeader(CigText::Get(TEXT("tablet.cat"))));
			if (const UCigCatSystem* Cat = GM->CatSys.Get())
			{
				Rows.Add(MakeRow(Cat->CatName, FString(), CigUI::White));
				Rows.Add(MakeBarRow(CigText::Get(TEXT("tablet.satiety")), Cat->Food / 100.f, FLinearColor(0.9f, 0.6f, 0.3f), CigUI::Dim));
				Rows.Add(MakeBarRow(CigText::Get(TEXT("tablet.attention")), Cat->Attention / 100.f, FLinearColor(0.9f, 0.5f, 0.7f), CigUI::Dim));
			}
			break;
		}

		default:
			break;
		}

		return Rows;
	}
}

#undef LOCTEXT_NAMESPACE
