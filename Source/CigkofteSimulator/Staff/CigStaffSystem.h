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

// A walk-in the player can hire. Candidates are not saved: they are people who
// happened to drop by, so regenerating them on load costs nothing and keeps the
// save free of throwaway state.
struct FCigStaffAday
{
	FString Name;
	int32 Arketip = 0;
	float Hiz = 1.f;
	float Titizlik = 1.f;
	float GulerYuz = 1.f;
	int32 MaasBeklentisi = 100;
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

	// Which archetype they came from, and the traits they were hired with. All
	// three sit at 1 for an apprentice hired before the table existed, which is
	// exactly the behaviour that save migrates to.
	int32 Arketip = 0;
	float Hiz = 1.f;
	float Titizlik = 1.f;
	float GulerYuz = 1.f;

	// Consecutive days the wage could not be paid. Morale carries the damage;
	// this only counts so the messages can escalate honestly.
	int32 OdenmemisGun = 0;
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

	// Hires the candidate at AdayIndex. Hire() without an index takes the first,
	// which is what the story step and the older tablet shortcut expect.
	void Hire();
	void HireAday(int32 AdayIndex);
	void CycleTask();
	void GiveRaise();

	// Turns down the rival's offer by matching it. Costs the difference from
	// here on, which is the point: keeping someone good is not free.
	void KarsiTeklifVer();

	// Recreates the apprentice's NPC after a save is loaded.
	void RestoreNPC();

	static FString TaskName(ECigStaffTask T);
	static FString SpecName(ECigStaffSpec S);

	// Chance of fumbling one work tick. Pulled out as a free function because it
	// is the whole competence model and the only part worth testing on its own
	// (see Tests/CigStaffTests.cpp). Tidiness guards against mistakes,
	// experience grinds them down, exhaustion brings them back.
	static float HataOlasiligi(float Titizlik, int32 Seviye, float Enerji);

	// Seconds between work ticks. Faster staff and higher levels both shorten it.
	static float IsAraligi(float Hiz, int32 Seviye);

	FCigApprentice Apprentice;

	// Refreshed while nobody is hired; the player picks from these.
	TArray<FCigStaffAday> Adaylar;

	// Standing offer from a rival, 0 when there is none. The player either
	// matches it or loses the apprentice at the end of the next day.
	int32 TransferTeklifi = 0;

	UPROPERTY() TObjectPtr<ACigkofteCustomer> ApprenticeNPC;

private:
	float WorkTimer = 0.f;

	void DoWork();
	void GainXP(int32 Amount);
	void UpdateNPCPosition();

	void AdaylariYenile();
	void HataYap();
	void TransferTeklifiniIsle();
	void IstenAyril(const FString& SebepAnahtari);
};
