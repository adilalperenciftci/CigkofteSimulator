#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/CigkofteTypes.h"
#include "CigkofteGameMode.generated.h"

class UCigEventBus;
class UCigWorldBuilder;
class UCigDaySystem;
class UCigCookingSystem;
class UCigOrderSystem;
class UCigCustomerSystem;
class UCigEconomySystem;
class UCigPricingSystem;
class UCigSaleSystem;
class UCigInspectionSystem;
class UCigSocialSystem;
class UCigInventorySystem;
class UCigProgressionSystem;
class UCigQuestSystem;
class UCigEventSystem;
class UCigDeliverySystem;
class UCigHygieneSystem;
class UCigStaffSystem;
class UCigCatSystem;
class UCigRivalSystem;
class UCigReviewSystem;
class UCigSkillSystem;
class UCigAchievementSystem;
class UCigSystem;
class UCigSaveGame;
class ACigCar;
class ACigkofteStation;

// One entry in the HUD message feed.
struct FCigMessage
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float TimeLeft = 6.f;
};

// Tablet tabs.
enum class ECigTabletTab : uint8
{
	Stok = 0,
	Tarifler,
	Fiyatlar,
	Dukkan,
	Tedarikci,
	Rakipler,
	Yorumlar,
	Gorevler,
	Personel,
	Yetenekler,
	Basarimlar,
	COUNT
};

// Colour-blind mode. Indicators that lean on colour (the patience bar, the
// screen flash) both switch palette and add a colour-independent marker.
enum class ECigColorBlindMode : uint8
{
	Kapali = 0,
	KirmiziYesil,  // protanopia/deuteranopia - red and green are hard to tell apart
	MaviSari,      // tritanopia
	COUNT
};

// Game settings (stored in the save; this is the live copy).
// Adding a field here is not enough - write the place that actually applies it
// too, or you have shown the player a setting that does nothing.
struct FCigRuntimeSettings
{
	float FOV = 90.f;
	float MouseSensitivity = 1.f;
	bool bHeadBob = true;
	float MasterVolume = 1.f;
	float EffectsVolume = 1.f;
	float MusicVolume = 0.7f;
	int32 QualityLevel = 2;
	int32 WindowMode = 0;
	int32 ResolutionIndex = 1;

	// 0 Turkish, 1 English. UI text is read from Config/Text/Strings.csv (see
	// Core/CigText.h); ApplySettings passes this through to CigText.
	int32 Language = 0;

	// --- Accessibility ---
	// A player multiplier on top of the automatic resolution-derived HUD scale.
	float UIScaleMult = 1.f;
	// Screen-edge flash (success/failure feedback). Can be turned off.
	bool bScreenFlash = true;
	int32 ColorBlindMode = 0;
};

