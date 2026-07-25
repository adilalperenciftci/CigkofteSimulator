#pragma once

#include "CoreMinimal.h"
#include "Core/CigBalance.h"

// Shop upgrades. Each one changes the world visually and has a gameplay effect.
enum class ECigUpgrade : uint8
{
	BuyukBuzdolabi = 0, // freshness drops far more slowly
	IkinciYogurma,      // +40% kneading yield
	HizliDograma,       // chopping finishes in 2 strokes
	YeniTabela,         // +15% customer arrival rate
	Klima,              // +20% customer patience
	MuzikSistemi,       // +1 atmosphere, +8% patience
	IkinciKasa,         // +2 queue capacity
	DisOturma,          // more family customers, +0.5 atmosphere
	BuyukCop,           // bin fills twice as slowly
	HijyenEkipmani,     // dirt builds up 35% slower
	IyiLavabo,          // washing hands also partly cleans the counter
	Dekorasyon,         // +1 atmosphere, +10% reputation gain
	YeniSube,           // +250 TL passive daily income
	COUNT
};

// Name, description, cost and unlock level come from
// Config/Balance/Upgrades.csv; without the file, the defaults in
// CigBalance.cpp are used.
inline const FCigUpgradeRow& CigUpgradeDef(ECigUpgrade U)
{
	return CigBalance::Upgrade((int32)U);
}
