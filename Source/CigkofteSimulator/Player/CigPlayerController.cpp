#include "Player/CigPlayerController.h"
#include "Debug/CigCheatManager.h"

ACigPlayerController::ACigPlayerController()
{
	CheatClass = UCigCheatManager::StaticClass();
}

void ACigPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// Create the cheat manager by hand so debug commands work in packaged builds too.
	if (!CheatManager)
	{
		AddCheats(true);
	}
}
