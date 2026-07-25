#pragma once

#include "CoreMinimal.h"
#include "Core/CigkofteTypes.h"

// ============================== Unlocks ==============================
// The game does not start with everything open: stations and neighbourhood
// districts unlock by level, so day one is a plain counter and later days are
// a business that keeps growing.

// Level at which a station unlocks. 1 = open from the start.
inline int32 CigStationUnlockLevel(ECigStation S)
{
	switch (S)
	{
	// Hygiene upkeep stations are open from the start: cleanliness is penalised
	// from day one, so the player must always be able to wipe the counter, clear
	// cat hair and do the dishes. (Sink and bin already default to 1.)
	case ECigStation::Temizlik:  return 1;
	case ECigStation::Bulasik:   return 1;
	case ECigStation::Dograma:   return 2;
	case ECigStation::Cay:       return 2;
	case ECigStation::Eldiven:   return 3;
	case ECigStation::Paketleme: return 3;
	case ECigStation::YanUrun:   return 3;
	case ECigStation::MamaKabi:  return 4;
	case ECigStation::Buzdolabi: return 4;
	case ECigStation::Reklam:    return 5;
	case ECigStation::IsotPlus:  return 6;
	default:                     return 1;
	}
}

// Readable station name used in messages.
inline FString CigStationLabel(ECigStation S)
{
	switch (S)
	{
	case ECigStation::Bulgur:    return TEXT("Bulgur tezgâhı");
	case ECigStation::Isot:      return TEXT("İsot tezgâhı");
	case ECigStation::Salca:     return TEXT("Salça tezgâhı");
	case ECigStation::Su:        return TEXT("Su tezgâhı");
	case ECigStation::Baharat:   return TEXT("Baharat tezgâhı");
	case ECigStation::Yogurma:   return TEXT("Yoğurma tezgâhı");
	case ECigStation::Servis:    return TEXT("Servis bankosu");
	case ECigStation::Lavabo:    return TEXT("Lavabo");
	case ECigStation::Cop:       return TEXT("Çöp kovası");
	case ECigStation::Eldiven:   return TEXT("Eldiven rafı");
	case ECigStation::IsotPlus:  return TEXT("Özel isot");
	case ECigStation::Reklam:    return TEXT("Reklam panosu");
	case ECigStation::Dograma:   return TEXT("Doğrama tezgâhı");
	case ECigStation::Lavas:     return TEXT("Lavaş tezgâhı");
	case ECigStation::Paketleme: return TEXT("Paketleme tezgâhı");
	case ECigStation::Buzdolabi: return TEXT("Buzdolabı");
	case ECigStation::Temizlik:  return TEXT("Temizlik seti");
	case ECigStation::Bulasik:   return TEXT("Bulaşık evyesi");
	case ECigStation::Cay:       return TEXT("Çay ocağı");
	case ECigStation::MamaKabi:  return TEXT("Mama kabı");
	case ECigStation::Tarif:     return TEXT("Tarif panosu");
	case ECigStation::YanUrun:   return TEXT("Yan ürün tezgâhı");
	default:                     return TEXT("İstasyon");
	}
}

// Short blurb shown when the unlock fires.
inline FString CigStationUnlockName(ECigStation S)
{
	switch (S)
	{
	case ECigStation::Dograma:   return TEXT("Doğrama tezgâhı — garnitür hazırlayabilirsin");
	case ECigStation::Temizlik:  return TEXT("Temizlik seti — tezgâhı silebilirsin");
	case ECigStation::Cay:       return TEXT("Çay ocağı — enerji tazeleme");
	case ECigStation::Eldiven:   return TEXT("Eldiven rafı");
	case ECigStation::Paketleme: return TEXT("Paketleme tezgâhı — paket servis ve rafa kaldırma");
	case ECigStation::YanUrun:   return TEXT("Yan ürün tezgâhı — içli köfte, çorba, künefe, çay");
	case ECigStation::Bulasik:   return TEXT("Bulaşık evyesi");
	case ECigStation::MamaKabi:  return TEXT("Mama kabı — dükkânın kedisi geldi");
	case ECigStation::Buzdolabi: return TEXT("Buzdolabı — hamuru saklayabilirsin");
	case ECigStation::Reklam:    return TEXT("Reklam panosu");
	case ECigStation::IsotPlus:  return TEXT("Özel isot tedariki");
	default:                     return FString();
	}
}

// ============================== Neighbourhood districts ==============================

enum class ECigDistrict : uint8
{
	SemtPazari = 0,   // level 2
	Meydan,           // level 3
	OkulPark,         // level 4
	SanayiDepo,       // level 5
	Stadyum,          // level 6
	SahilKordon,      // level 7
	COUNT
};

struct FCigDistrictDef
{
	const TCHAR* Name;
	const TCHAR* Teaser;   // blurb shown on the barrier while it is locked
	int32 Level;
};

inline const FCigDistrictDef& CigDistrictDef(ECigDistrict D)
{
	static const FCigDistrictDef Defs[(int32)ECigDistrict::COUNT] = {
		{ TEXT("SEMT PAZARI"),      TEXT("Ucuz malzeme, taze sebze"),        2 },
		{ TEXT("CUMHURIYET MEYDANI"), TEXT("Kalabalik meydan, cok musteri"), 3 },
		{ TEXT("OKUL VE PARK"),     TEXT("Ogrenci akini"),                   4 },
		{ TEXT("SANAYI - TEDARIKCI DEPOSU"), TEXT("Toptan alim, buyuk siparis"), 5 },
		{ TEXT("SEHIR STADI"),      TEXT("Mac gunu izdihami"),               6 },
		{ TEXT("SAHIL KORDONU"),    TEXT("Yazlik kalabalik, yuksek bahsis"), 7 }
	};
	return Defs[FMath::Clamp((int32)D, 0, (int32)ECigDistrict::COUNT - 1)];
}
