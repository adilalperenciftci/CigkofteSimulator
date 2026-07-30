#pragma once

#include "CoreMinimal.h"
#include "Core/CigBalance.h"
#include "Core/CigText.h"

// ============================== Ingredients & heat ==============================

enum class ECigIngredient : uint8
{
	Bulgur = 0,
	Isot,
	Salca,
	Su,
	Baharat,
	COUNT
};

enum class ECigSpice : uint8
{
	AzAci = 0,
	Orta,
	CokAci
};

// ============================== Stations ==============================

enum class ECigStation : uint8
{
	Bulgur = 0,
	Isot,
	Salca,
	Su,
	Baharat,
	Yogurma,
	Servis,
	Lavabo,
	Cop,
	Eldiven,
	IsotPlus,
	Reklam,
	Dograma,
	// Wrap assembly & shop-running stations
	Lavas,      // lay down a flatbread, start building a wrap
	Paketleme,  // switch takeaway/plate; move a rolled wrap to the shelf
	Buzdolabi,  // put dough in the fridge / take it out (keeps it fresh)
	Temizlik,   // wipe the counter and the chopping area
	Bulasik,    // wash the dishes that have piled up
	Cay,        // tea break (energy)
	MamaKabi,   // cat food
	Tarif,      // recipe selection board
	YanUrun     // menu sides: icli kofte, soup, kunefe, tea
};

// ============================== Stock ==============================
// 0-4 are the main ingredients, aligned with ECigIngredient; toppings and
// consumables follow.

constexpr int32 CigStockMarul = 5;
constexpr int32 CigStockAyran = 6;
constexpr int32 CigStockMaydanoz = 7;
constexpr int32 CigStockDomates = 8;
constexpr int32 CigStockTursu = 9;
constexpr int32 CigStockSogan = 10;
constexpr int32 CigStockLimon = 11;
constexpr int32 CigStockNarEksisi = 12;
constexpr int32 CigStockLavas = 13;
// Menu sides (bought ready-made, added to an order at the counter)
constexpr int32 CigStockIcliKofte = 14;
constexpr int32 CigStockCorba = 15;
constexpr int32 CigStockKunefe = 16;
constexpr int32 CigStockCayBardak = 17;
constexpr int32 CigStockCount = 18;

// ============================== Menu: sides ==============================
// Ready-made items sold alongside a wrap. Picked at the sides station.

enum class ECigSide : uint8
{
	Yok = 0,
	IcliKofte,
	Corba,
	Kunefe,
	Cay,
	COUNT
};

inline FString CigSideName(ECigSide S)
{
	switch (S)
	{
	case ECigSide::IcliKofte: return CigText::Get(TEXT("side.iclikofte"));
	case ECigSide::Corba:     return CigText::Get(TEXT("side.corba"));
	case ECigSide::Kunefe:    return CigText::Get(TEXT("side.kunefe"));
	case ECigSide::Cay:       return CigText::Get(TEXT("side.cay"));
	default:                  return CigText::Get(TEXT("side.none"));
	}
}

// Stock item backing a side (-1 = none).
inline int32 CigSideStockIndex(ECigSide S)
{
	switch (S)
	{
	case ECigSide::IcliKofte: return CigStockIcliKofte;
	case ECigSide::Corba:     return CigStockCorba;
	case ECigSide::Kunefe:    return CigStockKunefe;
	case ECigSide::Cay:       return CigStockCayBardak;
	default:                  return -1;
	}
}

// ============================== Priceable products ==============================
// Everything the player can put a price on from the tablet. Indices address
// Config/Balance/Pricing.csv the same way the CigStock* constants address
// Stock.csv, so the pricing table needs no enum of its own.

constexpr int32 CigUrunDurum = 0;          // single-portion wrap
constexpr int32 CigUrunCiftPorsiyon = 1;   // two-portion wrap
constexpr int32 CigUrunGarnitur = 2;       // charged per topping on the wrap
constexpr int32 CigUrunAyran = 3;
constexpr int32 CigUrunIcliKofte = 4;
constexpr int32 CigUrunCorba = 5;
constexpr int32 CigUrunKunefe = 6;
constexpr int32 CigUrunCay = 7;
constexpr int32 CigUrunCount = 8;

// Product index of a side (-1 = none).
inline int32 CigSideUrunIndex(ECigSide S)
{
	switch (S)
	{
	case ECigSide::IcliKofte: return CigUrunIcliKofte;
	case ECigSide::Corba:     return CigUrunCorba;
	case ECigSide::Kunefe:    return CigUrunKunefe;
	case ECigSide::Cay:       return CigUrunCay;
	default:                  return -1;
	}
}

