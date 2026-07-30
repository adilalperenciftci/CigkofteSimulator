#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Game/CigkofteGameMode.h"
#include "CigkofteHUD.generated.h"

class UFont;

// The HUD, drawn entirely on Canvas.
// Rules:
//  - Vertical layout always advances by the real font height (LineHeight);
//    never by a fixed offset.
//  - Body text never drops below BodyScale (readable at 1080p).
//  - Recipes and ratios are described to the player in "measures", never as
//    decimal fractions.
UCLASS()
class ACigkofteHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	// --- Scale & fonts (set up at the top of DrawHUD each frame) ---
	float UIScale = 1.f;
	FVector2D ScreenSize = FVector2D(1920.f, 1080.f);
	UFont* FontBody = nullptr;   // large engine font - main body
	UFont* FontFine = nullptr;   // medium engine font - secondary small notes only

	// Text scales (multiplied by UIScale)
	//
	// Body and fine were set small enough that the recipe panel and the key
	// hints along the bottom were hard to read at 1080p - the resolution most
	// people will play this at, where UIScale is exactly 1. The style guide asks
	// for nothing under 16px after UIScale; these are the two that were under
	// it. Head and title were already fine and are left alone, which also keeps
	// the hierarchy from flattening.
	static constexpr float TitleScale = 2.3f;
	static constexpr float HeadScale = 1.5f;
	static constexpr float BodyScale = 1.32f;
	static constexpr float FineScale = 1.15f;

	float SX(float DesignX) const { return DesignX * UIScale; }

	// --- Accessibility ---
	// The player's colour-blind mode, refreshed from the settings each frame.
	ECigColorBlindMode ColorBlind = ECigColorBlindMode::Kapali;

	// Urgency indicator. Frac: 1 = relaxed, 0 = critical.
	// The palette follows the mode - blue and orange for red-green difficulty.
	FLinearColor UrgencyColor(float Frac) const;
	// A colour-independent marker. Returns an empty string when the mode is off,
	// to keep the screen uncluttered.
	FString UrgencyMark(float Frac) const;

	// --- Basic draw helpers ---
	void Panel(float X, float Y, float W, float H, float Alpha = 0.62f);
	void PanelHeader(float X, float Y, float W, const FString& Title, const FLinearColor& Accent);
	void Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& Fill);
	void Text(const FString& S, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale = 1.f);
	void CenterText(const FString& S, float Y, UFont* Font, float Scale, const FLinearColor& Color);
	// Text drawn straight onto the world, with no panel behind it. The shop floor
	// is light and the walls are cream, so a grey label on either disappears; a
	// dark offset copy gives it an edge whatever it lands on. Panelled text does
	// not need this - the panel already supplies the contrast.
	void ShadowText(const FString& S, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale = 1.f);
	void ShadowCenterText(const FString& S, float Y, UFont* Font, float Scale, const FLinearColor& Color);
	float TextWidth(const FString& S, UFont* Font, float Scale) const;
	float LineHeight(UFont* Font, float Scale) const;
	// Draws a line and advances Y by the real height plus spacing.
	void Row(const FString& S, const FLinearColor& C, float X, float& Y, UFont* Font, float Scale, float PadMult = 1.12f);
	static FString Stars(float Value01to5);

	// --- In-game panels ---
	void DrawStatusPanel(ACigkofteGameMode* GM);
	void DrawMessages(ACigkofteGameMode* GM);
	void DrawBowlPanel(ACigkofteGameMode* GM);
	void DrawWrapPanel(ACigkofteGameMode* GM);
	void DrawCustomerPanel(ACigkofteGameMode* GM);
	void DrawDeliveryPanel(ACigkofteGameMode* GM);
	void DrawEnergyBar(ACigkofteGameMode* GM);
	void DrawTutorial(ACigkofteGameMode* GM);
	void DrawPrompt(ACigkofteGameMode* GM);
	void DrawSettings(ACigkofteGameMode* GM);
	void DrawKeybinds(ACigkofteGameMode* GM);
	void DrawDebugOverlay(ACigkofteGameMode* GM);
	void DrawOverlays(ACigkofteGameMode* GM);
	void DrawTitleScreen(ACigkofteGameMode* GM);


	void DrawScreenFlash(ACigkofteGameMode* GM);

	// The screen layer for weather and evening gloom (rain streaks, heat yellow,
	// night blue). It complements the world lighting from the HUD side.
	void DrawWeatherOverlay(ACigkofteGameMode* GM);

	// --- Smoothing state ---
	float WrapPanelAlpha = 0.f;
	float CustomerPanelAlpha = 0.f;
	float TabletAlpha = 0.f;
	float TabletSlide = 0.f;

	// Pop animation for the money counter
	int32 LastMoney = INT32_MIN;
	float MoneyPop = 0.f;
};
