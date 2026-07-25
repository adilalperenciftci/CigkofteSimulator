#include "Core/CigInput.h"
#include "Core/CigText.h"
#include "Core/CigLog.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FCigBinding
	{
		const TCHAR* TextKey;   // Config/Text/Strings.csv anahtarı
		FKey Default;           // klavye/fare
		FKey Pad;               // gamepad (yeniden atanmaz)
		FKey Current;           // oyuncunun seçtiği
	};

	// The order must match ECigAction exactly.
	FCigBinding& BindingAt(int32 Index);

	TArray<FCigBinding>& CigBindings()
	{
		static TArray<FCigBinding> Data = []()
		{
			TArray<FCigBinding> B;
			auto Add = [&B](const TCHAR* TextKey, const FKey& Def, const FKey& Pad)
			{
				B.Add({ TextKey, Def, Pad, Def });
			};
			Add(TEXT("action.moveforward"), EKeys::W,                 EKeys::Invalid);
			Add(TEXT("action.moveback"),    EKeys::S,                 EKeys::Invalid);
			Add(TEXT("action.moveleft"),    EKeys::A,                 EKeys::Invalid);
			Add(TEXT("action.moveright"),   EKeys::D,                 EKeys::Invalid);
			Add(TEXT("action.run"),         EKeys::LeftShift,         EKeys::Gamepad_LeftThumbstick);
			Add(TEXT("action.jump"),        EKeys::SpaceBar,          EKeys::Gamepad_FaceButton_Top);
			Add(TEXT("action.interact"),    EKeys::E,                 EKeys::Gamepad_FaceButton_Bottom);
			Add(TEXT("action.knead"),       EKeys::LeftMouseButton,   EKeys::Gamepad_RightTrigger);
			Add(TEXT("action.wrap"),        EKeys::F,                 EKeys::Gamepad_FaceButton_Left);
			Add(TEXT("action.shelf"),       EKeys::G,                 EKeys::Gamepad_RightThumbstick);
			Add(TEXT("action.tablet"),      EKeys::T,                 EKeys::Gamepad_Special_Left);
			Add(TEXT("action.settings"),    EKeys::O,                 EKeys::Invalid);
			Add(TEXT("action.pause"),       EKeys::P,                 EKeys::Gamepad_Special_Right);
			check(B.Num() == (int32)ECigAction::COUNT);
			return B;
		}();
		return Data;
	}

	FCigBinding& BindingAt(int32 Index)
	{
		TArray<FCigBinding>& B = CigBindings();
		return B[FMath::Clamp(Index, 0, B.Num() - 1)];
	}
}

namespace CigInput
{
	FString ActionName(ECigAction Action)
	{
		return CigText::Get(BindingAt((int32)Action).TextKey);
	}

	FKey Key(ECigAction Action)    { return BindingAt((int32)Action).Current; }
	FKey PadKey(ECigAction Action) { return BindingAt((int32)Action).Pad; }

	void SetKey(ECigAction Action, const FKey& NewKey)
	{
		if (!NewKey.IsValid())
		{
			return;
		}

		// If another action already uses this key, take it away. Two actions on one
		// key leaves the player unable to work out why two things happen at once.
		TArray<FCigBinding>& B = CigBindings();
		const int32 Target = (int32)Action;
		for (int32 i = 0; i < B.Num(); ++i)
		{
			if (i != Target && B[i].Current == NewKey)
			{
				B[i].Current = EKeys::Invalid;
				UE_LOG(LogCig, Log, TEXT("Tuş çakışması: '%s' eyleminin ataması kaldırıldı."), B[i].TextKey);
			}
		}
		B[Target].Current = NewKey;
	}

	void ResetToDefaults()
	{
		for (FCigBinding& Bind : CigBindings())
		{
			Bind.Current = Bind.Default;
		}
	}

	bool IsRemapped(ECigAction Action)
	{
		const FCigBinding& Bind = BindingAt((int32)Action);
		return Bind.Current != Bind.Default;
	}

	bool IsDown(const APlayerController* PC, ECigAction Action)
	{
		if (!PC)
		{
			return false;
		}
		const FCigBinding& Bind = BindingAt((int32)Action);
		return (Bind.Current.IsValid() && PC->IsInputKeyDown(Bind.Current))
			|| (Bind.Pad.IsValid() && PC->IsInputKeyDown(Bind.Pad));
	}

	bool WasPressed(const APlayerController* PC, ECigAction Action)
	{
		if (!PC)
		{
			return false;
		}
		const FCigBinding& Bind = BindingAt((int32)Action);
		return (Bind.Current.IsValid() && PC->WasInputKeyJustPressed(Bind.Current))
			|| (Bind.Pad.IsValid() && PC->WasInputKeyJustPressed(Bind.Pad));
	}

	bool WasReleased(const APlayerController* PC, ECigAction Action)
	{
		if (!PC)
		{
			return false;
		}
		const FCigBinding& Bind = BindingAt((int32)Action);
		return (Bind.Current.IsValid() && PC->WasInputKeyJustReleased(Bind.Current))
			|| (Bind.Pad.IsValid() && PC->WasInputKeyJustReleased(Bind.Pad));
	}

	TArray<FString> SaveBindings()
	{
		TArray<FString> Out;
		Out.Reserve((int32)ECigAction::COUNT);
		for (const FCigBinding& Bind : CigBindings())
		{
			Out.Add(Bind.Current.IsValid() ? Bind.Current.GetFName().ToString() : FString());
		}
		return Out;
	}

	void LoadBindings(const TArray<FString>& Names)
	{
		TArray<FCigBinding>& B = CigBindings();
		for (int32 i = 0; i < B.Num(); ++i)
		{
			if (!Names.IsValidIndex(i))
			{
				continue;   // eski kayıt: varsayılan kalır
			}
			if (Names[i].IsEmpty())
			{
				B[i].Current = EKeys::Invalid;
				continue;
			}
			const FKey Loaded(*Names[i]);
			// An unrecognised key name (engine update, hand-edited save) falls back
			// to the default, so no action is left dead.
			B[i].Current = Loaded.IsValid() ? Loaded : B[i].Default;
		}
	}
}
