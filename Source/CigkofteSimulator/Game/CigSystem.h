#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CigSystem.generated.h"

class ACigkofteGameMode;
class UCigRandomSubsystem;
class UCigEventBus;

// The base class for every gameplay system. GameMode creates them, initialises
// them and calls UpdateSystem each frame; the day start/end hooks fan out here.
UCLASS(Abstract)
class UCigSystem : public UObject
{
	GENERATED_BODY()

public:
	void InitSystem(ACigkofteGameMode* InGM);

	virtual void OnInit() {}
	virtual void UpdateSystem(float DeltaSeconds) {}
	virtual void OnDayStart(int32 Day) {}
	virtual void OnDayEnd(int32 Day) {}

	virtual UWorld* GetWorld() const override;

protected:
	// Gameplay randomness. Use this instead of FMath::FRand so the stream stays
	// deterministic and writable to a save (see Core/CigRandomSubsystem.h). Valid
	// once InitSystem has run; the GameInstance subsystem lives for the session.
	UCigRandomSubsystem& Rng() const;

	// Event broadcasting between systems (see Game/CigEventBus.h). Broadcast an
	// event rather than calling another system directly - a publisher should not
	// know its listeners.
	UCigEventBus& Bus() const;

	UPROPERTY() TObjectPtr<ACigkofteGameMode> GM;

private:
	UPROPERTY() TObjectPtr<UCigRandomSubsystem> RngSub;
};
