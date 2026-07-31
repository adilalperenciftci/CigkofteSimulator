#include "Player/CigPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Debug/CigCheatManager.h"
#endif

ACigPlayerController::ACigPlayerController()
{
	// Development packages use the deterministic capture and benchmark commands.
	// A retail Shipping controller must not assign or create the cheat manager.
#if !UE_BUILD_SHIPPING
	CheatClass = UCigCheatManager::StaticClass();
#else
	CheatClass = nullptr;
#endif
}

void ACigPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// Create it by hand only in non-Shipping builds so development capture tools
	// remain available without exposing them to the retail executable.
#if !UE_BUILD_SHIPPING
	if (!CheatManager)
	{
		AddCheats(true);
	}
#endif
}
