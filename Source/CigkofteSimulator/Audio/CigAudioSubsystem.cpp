#include "Audio/CigAudioSubsystem.h"
#include "Core/CigLog.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	// Project paths for the imported CC0 (Kenney) sounds.
	// An empty path or a missing asset makes the event silently skip.
	const TCHAR* GSoundPaths[(uint8)ECigSound::COUNT] = {
		TEXT("/Game/Audio/S_Knead.S_Knead"),         // Knead
		TEXT("/Game/Audio/S_Chop.S_Chop"),           // Chop
		TEXT("/Game/Audio/S_Serve.S_Serve"),         // Serve
		TEXT("/Game/Audio/S_Cash.S_Cash"),           // Cash
		TEXT("/Game/Audio/S_Success.S_Success"),     // Success
		TEXT("/Game/Audio/S_Failure.S_Failure"),     // Failure
		TEXT("/Game/Audio/S_Angry.S_Angry"),         // CustomerAngry
		nullptr,                                      // CatMeow (uygun asset yok, sessiz)
		TEXT("/Game/Audio/S_Pot.S_Pot"),             // CarEngine (yaklaşık: metal ses)
		TEXT("/Game/Audio/S_Quest.S_Quest"),         // QuestComplete
		TEXT("/Game/Audio/S_UIClick.S_UIClick"),     // UIClick
		TEXT("/Game/Audio/S_UINav.S_UINav"),         // UINav
		TEXT("/Game/Audio/S_DayStart.S_DayStart"),   // DayStart
		TEXT("/Game/Audio/S_DayEnd.S_DayEnd"),       // DayEnd
		TEXT("/Game/Audio/S_WrapCloth.S_WrapCloth"), // WrapCloth
		TEXT("/Game/Audio/S_Pot.S_Pot"),             // Pot
		TEXT("/Game/Audio/S_Knife.S_Knife"),         // Knife
		TEXT("/Game/Audio/S_MenuMusic.S_MenuMusic")  // MenuMusic
	};
}

void UCigAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogCig, Log, TEXT("Ses altyapısı hazır (Kenney CC0 sesleri /Game/Audio)"));
}

USoundBase* UCigAudioSubsystem::ResolveSound(ECigSound Sound)
{
	const uint8 Idx = (uint8)Sound;
	if (Idx >= (uint8)ECigSound::COUNT)
	{
		return nullptr;
	}
	if (TObjectPtr<USoundBase>* Found = LoadedSounds.Find(Idx))
	{
		return Found->Get();
	}
	if (bTriedLoad[Idx])
	{
		return nullptr;
	}
	bTriedLoad[Idx] = true;
	if (GSoundPaths[Idx])
	{
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, GSoundPaths[Idx]))
		{
			LoadedSounds.Add(Idx, S);
			return S;
		}
		UE_LOG(LogCig, Warning, TEXT("Ses bulunamadı: %s"), GSoundPaths[Idx]);
	}
	return nullptr;
}

void UCigAudioSubsystem::PlayInternal(ECigSound Sound, float Volume)
{
	if (Volume <= 0.01f)
	{
		return;
	}
	USoundBase* S = ResolveSound(Sound);
	if (!S)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::PlaySound2D(World, S, Volume);
	}
}

void UCigAudioSubsystem::Play(ECigSound Sound, float VolumeMult)
{
	PlayInternal(Sound, MasterVolume * EffectsVolume * VolumeMult);
}

void UCigAudioSubsystem::PlayMusicSting(ECigSound Sound, float VolumeMult)
{
	PlayInternal(Sound, MasterVolume * MusicVolume * VolumeMult);
}

void UCigAudioSubsystem::TickMenuMusic(float DeltaSeconds, bool bMenuActive)
{
	if (!bMenuActive)
	{
		MenuMusicTimer = 0.f;
		return;
	}
	MenuMusicTimer -= DeltaSeconds;
	if (MenuMusicTimer <= 0.f)
	{
		PlayMusicSting(ECigSound::MenuMusic, 0.55f);
		MenuMusicTimer = 9.f; // jingle döngü aralığı
	}
}
