#include "Core/CigRandomSubsystem.h"
#include "Core/CigLog.h"

void UCigRandomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// If there is a save, GameMode overwrites this via RestoreState during load.
	Reseed();
}

void UCigRandomSubsystem::Reseed()
{
	// The low 31 bits of FDateTime::Now().GetTicks(): still distinct for two
	// sessions started in the same second.
	const int32 NewSeed = (int32)(FDateTime::Now().GetTicks() & 0x7fffffff);
	SeedWith(NewSeed);
}

void UCigRandomSubsystem::SeedWith(int32 InSeed)
{
	RunSeed = InSeed;
	Draws = 0;
	Stream.Initialize(InSeed);
	UE_LOG(LogCig, Log, TEXT("RNG seed: %d"), RunSeed);
}

void UCigRandomSubsystem::RestoreState(int32 InRunSeed, int32 InStateSeed)
{
	RunSeed = InRunSeed;
	Draws = 0;
	// The stream position is restored directly, never Reset back to InitialSeed -
	// otherwise loading a save would replay the same day with the same numbers.
	Stream.Initialize(InStateSeed);
}
