#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigEventSystem.generated.h"

// A daily event definition. Duration <= 0 means it lasts all day.
struct FCigEventDef
{
	const TCHAR* Name;
	const TCHAR* StartMsg;
	const TCHAR* EndMsg;
	int32 MinDay;
	float Chance;
	float Duration;
	float SpawnMult;
	float PatienceMult;
	float PriceMult;
	float DeliveryMult;   // paket servis sıklığı
	float StockCostMult;
	int32 SpecialType;    // 0 yok, 1 müşteri patlaması, 2 müfettiş, 3 VIP, 4 fenomen, 5 toplu sipariş, 6 kıtlık, 7 buzdolabı arızası
};

constexpr int32 CigEventDefCount = 14;

// Aktif bir olay.
struct FCigActiveEvent
{
	int32 DefIndex = 0;
	float TimeLeft = 0.f; // <0 ise gün boyu
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

	static const FCigEventDef& Def(int32 Index);

	// Triggers an event by hand, for debugging and tests.
	void TriggerEvent(int32 DefIndex);

	// Whether a specific event is active right now (read by the atmosphere layer).
	bool IsEventActive(int32 DefIndex) const;

	// The event indices the atmosphere layer uses.
	static constexpr int32 EventRain = 2;      // Yağmur
	static constexpr int32 EventHeat = 3;      // Sıcak Hava
	static constexpr int32 EventPowerOut = 6;  // Elektrik Kesintisi

	float SpawnMult() const;
	float PatienceMult() const;
	float PriceMult() const;
	float DeliveryMult() const;
	float StockCostMult() const;
	float SupplyDelayMult() const;
	bool IsFridgeBroken() const;

	TArray<FCigActiveEvent> Active;

private:
	void StartEvent(int32 DefIndex);
	void EndEvent(int32 ActiveIndex);
	void ApplySpecialStart(const FCigEventDef& D);
};
