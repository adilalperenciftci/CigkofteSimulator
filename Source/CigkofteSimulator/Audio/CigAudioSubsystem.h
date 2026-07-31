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

	// The effects channel (scaled by Master * Effects). Pitch shifts the same
	// recording rather than needing a second one, which is how sixteen one-shots
	// end up covering twenty-odd stations.
	void Play(ECigSound Sound, float VolumeMult = 1.f, float Pitch = 1.f);

	// The sound a station makes when the player interacts with it, with a little
	// random detune so repeated use does not turn into a metronome.
	void PlayStation(ECigStation Station);

	// Kneading rises in pitch as the dough comes together, so the ear knows how
	// far along the batch is without looking at the bowl.
	void PlayKnead(float Progress01);

	// The pitch that progress maps to. Pure so the curve can be checked without
	// an audio device (see Tests/CigAudioTests.cpp).
	static float YogurmaPerdesi(float Progress01);

	// Customer reactions: a murmur when they leave happy, a sigh when patience
	// is running out.
	void PlayCustomerReaction(bool bHappy);

	// The ambience bed: street noise by day, quieter at night, rain when it
	// rains. GameMode calls this each frame.
	//
	// The repo ships one-shots only, so there is no ambience asset to loop yet
	// and this stays silent - the same deal the mesh library makes. Drop a loop
	// at /Game/Audio/S_AmbStreet, S_AmbNight or S_AmbRain and it starts working
	// with no code change.
	void TickAmbience(float DeltaSeconds, float DayProgress01, int32 Weather, bool bPlaying);

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

	// The looping ambience component, created the first time an ambience asset
	// actually resolves. Null while none exists.
	UPROPERTY() TObjectPtr<class UAudioComponent> AmbienceComp;
	int32 AktifOrtam = -1;
	bool bOrtamAranmis[3] = { false };
	UPROPERTY() TMap<int32, TObjectPtr<USoundBase>> OrtamSesleri;

	USoundBase* ResolveSound(ECigSound Sound);

public:
	// Whether the asset behind a sound is present and loadable. Keeps the path
	// table in its one home rather than repeating a path at the call site.
	bool HasSound(ECigSound Sound) { return ResolveSound(Sound) != nullptr; }

private:
	USoundBase* ResolveOrtam(int32 OrtamIndex);
	void PlayInternal(ECigSound Sound, float Volume, float Pitch);
};
