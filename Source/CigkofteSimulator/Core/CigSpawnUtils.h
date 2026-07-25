#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

// Spawn parameters that skip the collision check and place the actor anyway.
inline FActorSpawnParameters CigAlwaysSpawnParams()
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return P;
}