// The top-level coordinator: it builds the systems, ticks them and routes the
// flow between them (interaction, tablet, saving). The gameplay rules live in
// the systems themselves.
UCLASS()
class ACigkofteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACigkofteGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void Tick(float DeltaSeconds) override;

	// --- Systems ---
	// The event bus is built first so the others can subscribe to it during
	// InitSystem (see Game/CigEventBus.h).
	UPROPERTY() TObjectPtr<UCigEventBus> Bus;
	UPROPERTY() TObjectPtr<UCigWorldBuilder> WorldBuilder;
	UPROPERTY() TObjectPtr<UCigDaySystem> Days;
	UPROPERTY() TObjectPtr<UCigCookingSystem> Cooking;
	UPROPERTY() TObjectPtr<UCigOrderSystem> Orders;
	UPROPERTY() TObjectPtr<UCigCustomerSystem> Customers;
	UPROPERTY() TObjectPtr<UCigEconomySystem> Economy;
	UPROPERTY() TObjectPtr<UCigPricingSystem> Pricing;
	UPROPERTY() TObjectPtr<UCigSaleSystem> Sales;
	UPROPERTY() TObjectPtr<UCigInspectionSystem> Inspection;
	UPROPERTY() TObjectPtr<UCigSocialSystem> Social;
	UPROPERTY() TObjectPtr<UCigInventorySystem> Inventory;
	UPROPERTY() TObjectPtr<UCigProgressionSystem> Progression;
	UPROPERTY() TObjectPtr<UCigQuestSystem> Quests;
	UPROPERTY() TObjectPtr<UCigEventSystem> Events;
	UPROPERTY() TObjectPtr<UCigDeliverySystem> Delivery;
	UPROPERTY() TObjectPtr<UCigHygieneSystem> Hygiene;
	UPROPERTY() TObjectPtr<UCigStaffSystem> Staff;
	UPROPERTY() TObjectPtr<UCigCatSystem> CatSys;
	UPROPERTY() TObjectPtr<UCigRivalSystem> Rivals;
	UPROPERTY() TObjectPtr<UCigReviewSystem> Reviews;
	UPROPERTY() TObjectPtr<UCigSkillSystem> Skills;
	UPROPERTY() TObjectPtr<UCigAchievementSystem> Achievements;

	UPROPERTY() TObjectPtr<ACigCar> PlayerCar;

	// --- Startup (splash, title screen, main menu) ---
	float IntroTime = 0.f;
	static constexpr float SplashDuration = 3.2f;
	int32 TitleCursor = 0;
	// Menu entries: 0 Continue/Start, 1 New Game, 2 Settings, 3 Quit
	static constexpr int32 TitleItemCount = 4;
	void TitleNav(int32 Dir);
	void TitleSelect();
	bool HasExistingSave() const;

	// --- Player actions ---
	void HandleInteract(ACigkofteStation* Station);
	void KneadPress();
	void DrinkTea();
	void RestartGame();
	void StartFirstDayIfIntro();

	// --- Tablet ---
	bool bTabletOpen = false;
	// The tablet is drawn entirely in UMG (see UI/CigTabletWidget.h). The Canvas
	// version was removed once every tab had moved - maintaining two separate
	// draw paths would have cost more than the move gained.
	UPROPERTY() TObjectPtr<class UCigTabletWidget> TabletWidget;
	// Refreshes the UMG side when the tablet's content changes.
	void RefreshTabletWidget();

	ECigTabletTab TabletTab = ECigTabletTab::Stok;
	int32 TabletScroll = 0;
	void ToggleTablet();
	void CycleTabletTab(int32 Dir);
	void TabletScrollBy(int32 Dir);
	void TabletKey(int32 Num); // 1-9 and 0

	// --- Settings screen ---
	bool bSettingsOpen = false;
	int32 SettingsCursor = 0;
	// The row count lives in one place: the HUD validates its list against it and
	// the cursor wraps on it.
	static constexpr int32 SettingsCount = 15;
	FCigRuntimeSettings Settings;

	// --- Key binding screen ---
	// A sub-screen opened from the settings. While capturing, the next key press
	// is assigned to that action (Escape cancels).
	bool bKeybindsOpen = false;
	int32 KeybindCursor = 0;
	bool bAwaitingKey = false;
	void ToggleKeybinds();
	void KeybindNav(int32 Dir);
	void KeybindBeginCapture();
	void KeybindResetAll();
	// Applies the key pressed while capturing. Returns true if it was applied.
	bool KeybindCaptureKey(const FKey& PressedKey);
	void ToggleSettings();
	void SettingsNav(int32 Dir);
	void SettingsAdjust(int32 Dir);
	void ApplySettings();

	// --- Pause menu (Esc/P) ---
	bool bPauseMenuOpen = false;
	int32 PauseMenuCursor = 0;
	static constexpr int32 PauseMenuItemCount = 4; // Resume, Settings, Save, Quit
	void TogglePauseMenu();
	void PauseMenuNav(int32 Dir);
	void PauseMenuSelect();

	// --- Debug HUD ---
	bool bShowDebugHUD = false;

	// --- Screen-edge flash (game feel: green success / red failure) ---
	float FlashTimer = 0.f;
	float FlashDuration = 0.5f;
	FLinearColor FlashColor = FLinearColor::Green;
	void RequestFlash(const FLinearColor& Color, float Duration = 0.5f);

	// --- Messages & effects ---
	TArray<FCigMessage> Messages;
	void AddMessage(const FString& Text, const FLinearColor& Color = FLinearColor::White);
	void SpawnFloatText(const FVector& Loc, const FString& Text, const FColor& Color, float Size = 34.f);
	void PlaySound(ECigSound Sound);

	// --- Day broadcasts ---
	void BroadcastDayStart(int32 Day);
	void BroadcastDayEnd(int32 Day);

	// --- Saving ---
	void RequestSave();
	void RequestLoad();
	void CaptureSave(UCigSaveGame& Save) const;
	void ApplySave(const UCigSaveGame& Save);
	bool bLoadedFromSave = false;

	bool PlayerDriving() const;

private:
	UPROPERTY() TArray<TObjectPtr<UCigSystem>> AllSystems;

	template <typename T>
	T* CreateSystem(TObjectPtr<T>& Slot)
	{
		T* Sys = NewObject<T>(this);
		Slot = Sys;
		AllSystems.Add(Sys);
		return Sys;
	}
};
