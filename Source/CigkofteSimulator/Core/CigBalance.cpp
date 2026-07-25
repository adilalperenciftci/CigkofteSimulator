#include "Core/CigBalance.h"
#include "Core/CigLog.h"
#include "Core/CigText.h"
#include "Core/CigUnlocks.h"
#include "Core/CigUpgrades.h"
#include "Core/CigkofteTypes.h"
#include "Progression/CigSkillSystem.h"
#include "Progression/CigAchievementSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"

namespace
{
	// ------------------------------------------------------------------ CSV reading

	// A view over one CSV row, addressed by column name. If the column is
	// missing or the cell is empty the target is left alone, so the default
	// survives - which means a CSV only has to carry the columns being changed.
	struct FCigCsvRow
	{
		const TArray<const TCHAR*>& Cells;
		const TMap<FString, int32>& Cols;

		const TCHAR* Find(const TCHAR* Col) const
		{
			const int32* Idx = Cols.Find(Col);
			if (!Idx || !Cells.IsValidIndex(*Idx))
			{
				return nullptr;
			}
			const TCHAR* Cell = Cells[*Idx];
			return (Cell && *Cell) ? Cell : nullptr;
		}

		void Str(const TCHAR* Col, FString& Out) const
		{
			if (const TCHAR* C = Find(Col)) { Out = FString(C).TrimStartAndEnd(); }
		}
		void Int(const TCHAR* Col, int32& Out) const
		{
			if (const TCHAR* C = Find(Col)) { Out = FCString::Atoi(C); }
		}
		void Flt(const TCHAR* Col, float& Out) const
		{
			if (const TCHAR* C = Find(Col)) { Out = FCString::Atof(C); }
		}
	};

