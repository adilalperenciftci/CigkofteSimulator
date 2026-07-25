#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CigRandomSubsystem.generated.h"

// Every bit of randomness that affects gameplay flows through one FRandomStream.
//
// Why: two sessions started from the same seed play the same day the same way. A
// playtester reporting "customers piled up on day 5" can hand over the seed and
// `CigSetSeed` reproduces it exactly. Loading a save resumes the stream where it
// left off, so reloading and replaying a day does not silently reroll it.
//
// What stays outside is a deliberate split: world decoration, cat fur colour,
// traffic, customer mesh variation and dialogue line picking all stay on the
// global FMath RNG. None of them change game state, and letting them into the
// stream would burn a different number of draws per run and break determinism.
UCLASS()
class UCigRandomSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// New game: derives a fresh seed from the clock.
	void Reseed();

	// Restarts from a given seed (save load, tests, console command).
	void SeedWith(int32 InSeed);

	// The session's starting seed - shown to the player, quoted in bug reports.
	int32 InitialSeed() const { return RunSeed; }

	// The stream's exact position right now. FRandomStream is an LCG, so this
	// single number is the whole state; save it, hand it back, and it resumes.
	int32 StateSeed() const { return Stream.GetCurrentSeed(); }

	// Restore from a save. InRunSeed is display only; InStateSeed drives the stream.
	void RestoreState(int32 InRunSeed, int32 InStateSeed);

	// Draws taken since the session started (debugging / verification).
	int64 DrawCount() const { return Draws; }

	// --- Draws: same signatures as their FMath counterparts ---
	float FRand() { ++Draws; return Stream.FRand(); }
	float FRandRange(float Min, float Max) { ++Draws; return Stream.FRandRange(Min, Max); }
	int32 RandRange(int32 Min, int32 Max) { ++Draws; return Stream.RandRange(Min, Max); }
	int32 Rand() { ++Draws; return (int32)(Stream.GetUnsignedInt() & 0x7fffffffu); }

	// Reads better: `if (Rng.Chance(0.35f))` is the same as `FRand() < 0.35f`.
	bool Chance(float Probability) { return FRand() < Probability; }

	// Picks an array element. Returns -1 for an empty array without taking a
	// draw, so an empty container never shifts the stream's position.
	int32 PickIndex(int32 Num) { return Num > 0 ? RandRange(0, Num - 1) : -1; }

private:
	int32 RunSeed = 0;
	int64 Draws = 0;
	FRandomStream Stream;
};
