#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "CigCustomerSystem.generated.h"

class ACigkofteCustomer;

// A stored regular customer. Persisted in the save file.
struct FCigLoyalCustomer
{
	int32 Id = 0;
	FString Name;
	int32 Seed = 0;
	FCigOrderSpec Favorite;
	int32 Visits = 0;
	float Satisfaction = 60.f;
	float Trust = 50.f;
	int32 LastVisitDay = 0;
	float AvgTip = 0.f;
	uint16 Traits = 0;
	int32 RememberedMistakes = 0;
};

// The queue, customer generation, traits, loyalty and the inspector.
UCLASS()
class UCigCustomerSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;
	virtual void OnDayStart(int32 Day) override;
	virtual void OnDayEnd(int32 Day) override;

	ACigkofteCustomer* FrontCustomer() const;

	// E at the service counter: hand the prepared wrap to the next customer.
	void ServeFront();

	void SpawnCustomer(bool bForceVIP = false, bool bForceInfluencer = false);
	void RemoveCustomer(ACigkofteCustomer* C, bool bAngry);

	int32 MaxQueue() const;

	UPROPERTY() TArray<TObjectPtr<ACigkofteCustomer>> Queue;

	// Customers sitting at a table eating (after being served).
	struct FSeatedGuest
	{
		TWeakObjectPtr<ACigkofteCustomer> Customer;
		int32 SeatIndex = -1;
		float EatTimer = 0.f;
	};
	TArray<FSeatedGuest> Seated;

	TArray<FCigLoyalCustomer> Loyals;
	int32 NextLoyalId = 1;

	// --- Inspector ---
	UPROPERTY() TObjectPtr<ACigkofteCustomer> Inspector;
	float InspectorTimer = -1.f;
	void TriggerInspectorNow();

private:
	float CustomerTimer = 5.f;

	// --- Actor pool ---
	// On a busy day dozens of customers come and go every minute, and a
	// SpawnActor/Destroy for each put needless pressure on the GC. A customer
	// reaching the exit hides and drops into Pool, ready for the next arrival.
	UPROPERTY() TArray<TObjectPtr<ACigkofteCustomer>> Pool;
	// Customers this system spawned that are still in the scene, including those
	// not in the queue, so the ones on their way out can be swept up too.
	UPROPERTY() TArray<TObjectPtr<ACigkofteCustomer>> Live;

	// Takes from the pool, spawning when it is empty. The actor returned is always clean.
	ACigkofteCustomer* AcquireCustomer(const FVector& SpawnPos);
	// Moves customers that reached the exit (bAwaitingRecycle) into the pool.
	void RecycleFinished();

	ECigTrait RollTraits(int32 Day) const;
	// Picks a single trait from the weighted pool (Config/Balance/Traits.csv).
	ECigTrait PickPooledTrait(int32 Day) const;
	float NextCustomerInterval() const;
	FVector QueueSlot(int32 Index) const;
	void UpdateInspector(float DeltaSeconds);
	void MaybeCreateLoyal(ACigkofteCustomer* C, float Accuracy, float Tip);
	FCigLoyalCustomer* FindLoyal(int32 Id);

	// Sub-steps split out of ServeFront (one responsibility each).
	void UpdateLoyaltyAfterServe(ACigkofteCustomer* C, class UCigEconomySystem* Eco, float Accuracy, int32 Tip);
	void FinishCustomerVisit(ACigkofteCustomer* C);
	// Teslim is the wrap that was actually handed over, not the order that was
	// asked for. The two are the same only when the player got it right, and the
	// customer's line is about the difference.
	void RequestServeDialogue(ACigkofteCustomer* C, const struct FCigWrapBuild& Teslim,
		float Accuracy, float Quality, int32 FinalPrice, bool bTipped);
};
