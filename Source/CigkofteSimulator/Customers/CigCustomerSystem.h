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

	// Returns the customer that arrived, or null when the pool or the systems it
	// needs would not give one. It used to return nothing; a party has to be able
	// to tag the people it just brought in, and rediscovering them by looking at
	// the back of the queue would be a guess.
	ACigkofteCustomer* SpawnCustomer(bool bForceVIP = false, bool bForceInfluencer = false);
	void RemoveCustomer(ACigkofteCustomer* C, bool bAngry);

	// Brings in a party of Size that arrived together, and returns how many
	// actually walked in. Public because the cheat manager and the tests both
	// need to make a party happen on demand rather than wait for the roll.
	//
	// Does not check whether they fit: CigCustomerGroup::JudgeIntake decides that,
	// and it is deliberately not called from in here so the decision stays in one
	// testable place rather than being made twice with different answers.
	int32 SpawnGroup(int32 Size);

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

	// Parties turned away at the door, split by reason. Counted rather than only
	// messaged, because "the shop is losing groups" is a balance question and a
	// message that scrolls past is not evidence of anything.
	int32 GroupsAdmitted = 0;
	int32 GroupsTurnedAway = 0;

	// --- Inspector ---
	UPROPERTY() TObjectPtr<ACigkofteCustomer> Inspector;
	float InspectorTimer = -1.f;
	void TriggerInspectorNow();

	// Sends the queue, the seated guests and the inspector home. Called when the
	// shop shuts, which is now a window before the books are closed.
	void SendEveryoneHome();

#if WITH_DEV_AUTOMATION_TESTS
	// Makes the next acquisition after this many succeed fail, so a test can ask
	// what a party does when the pool refuses halfway through. Negative disables
	// it, which is every build the player ever runs.
	//
	// A seam rather than a mock: the failure it simulates is real (SpawnActor can
	// return null), it has no other trigger a test could reach, and the atomicity
	// it exists to prove is exactly the kind of thing that silently rots.
	int32 FailAcquireAfterForTest = -1;
#endif

	// Customers that stopped because navigation could not reach their target, and
	// what became of them. Read by tests: a shop that strands customers is a
	// layout defect, and one that leaks them is a code defect, so the two are
	// counted apart rather than together.
	int32 StrandedRecovered = 0;
	int32 StrandedRecycled = 0;

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
	// Hands an acquired but uninitialised body straight back to the pool. Not a
	// customer leaving: nothing about them ever became visible to the game.
	void ReturnUnusedCustomer(ACigkofteCustomer* C);
	// Whether an arrival can be set up at all. Asked before any body is taken, so
	// a party never acquires actors it cannot then give identities to.
	bool CanAdmitArrival() const;
	// Gives an acquired body its traits, order, patience and queue slot. Split from
	// acquisition because everything in here is externally visible - shared RNG
	// draws, a regular's visit count, an arrival message - and so must not run for
	// a customer that might still have to be handed back.
	void InitializeArrival(ACigkofteCustomer* C, bool bForceVIP, bool bForceInfluencer);
	// Moves customers that reached the exit (bAwaitingRecycle) into the pool.
	void RecycleFinished();
	// Takes ownership back from customers navigation could not move, and gets
	// them out of the shop. Bounded: one attempt to send them home, then the pool.
	void RecoverStrandedCustomers();
	// Releases the seat and queue slot a customer holds. Seat release goes through
	// the world builder, because Seated is a view of a reservation that lives
	// there - dropping the record alone leaks the chair.
	void ReleaseCustomerOwnership(ACigkofteCustomer* C);

	ECigTrait RollTraits(int32 Day) const;
	// Picks a single trait from the weighted pool (Config/Balance/Traits.csv).
	ECigTrait PickPooledTrait(int32 Day) const;
	float NextCustomerInterval() const;
	FVector QueueSlot(int32 Index) const;
	// Everybody still queueing who came in with C, C included. A party leaves
	// together, so the walkout path needs the whole party before it removes any
	// of it - removing as it iterates would drop half of them.
	TArray<ACigkofteCustomer*> PartyMembersOf(const ACigkofteCustomer* C) const;
	// Removes one customer from the waiting party without dismissing their
	// companions. The remaining members keep one group identity and agree on the
	// new headcount; a lone remainder becomes an ordinary customer.
	void DetachFromWaitingParty(ACigkofteCustomer* C);
	// The next unused party id. Ids only have to be unique among the customers
	// alive right now, which a counter gives for free.
	int32 NextGroupId = 1;
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
