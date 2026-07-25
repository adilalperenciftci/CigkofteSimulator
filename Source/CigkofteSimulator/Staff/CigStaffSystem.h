#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "CigStaffSystem.generated.h"

class ACigkofteCustomer;

// Jobs an apprentice can be assigned.
enum class ECigStaffTask : uint8
{
	Dograma = 0,
	Temizlik,
	Kasa,       // serves simple orders
	Paket,      // makes simple packages from dough
	Stok,       // reorders stock as it runs low
	COUNT
};

// Specialisms (earned at level 3 from whichever job was done most).
enum class ECigStaffSpec : uint8
{
	Yok = 0,
	ServisUzmani,
	DogramaUzmani,
	HijyenUzmani,
	PaketUzmani,
	CigkofteUstasi
};

// Apprentice state.
struct FCigApprentice
{
	bool bHired = false;
	FString Name;
	int32 Level = 1;
	int32 XP = 0;
	float Energy = 100.f;
	float Morale = 70.f;
	int32 Salary = 100;
	ECigStaffTask Task = ECigStaffTask::Dograma;
	ECigStaffSpec Spec = ECigStaffSpec::Yok;
	int32 TaskCounts[(int32)ECigStaffTask::COUNT] = { 0 };
	int32 DaysSinceRaise = 0;
	bool bWantsRaise = false;
};

// Apprentice management: hiring, job assignment, progression, morale and wages.
UCLASS()
class UCigStaffSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;
	virtual void OnDayStart(int32 Day) override;
	virtual void OnDayEnd(int32 Day) override;

	void Hire();
	void CycleTask();
	void GiveRaise();

	// Recreates the apprentice's NPC after a save is loaded.
	void RestoreNPC();

	static FString TaskName(ECigStaffTask T);
	static FString SpecName(ECigStaffSpec S);

	FCigApprentice Apprentice;

	UPROPERTY() TObjectPtr<ACigkofteCustomer> ApprenticeNPC;

private:
	float WorkTimer = 0.f;

	void DoWork();
	void GainXP(int32 Amount);
	void UpdateNPCPosition();
};
