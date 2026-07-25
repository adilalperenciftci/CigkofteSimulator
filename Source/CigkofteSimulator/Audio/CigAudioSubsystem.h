#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CigkofteTypes.h"
#include "CigAudioSubsystem.generated.h"

class USoundBase;

// The sound event layer. Sounds are loaded from the imported CC0 (Kenney)
// recordings under /Game/Audio; a missing asset just makes the event silent.
UCLASS()
class UCigAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// The effects channel (scaled by Master * Effects).
	void Play(ECigSound Sound, float VolumeMult = 1.f);

	// The music channel: plays the jingle at the music volume.
	void PlayMusicSting(ECigSound Sound, float VolumeMult = 1.f);

	// Menu music: while active it repeats the jingle at a fixed interval.
	// GameMode calls this each frame.
	void TickMenuMusic(float DeltaSeconds, bool bMenuActive);

	float MasterVolume = 1.f;
	float EffectsVolume = 1.f;
	float MusicVolume = 0.7f;

private:
	UPROPERTY() TMap<uint8, TObjectPtr<USoundBase>> LoadedSounds;
	bool bTriedLoad[(uint8)ECigSound::COUNT] = { false };
	float MenuMusicTimer = 0.f;

	USoundBase* ResolveSound(ECigSound Sound);
	void PlayInternal(ECigSound Sound, float Volume);
};