// List price of a side before the player's own markup. The markup lives in
// UCigPricingSystem because it is player state, not balance data.
inline int32 CigSidePrice(ECigSide S)
{
	const int32 Urun = CigSideUrunIndex(S);
	return Urun >= 0 ? CigBalance::Pricing(Urun).TabanFiyat : 0;
}

// ============================== Wrap toppings ==============================

enum class ECigTopping : uint8
{
	Marul = 0,
	Maydanoz,
	Domates,
	Tursu,
	Sogan,
	Limon,
	NarEksisi,
	COUNT
};

inline int32 CigStockIndexForTopping(ECigTopping T)
{
	switch (T)
	{
	case ECigTopping::Marul:     return CigStockMarul;
	case ECigTopping::Maydanoz:  return CigStockMaydanoz;
	case ECigTopping::Domates:   return CigStockDomates;
	case ECigTopping::Tursu:     return CigStockTursu;
	case ECigTopping::Sogan:     return CigStockSogan;
	case ECigTopping::Limon:     return CigStockLimon;
	case ECigTopping::NarEksisi: return CigStockNarEksisi;
	default:                     return -1;
	}
}

inline FString CigToppingName(ECigTopping T)
{
	switch (T)
	{
	case ECigTopping::Marul:     return CigText::Get(TEXT("top.marul"));
	case ECigTopping::Maydanoz:  return CigText::Get(TEXT("top.maydanoz"));
	case ECigTopping::Domates:   return CigText::Get(TEXT("top.domates"));
	case ECigTopping::Tursu:     return CigText::Get(TEXT("top.tursu"));
	case ECigTopping::Sogan:     return CigText::Get(TEXT("top.sogan"));
	case ECigTopping::Limon:     return CigText::Get(TEXT("top.limon"));
	case ECigTopping::NarEksisi: return CigText::Get(TEXT("top.nareksisi"));
	default:                     return CigText::Get(TEXT("common.unknown"));
	}
}

// ============================== Order spec ==============================
// The wrap a customer asked for; compared against what the player built.

struct FCigOrderSpec
{
	ECigSpice Spice = ECigSpice::Orta;
	int32 Portion = 1;              // 1 wrap, 2 portions
	uint8 ToppingMask = 0;          // toppings asked for (bit = ECigTopping)
	bool bWantsAyran = false;
	bool bPacked = false;           // takeaway or plate
	ECigSide Side = ECigSide::Yok;  // menu side asked for alongside

	bool WantsTopping(ECigTopping T) const { return (ToppingMask & (1 << (uint8)T)) != 0; }
	void SetTopping(ECigTopping T, bool bOn)
	{
		if (bOn) { ToppingMask |= (1 << (uint8)T); }
		else { ToppingMask &= ~(1 << (uint8)T); }
	}
};

// ============================== Customer traits ==============================

enum class ECigTrait : uint16
{
	None            = 0,
	Impatient       = 1 << 0,
	Patient         = 1 << 1,
	PriceSensitive  = 1 << 2,
	Generous        = 1 << 3,
	HygieneSensitive= 1 << 4,
	QualityFocused  = 1 << 5,
	Influencer      = 1 << 6,
	Tourist         = 1 << 7,
	Student         = 1 << 8,
	Family          = 1 << 9,
	Regular         = 1 << 10,
	SpicyFoodLover  = 1 << 11,
	Indecisive      = 1 << 12,
	SecretCritic    = 1 << 13
};
ENUM_CLASS_FLAGS(ECigTrait)

constexpr int32 CigTraitCount = 14;

// The name of a single-bit trait; comes from Config/Balance/Traits.csv.
inline FString CigTraitName(ECigTrait T)
{
	const int32 Index = CigBalance::TraitIndexOfMask((uint16)T);
	return Index >= 0 ? CigBalance::TraitName(Index) : CigText::Get(TEXT("common.unknown"));
}

// ============================== Sound events ==============================

enum class ECigSound : uint8
{
	Knead = 0,
	Chop,
	Serve,
	Cash,
	Success,
	Failure,
	CustomerAngry,
	CatMeow,
	CarEngine,
	QuestComplete,
	UIClick,
	UINav,
	DayStart,   // gün başı kısa jingle
	DayEnd,     // gün sonu jingle
	WrapCloth,  // dürüm sarma hışırtısı
	Pot,        // buzdolabı/kap sesi
	Knife,      // hızlı doğrama
	MenuMusic,  // menü müzik parçası (jingle döngüsü)
	COUNT
};

