#include "Audio/CigAudioSubsystem.h"
#include "Core/CigLog.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Core/CigBalance.h"

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

void UCigAudioSubsystem::PlayInternal(ECigSound Sound, float Volume, float Pitch)
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
		UGameplayStatics::PlaySound2D(World, S, Volume, Pitch);
	}
}

void UCigAudioSubsystem::Play(ECigSound Sound, float VolumeMult, float Pitch)
{
	PlayInternal(Sound, MasterVolume * EffectsVolume * VolumeMult, Pitch);
}

float UCigAudioSubsystem::YogurmaPerdesi(float Progress01)
{
	const float Bas = CigBalance::Audio(TEXT("YogurmaPerdeBaslangic"), 0.85f);
	const float Bit = CigBalance::Audio(TEXT("YogurmaPerdeBitis"), 1.45f);
	return FMath::Lerp(Bas, Bit, FMath::Clamp(Progress01, 0.f, 1.f));
}

void UCigAudioSubsystem::PlayKnead(float Progress01)
{
	Play(ECigSound::Knead, 1.f, YogurmaPerdesi(Progress01));
}

void UCigAudioSubsystem::PlayStation(ECigStation Station)
{
	// Sixteen one-shots have to cover twenty-odd stations, so each picks the
	// closest recording and a detune keeps neighbouring stations from sounding
	// identical. Same trick the mesh library plays with primitives: use what is
	// there rather than ship silence.
	ECigSound Sound = ECigSound::UIClick;
	float Perde = 1.f;

	switch (Station)
	{
	case ECigStation::Bulgur:    Sound = ECigSound::Pot;       Perde = 1.15f; break;
	case ECigStation::Isot:      Sound = ECigSound::Pot;       Perde = 1.30f; break;
	case ECigStation::Salca:     Sound = ECigSound::Pot;       Perde = 0.90f; break;
	case ECigStation::Su:        Sound = ECigSound::Pot;       Perde = 1.45f; break;
	case ECigStation::Baharat:   Sound = ECigSound::Pot;       Perde = 1.60f; break;
	case ECigStation::Dograma:   Sound = ECigSound::Knife;     Perde = 1.00f; break;
	case ECigStation::Lavas:     Sound = ECigSound::WrapCloth; Perde = 1.20f; break;
	case ECigStation::Paketleme: Sound = ECigSound::WrapCloth; Perde = 0.85f; break;
	case ECigStation::Servis:    Sound = ECigSound::Serve;     Perde = 1.00f; break;
	case ECigStation::Buzdolabi: Sound = ECigSound::Pot;       Perde = 0.70f; break;
	case ECigStation::Cay:       Sound = ECigSound::Pot;       Perde = 1.75f; break;
	case ECigStation::Bulasik:   Sound = ECigSound::Pot;       Perde = 0.80f; break;
	case ECigStation::YanUrun:   Sound = ECigSound::Serve;     Perde = 1.25f; break;
	default:                     Sound = ECigSound::UIClick;   Perde = 1.00f; break;
	}

	const float Sapma = CigBalance::Audio(TEXT("IstasyonPerdeSapmasi"), 0.08f);
	Play(Sound, 1.f, Perde + FMath::FRandRange(-Sapma, Sapma));
}

void UCigAudioSubsystem::PlayCustomerReaction(bool bHappy)
{
	const float Ses = CigBalance::Audio(TEXT("MusteriTepkiSesi"), 0.6f);
	// A contented murmur is the success sting played low and slow; the sigh is
	// the angry sound taken down far enough to read as impatience, not fury.
	Play(bHappy ? ECigSound::Success : ECigSound::CustomerAngry, Ses, bHappy ? 0.80f : 0.65f);
}

USoundBase* UCigAudioSubsystem::ResolveOrtam(int32 OrtamIndex)
{
	// 0 daytime street, 1 night, 2 rain. Nothing ships at these paths yet, so
	// each is looked for once and then left alone.
	static const TCHAR* GOrtamYollari[3] = {
		TEXT("/Game/Audio/S_AmbStreet.S_AmbStreet"),
		TEXT("/Game/Audio/S_AmbNight.S_AmbNight"),
		TEXT("/Game/Audio/S_AmbRain.S_AmbRain")
	};

	if (OrtamIndex < 0 || OrtamIndex >= 3)
	{
		return nullptr;
	}
	if (TObjectPtr<USoundBase>* Found = OrtamSesleri.Find(OrtamIndex))
	{
		return Found->Get();
	}
	if (bOrtamAranmis[OrtamIndex])
	{
		return nullptr;
	}
	bOrtamAranmis[OrtamIndex] = true;
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, GOrtamYollari[OrtamIndex]))
	{
		OrtamSesleri.Add(OrtamIndex, S);
		return S;
	}
	UE_LOG(LogCig, Log, TEXT("Ortam sesi yok: %s (katman sessiz)"), GOrtamYollari[OrtamIndex]);
	return nullptr;
}

void UCigAudioSubsystem::TickAmbience(float DeltaSeconds, float DayProgress01, int32 Weather, bool bPlaying)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bPlaying)
	{
		if (AmbienceComp)
		{
			AmbienceComp->Stop();
			AmbienceComp = nullptr;
			AktifOrtam = -1;
		}
		return;
	}

	// Rain wins over the clock: a wet evening sounds like rain, not like dusk.
	const int32 Istenen = Weather == 1 ? 2 : (DayProgress01 > 0.75f ? 1 : 0);

	if (Istenen != AktifOrtam)
	{
		if (AmbienceComp)
		{
			AmbienceComp->Stop();
			AmbienceComp = nullptr;
		}
		AktifOrtam = Istenen;

		if (USoundBase* S = ResolveOrtam(Istenen))
		{
			AmbienceComp = UGameplayStatics::SpawnSound2D(World, S, 1.f, 1.f, 0.f, nullptr, true, false);
		}
	}

	if (AmbienceComp)
	{
		const float Taban = CigBalance::Audio(TEXT("OrtamSesi"), 0.35f);
		const float Gece = Istenen == 1 ? CigBalance::Audio(TEXT("OrtamGeceKisilma"), 0.55f) : 1.f;
		AmbienceComp->SetVolumeMultiplier(MasterVolume * EffectsVolume * Taban * Gece);
	}
}

void UCigAudioSubsystem::PlayMusicSting(ECigSound Sound, float VolumeMult)
{
	PlayInternal(Sound, MasterVolume * MusicVolume * VolumeMult, 1.f);
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