	// Reads one balance file and calls Visit for each data row. Returns quietly
	// when the file is absent: playing on the defaults is a valid state.
	void ForEachCsvRow(const TCHAR* FileName, TFunctionRef<void(const FCigCsvRow&)> Visit)
	{
		const FString Path = FPaths::ProjectDir() / TEXT("Config/Balance") / FileName;
		if (!FPaths::FileExists(Path))
		{
			return;
		}

		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *Path))
		{
			UE_LOG(LogCig, Warning, TEXT("Denge dosyası okunamadı: %s — varsayılanlar kullanılıyor."), FileName);
			return;
		}

		const FCsvParser Parser(MoveTemp(Raw));
		const FCsvParser::FRows& Rows = Parser.GetRows();
		if (Rows.Num() < 2)
		{
			UE_LOG(LogCig, Warning, TEXT("Denge dosyası boş ya da yalnızca başlık içeriyor: %s"), FileName);
			return;
		}

		// The header row maps column names to indices; column order is irrelevant.
		TMap<FString, int32> Cols;
		for (int32 i = 0; i < Rows[0].Num(); ++i)
		{
			const TCHAR* H = Rows[0][i];
			if (H && *H)
			{
				Cols.Add(FString(H).TrimStartAndEnd(), i);
			}
		}

		int32 Applied = 0;
		for (int32 r = 1; r < Rows.Num(); ++r)
		{
			if (Rows[r].Num() == 0)
			{
				continue;
			}
			const FCigCsvRow Row{ Rows[r], Cols };
			Visit(Row);
			++Applied;
		}
		UE_LOG(LogCig, Log, TEXT("Denge dosyası uygulandı: %s (%d satır)"), FileName, Applied);
	}

	// Finds the default row whose Key column matches.
	template <typename TRow>
	TRow* FindByKey(TArray<TRow>& Table, const FCigCsvRow& Row)
	{
		FString Key;
		Row.Str(TEXT("Key"), Key);
		if (Key.IsEmpty())
		{
			return nullptr;
		}
		for (TRow& R : Table)
		{
			if (R.Key.Equals(Key, ESearchCase::IgnoreCase))
			{
				return &R;
			}
		}
		UE_LOG(LogCig, Warning, TEXT("Denge tablosunda bilinmeyen anahtar: %s — satır atlandı."), *Key);
		return nullptr;
	}

	// ------------------------------------------------------------------ defaults

	// FTableRowBase carries a virtual destructor, so the row types are not
	// aggregates; the defaults are written with these small makers rather than
	// with brace initialisation.

	FCigSkillRow MakeSkill(const TCHAR* Key, const TCHAR* Name, const TCHAR* Desc, int32 MaxRank, float EffectPerRank)
	{
		FCigSkillRow R;
		R.Key = Key;
		R.Name = Name;
		R.Desc = Desc;
		R.MaxRank = MaxRank;
		R.EffectPerRank = EffectPerRank;
		return R;
	}

	TArray<FCigSkillRow> DefaultSkills()
	{
		return {
			MakeSkill(TEXT("HizliEl"),        TEXT("Hızlı El"),        TEXT("Yoğurma vuruşları rütbe başına %15 daha etkili"), 3, 0.15f),
			MakeSkill(TEXT("KeskinBicak"),    TEXT("Keskin Bıçak"),    TEXT("Doğrama rütbe başına 1 vuruş azalır"),             2, 1.00f),
			MakeSkill(TEXT("Guleryuzlu"),     TEXT("Güler Yüz"),       TEXT("Bahşiş ve itibar kazancı rütbe başına %12 artar"), 3, 0.12f),
			MakeSkill(TEXT("TemizIsci"),      TEXT("Temiz İşçi"),      TEXT("Kirlenme rütbe başına %15 yavaşlar"),              3, 0.85f),
			MakeSkill(TEXT("DayanikliBunye"), TEXT("Dayanıklı Bünye"), TEXT("Enerji tüketimi rütbe başına %20 azalır"),         3, 0.80f),
			MakeSkill(TEXT("HizliAyak"),      TEXT("Hızlı Ayak"),      TEXT("Koşu hızı rütbe başına %8 artar"),                 3, 0.08f),
			MakeSkill(TEXT("PazarlikUstasi"), TEXT("Pazarlık Ustası"), TEXT("Stok maliyeti rütbe başına %8 düşer"),             3, 0.92f),
			MakeSkill(TEXT("MutfakUstasi"),   TEXT("Mutfak Ustası"),   TEXT("Hamur kalitesi rütbe başına %5 artar"),            3, 0.05f)
		};
	}

	FCigUpgradeRow MakeUpgrade(const TCHAR* Key, const TCHAR* Name, const TCHAR* Desc, int32 Cost, int32 MinLevel)
	{
		FCigUpgradeRow R;
		R.Key = Key;
		R.Name = Name;
		R.Desc = Desc;
		R.Cost = Cost;
		R.MinLevel = MinLevel;
		return R;
	}

	TArray<FCigUpgradeRow> DefaultUpgrades()
	{
		return {
			MakeUpgrade(TEXT("BuyukBuzdolabi"), TEXT("Büyük Buzdolabı"),  TEXT("Hamur tazeliği çok daha yavaş düşer"),     700,  2),
			MakeUpgrade(TEXT("IkinciYogurma"),  TEXT("İkinci Yoğurma"),   TEXT("Yoğurma vuruşları %40 daha etkili"),       900,  3),
			MakeUpgrade(TEXT("HizliDograma"),   TEXT("Hızlı Doğrama"),    TEXT("Garnitür 2 vuruşta hazır"),                500,  2),
			MakeUpgrade(TEXT("YeniTabela"),     TEXT("Yeni Tabela"),      TEXT("Müşteriler %15 daha sık gelir"),           600,  1),
			MakeUpgrade(TEXT("Klima"),          TEXT("Klima"),            TEXT("Müşteri sabrı +%20"),                      800,  2),
			MakeUpgrade(TEXT("MuzikSistemi"),   TEXT("Müzik Sistemi"),    TEXT("Atmosfer +1 yıldız, sabır +%8"),           450,  1),
			MakeUpgrade(TEXT("IkinciKasa"),     TEXT("İkinci Kasa"),      TEXT("Kuyruk kapasitesi +2"),                    750,  3),
			MakeUpgrade(TEXT("DisOturma"),      TEXT("Dış Oturma Alanı"), TEXT("Aile müşterileri artar, atmosfer +0.5"),   650,  2),
			MakeUpgrade(TEXT("BuyukCop"),       TEXT("Büyük Çöp Kutusu"), TEXT("Çöp iki kat geç dolar"),                   300,  1),
			MakeUpgrade(TEXT("HijyenEkipmani"), TEXT("Hijyen Ekipmanı"),  TEXT("Kirlenme %35 yavaşlar"),                   550,  2),
			MakeUpgrade(TEXT("IyiLavabo"),      TEXT("Daha İyi Lavabo"),  TEXT("El yıkarken tezgah da kısmen temizlenir"), 400,  1),
			MakeUpgrade(TEXT("Dekorasyon"),     TEXT("Dekorasyon"),       TEXT("Atmosfer +1 yıldız, itibar kazancı +%10"), 500,  1),
			MakeUpgrade(TEXT("YeniSube"),       TEXT("Yeni Şube"),        TEXT("Her gün +250 TL pasif gelir"),            4000,  6)
		};
	}

	FCigTraitRow MakeTrait(const TCHAR* Key, const TCHAR* Name, float Weight, int32 MinDay,
		float RareChance, float PatienceMult, float TipChanceDelta, float TipMultOverride)
	{
		FCigTraitRow R;
		R.Key = Key;
		R.Name = Name;
		R.Weight = Weight;
		R.MinDay = MinDay;
		R.RareChance = RareChance;
		R.PatienceMult = PatienceMult;
		R.TipChanceDelta = TipChanceDelta;
		R.TipMultOverride = TipMultOverride;
		return R;
	}

	TArray<FCigTraitRow> DefaultTraits()
	{
		// The order is ECigTrait's bit order and must not change.
		// Rows with Weight 0 never enter the normal pool: the influencer and the
		// undercover critic arrive on their own RareChance rolls, and the regular
		// is only assigned when a returning customer shows up.
		return {
			MakeTrait(TEXT("Impatient"),        TEXT("Sabırsız"),         1.f, 1, 0.00f, 0.60f,  0.00f, 0.00f),
			MakeTrait(TEXT("Patient"),          TEXT("Sakin"),            1.f, 1, 0.00f, 1.50f,  0.00f, 0.00f),
			MakeTrait(TEXT("PriceSensitive"),   TEXT("Hesaplı"),          1.f, 1, 0.00f, 1.00f, -0.20f, 0.00f),
			MakeTrait(TEXT("Generous"),         TEXT("Eli Açık"),         1.f, 1, 0.00f, 1.00f,  0.35f, 0.25f),
			MakeTrait(TEXT("HygieneSensitive"), TEXT("Titiz"),            1.f, 1, 0.00f, 1.00f,  0.00f, 0.00f),
			MakeTrait(TEXT("QualityFocused"),   TEXT("Gurme"),            1.f, 1, 0.00f, 1.00f,  0.00f, 0.00f),
			MakeTrait(TEXT("Influencer"),       TEXT("Fenomen"),          0.f, 3, 0.06f, 1.00f,  0.00f, 0.00f),
			MakeTrait(TEXT("Tourist"),          TEXT("Turist"),           1.f, 1, 0.00f, 1.00f,  0.00f, 0.00f),
			MakeTrait(TEXT("Student"),          TEXT("Öğrenci"),          1.f, 1, 0.00f, 1.00f, -0.15f, 0.00f),
			MakeTrait(TEXT("Family"),           TEXT("Aile"),             1.f, 1, 0.00f, 1.00f,  0.00f, 0.00f),
			MakeTrait(TEXT("Regular"),          TEXT("Müdavim"),          0.f, 1, 0.00f, 1.00f,  0.00f, 0.00f),
			MakeTrait(TEXT("SpicyFoodLover"),   TEXT("Acı Delisi"),       1.f, 1, 0.00f, 1.00f,  0.00f, 0.00f),
			MakeTrait(TEXT("Indecisive"),       TEXT("Kararsız"),         1.f, 1, 0.00f, 1.00f,  0.00f, 0.00f),
			MakeTrait(TEXT("SecretCritic"),     TEXT("Gizli Eleştirmen"), 0.f, 2, 0.07f, 1.00f,  0.00f, 0.00f)
		};
	}

	FCigStockRow MakeStock(int32 Index, const TCHAR* Label, int32 BaseCost, int32 StartAmount, int32 OrderAmount)
	{
		FCigStockRow R;
		R.Index = Index;
		R.Label = Label;
		R.BaseCost = BaseCost;
		R.StartAmount = StartAmount;
		R.OrderAmount = OrderAmount;
		return R;
	}

	TArray<FCigStockRow> DefaultStock()
	{
		return {
			MakeStock( 0, TEXT("Bulgur"),            50, 20, 10),
			MakeStock( 1, TEXT("İsot"),              70, 12, 10),
			MakeStock( 2, TEXT("Salça"),             60, 12, 10),
			MakeStock( 3, TEXT("Su"),                25, 20, 10),
			MakeStock( 4, TEXT("Baharat"),           50, 10, 10),
			MakeStock( 5, TEXT("Marul"),             45,  6, 10),
			MakeStock( 6, TEXT("Ayran"),             60,  6, 10),
			MakeStock( 7, TEXT("Maydanoz"),          35,  6, 10),
			MakeStock( 8, TEXT("Domates"),           40,  6, 10),
			MakeStock( 9, TEXT("Turşu"),             45,  6, 10),
			MakeStock(10, TEXT("Soğan"),             30,  6, 10),
			MakeStock(11, TEXT("Limon"),             35,  6, 10),
			MakeStock(12, TEXT("Nar Ekşisi"),        50,  6, 10),
			MakeStock(13, TEXT("Lavaş"),             40, 10, 10),
			MakeStock(14, TEXT("İçli Köfte"),       220,  0, 10),
			MakeStock(15, TEXT("Mercimek Çorbası"), 160,  0, 10),
			MakeStock(16, TEXT("Künefe"),           340,  0, 10),
			MakeStock(17, TEXT("Çay"),               40,  0, 10)
		};
	}

	FCigAchievementRow MakeAchievement(const TCHAR* Key, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Stat, float Threshold)
	{
		FCigAchievementRow R;
		R.Key = Key;
		R.Name = Name;
		R.Desc = Desc;
		R.Stat = Stat;
		R.Threshold = Threshold;
		return R;
	}

	TArray<FCigAchievementRow> DefaultAchievements()
	{
		return {
			MakeAchievement(TEXT("IlkDurum"),     TEXT("Bereketli Olsun"),    TEXT("İlk dürümünü sat"),                     TEXT("TotalServed"),          1.f),
			MakeAchievement(TEXT("YuzDurum"),     TEXT("Mahallenin Dilinde"), TEXT("100 dürüm sat"),                        TEXT("TotalServed"),        100.f),
			MakeAchievement(TEXT("BinDurum"),     TEXT("Dürüm Makinesi"),     TEXT("1000 dürüm sat"),                       TEXT("TotalServed"),       1000.f),
			MakeAchievement(TEXT("IlkBinLira"),   TEXT("İlk Bin"),            TEXT("Toplam 1.000 TL kazan"),                TEXT("TotalEarned"),       1000.f),
			MakeAchievement(TEXT("OnBinLira"),    TEXT("On Bin"),             TEXT("Toplam 10.000 TL kazan"),               TEXT("TotalEarned"),      10000.f),
			MakeAchievement(TEXT("KasaDolu"),     TEXT("Kasa Dolu"),          TEXT("Kasanda aynı anda 20.000 TL bulundur"), TEXT("Money"),            20000.f),
			MakeAchievement(TEXT("MukemmelOn"),   TEXT("Usta İşi"),           TEXT("10 kusursuz sipariş çıkar"),            TEXT("TotalPerfectOrders"),  10.f),
			MakeAchievement(TEXT("MukemmelYuz"),  TEXT("Kusursuz Yüz"),       TEXT("100 kusursuz sipariş çıkar"),           TEXT("TotalPerfectOrders"), 100.f),
			MakeAchievement(TEXT("IlkTeslimat"),  TEXT("Kurye"),              TEXT("İlk paket servisini teslim et"),        TEXT("TotalDeliveries"),      1.f),
			MakeAchievement(TEXT("ElliTeslimat"), TEXT("Mahalle Kuryesi"),    TEXT("50 paket servis teslim et"),            TEXT("TotalDeliveries"),     50.f),
			MakeAchievement(TEXT("BesYildiz"),    TEXT("Beş Yıldız"),         TEXT("İtibarı 90'ın üstüne çıkar"),           TEXT("Rep"),                 90.f),
			MakeAchievement(TEXT("TemizMutfak"),  TEXT("Tertemiz"),           TEXT("Hijyeni %95'in üstünde tut"),           TEXT("Hygiene"),             95.f),
			MakeAchievement(TEXT("OnuncuGun"),    TEXT("Onuncu Gün"),         TEXT("10. güne ulaş"),                        TEXT("Day"),                 10.f),
			MakeAchievement(TEXT("OtuzuncuGun"),  TEXT("Bir Ay Dayandı"),     TEXT("30. güne ulaş"),                        TEXT("Day"),                 30.f),
			MakeAchievement(TEXT("SonSeviye"),    TEXT("Çiğköfte Ustası"),    TEXT("Seviye 10'a ulaş"),                     TEXT("Level"),               10.f),
			MakeAchievement(TEXT("EvSahibi"),     TEXT("Ev Sahibi"),          TEXT("Mahalledeki evi satın al"),             TEXT("OwnHouse"),             1.f),
			MakeAchievement(TEXT("KediDostu"),    TEXT("Kedi Dostu"),         TEXT("Dükkânın kedisini doyur ve sev"),       TEXT("CatHappy"),            90.f),
			MakeAchievement(TEXT("CirakPatronu"), TEXT("Patron"),             TEXT("Bir çırak işe al"),                     TEXT("ApprenticeHired"),      1.f),
			MakeAchievement(TEXT("TumDukkan"),    TEXT("Tam Donanım"),        TEXT("Bütün dükkân geliştirmelerini al"),     TEXT("AllUpgrades"),          1.f),
			MakeAchievement(TEXT("Devretme"),     TEXT("Devretme"),           TEXT("Dükkânı devredip yeniden başla"),       TEXT("PrestigeCount"),        1.f)
		};
	}

	FCigPricingRow MakePricing(int32 Index, const TCHAR* Label, int32 TabanFiyat, float Esneklik,
		float MinCarpan, float MaxCarpan)
	{
		FCigPricingRow R;
		R.Index = Index;
		R.Label = Label;
		R.TabanFiyat = TabanFiyat;
		R.Esneklik = Esneklik;
		R.MinCarpan = MinCarpan;
		R.MaxCarpan = MaxCarpan;
		return R;
	}

	TArray<FCigPricingRow> DefaultPricing()
	{
		// Staples carry a lower elasticity than treats on purpose: nobody walks
		// past the shop over five lira on a wrap, but they will skip the kunefe.
		return {
			MakePricing(CigUrunDurum,        TEXT("Dürüm"),               65, 1.2f, 0.6f, 1.8f),
			MakePricing(CigUrunCiftPorsiyon, TEXT("Çift porsiyon dürüm"), 110, 1.3f, 0.6f, 1.8f),
			MakePricing(CigUrunGarnitur,     TEXT("Garnitür (adet)"),      5, 0.8f, 0.5f, 2.0f),
			MakePricing(CigUrunAyran,        TEXT("Ayran"),               20, 1.1f, 0.5f, 2.0f),
			MakePricing(CigUrunIcliKofte,    TEXT("İçli köfte"),          45, 1.5f, 0.5f, 2.0f),
			MakePricing(CigUrunCorba,        TEXT("Mercimek çorbası"),    35, 1.4f, 0.5f, 2.0f),
			MakePricing(CigUrunKunefe,       TEXT("Künefe"),              70, 1.8f, 0.5f, 2.0f),
			MakePricing(CigUrunCay,          TEXT("Çay"),                 10, 0.9f, 0.5f, 2.0f)
		};
	}

	FCigParamRow MakeInspection(const TCHAR* Key, const TCHAR* Label, float Deger)
	{
		FCigParamRow R;
		R.Key = Key;
		R.Label = Label;
		R.Deger = Deger;
		return R;
	}

	TArray<FCigParamRow> DefaultInspection()
	{
		return {
			MakeInspection(TEXT("GecerNotu"),            TEXT("Geçme notu (0-100)"),                       60.f),
			MakeInspection(TEXT("HijyenAgirligi"),       TEXT("Hijyenin denetim puanındaki ağırlığı"),      0.50f),
			MakeInspection(TEXT("TazelikAgirligi"),      TEXT("Stok tazeliğinin ağırlığı"),                 0.30f),
			MakeInspection(TEXT("RuhsatAgirligi"),       TEXT("Ruhsatın ağırlığı"),                         0.20f),
			MakeInspection(TEXT("RuhsatsizTavan"),       TEXT("Ruhsatsızken alınabilecek en yüksek puan"), 45.f),
			MakeInspection(TEXT("CezaTabani"),           TEXT("Taban para cezası (TL)"),                  300.f),
			MakeInspection(TEXT("RuhsatsizCezaCarpani"), TEXT("Ruhsat geçersizse ceza çarpanı"),            2.0f),
			MakeInspection(TEXT("KapatmaEsigi"),         TEXT("Kaç başarısız denetimde geçici kapatma"),    3.f),
			MakeInspection(TEXT("RuhsatSuresi"),         TEXT("Ruhsat geçerlilik süresi (gün)"),           14.f),
			MakeInspection(TEXT("RuhsatUcreti"),         TEXT("Ruhsat yenileme ücreti (TL)"),             250.f),
			MakeInspection(TEXT("RusvetOrani"),          TEXT("Rüşvetin cezaya oranı"),                     0.60f),
			MakeInspection(TEXT("RusvetRiskArtisi"),     TEXT("Her rüşvette yakalanma riski artışı"),       0.15f),
			MakeInspection(TEXT("SikayetRiskArtisi"),    TEXT("Rakip şikâyetinin denetim olasılığına etkisi"), 0.25f)
		};
	}

	TArray<FCigParamRow> DefaultSocial()
	{
		return {
			MakeInspection(TEXT("TakipciEtkisi"),        TEXT("Takipçi çarpanının gücü"),                        0.18f),
			MakeInspection(TEXT("TakipciTavan"),         TEXT("Takipçiden gelebilecek en yüksek çarpan"),        1.80f),
			MakeInspection(TEXT("GunlukGonderi"),        TEXT("Günlük gönderi hakkı"),                           2.f),
			MakeInspection(TEXT("TanitimTakipci"),       TEXT("Ürün tanıtımının getirdiği takipçi"),            40.f),
			MakeInspection(TEXT("KampanyaTakipci"),      TEXT("Kampanya duyurusunun getirdiği takipçi"),        15.f),
			MakeInspection(TEXT("KampanyaUcreti"),       TEXT("Kampanya duyurusu ücreti (TL)"),                150.f),
			MakeInspection(TEXT("KampanyaSpawnEtkisi"),  TEXT("Kampanyanın o günkü müşteri akışına etkisi"),     0.25f),
			MakeInspection(TEXT("SavunTakipci"),         TEXT("Savunmanın takipçi etkisi (kötü yoruma)"),      -25.f),
			MakeInspection(TEXT("SavunItibar"),          TEXT("Savunmanın itibar etkisi"),                      -2.f),
			MakeInspection(TEXT("OzurTakipci"),          TEXT("Özrün takipçi etkisi"),                          10.f),
			MakeInspection(TEXT("OzurItibar"),           TEXT("Özrün itibar etkisi"),                            3.f),
			MakeInspection(TEXT("GormezdenTakipci"),     TEXT("Görmezden gelmenin takipçi etkisi"),             -8.f),
			MakeInspection(TEXT("FenomenKazanc"),        TEXT("Memnun fenomenin getirdiği takipçi"),           350.f),
			MakeInspection(TEXT("FenomenKayip"),         TEXT("Küsen fenomenin götürdüğü takipçi oranı"),        0.35f),
			MakeInspection(TEXT("ViralSans"),            TEXT("Viral gün olasılığı (takipçi başına)"),           0.00004f),
			MakeInspection(TEXT("ViralCarpan"),          TEXT("Viral günde müşteri akışı çarpanı"),              3.0f),
			MakeInspection(TEXT("ViralStokEsigi"),       TEXT("Viral günün ters tepmemesi için gereken stok"),  25.f),
			MakeInspection(TEXT("ViralTersTepmeItibar"), TEXT("Viral gün stoksuz geçerse itibar kaybı"),       -15.f)
		};
	}

	FCigEventRow MakeEvent(const TCHAR* Key, const TCHAR* Label, int32 MinGun, float Sans, float Sure,
		float Spawn, float Sabir, float Fiyat, float Teslimat, float Stok, int32 TakvimPeriyodu)
	{
		FCigEventRow R;
		R.Key = Key;
		R.Label = Label;
		R.MinGun = MinGun;
		R.Sans = Sans;
		R.Sure = Sure;
		R.SpawnCarpani = Spawn;
		R.SabirCarpani = Sabir;
		R.FiyatCarpani = Fiyat;
		R.TeslimatCarpani = Teslimat;
		R.StokCarpani = Stok;
		R.TakvimPeriyodu = TakvimPeriyodu;
		return R;
	}

	TArray<FCigEventRow> DefaultEvents()
	{
		// Order is load-bearing: CigEventSystem exposes EventRain, EventHeat and
		// EventPowerOut as indices into this table.
		return {
			MakeEvent(TEXT("OkulCikisi"),        TEXT("Okul Çıkışı"),        1, 0.20f, 45.f, 1.8f, 0.80f, 0.95f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("MacGunu"),           TEXT("Maç Günü"),           2, 0.15f, 60.f, 1.6f, 0.70f, 1.10f, 1.2f, 1.0f,  7),
			MakeEvent(TEXT("Yagmur"),            TEXT("Yağmur"),             1, 0.18f, -1.f, 0.6f, 1.10f, 1.00f, 1.7f, 1.0f,  0),
			MakeEvent(TEXT("SicakHava"),         TEXT("Sıcak Hava"),         2, 0.15f, -1.f, 1.1f, 0.90f, 1.00f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("IsotZammi"),         TEXT("İsot Zammı"),         3, 0.12f, -1.f, 1.0f, 1.00f, 1.00f, 1.0f, 1.5f,  0),
			MakeEvent(TEXT("TedarikGecikmesi"),  TEXT("Tedarik Gecikmesi"),  2, 0.12f, -1.f, 1.0f, 1.00f, 1.00f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("ElektrikKesintisi"), TEXT("Elektrik Kesintisi"), 3, 0.10f, 50.f, 0.9f, 1.00f, 1.00f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("BelediyeDenetimi"),  TEXT("Belediye Denetimi"),  2, 0.12f, -1.f, 1.0f, 1.00f, 1.00f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("UnluMusteri"),       TEXT("Ünlü Müşteri"),       3, 0.10f, 40.f, 1.2f, 1.00f, 1.00f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("FenomenZiyareti"),   TEXT("Fenomen Ziyareti"),   4, 0.10f, 40.f, 1.1f, 1.00f, 1.00f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("RakipKampanyasi"),   TEXT("Rakip Kampanyası"),   2, 0.12f, -1.f, 0.7f, 1.00f, 1.00f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("SokakFestivali"),    TEXT("Sokak Festivali"),    4, 0.10f, -1.f, 1.9f, 0.85f, 1.15f, 1.3f, 1.0f, 12),
			MakeEvent(TEXT("TopluSiparis"),      TEXT("Toplu Sipariş"),      3, 0.12f, 60.f, 1.0f, 1.00f, 1.00f, 1.0f, 1.0f,  0),
			MakeEvent(TEXT("MalzemeKitligi"),    TEXT("Malzeme Kıtlığı"),    3, 0.08f, -1.f, 1.0f, 1.00f, 1.00f, 1.0f, 1.3f,  0)
		};
	}

	FCigStaffRow MakeStaff(const TCHAR* Key, const TCHAR* Label, float Hiz, float Titizlik,
		float GulerYuz, int32 MaasBeklentisi)
	{
		FCigStaffRow R;
		R.Key = Key;
		R.Label = Label;
		R.Hiz = Hiz;
		R.Titizlik = Titizlik;
		R.GulerYuz = GulerYuz;
		R.MaasBeklentisi = MaasBeklentisi;
		return R;
	}

	TArray<FCigStaffRow> DefaultStaff()
	{
		// Ordered cheap to dear, because the hiring screen lists them in table
		// order and reading it as a price ladder makes the trade-off obvious.
		return {
			MakeStaff(TEXT("Cirak"),      TEXT("Çırak"),           0.85f, 0.85f, 1.00f,  90),
			MakeStaff(TEXT("Ogrenci"),    TEXT("Öğrenci"),         0.80f, 0.95f, 1.15f,  80),
			MakeStaff(TEXT("Guleryuzlu"), TEXT("Güler yüzlü"),     0.95f, 0.95f, 1.35f, 150),
			MakeStaff(TEXT("Titiz"),      TEXT("Titiz eleman"),    0.95f, 1.35f, 1.00f, 170),
			MakeStaff(TEXT("Aceleci"),    TEXT("Aceleci"),         1.35f, 0.70f, 0.85f, 140),
			MakeStaff(TEXT("Tecrubeli"),  TEXT("Tecrübeli usta"),  1.30f, 1.10f, 0.90f, 230)
		};
	}

	FCigMahalleRow MakeMahalle(int32 Index, const TCHAR* Label, float GelirCarpani)
	{
		FCigMahalleRow R;
		R.Index = Index;
		R.Label = Label;
		R.GelirCarpani = GelirCarpani;
		return R;
	}

	TArray<FCigMahalleRow> DefaultMahalle()
	{
		return {
			MakeMahalle(0, TEXT("Ana cadde"),                 1.00f),
			MakeMahalle(1, TEXT("Semt Pazarı"),               0.90f),
			MakeMahalle(2, TEXT("Cumhuriyet Meydanı"),        1.05f),
			MakeMahalle(3, TEXT("Okul ve Park"),              0.95f),
			MakeMahalle(4, TEXT("Sanayi - Tedarikçi Deposu"), 0.85f),
			MakeMahalle(5, TEXT("Şehir Stadı"),               1.10f),
			MakeMahalle(6, TEXT("Sahil Kordonu"),             1.30f)
		};
	}

	// ------------------------------------------------------------------ table storage

	struct FCigBalanceTables
	{
		TArray<FCigSkillRow> Skills;
		TArray<FCigUpgradeRow> Upgrades;
		TArray<FCigTraitRow> Traits;
		TArray<FCigStockRow> Stock;
		TArray<FCigAchievementRow> Achievements;
		TArray<FCigPricingRow> Pricing;
		TArray<FCigMahalleRow> Mahalle;
		TArray<FCigStaffRow> Staff;
		TArray<FCigEventRow> Events;
		TArray<FCigParamRow> Inspection;
		TArray<FCigParamRow> Social;
		bool bLoaded = false;

		void Load()
		{
			Skills = DefaultSkills();
			Upgrades = DefaultUpgrades();
			Traits = DefaultTraits();
			Stock = DefaultStock();
			Achievements = DefaultAchievements();
			Pricing = DefaultPricing();
			Mahalle = DefaultMahalle();
			Staff = DefaultStaff();
			Events = DefaultEvents();
			Inspection = DefaultInspection();
			Social = DefaultSocial();

			ForEachCsvRow(TEXT("Skills.csv"), [this](const FCigCsvRow& Row)
			{
				if (FCigSkillRow* R = FindByKey(Skills, Row))
				{
					Row.Str(TEXT("Name"), R->Name);
					Row.Str(TEXT("Desc"), R->Desc);
					Row.Int(TEXT("MaxRank"), R->MaxRank);
					Row.Flt(TEXT("EffectPerRank"), R->EffectPerRank);
				}
			});

			ForEachCsvRow(TEXT("Upgrades.csv"), [this](const FCigCsvRow& Row)
			{
				if (FCigUpgradeRow* R = FindByKey(Upgrades, Row))
				{
					Row.Str(TEXT("Name"), R->Name);
					Row.Str(TEXT("Desc"), R->Desc);
					Row.Int(TEXT("Cost"), R->Cost);
					Row.Int(TEXT("MinLevel"), R->MinLevel);
				}
			});

			ForEachCsvRow(TEXT("Traits.csv"), [this](const FCigCsvRow& Row)
			{
				if (FCigTraitRow* R = FindByKey(Traits, Row))
				{
					Row.Str(TEXT("Name"), R->Name);
					Row.Flt(TEXT("Weight"), R->Weight);
					Row.Int(TEXT("MinDay"), R->MinDay);
					Row.Flt(TEXT("RareChance"), R->RareChance);
					Row.Flt(TEXT("PatienceMult"), R->PatienceMult);
					Row.Flt(TEXT("TipChanceDelta"), R->TipChanceDelta);
					Row.Flt(TEXT("TipMultOverride"), R->TipMultOverride);
				}
			});

			ForEachCsvRow(TEXT("Stock.csv"), [this](const FCigCsvRow& Row)
			{
				int32 Index = -1;
				Row.Int(TEXT("Index"), Index);
				if (!Stock.IsValidIndex(Index))
				{
					UE_LOG(LogCig, Warning, TEXT("Stock.csv: geçersiz Index %d — satır atlandı."), Index);
					return;
				}
				FCigStockRow& R = Stock[Index];
				Row.Str(TEXT("Label"), R.Label);
				Row.Int(TEXT("BaseCost"), R.BaseCost);
				Row.Int(TEXT("StartAmount"), R.StartAmount);
				Row.Int(TEXT("OrderAmount"), R.OrderAmount);
			});

			ForEachCsvRow(TEXT("Pricing.csv"), [this](const FCigCsvRow& Row)
			{
				int32 Index = -1;
				Row.Int(TEXT("Index"), Index);
				if (!Pricing.IsValidIndex(Index))
				{
					UE_LOG(LogCig, Warning, TEXT("Pricing.csv: geçersiz Index %d — satır atlandı."), Index);
					return;
				}
				FCigPricingRow& R = Pricing[Index];
				Row.Str(TEXT("Label"), R.Label);
				Row.Int(TEXT("TabanFiyat"), R.TabanFiyat);
				Row.Flt(TEXT("Esneklik"), R.Esneklik);
				Row.Flt(TEXT("MinCarpan"), R.MinCarpan);
				Row.Flt(TEXT("MaxCarpan"), R.MaxCarpan);
			});

			ForEachCsvRow(TEXT("Mahalle.csv"), [this](const FCigCsvRow& Row)
			{
				int32 Index = -1;
				Row.Int(TEXT("Index"), Index);
				if (!Mahalle.IsValidIndex(Index))
				{
					UE_LOG(LogCig, Warning, TEXT("Mahalle.csv: geçersiz Index %d — satır atlandı."), Index);
					return;
				}
				FCigMahalleRow& R = Mahalle[Index];
				Row.Str(TEXT("Label"), R.Label);
				Row.Flt(TEXT("GelirCarpani"), R.GelirCarpani);
			});

			ForEachCsvRow(TEXT("Inspection.csv"), [this](const FCigCsvRow& Row)
			{
				if (FCigParamRow* R = FindByKey(Inspection, Row))
				{
					Row.Str(TEXT("Label"), R->Label);
					Row.Flt(TEXT("Deger"), R->Deger);
				}
			});

			ForEachCsvRow(TEXT("Social.csv"), [this](const FCigCsvRow& Row)
			{
				if (FCigParamRow* R = FindByKey(Social, Row))
				{
					Row.Str(TEXT("Label"), R->Label);
					Row.Flt(TEXT("Deger"), R->Deger);
				}
			});

			ForEachCsvRow(TEXT("Events.csv"), [this](const FCigCsvRow& Row)
			{
				if (FCigEventRow* R = FindByKey(Events, Row))
				{
					Row.Str(TEXT("Label"), R->Label);
					Row.Int(TEXT("MinGun"), R->MinGun);
					Row.Flt(TEXT("Sans"), R->Sans);
					Row.Flt(TEXT("Sure"), R->Sure);
					Row.Flt(TEXT("SpawnCarpani"), R->SpawnCarpani);
					Row.Flt(TEXT("SabirCarpani"), R->SabirCarpani);
					Row.Flt(TEXT("FiyatCarpani"), R->FiyatCarpani);
					Row.Flt(TEXT("TeslimatCarpani"), R->TeslimatCarpani);
					Row.Flt(TEXT("StokCarpani"), R->StokCarpani);
					Row.Int(TEXT("TakvimPeriyodu"), R->TakvimPeriyodu);
				}
			});

			ForEachCsvRow(TEXT("Staff.csv"), [this](const FCigCsvRow& Row)
			{
				if (FCigStaffRow* R = FindByKey(Staff, Row))
				{
					Row.Str(TEXT("Label"), R->Label);
					Row.Flt(TEXT("Hiz"), R->Hiz);
					Row.Flt(TEXT("Titizlik"), R->Titizlik);
					Row.Flt(TEXT("GulerYuz"), R->GulerYuz);
					Row.Int(TEXT("MaasBeklentisi"), R->MaasBeklentisi);
				}
			});

			ForEachCsvRow(TEXT("Achievements.csv"), [this](const FCigCsvRow& Row)
			{
				if (FCigAchievementRow* R = FindByKey(Achievements, Row))
				{
					Row.Str(TEXT("Name"), R->Name);
					Row.Str(TEXT("Desc"), R->Desc);
					Row.Str(TEXT("Stat"), R->Stat);
					Row.Flt(TEXT("Threshold"), R->Threshold);
				}
			});

			bLoaded = true;
		}
	};

	FCigBalanceTables& Tables()
	{
		static FCigBalanceTables Data;
		if (!Data.bLoaded)
		{
			Data.Load();
		}
		return Data;
	}

	// An out-of-range index returns the last row, so callers need no separate
	// validity check (this matches existing CigSkillDef/CigUpgradeDef behaviour).
	template <typename TRow>
	const TRow& At(const TArray<TRow>& Table, int32 Index)
	{
		return Table[FMath::Clamp(Index, 0, Table.Num() - 1)];
	}
}

