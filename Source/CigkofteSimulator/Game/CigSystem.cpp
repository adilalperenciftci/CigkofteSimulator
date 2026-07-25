#include "Game/CigSystem.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Core/CigRandomSubsystem.h"
#include "Engine/GameInstance.h"

void UCigSystem::InitSystem(ACigkofteGameMode* InGM)
{
	GM = InGM;

	UGameInstance* GI = GM ? GM->GetGameInstance() : nullptr;
	RngSub = GI ? GI->GetSubsystem<UCigRandomSubsystem>() : nullptr;
	checkf(RngSub, TEXT("UCigSystem, UCigRandomSubsystem olmadan başlatılamaz (GameInstance yok mu?)"));

	OnInit();
}

UCigRandomSubsystem& UCigSystem::Rng() const
{
	return *RngSub;
}

UCigEventBus& UCigSystem::Bus() const
{
	// GameMode builds the bus as the first system, so it is always ready by the
	// time InitSystem runs.
	checkf(GM && GM->Bus, TEXT("Olay otobüsü yok — GameMode onu ilk sırada kurmalı"));
	return *GM->Bus;
}

UWorld* UCigSystem::GetWorld() const
{
	return GM ? GM->GetWorld() : nullptr;
}