// ============================== Name & colour helpers ==============================

inline FString CigIngredientName(ECigIngredient I)
{
	switch (I)
	{
	case ECigIngredient::Bulgur:  return CigText::Get(TEXT("ing.bulgur"));
	case ECigIngredient::Isot:    return CigText::Get(TEXT("ing.isot"));
	case ECigIngredient::Salca:   return CigText::Get(TEXT("ing.salca"));
	case ECigIngredient::Su:      return CigText::Get(TEXT("ing.su"));
	case ECigIngredient::Baharat: return CigText::Get(TEXT("ing.baharat"));
	default:                      return CigText::Get(TEXT("common.unknown"));
	}
}

inline FLinearColor CigIngredientColor(ECigIngredient I)
{
	switch (I)
	{
	case ECigIngredient::Bulgur:  return FLinearColor(0.85f, 0.70f, 0.40f);
	case ECigIngredient::Isot:    return FLinearColor(0.35f, 0.05f, 0.02f);
	case ECigIngredient::Salca:   return FLinearColor(0.70f, 0.10f, 0.05f);
	case ECigIngredient::Su:      return FLinearColor(0.20f, 0.45f, 0.90f);
	case ECigIngredient::Baharat: return FLinearColor(0.30f, 0.50f, 0.15f);
	default:                      return FLinearColor::White;
	}
}

inline FString CigSpiceName(ECigSpice S)
{
	switch (S)
	{
	case ECigSpice::AzAci:  return CigText::Get(TEXT("spice.mild"));
	case ECigSpice::Orta:   return CigText::Get(TEXT("spice.medium"));
	case ECigSpice::CokAci: return CigText::Get(TEXT("spice.hot"));
	default:                return CigText::Get(TEXT("common.unknown"));
	}
}

// The heat, without diacritics, for text drawn in the world.
//
// UTextRenderComponent's font has no dotted capital I or cedilla, so "COK ACILI"
// over a customer's head is the Turkish spelled the way the font can render it -
// which is also why the locked-station sign says SEVIYE rather than SEVİYE. The
// English column needs no such compromise; this is a separate row from
// spice.* rather than a separate mechanism, so a translator sees both.
inline FString CigSpiceNameAscii(ECigSpice S)
{
	switch (S)
	{
	case ECigSpice::AzAci:  return CigText::Get(TEXT("spice.mild.ascii"));
	case ECigSpice::Orta:   return CigText::Get(TEXT("spice.medium.ascii"));
	case ECigSpice::CokAci: return CigText::Get(TEXT("spice.hot.ascii"));
	default:                return TEXT("?");
	}
}

inline FString CigStockName(int32 I)
{
	if (I >= 0 && I < (int32)ECigIngredient::COUNT)
	{
		return CigIngredientName((ECigIngredient)I);
	}
	switch (I)
	{
	case CigStockMarul:     return CigText::Get(TEXT("top.marul"));
	case CigStockAyran:     return CigText::Get(TEXT("stock.ayran"));
	case CigStockMaydanoz:  return CigText::Get(TEXT("top.maydanoz"));
	case CigStockDomates:   return CigText::Get(TEXT("top.domates"));
	case CigStockTursu:     return CigText::Get(TEXT("top.tursu"));
	case CigStockSogan:     return CigText::Get(TEXT("top.sogan"));
	case CigStockLimon:     return CigText::Get(TEXT("top.limon"));
	case CigStockNarEksisi: return CigText::Get(TEXT("top.nareksisi"));
	case CigStockLavas:     return CigText::Get(TEXT("stock.lavas"));
	case CigStockIcliKofte: return CigText::Get(TEXT("side.iclikofte"));
	case CigStockCorba:     return CigText::Get(TEXT("side.corba"));
	case CigStockKunefe:    return CigText::Get(TEXT("side.kunefe"));
	case CigStockCayBardak: return CigText::Get(TEXT("side.cay"));
	default:                return CigText::Get(TEXT("common.unknown"));
	}
}

// Turns a dough quality value into a readable name.
inline FString CigQualityName(float Q)
{
	if (Q >= 95.f) return CigText::Get(TEXT("quality.legendary"));
	if (Q >= 85.f) return CigText::Get(TEXT("quality.master"));
	if (Q >= 70.f) return CigText::Get(TEXT("quality.good"));
	if (Q >= 50.f) return CigText::Get(TEXT("quality.normal"));
	if (Q >= 30.f) return CigText::Get(TEXT("quality.weak"));
	return CigText::Get(TEXT("quality.failed"));
}