// Table lengths must match the enums exactly; if one grows, this catches it.
static_assert((int32)ECigSkill::COUNT == 8, "Skills tablosu ECigSkill ile eşleşmiyor (CigBalance.cpp: DefaultSkills)");
static_assert((int32)ECigUpgrade::COUNT == 13, "Upgrades tablosu ECigUpgrade ile eşleşmiyor (CigBalance.cpp: DefaultUpgrades)");
static_assert((int32)ECigAchievement::COUNT == 20, "Achievements tablosu ECigAchievement ile eşleşmiyor (CigBalance.cpp: DefaultAchievements)");
static_assert(CigStockCount == 18, "Stock tablosu CigStockCount ile eşleşmiyor (CigBalance.cpp: DefaultStock)");
static_assert(CigTraitCount == 14, "Traits tablosu CigTraitCount ile eşleşmiyor (CigBalance.cpp: DefaultTraits)");
static_assert(CigUrunCount == 8, "Pricing tablosu CigUrunCount ile eşleşmiyor (CigBalance.cpp: DefaultPricing)");
static_assert((int32)ECigDistrict::COUNT == 6, "Mahalle tablosu ECigDistrict ile eşleşmiyor (CigBalance.cpp: DefaultMahalle)");

namespace CigBalance
{
	namespace
	{
		FString LocalizedBalanceText(const TCHAR* Prefix, const FString& Key, const TCHAR* Suffix, const FString& Fallback)
		{
			const FString TableKey = FString::Printf(TEXT("%s.%s.%s"), Prefix, *Key.ToLower(), Suffix);
			const FString Value = CigText::Get(*TableKey);
			return Value == TableKey ? Fallback : Value;
		}
	}

