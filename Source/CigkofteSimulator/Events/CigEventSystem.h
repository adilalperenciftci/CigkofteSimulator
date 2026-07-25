#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigEventSystem.generated.h"

constexpr int32 CigEventDefCount = 14;

// Aktif bir olay.
struct FCigActiveEvent
{
	int32 DefIndex = 0;
	float TimeLeft = 0.f; // <0 ise gün boyu
};

// A bulk order that lands days before it is due.
//
// The offer is the whole point: a wedding worth a week's takings is only worth
// taking if the shop can actually turn out that many wraps on the day, and
// accepting one it cannot is meant to hurt.
struct FCigTopluSiparis
{
	bool bTeklifVar = false;
	bool bKabulEdildi = false;
	int32 TeslimGunu = 0;
	int32 IstenenAdet = 0;
	int32 Odul = 0;

	// TotalServed at the moment the order was accepted; progress is measured
	// against it so wraps sold before accepting do not count.
	int32 BaslangicServis = 0;
};

// How a finished bulk order settles up.
struct FCigTopluSonuc
{
	float OdulOrani = 0.f;
	float ItibarFarki = 0.f;
};

// Random daily events; the other systems read their multipliers from here.
UCLASS()
class UCigEventSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;
	virtual void OnDayStart(int32 Day) override;
	virtual void OnDayEnd(int32 Day) override;

	// An event's wording comes from Config/Text/Strings.csv, keyed off the row
	// key: event.<key>.name / .start / .end. Nothing about an event lives in code
	// any more - the numbers are in Events.csv and the prose is in the text table.
	static FString EventName(int32 Index);
	static FString EventStartMsg(int32 Index);
	static FString EventEndMsg(int32 Index);

	// Triggers an event by hand, for debugging and tests.
	void TriggerEvent(int32 DefIndex);

	// Whether a specific event is active right now (read by the atmosphere layer).
	bool IsEventActive(int32 DefIndex) const;

	// The event indices the atmosphere layer uses.
	static constexpr int32 EventRain = 2;      // Yağmur
	static constexpr int32 EventHeat = 3;      // Sıcak Hava
	static constexpr int32 EventPowerOut = 6;  // Elektrik Kesintisi

	// The bulk order draws its notice period and odds from this row.
	static constexpr int32 EventTopluSiparis = 12;
	static constexpr int32 EventTedarikGecikmesi = 5;

	float SpawnMult() const;
	float PatienceMult() const;
	float PriceMult() const;
	float DeliveryMult() const;
	float StockCostMult() const;
	float SupplyDelayMult() const;
	bool IsFridgeBroken() const;

	// Settles a finished bulk order. Pure so the reward-versus-penalty curve can
	// be checked without playing a week (see Tests/CigEventTests.cpp). Falling
	// just short still pays something, because a shop that delivered 19 of 20
	// has not failed the way one that delivered 3 has.
	static FCigTopluSonuc TopluSiparisSonucu(int32 Yapilan, int32 Istenen);

	// Retires every active event through EndEvent and returns how many were
	// retired. Day-long events carry TimeLeft < 0 and are never picked up by
	// UpdateSystem, so this is the only thing that ends them; emptying the array
	// instead would skip their end message. Also used by the debug commands to
	// clear the board.
	int32 TumOlaylariBitir();

	void TopluSiparisiKabulEt();
	void TopluSiparisiReddet();

	TArray<FCigActiveEvent> Active;
	FCigTopluSiparis TopluSiparis;

private:
	void StartEvent(int32 DefIndex);
	void EndEvent(int32 ActiveIndex);

	void ApplySpecialStart(int32 OzelTur);

	void TopluSiparisTeklifEt(int32 Day);
	void TopluSiparisiSonuclandir(int32 Day);
};