	const FCigSkillRow& Skill(int32 Index)             { return At(Tables().Skills, Index); }
	const FCigUpgradeRow& Upgrade(int32 Index)         { return At(Tables().Upgrades, Index); }
	const FCigStockRow& Stock(int32 Index)             { return At(Tables().Stock, Index); }
	const FCigAchievementRow& Achievement(int32 Index) { return At(Tables().Achievements, Index); }
	const FCigPricingRow& Pricing(int32 Index)         { return At(Tables().Pricing, Index); }
	const FCigMahalleRow& Mahalle(int32 Index)         { return At(Tables().Mahalle, Index); }
	const FCigStaffRow& Staff(int32 Index)             { return At(Tables().Staff, Index); }
	const FCigEventRow& Event(int32 Index)             { return At(Tables().Events, Index); }

	namespace
	{
		float ParamAra(const TArray<FCigParamRow>& Table, const TCHAR* Key, float Fallback)
		{
			for (const FCigParamRow& R : Table)
			{
				if (R.Key.Equals(Key, ESearchCase::IgnoreCase))
				{
					return R.Deger;
				}
			}
			return Fallback;
		}
	}

	float Inspection(const TCHAR* Key, float Fallback) { return ParamAra(Tables().Inspection, Key, Fallback); }
	float Social(const TCHAR* Key, float Fallback)     { return ParamAra(Tables().Social, Key, Fallback); }
	int32 MahalleCount() { return Tables().Mahalle.Num(); }
	int32 StaffCount() { return Tables().Staff.Num(); }
	const FCigTraitRow& Trait(int32 Index)             { return At(Tables().Traits, Index); }
	FString SkillName(int32 Index)                     { const FCigSkillRow& R = Skill(Index); return LocalizedBalanceText(TEXT("bal.skill"), R.Key, TEXT("name"), R.Name); }
	FString SkillDesc(int32 Index)                     { const FCigSkillRow& R = Skill(Index); return LocalizedBalanceText(TEXT("bal.skill"), R.Key, TEXT("desc"), R.Desc); }
	FString UpgradeName(int32 Index)                   { const FCigUpgradeRow& R = Upgrade(Index); return LocalizedBalanceText(TEXT("bal.upgrade"), R.Key, TEXT("name"), R.Name); }
	FString UpgradeDesc(int32 Index)                   { const FCigUpgradeRow& R = Upgrade(Index); return LocalizedBalanceText(TEXT("bal.upgrade"), R.Key, TEXT("desc"), R.Desc); }
	FString TraitName(int32 Index)                     { const FCigTraitRow& R = Trait(Index); return LocalizedBalanceText(TEXT("bal.trait"), R.Key, TEXT("name"), R.Name); }
	FString AchievementName(int32 Index)               { const FCigAchievementRow& R = Achievement(Index); return LocalizedBalanceText(TEXT("bal.achievement"), R.Key, TEXT("name"), R.Name); }
	FString AchievementDesc(int32 Index)               { const FCigAchievementRow& R = Achievement(Index); return LocalizedBalanceText(TEXT("bal.achievement"), R.Key, TEXT("desc"), R.Desc); }

	int32 TraitCount() { return Tables().Traits.Num(); }

	int32 TraitIndexOfMask(uint16 SingleBitMask)
	{
		if (SingleBitMask == 0 || (SingleBitMask & (SingleBitMask - 1)) != 0)
		{
			return -1; // sıfır ya da birden çok bit
		}
		return FMath::FloorLog2(SingleBitMask);
	}

	uint16 TraitMaskOfIndex(int32 Index)
	{
		return (Index >= 0 && Index < 16) ? (uint16)(1u << Index) : 0;
	}

	float TraitPatienceMult(uint16 TraitMask)
	{
		float Mult = 1.f;
		const int32 Count = TraitCount();
		for (int32 i = 0; i < Count; ++i)
		{
			if (TraitMask & TraitMaskOfIndex(i))
			{
				Mult *= Trait(i).PatienceMult;
			}
		}
		return Mult;
	}

	float TraitTipChanceDelta(uint16 TraitMask)
	{
		float Delta = 0.f;
		const int32 Count = TraitCount();
		for (int32 i = 0; i < Count; ++i)
		{
			if (TraitMask & TraitMaskOfIndex(i))
			{
				Delta += Trait(i).TipChanceDelta;
			}
		}
		return Delta;
	}

	float TraitTipMultOverride(uint16 TraitMask)
	{
		float Best = 0.f;
		const int32 Count = TraitCount();
		for (int32 i = 0; i < Count; ++i)
		{
			if (TraitMask & TraitMaskOfIndex(i))
			{
				Best = FMath::Max(Best, Trait(i).TipMultOverride);
			}
		}
		return Best;
	}

	void Reload()
	{
		Tables().Load();
		UE_LOG(LogCig, Log, TEXT("Denge tabloları yeniden yüklendi."));
	}
}
