#include "UI/CigkofteHUD.h"
#include "Core/CigText.h"
#include "Core/CigInput.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigDaySystem.h"
#include "Cooking/CigCookingSystem.h"
#include "Cooking/CigMixDiagnosis.h"
#include "Orders/CigOrderSystem.h"
#include "Customers/CigCustomerSystem.h"
#include "Customers/CigkofteCustomer.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigRivalSystem.h"
#include "Economy/CigReviewSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Progression/CigAchievementSystem.h"
#include "Quests/CigQuestSystem.h"
#include "Events/CigEventSystem.h"
#include "Delivery/CigDeliverySystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Staff/CigStaffSystem.h"
#include "Cat/CigCatSystem.h"
#include "Player/CigkoftePlayerCharacter.h"
#include "World/CigkofteStation.h"
#include "World/CigWorldBuilder.h"
#include "Vehicles/CigCar.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor GWhite(1.f, 1.f, 1.f);
	const FLinearColor GDim(0.82f, 0.82f, 0.82f);
	const FLinearColor GGold(1.f, 0.85f, 0.3f);
	const FLinearColor GGood(0.45f, 1.f, 0.45f);
	const FLinearColor GBad(1.f, 0.42f, 0.32f);
	const FLinearColor GInfo(0.6f, 0.9f, 1.f);
	const FLinearColor GOrange(1.f, 0.55f, 0.15f);
	const FLinearColor GPanelBg(0.04f, 0.035f, 0.03f, 1.f);
}

// ---------------------------------------------------------------- helpers

void ACigkofteHUD::Panel(float X, float Y, float W, float H, float Alpha)
{
	FLinearColor C = GPanelBg;
	C.A = Alpha;
	DrawRect(C, X, Y, W, H);
}

void ACigkofteHUD::PanelHeader(float X, float Y, float W, const FString& Title, const FLinearColor& Accent)
{
	const float HH = LineHeight(FontBody, HeadScale) * 1.25f;
	FLinearColor Band = Accent;
	Band.A = 0.85f;
	DrawRect(Band, X, Y, SX(6.f), HH);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), X + SX(6.f), Y, W - SX(6.f), HH);
	Text(Title, GWhite, X + SX(16.f), Y + HH * 0.1f, FontBody, HeadScale);
}

void ACigkofteHUD::Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& Fill)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X, Y, W, H);
	DrawRect(Fill, X + 1.f, Y + 1.f, (W - 2.f) * FMath::Clamp(Frac, 0.f, 1.f), H - 2.f);
}

void ACigkofteHUD::Text(const FString& S, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
{
	DrawText(S, Color, X, Y, Font, Scale * UIScale);
}

float ACigkofteHUD::TextWidth(const FString& S, UFont* Font, float Scale) const
{
	if (!Canvas)
	{
		return 0.f;
	}
	const FVector2D Size = Canvas->K2_TextSize(Font, S, FVector2D(Scale * UIScale, Scale * UIScale));
	return Size.X;
}

float ACigkofteHUD::LineHeight(UFont* Font, float Scale) const
{
	if (!Canvas)
	{
		return 22.f * Scale * UIScale;
	}
	const FVector2D Size = Canvas->K2_TextSize(Font, TEXT("Ağy"), FVector2D(Scale * UIScale, Scale * UIScale));
	return Size.Y;
}

void ACigkofteHUD::Row(const FString& S, const FLinearColor& C, float X, float& Y, UFont* Font, float Scale, float PadMult)
{
	Text(S, C, X, Y, Font, Scale);
	Y += LineHeight(Font, Scale) * PadMult;
}

void ACigkofteHUD::CenterText(const FString& S, float Y, UFont* Font, float Scale, const FLinearColor& Color)
{
	const float W = TextWidth(S, Font, Scale);
	DrawText(S, Color, (ScreenSize.X - W) * 0.5f, Y, Font, Scale * UIScale);
}

void ACigkofteHUD::ShadowText(const FString& S, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
{
	const float Off = FMath::Max(1.f, SX(2.f));
	DrawText(S, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + Off, Y + Off, Font, Scale * UIScale);
	DrawText(S, Color, X, Y, Font, Scale * UIScale);
}

void ACigkofteHUD::ShadowCenterText(const FString& S, float Y, UFont* Font, float Scale, const FLinearColor& Color)
{
	const float W = TextWidth(S, Font, Scale);
	ShadowText(S, Color, (ScreenSize.X - W) * 0.5f, Y, Font, Scale);
}

FString ACigkofteHUD::Stars(float Value01to5)
{
	const int32 Full = FMath::Clamp(FMath::RoundToInt(Value01to5), 0, 5);
	FString S;
	for (int32 i = 0; i < 5; ++i)
	{
		S += (i < Full) ? TEXT("*") : TEXT("-");
	}
	return S;
}

// ---------------------------------------------------------------- main draw

void ACigkofteHUD::DrawHUD()
{
	Super::DrawHUD();

	ACigkofteGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACigkofteGameMode>() : nullptr;
	if (!GM || !Canvas || !GM->Days)
	{
		return;
	}

	ScreenSize = FVector2D(Canvas->SizeX, Canvas->SizeY);
	// Base scale derived from resolution, times the player's accessibility factor.
	UIScale = FMath::Max(0.6f, ScreenSize.Y / 1080.f) * FMath::Clamp(GM->Settings.UIScaleMult, 0.7f, 1.6f);
	ColorBlind = (ECigColorBlindMode)FMath::Clamp(GM->Settings.ColorBlindMode, 0, (int32)ECigColorBlindMode::COUNT - 1);
	FontBody = GEngine->GetLargeFont();
	FontFine = GEngine->GetMediumFont();

	const float Dt = GetWorld()->GetDeltaSeconds();
	const bool bWrapVisible = GM->Orders && GM->Orders->Wrap.bActive;
	WrapPanelAlpha = FMath::FInterpTo(WrapPanelAlpha, bWrapVisible ? 1.f : 0.f, Dt, 8.f);
	ACigkofteCustomer* Front = GM->Customers ? GM->Customers->FrontCustomer() : nullptr;
	const bool bCustomerVisible = Front && Front->bArrived && !Front->bLeaving;
	CustomerPanelAlpha = FMath::FInterpTo(CustomerPanelAlpha, bCustomerVisible ? 1.f : 0.f, Dt, 8.f);
	TabletAlpha = FMath::FInterpTo(TabletAlpha, GM->bTabletOpen ? 1.f : 0.f, Dt, 10.f);
	TabletSlide = FMath::FInterpTo(TabletSlide, GM->bTabletOpen ? 0.f : 1.f, Dt, 10.f);

	if (GM->Days->Phase == ECigPhase::Playing)
	{
		DrawStatusPanel(GM);
		DrawMessages(GM);
		DrawBowlPanel(GM);
		if (WrapPanelAlpha > 0.02f)
		{
			DrawWrapPanel(GM);
		}
		if (CustomerPanelAlpha > 0.02f)
		{
			DrawCustomerPanel(GM);
		}
		DrawDeliveryPanel(GM);
		DrawEnergyBar(GM);
		DrawTutorial(GM);
		DrawPrompt(GM);
		if (TabletAlpha > 0.02f)
		{
		}
	}

	if (GM->bSettingsOpen)
	{
		DrawSettings(GM);
		if (GM->bKeybindsOpen)
		{
			DrawKeybinds(GM);
		}
	}
	if (GM->bShowDebugHUD)
	{
		DrawDebugOverlay(GM);
	}
	DrawOverlays(GM);
	DrawWeatherOverlay(GM);
	DrawScreenFlash(GM);
}

void ACigkofteHUD::DrawWeatherOverlay(ACigkofteGameMode* GM)
{
	UCigWorldBuilder* WB = GM->WorldBuilder.Get();
	if (!WB || !GM->Days || GM->Days->Phase != ECigPhase::Playing)
	{
		return;
	}

	const float W = ScreenSize.X;
	const float H = ScreenSize.Y;

	// Evening gloom: a faint navy veil
	if (WB->Evening > 0.02f)
	{
		FLinearColor Dusk(0.05f, 0.07f, 0.16f);
		Dusk.A = WB->Evening * 0.30f;
		DrawRect(Dusk, 0.f, 0.f, W, H);
	}

	// Power cut: extra darkening
	if (WB->bPowerOut)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.02f, 0.22f), 0.f, 0.f, W, H);
	}

	if (WB->Weather == 1)
	{
		// Rain: slanted drifting streaks over a cold grey veil
		DrawRect(FLinearColor(0.45f, 0.52f, 0.62f, 0.12f), 0.f, 0.f, W, H);

		const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const float Len = SX(46.f);
		const float Slant = SX(11.f);
		for (int32 i = 0; i < 90; ++i)
		{
			// Fixed horizontal slot per drop, vertical position drifts with speed
			const float Seed = (float)i * 137.51f;
			const float X = FMath::Fmod(Seed * 7.3f, W);
			const float Speed = 900.f + FMath::Fmod(Seed, 420.f);
			const float Y = FMath::Fmod(Seed * 3.1f + Time * Speed, H + Len) - Len;
			DrawLine(X, Y, X - Slant, Y + Len, FLinearColor(0.78f, 0.85f, 0.95f, 0.35f), 1.4f);
		}
	}
	else if (WB->Weather == 2)
	{
		// Scorching heat: a warm yellow veil plus a soft glare band from the top
		DrawRect(FLinearColor(1.f, 0.72f, 0.28f, 0.10f), 0.f, 0.f, W, H);
		DrawRect(FLinearColor(1.f, 0.85f, 0.45f, 0.07f), 0.f, 0.f, W, H * 0.32f);
	}
}

void ACigkofteHUD::DrawKeybinds(ACigkofteGameMode* GM)
{
	const float BodyLH = LineHeight(FontBody, BodyScale);
	const float RowH = BodyLH * 1.35f;
	const int32 N = (int32)ECigAction::COUNT;

	const float W = SX(700.f);
	const float H = LineHeight(FontBody, HeadScale) * 1.4f + RowH * N + BodyLH * 3.f + SX(40.f);
	const float X = (ScreenSize.X - W) * 0.5f;
	const float Y0 = (ScreenSize.Y - H) * 0.5f;

	DrawRect(FLinearColor(0.02f, 0.02f, 0.04f, 0.97f), X, Y0, W, H);
	const float HeaderH = LineHeight(FontBody, HeadScale) * 1.4f;
	DrawRect(FLinearColor(0.2f, 0.35f, 0.15f, 0.9f), X, Y0, W, HeaderH);
	Text(CigText::Get(TEXT("settings.keybinds")), GWhite, X + SX(20.f), Y0 + HeaderH * 0.12f, FontBody, HeadScale);

	float Y = Y0 + HeaderH + SX(14.f);
	const float ValueX = X + SX(360.f);
	for (int32 i = 0; i < N; ++i)
	{
		const ECigAction Action = (ECigAction)i;
		const bool bSel = GM->KeybindCursor == i;
		if (bSel)
		{
			DrawRect(FLinearColor(0.4f, 0.8f, 0.2f, 0.3f), X + SX(14.f), Y - SX(3.f), W - SX(28.f), RowH - SX(2.f));
		}

		// While capturing, the selected row says "press a key" so the player knows
		// what is expected.
		FString Value;
		if (bSel && GM->bAwaitingKey)
		{
			Value = CigText::Get(TEXT("settings.keybinds.press"));
		}
		else
		{
			const FKey K = CigInput::Key(Action);
			Value = K.IsValid() ? K.GetDisplayName().ToString() : CigText::Get(TEXT("settings.keybinds.unbound"));
		}

		// A binding that differs from the default is highlighted.
		const FLinearColor C = bSel ? GGold : (CigInput::IsRemapped(Action) ? FLinearColor(0.7f, 0.95f, 0.7f) : GWhite);
		Text(CigInput::ActionName(Action), C, X + SX(30.f), Y, FontBody, BodyScale * 1.05f);
		Text(Value, C, ValueX, Y, FontBody, BodyScale * 1.05f);
		Y += RowH;
	}

	Y += BodyLH * 0.4f;
	Row(CigText::Get(TEXT("settings.keybinds.hint")), GDim, X + SX(30.f), Y, FontBody, BodyScale, 1.15f);
}

FLinearColor ACigkofteHUD::UrgencyColor(float Frac) const
{
	const float F = FMath::Clamp(Frac, 0.f, 1.f);
	switch (ColorBlind)
	{
	case ECigColorBlindMode::KirmiziYesil:
		// Protanopia and deuteranopia cannot separate red from green. The
		// blue-orange axis reads for both, and the two differ in brightness too.
		return FMath::Lerp(FLinearColor(0.95f, 0.55f, 0.05f), FLinearColor(0.15f, 0.55f, 0.95f), F);
	case ECigColorBlindMode::MaviSari:
		// With tritanopia red and green are fine; only the brightness gap is widened.
		return FMath::Lerp(FLinearColor(0.85f, 0.10f, 0.10f), FLinearColor(0.10f, 0.80f, 0.35f), F);
	default:
		return FMath::Lerp(FLinearColor::Red, FLinearColor::Green, F);
	}
}

FString ACigkofteHUD::UrgencyMark(float Frac) const
{
	if (ColorBlind == ECigColorBlindMode::Kapali)
	{
		return FString();
	}
	// Fewer filled squares means more urgency; it reads even without colour.
	if (Frac >= 0.66f) { return TEXT("[|||]"); }
	if (Frac >= 0.33f) { return TEXT("[|| ]"); }
	if (Frac >= 0.12f) { return TEXT("[|  ]"); }
	return TEXT("[ ! ]");
}

void ACigkofteHUD::DrawScreenFlash(ACigkofteGameMode* GM)
{
	// Accessibility: nothing is drawn when flashing is off. No information is
	// lost - the same feedback also arrives in the message feed and in audio.
	if (GM->FlashTimer <= 0.f || !GM->Settings.bScreenFlash)
	{
		return;
	}
	const float A = FMath::Clamp(GM->FlashTimer / FMath::Max(0.1f, GM->FlashDuration), 0.f, 1.f);
	const float W = ScreenSize.X;
	const float H = ScreenSize.Y;
	const float Band = SX(90.f);
	FLinearColor C = GM->FlashColor;
	C.A = A * 0.35f;
	// A soft vignette band along all four screen edges
	DrawRect(C, 0.f, 0.f, W, Band);
	DrawRect(C, 0.f, H - Band, W, Band);
	DrawRect(C, 0.f, 0.f, Band, H);
	DrawRect(C, W - Band, 0.f, Band, H);
}

// ---------------------------------------------------------------- status panel

void ACigkofteHUD::DrawStatusPanel(ACigkofteGameMode* GM)
{
	UCigDaySystem* Days = GM->Days.Get();
	UCigEconomySystem* Eco = GM->Economy.Get();
	UCigProgressionSystem* Prog = GM->Progression.Get();
	UCigHygieneSystem* Hyg = GM->Hygiene.Get();
	if (!Days || !Eco || !Prog || !Hyg)
	{
		return;
	}

	const float X = SX(24.f);
	const float W = SX(430.f);
	const float BodyLH = LineHeight(FontBody, BodyScale);
	const float BarH = BodyLH * 0.55f;

	// Height: title (HeadScale) + money row (HeadScale) + 7.5 body rows. These
	// must be the exact same factors as the row advances below, or the last row
	// (shop score) spills outside the panel.
	const float H = LineHeight(FontBody, HeadScale) * 2.3f + BodyLH * 7.5f + SX(24.f);
	float Y = SX(24.f);
	Panel(X, Y, W, H);
	PanelHeader(X, Y, W, CigText::Format(TEXT("hud.daytitle"), Days->Day), GOrange);
	Y += LineHeight(FontBody, HeadScale) * 1.25f + SX(8.f);

	const float TX = X + SX(16.f);

	// A short pop when money goes up
	const float Dt = GetWorld()->GetDeltaSeconds();
	if (LastMoney != INT32_MIN && Eco->Money > LastMoney)
	{
		MoneyPop = 1.f;
	}
	LastMoney = Eco->Money;
	MoneyPop = FMath::Max(0.f, MoneyPop - Dt * 3.f);
	const float MoneyScale = HeadScale * (1.f + 0.25f * MoneyPop);
	Text(CigText::Format(TEXT("hud.money"), Eco->Money), Eco->Money >= 0 ? GGold : FLinearColor::Red, TX, Y, FontBody, MoneyScale);
	Y += LineHeight(FontBody, HeadScale) * 1.05f;
	Row(CigText::Format(TEXT("hud.timeleft"), FMath::CeilToInt(FMath::Max(0.f, Days->TimeLeft))), GDim, TX, Y, FontBody, BodyScale, 1.05f);

	// Level and XP
	{
		const int32 Next = Prog->XPForNext();
		const float XPFrac = Next > 0 ? FMath::Clamp((float)Prog->XP / (float)Next, 0.f, 1.f) : 1.f;
		Text(CigText::Format(TEXT("hud.level"), Prog->Level), FLinearColor(0.5f, 0.85f, 1.f), TX, Y, FontBody, BodyScale);
		Bar(TX + SX(150.f), Y + BodyLH * 0.2f, W - SX(190.f), BarH, XPFrac, FLinearColor(0.3f, 0.7f, 1.f));
		Y += BodyLH * 1.15f;
	}

	// Popularity
	{
		Text(CigText::Get(TEXT("hud.popularity")), GDim, TX, Y, FontBody, BodyScale);
		Bar(TX + SX(150.f), Y + BodyLH * 0.2f, W - SX(190.f), BarH, Prog->Rep / 100.f, FLinearColor(0.9f, 0.7f, 0.2f));
		Y += BodyLH * 1.05f;
		Row(Prog->PopularityTitle(), GGold, TX + SX(150.f), Y, FontBody, BodyScale, 1.1f);
	}

	// Hygiene
	{
		const float Hygiene = Hyg->OverallHygiene();
		Text(CigText::Get(TEXT("hud.hygiene")), GDim, TX, Y, FontBody, BodyScale);
		Bar(TX + SX(150.f), Y + BodyLH * 0.2f, W - SX(190.f), BarH, Hygiene / 100.f,
			Hygiene < 50.f ? GBad : FLinearColor(0.3f, 0.8f, 1.f));
		Y += BodyLH * 1.1f;
		const FString Problem = Hyg->WorstProblem();
		if (!Problem.IsEmpty())
		{
			Row(Problem, FLinearColor(1.f, 0.75f, 0.5f), TX, Y, FontBody, BodyScale, 1.05f);
		}
		else
		{
			Y += BodyLH * 1.05f;
		}
	}

	// Shop score / event
	if (GM->Reviews)
	{
		FString Line = CigText::Format(TEXT("hud.shopstars"), *Stars(GM->Reviews->ShopScore()));
		if (GM->Events && GM->Events->Active.Num() > 0)
		{
			Line += CigText::Format(TEXT("hud.event"), *UCigEventSystem::EventName(GM->Events->Active[0].DefIndex));
		}
		Row(Line, FLinearColor(1.f, 0.8f, 0.4f), TX, Y, FontBody, BodyScale, 1.f);
	}
}

// ---------------------------------------------------------------- messages

void ACigkofteHUD::DrawMessages(ACigkofteGameMode* GM)
{
	const float LH = LineHeight(FontBody, BodyScale);
	float MsgY = SX(24.f);

	// Start below the delivery panel when there is one.
	//
	// The panel is 720 wide and centred; the message feed is right-aligned and
	// starts 24 from the top. A long message reaches back far enough to sit on
	// top of the panel, and the screenshot pass caught two lines of delivery text
	// printed over one another. Neither element is wrong on its own - they were
	// simply never asked to share the top of the screen.
	const UCigDeliverySystem* Del = GM ? GM->Delivery.Get() : nullptr;
	if (Del && Del->Orders.Num() > 0)
	{
		MsgY += LineHeight(FontBody, HeadScale) + Del->Orders.Num() * LH * 1.15f + SX(24.f);
	}
	for (const FCigMessage& M : GM->Messages)
	{
		const float Alpha = FMath::Clamp(M.TimeLeft / 1.5f, 0.f, 1.f);
		FLinearColor C = M.Color;
		C.A = Alpha;
		const float TW = TextWidth(M.Text, FontBody, BodyScale);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), ScreenSize.X - TW - SX(44.f), MsgY - SX(2.f), TW + SX(24.f), LH * 1.1f);
		Text(M.Text, C, ScreenSize.X - TW - SX(32.f), MsgY, FontBody, BodyScale);
		MsgY += LH * 1.2f;
	}
}

// ---------------------------------------------------------------- bowl panel

void ACigkofteHUD::DrawBowlPanel(ACigkofteGameMode* GM)
{
	UCigCookingSystem* Cook = GM->Cooking.Get();
	UCigInventorySystem* Inv = GM->Inventory.Get();
	if (!Cook || !Inv)
	{
		return;
	}

	const FCigRecipe& R = UCigCookingSystem::Recipe(Cook->CurrentRecipe);
	int32 Target[(int32)ECigIngredient::COUNT];
	UCigCookingSystem::TargetCounts(R, Target);

	const float BodyLH = LineHeight(FontBody, BodyScale);
	const float X = SX(24.f);
	const float W = SX(470.f);

	// The mix diagnosis is needed before the panel is sized, because it adds
	// rows to it. Computed once and used twice rather than derived again below.
	const bool bDiagnose = !Cook->Dough.IsValid();
	const FCigMixDiagnosis Tani = bDiagnose
		? CigMix::Diagnose(Cook->Bowl, R.RatioSu, R.RatioSalca, R.RatioBaharat,
			R.IsotMinFrac, R.IsotMaxFrac, Cook->BowlTotal(), UCigCookingSystem::BowlCapacity)
		: FCigMixDiagnosis();

	// Height by row count: title + target + 5 ingredients + kneading + dough(2) + stock summary
	int32 RowCount = 8;
	if (Cook->Dough.IsValid()) { RowCount += 2; }
	if (Cook->FridgeDough.IsValid()) { RowCount += 1; }
	if (Tani.IsProblem()) { RowCount += Tani.bFixableByAdding ? 1 : 2; }
	const float H = LineHeight(FontBody, HeadScale) * 1.25f + BodyLH * (RowCount + 1.2f) + SX(20.f);
	float Y = ScreenSize.Y - H - SX(24.f);
	const float PanelTop = Y;
	Panel(X, Y, W, H);
	PanelHeader(X, Y, W, CigText::Format(TEXT("hud.bowl"), R.Name), FLinearColor(0.8f, 0.35f, 0.15f));
	Y += LineHeight(FontBody, HeadScale) * 1.25f + SX(8.f);

	const float TX = X + SX(16.f);
	Row(CigText::Format(TEXT("hud.targetmix"), *UCigCookingSystem::HumanIsotLevel(R)), GDim, TX, Y, FontBody, BodyScale, 1.1f);

	// Ingredients: current/target
	for (int32 i = 0; i < (int32)ECigIngredient::COUNT; ++i)
	{
		const ECigIngredient Ing = (ECigIngredient)i;
		DrawRect(CigIngredientColor(Ing), TX, Y + BodyLH * 0.15f, BodyLH * 0.6f, BodyLH * 0.6f);
		const bool bOk = Cook->Bowl[i] >= Target[i];
		Row(CigText::Format(TEXT("hud.scoop"), *CigIngredientName(Ing), Cook->Bowl[i], Target[i]),
			bOk ? GGood : GWhite, TX + BodyLH, Y, FontBody, BodyScale, 1.08f);
	}

	// What is wrong with the mix, while it can still be put right.
	//
	// The bowl was judged silently: the ratio error reached the player at the
	// till, minutes later, as a smaller payment and a worse review. The counts
	// above tell a careful reader what to add next; they do not say that the
	// batch about to be kneaded is already ruined. Only shown once a batch is
	// not on the counter, because a bowl behind a finished batch is not the
	// thing the player is working on.
	if (Tani.IsProblem())
	{
		// Amber while it is recoverable, red once the bowl is too full to
		// dilute - the colour is the difference between "fix this" and "start
		// over", which is the whole point of saying it at all.
		Row(CigText::Get(CigMix::TextKey(Tani.Problem)),
			Tani.bFixableByAdding ? GOrange : GBad, TX, Y, FontBody, BodyScale, 1.1f);
		if (!Tani.bFixableByAdding)
		{
			Row(CigText::Get(TEXT("mix.unfixable")), GBad, TX, Y, FontBody, BodyScale, 1.1f);
		}
	}

	// Kneading
	Text(CigText::Get(TEXT("hud.knead")), GDim, TX, Y, FontBody, BodyScale);
	Bar(TX + SX(140.f), Y + BodyLH * 0.2f, W - SX(180.f), BodyLH * 0.55f, Cook->KneadProgress / 100.f, FLinearColor(0.8f, 0.35f, 0.15f));
	Y += BodyLH * 1.15f;

	if (Cook->Dough.IsValid())
	{
		Row(CigText::Format(TEXT("hud.dough"), Cook->Dough.Servings, *CigQualityName(Cook->Dough.Quality), *CigSpiceName(Cook->Dough.Spice)),
			GGood, TX, Y, FontBody, BodyScale, 1.05f);
		Text(CigText::Get(TEXT("hud.freshness")), GDim, TX, Y, FontBody, BodyScale);
		Bar(TX + SX(140.f), Y + BodyLH * 0.2f, W - SX(180.f), BodyLH * 0.5f, Cook->Dough.Freshness / 100.f,
			Cook->Dough.Freshness < 30.f ? GBad : FLinearColor(0.5f, 0.9f, 0.5f));
		Y += BodyLH * 1.15f;
	}
	if (Cook->FridgeDough.IsValid())
	{
		Row(CigText::Format(TEXT("hud.fridge"), Cook->FridgeDough.Servings, Cook->FridgeDough.Freshness),
			GInfo, TX, Y, FontBody, BodyScale, 1.08f);
	}

	Row(CigText::Format(TEXT("hud.supplies"),
		Inv->Garnish, Inv->Stock[CigStockLavas], Inv->Stock[CigStockAyran],
		GM->Orders ? GM->Orders->Shelf.Num() : 0, UCigOrderSystem::MaxShelf),
		FLinearColor(0.75f, 0.92f, 0.75f), TX, Y, FontBody, BodyScale, 1.f);
}

// ---------------------------------------------------------------- wrap panel

void ACigkofteHUD::DrawWrapPanel(ACigkofteGameMode* GM)
{
	UCigOrderSystem* Orders = GM->Orders.Get();
	if (!Orders)
	{
		return;
	}
	const FCigWrapBuild& Wrap = Orders->Wrap;
	const float A = WrapPanelAlpha;
	const float BodyLH = LineHeight(FontBody, BodyScale);

	// Gamepad cursor: 0-6 toppings, 7 ayran, 8 side
	ACigkoftePlayerCharacter* Player = Cast<ACigkoftePlayerCharacter>(GetOwningPawn());
	const bool bPad = Player && Player->bGamepadActive;
	const int32 PadCursor = Player ? Player->GamepadWrapCursor : -1;

	const float W = SX(520.f);
	const float H = LineHeight(FontBody, HeadScale) * 1.25f + BodyLH * 8.5f + SX(20.f);
	const float X = SX(510.f);
	const float Y0 = ScreenSize.Y - H - SX(24.f) + (1.f - A) * SX(40.f);
	Panel(X, Y0, W, H, 0.62f * A);
	PanelHeader(X, Y0, W, CigText::Get(Wrap.bWrapped ? TEXT("hud.wrapdone") : TEXT("hud.wrapinprogress")), Wrap.bWrapped ? FLinearColor(0.3f, 0.7f, 0.3f) : GOrange);

	auto WithA = [A](FLinearColor C) { C.A *= A; return C; };
	float Y = Y0 + LineHeight(FontBody, HeadScale) * 1.25f + SX(8.f);
	const float TX = X + SX(16.f);

	Row(Wrap.Portions > 0
		? CigText::Format(TEXT("hud.wrapcontents"), Wrap.Portions, *CigSpiceName(Wrap.Spice))
		: CigText::Get(TEXT("hud.wrapempty")),
		WithA(Wrap.Portions > 0 ? GGood : FLinearColor(1.f, 0.8f, 0.4f)), TX, Y, FontBody, BodyScale, 1.15f);

	// Toppings in two columns
	Row(CigText::Get(bPad ? TEXT("hud.toppingspad") : TEXT("hud.toppingshint")),
		WithA(GDim), TX, Y, FontBody, BodyScale, 1.1f);
	const float ColW = (W - SX(32.f)) * 0.5f;
	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		const ECigTopping T = (ECigTopping)i;
		const bool bOn = Wrap.HasTopping(T);
		const float CX = TX + (i % 2) * ColW;
		const float CY = Y + (i / 2) * BodyLH * 1.05f;
		const bool bCursor = bPad && PadCursor == i;
		Text(FString::Printf(TEXT("%s%d  %s %s"), bCursor ? TEXT(">") : TEXT(" "), i + 1, bOn ? TEXT("[X]") : TEXT("[  ]"), *CigToppingName(T)),
			WithA(bOn ? GGood : (bCursor ? GGold : GDim)), CX, CY, FontBody, BodyScale);
	}
	Y += BodyLH * 1.05f * 4.f + BodyLH * 0.2f;

	Row(FString::Printf(TEXT("%s8  %s Ayran      %s"),
		(bPad && PadCursor == 7) ? TEXT(">") : TEXT(" "),
		Wrap.bAyran ? TEXT("[X]") : TEXT("[  ]"),
		*CigText::Get(Wrap.bPacked ? TEXT("hud.packed") : TEXT("hud.onplate"))),
		WithA(Wrap.bAyran ? GGood : ((bPad && PadCursor == 7) ? GGold : GDim)), TX, Y, FontBody, BodyScale, 1.15f);

	// Menu side
	{
		const bool bHasSide = Wrap.Side != ECigSide::Yok;
		const bool bCursor = bPad && PadCursor == 8;
		Row(CigText::Format(TEXT("hud.sideline"),
			bCursor ? TEXT(">") : TEXT(" "),
			bHasSide ? TEXT("[X]") : TEXT("[  ]"),
			*CigSideName(Wrap.Side),
			bHasSide ? *CigText::Format(TEXT("hud.sideprice"), CigSidePrice(Wrap.Side)) : TEXT("")),
			WithA(bHasSide ? GGood : (bCursor ? GGold : GDim)), TX, Y, FontBody, BodyScale, 1.15f);
	}

	Row(CigText::Get(Wrap.bWrapped ? TEXT("hud.wrappedhint") : TEXT("hud.wraphint")),
		WithA(GInfo), TX, Y, FontBody, BodyScale, 1.f);
}

// ---------------------------------------------------------------- customer panel

void ACigkofteHUD::DrawCustomerPanel(ACigkofteGameMode* GM)
{
	ACigkofteCustomer* C = GM->Customers ? GM->Customers->FrontCustomer() : nullptr;
	if (!C)
	{
		return;
	}
	const float A = CustomerPanelAlpha;
	const float BodyLH = LineHeight(FontBody, BodyScale);
	auto WithA = [A](FLinearColor Col) { Col.A *= A; return Col; };

	// Lists of wanted and unwanted toppings
	TArray<FString> Wants;
	for (int32 i = 0; i < (int32)ECigTopping::COUNT; ++i)
	{
		if (C->Spec.WantsTopping((ECigTopping)i))
		{
			Wants.Add(CigToppingName((ECigTopping)i));
		}
	}

	const float W = SX(500.f);
	const float H = LineHeight(FontBody, HeadScale) * 1.25f + BodyLH * 6.6f + SX(20.f);
	const float X = ScreenSize.X - W - SX(24.f);
	const float Y0 = ScreenSize.Y - H - SX(24.f) + (1.f - A) * SX(40.f);
	Panel(X, Y0, W, H, 0.62f * A);
	PanelHeader(X, Y0, W, C->LoyalName.IsEmpty()
		? CigText::Get(C->bVIP ? TEXT("hud.nextvip") : TEXT("hud.nextcustomer"))
		: CigText::Format(TEXT("hud.regular"), *C->LoyalName),
		C->bVIP ? GGold : FLinearColor(0.2f, 0.5f, 0.8f));

	float Y = Y0 + LineHeight(FontBody, HeadScale) * 1.25f + SX(8.f);
	const float TX = X + SX(16.f);

	Row(CigText::Format(TEXT("hud.spiceline"), CigSpiceName(C->Spec.Spice),
		CigText::Get(C->Spec.Portion >= 2 ? TEXT("hud.twoportions") : TEXT("hud.onewrap"))),
		WithA(FLinearColor(1.f, 0.65f, 0.45f)), TX, Y, FontBody, HeadScale * 0.9f, 1.15f);

	Row(Wants.Num() > 0 ? CigText::Format(TEXT("hud.wantsline"), FString::Join(Wants, TEXT(", "))) : CigText::Get(TEXT("hud.notoppings")),
		WithA(GWhite), TX, Y, FontBody, BodyScale, 1.1f);

	if (!C->Spec.WantsTopping(ECigTopping::Sogan))
	{
		Row(CigText::Get(TEXT("hud.noonion")), WithA(GBad), TX, Y, FontBody, BodyScale, 1.1f);
	}
	else
	{
		Y += BodyLH * 0.3f;
	}

	Row(CigText::Format(TEXT("hud.pair"),
		CigText::Get(C->Spec.bWantsAyran ? TEXT("hud.ayranyes") : TEXT("hud.noayran")),
		CigText::Get(C->Spec.bPacked ? TEXT("hud.takeaway") : TEXT("hud.plate"))),
		WithA(GInfo), TX, Y, FontBody, BodyScale, 1.15f);

	// Traits
	TArray<FString> TraitNames;
	for (int32 i = 0; i < CigTraitCount; ++i)
	{
		const ECigTrait T = (ECigTrait)(1 << i);
		if (EnumHasAnyFlags(C->Traits, T) && T != ECigTrait::SecretCritic)
		{
			TraitNames.Add(CigTraitName(T));
		}
	}
	if (TraitNames.Num() > 0)
	{
		Row(FString::Join(TraitNames, TEXT(" | ")), WithA(FLinearColor(0.75f, 0.75f, 1.f)), TX, Y, FontBody, BodyScale, 1.1f);
	}

	const float PatienceFrac = FMath::Clamp(C->Patience / FMath::Max(1.f, C->MaxPatience), 0.f, 1.f);
	Text(CigText::Get(TEXT("hud.patience")), WithA(GDim), TX, Y, FontBody, BodyScale);

	// In colour-blind mode a colour-independent urgency marker goes to the right
	// of the bar, and the bar is shortened just enough to make room for it.
	const FString Mark = UrgencyMark(PatienceFrac);
	const float MarkW = Mark.IsEmpty() ? 0.f : TextWidth(Mark, FontBody, BodyScale) + SX(10.f);
	Bar(TX + SX(110.f), Y + BodyLH * 0.2f, W - SX(150.f) - MarkW, BodyLH * 0.55f, PatienceFrac, WithA(UrgencyColor(PatienceFrac)));
	if (!Mark.IsEmpty())
	{
		Text(Mark, WithA(UrgencyColor(PatienceFrac)), TX + W - SX(40.f) - MarkW, Y, FontBody, BodyScale);
	}
}

// ---------------------------------------------------------------- delivery, energy & tutorial

void ACigkofteHUD::DrawDeliveryPanel(ACigkofteGameMode* GM)
{
	UCigDeliverySystem* Del = GM->Delivery.Get();
	if (!Del || Del->Orders.Num() == 0)
	{
		return;
	}

	APawn* Pawn = GetOwningPawn();
	const float BodyLH = LineHeight(FontBody, BodyScale);
	const float W = SX(720.f);
	const float H = LineHeight(FontBody, HeadScale) + Del->Orders.Num() * BodyLH * 1.15f + SX(16.f);
	const float X = (ScreenSize.X - W) * 0.5f;
	const float Y0 = SX(16.f);
	DrawRect(FLinearColor(0.25f, 0.2f, 0.f, 0.75f), X, Y0, W, H);
	CenterText(CigText::Get(TEXT("hud.delivery")), Y0 + SX(4.f), FontBody, HeadScale * 0.9f, GGold);

	float Y = Y0 + LineHeight(FontBody, HeadScale) + SX(6.f);
	for (const FCigDeliveryOrder& O : Del->Orders)
	{
		const float DistM = Pawn ? FVector::Dist2D(Pawn->GetActorLocation(), O.Pos) / 100.f : 0.f;
		Row(CigText::Format(TEXT("hud.deliveryrow"),
			O.bBulk ? *CigText::Get(TEXT("hud.bulkprefix")) : TEXT(""), *O.Address, FMath::CeilToInt(O.TimeLeft), FMath::RoundToInt(DistM),
			*UCigOrderSystem::DescribeSpec(O.Spec)),
			O.TimeLeft < 20.f ? GBad : GWhite, X + SX(18.f), Y, FontBody, BodyScale, 1.15f);
	}
}

void ACigkofteHUD::DrawEnergyBar(ACigkofteGameMode* GM)
{
	ACigkoftePlayerCharacter* Player = Cast<ACigkoftePlayerCharacter>(GetOwningPawn());
	if (!Player)
	{
		return;
	}
	const float BodyLH = LineHeight(FontBody, BodyScale);
	const float W = SX(220.f);
	const float X = (ScreenSize.X - W) * 0.5f;
	const float Y = ScreenSize.Y - SX(76.f);

	ShadowText(CigText::Get(TEXT("hud.energy")), GDim, X - SX(84.f), Y - BodyLH * 0.15f, FontBody, BodyScale);
	Bar(X, Y, W, BodyLH * 0.5f, Player->Energy / 100.f, Player->Energy < 30.f ? GBad : FLinearColor(0.4f, 0.9f, 0.5f));

	if (Player->bDriving && GM->PlayerCar)
	{
		ShadowText(CigText::Get(TEXT("hud.fuel")), GDim, X - SX(84.f), Y + BodyLH * 0.6f, FontBody, BodyScale);
		Bar(X, Y + BodyLH * 0.75f, W, BodyLH * 0.4f, GM->PlayerCar->Fuel / 100.f, FLinearColor(0.9f, 0.7f, 0.2f));
	}
}

void ACigkofteHUD::DrawTutorial(ACigkofteGameMode* GM)
{
	UCigQuestSystem* Quests = GM->Quests.Get();
	if (!Quests || !Quests->bTutorialActive)
	{
		return;
	}
	const FString Line = Quests->TutorialText();
	if (Line.IsEmpty())
	{
		return;
	}
	const float LH = LineHeight(FontBody, BodyScale);
	const float TW = TextWidth(Line, FontBody, BodyScale);
	const float X = (ScreenSize.X - TW) * 0.5f;
	const float Y = SX(130.f);
	DrawRect(FLinearColor(0.05f, 0.2f, 0.35f, 0.85f), X - SX(18.f), Y - SX(6.f), TW + SX(36.f), LH * 1.35f);
	Text(Line, FLinearColor(0.7f, 0.95f, 1.f), X, Y, FontBody, BodyScale);
}

// ---------------------------------------------------------------- crosshair & prompt

void ACigkofteHUD::DrawPrompt(ACigkofteGameMode* GM)
{
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.8f), ScreenSize.X * 0.5f - SX(2.f), ScreenSize.Y * 0.5f - SX(2.f), SX(4.f), SX(4.f));

	ACigkoftePlayerCharacter* PC = Cast<ACigkoftePlayerCharacter>(GetOwningPawn());
	if (!PC)
	{
		return;
	}

	const float PromptY = ScreenSize.Y * 0.5f + SX(36.f);
	if (PC->bDriving)
	{
		ShadowCenterText(CigText::Get(TEXT("hud.deliverprompt")), ScreenSize.Y - SX(120.f), FontBody, BodyScale, GInfo);
	}
	else if (PC->bCarFocused)
	{
		const int32 Level = GM->Progression ? GM->Progression->Level : 1;
		ShadowCenterText(CigText::Get(Level >= 4 ? TEXT("hud.entercar") : TEXT("hud.carlocked")), PromptY, FontBody, HeadScale, FLinearColor(1.f, 0.8f, 0.4f));
	}
	else if (PC->bCatFocused)
	{
		const FString CatName = GM->CatSys ? GM->CatSys->CatName : CigText::Get(TEXT("hud.cat"));
		ShadowCenterText(CigText::Format(TEXT("hud.petcat"), *CatName), PromptY, FontBody, HeadScale, FLinearColor(1.f, 0.8f, 0.9f));
	}
	else if (PC->FocusedStation)
	{
		const ECigStation T = PC->FocusedStation->StationType;
		const bool bStationLocked = PC->FocusedStation->IsLocked();
		FString Prompt = PC->FocusedStation->GetPromptText();
		if (bStationLocked)
		{
			// Locked station: only the "unlocks at level N" text, dimmed.
		}
		else if (T == ECigStation::Eldiven || T == ECigStation::IsotPlus || T == ECigStation::Reklam)
		{
			Prompt = GM->Economy ? GM->Economy->UpgradeText(T) : Prompt;
		}
		else if ((uint8)T < (uint8)ECigIngredient::COUNT && GM->Inventory)
		{
			Prompt += CigText::Format(TEXT("hud.stockparen"), GM->Inventory->Stock[(uint8)T]);
		}
		else if (T == ECigStation::Dograma && GM->Inventory && GM->Economy)
		{
			int32 Needed = GM->Economy->HasUpgrade(ECigUpgrade::HizliDograma) ? 2 : 4;
			if (GM->Skills)
			{
				Needed = FMath::Max(1, Needed - GM->Skills->ChopReduction());
			}
			Prompt = CigText::Format(TEXT("hud.chopprompt"), GM->Inventory->ChopCombo, Needed, GM->Inventory->Stock[CigStockMarul], GM->Inventory->Garnish);
		}
		ShadowCenterText(Prompt, PromptY, FontBody, HeadScale,
			bStationLocked ? FLinearColor(0.72f, 0.72f, 0.76f) : FLinearColor(1.f, 1.f, 0.6f));
	}

	// The key row sits at the very bottom, over the shop floor. It was drawn at
	// 0.62 grey, which the floor swallowed whole; it is the one line a new player
	// needs to find, so it gets both the lighter tone and the shadow.
	const bool bPad = PC && PC->bGamepadActive;
	ShadowCenterText(CigText::Get(bPad ? TEXT("hud.controls.pad") : TEXT("hud.controls.keyboard")),
		ScreenSize.Y - SX(34.f), FontBody, BodyScale * 0.9f, FLinearColor(0.86f, 0.86f, 0.86f));
}

// ---------------------------------------------------------------- settings

void ACigkofteHUD::DrawSettings(ACigkofteGameMode* GM)
{
	const float BodyLH = LineHeight(FontBody, BodyScale);
	const float RowH = BodyLH * 1.5f;
	const float W = SX(780.f);
	const float H = LineHeight(FontBody, HeadScale) * 1.4f + RowH * ACigkofteGameMode::SettingsCount + BodyLH * 3.5f + SX(40.f);
	const float X = (ScreenSize.X - W) * 0.5f;
	const float Y0 = (ScreenSize.Y - H) * 0.5f;

	DrawRect(FLinearColor(0.02f, 0.02f, 0.04f, 0.96f), X, Y0, W, H);
	const float HeaderH = LineHeight(FontBody, HeadScale) * 1.4f;
	DrawRect(FLinearColor(0.35f, 0.25f, 0.1f, 0.9f), X, Y0, W, HeaderH);
	Text(CigText::Get(TEXT("settings.title")), GWhite, X + SX(20.f), Y0 + HeaderH * 0.12f, FontBody, HeadScale);

	static const FIntPoint Resolutions[4] = { {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440} };
	static const TCHAR* WindowKeys[3] = { TEXT("window.windowed"), TEXT("window.fullscreen"), TEXT("window.borderless") };
	static const TCHAR* QualityKeys[4] = { TEXT("quality.low"), TEXT("quality.medium"), TEXT("quality.high"), TEXT("quality.epic") };
	static const TCHAR* ColorBlindKeys[3] = { TEXT("colorblind.off"), TEXT("colorblind.redgreen"), TEXT("colorblind.blueyellow") };

	// Labels are aligned to a fixed width. So a longer translation cannot shift
	// the column, the value is drawn separately at a fixed X (in the loop below).
	const FCigRuntimeSettings& S = GM->Settings;
	auto OnOff = [](bool b) { return CigText::Get(b ? TEXT("common.on") : TEXT("common.off")); };

	TArray<TPair<FString, FString>> Rows;   // label, value
	Rows.Emplace(CigText::Get(TEXT("settings.resolution")),
		FString::Printf(TEXT("%d x %d"), Resolutions[S.ResolutionIndex].X, Resolutions[S.ResolutionIndex].Y));
	Rows.Emplace(CigText::Get(TEXT("settings.windowmode")), CigText::Get(WindowKeys[FMath::Clamp(S.WindowMode, 0, 2)]));
	Rows.Emplace(CigText::Get(TEXT("settings.quality")), CigText::Get(QualityKeys[FMath::Clamp(S.QualityLevel, 0, 3)]));
	Rows.Emplace(CigText::Get(TEXT("settings.fov")), FString::Printf(TEXT("%.0f"), S.FOV));
	Rows.Emplace(CigText::Get(TEXT("settings.sensitivity")), FString::Printf(TEXT("%.1f"), S.MouseSensitivity));
	Rows.Emplace(CigText::Get(TEXT("settings.headbob")), OnOff(S.bHeadBob));
	Rows.Emplace(CigText::Get(TEXT("settings.mastervolume")), FString::Printf(TEXT("%%%.0f"), S.MasterVolume * 100.f));
	Rows.Emplace(CigText::Get(TEXT("settings.effectsvolume")), FString::Printf(TEXT("%%%.0f"), S.EffectsVolume * 100.f));
	Rows.Emplace(CigText::Get(TEXT("settings.musicvolume")), FString::Printf(TEXT("%%%.0f"), S.MusicVolume * 100.f));
	Rows.Emplace(CigText::Get(TEXT("settings.uiscale")), FString::Printf(TEXT("%%%.0f"), S.UIScaleMult * 100.f));
	Rows.Emplace(CigText::Get(TEXT("settings.screenflash")), OnOff(S.bScreenFlash));
	Rows.Emplace(CigText::Get(TEXT("settings.colorblind")), CigText::Get(ColorBlindKeys[FMath::Clamp(S.ColorBlindMode, 0, 2)]));
	Rows.Emplace(CigText::Get(TEXT("settings.language")), CigText::LanguageName(S.Language));
	Rows.Emplace(CigText::Get(TEXT("settings.keybinds")), TEXT(">"));
	Rows.Emplace(CigText::Get(TEXT("settings.resettutorial")), FString());

	// The list and the cursor must stay the same length, or a row becomes unselectable.
	checkf(Rows.Num() == ACigkofteGameMode::SettingsCount,
		TEXT("Ayarlar satır sayısı (%d) SettingsCount (%d) ile uyuşmuyor"), Rows.Num(), ACigkofteGameMode::SettingsCount);

	float Y = Y0 + HeaderH + SX(14.f);
	const float ValueX = X + SX(340.f);   // value column, independent of label length
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const bool bSel = GM->SettingsCursor == i;
		if (bSel)
		{
			DrawRect(FLinearColor(0.9f, 0.55f, 0.1f, 0.35f), X + SX(14.f), Y - SX(3.f), W - SX(28.f), RowH - SX(2.f));
		}
		const FLinearColor C = bSel ? GGold : GWhite;
		Text(Rows[i].Key, C, X + SX(30.f), Y, FontBody, BodyScale * 1.1f);
		if (!Rows[i].Value.IsEmpty())
		{
			Text(Rows[i].Value, C, ValueX, Y, FontBody, BodyScale * 1.1f);
		}
		Y += RowH;
	}

	Y += BodyLH * 0.4f;
	Row(CigText::Get(TEXT("settings.hint.nav")), GDim, X + SX(30.f), Y, FontBody, BodyScale, 1.15f);
	Row(CigText::Get(TEXT("hud.keyshint")), GDim, X + SX(30.f), Y, FontBody, BodyScale, 1.f);
}

// ---------------------------------------------------------------- debug

void ACigkofteHUD::DrawDebugOverlay(ACigkofteGameMode* GM)
{
	const float BodyLH = LineHeight(FontFine, FineScale);
	const float X = ScreenSize.X - SX(400.f);
	float Y = SX(240.f);

	extern ENGINE_API float GAverageFPS;
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("FPS: %.0f"), GAverageFPS));
	if (GM->Days)
	{
		Lines.Add(FString::Printf(TEXT("Faz %d | Gün %d | %.0f sn"), (int32)GM->Days->Phase, GM->Days->Day, GM->Days->TimeLeft));
	}
	if (GM->Customers)
	{
		Lines.Add(FString::Printf(TEXT("Kuyruk %d/%d | Sadık %d | Müfettiş %s"), GM->Customers->Queue.Num(), GM->Customers->MaxQueue(), GM->Customers->Loyals.Num(), GM->Customers->Inspector ? TEXT("var") : TEXT("yok")));
	}
	if (GM->Cooking)
	{
		Lines.Add(FString::Printf(TEXT("Hamur %d K%.0f T%.0f | Tarif %d"), GM->Cooking->Dough.Servings, GM->Cooking->Dough.Quality, GM->Cooking->Dough.Freshness, GM->Cooking->CurrentRecipe));
	}
	if (GM->Orders)
	{
		Lines.Add(FString::Printf(TEXT("Dürüm %s | Raf %d"), GM->Orders->Wrap.bActive ? TEXT("aktif") : TEXT("yok"), GM->Orders->Shelf.Num()));
	}
	if (GM->Hygiene)
	{
		Lines.Add(FString::Printf(TEXT("Hijyen %.0f"), GM->Hygiene->OverallHygiene()));
	}
	if (GM->Quests)
	{
		Lines.Add(FString::Printf(TEXT("Görev %d/%d | Hikâye %d | Tut %d"), GM->Quests->QuestProgress, GM->Quests->QuestTarget, (int32)GM->Quests->StoryStage, (int32)GM->Quests->TutorialStep));
	}
	if (GM->Delivery)
	{
		Lines.Add(FString::Printf(TEXT("Teslimat %d aktif, bugün %d"), GM->Delivery->Orders.Num(), GM->Delivery->DayDelivered));
	}
	if (GM->Events)
	{
		Lines.Add(FString::Printf(TEXT("Olay %d aktif"), GM->Events->Active.Num()));
	}
	if (GM->Economy)
	{
		Lines.Add(FString::Printf(TEXT("Para %d | Tedarikçi %d"), GM->Economy->Money, GM->Economy->CurrentSupplier));
	}

	Panel(X, Y, SX(380.f), Lines.Num() * BodyLH * 1.25f + SX(16.f), 0.8f);
	Y += SX(8.f);
	for (const FString& L : Lines)
	{
		Row(L, FLinearColor(0.7f, 1.f, 0.7f), X + SX(10.f), Y, FontFine, FineScale, 1.25f);
	}
}

// ---------------------------------------------------------------- overlays

void ACigkofteHUD::DrawTitleScreen(ACigkofteGameMode* GM)
{
	const float T = GM->IntroTime;
	const float SplashEnd = ACigkofteGameMode::SplashDuration;
	const float W = ScreenSize.X;
	const float H = ScreenSize.Y;

	if (T < SplashEnd)
	{
		// --- Company splash ---
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, W, H);
		const float FadeIn = FMath::Clamp(T / 0.7f, 0.f, 1.f);
		const float FadeOut = FMath::Clamp((SplashEnd - T) / 0.6f, 0.f, 1.f);
		const float Alpha = FMath::Min(FadeIn, FadeOut);
		const float Grow = 2.8f + 0.3f * FMath::Clamp(T / SplashEnd, 0.f, 1.f);

		const float LogoH = LineHeight(FontBody, Grow);
		float Y = H * 0.5f - LogoH;
		CenterText(TEXT("EG GAMES"), Y, FontBody, Grow, FLinearColor(1.f, 0.55f, 0.15f, Alpha));
		Y += LogoH + LineHeight(FontBody, 1.4f) * 0.35f;
		CenterText(CigText::Get(TEXT("title.presents.spaced")), Y, FontBody, 1.4f, FLinearColor(0.85f, 0.8f, 0.75f, Alpha * 0.9f));

		const float HintA = 0.35f + 0.25f * FMath::Sin(T * 4.f);
		CenterText(CigText::Get(TEXT("title.skip")), H - LineHeight(FontBody, BodyScale) * 2.2f, FontBody, BodyScale, FLinearColor(0.5f, 0.5f, 0.5f, HintA));
		return;
	}

	// --- Title screen and main menu ---
	const float TitleT = T - SplashEnd;
	const float A = FMath::Clamp(TitleT / 0.6f, 0.f, 1.f);

	// Dim the background slightly (the low poly shop stays visible behind it)
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * A), 0.f, 0.f, W, H);

	const float Pulse = 1.f + 0.025f * FMath::Sin(TitleT * 2.f);
	float Y = H * 0.16f;
	CenterText(CigText::Get(TEXT("title.presents")), Y, FontBody, BodyScale, FLinearColor(1.f, 0.55f, 0.15f, A));
	Y += LineHeight(FontBody, BodyScale) * 1.6f;
	CenterText(CigText::Get(TEXT("title.name")), Y, FontBody, TitleScale * Pulse, FLinearColor(1.f, 0.85f, 0.3f, A));
	Y += LineHeight(FontBody, TitleScale) * 1.4f;
	CenterText(CigText::Get(TEXT("title.tagline")), Y, FontBody, BodyScale, FLinearColor(0.82f, 0.82f, 0.82f, A));
	Y += LineHeight(FontBody, BodyScale) * 2.4f;

	// Menu
	const bool bHasSave = GM->HasExistingSave();
	const FString Items[ACigkofteGameMode::TitleItemCount] = {
		CigText::Get(bHasSave ? TEXT("menu.continue") : TEXT("menu.start")),
		CigText::Get(TEXT("menu.newgame")),
		CigText::Get(TEXT("menu.settings")),
		CigText::Get(TEXT("menu.quit"))
	};
	const float ItemH = LineHeight(FontBody, HeadScale) * 1.7f;
	for (int32 i = 0; i < ACigkofteGameMode::TitleItemCount; ++i)
	{
		const bool bSel = GM->TitleCursor == i;
		const FString Label = bSel ? FString::Printf(TEXT(">  %s  <"), *Items[i]) : Items[i];
		if (bSel)
		{
			const float BW = SX(360.f);
			DrawRect(FLinearColor(0.9f, 0.55f, 0.1f, 0.3f * A), (W - BW) * 0.5f, Y - SX(6.f), BW, ItemH - SX(6.f));
		}
		CenterText(Label, Y, FontBody, HeadScale * (bSel ? 1.08f : 1.f), bSel ? FLinearColor(1.f, 0.85f, 0.3f, A) : FLinearColor(0.9f, 0.9f, 0.9f, A));
		Y += ItemH;
	}

	Y += LineHeight(FontBody, BodyScale) * 0.8f;
	CenterText(CigText::Get(TEXT("menu.hint")), Y, FontBody, BodyScale, FLinearColor(0.6f, 0.6f, 0.6f, A));
}

void ACigkofteHUD::DrawOverlays(ACigkofteGameMode* GM)
{
	UCigDaySystem* Days = GM->Days.Get();
	UCigEconomySystem* Eco = GM->Economy.Get();
	if (!Days)
	{
		return;
	}
	const float W = ScreenSize.X;
	const float H = ScreenSize.Y;

	// Pause menu
	if (UGameplayStatics::IsGamePaused(this) && GM->bPauseMenuOpen && !GM->bSettingsOpen)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, W, H);

		const float ItemH = LineHeight(FontBody, HeadScale) * 1.8f;
		const float MW = SX(480.f);
		const float HeaderH = LineHeight(FontBody, HeadScale) * 2.2f;
		const float MH = HeaderH + ItemH * 4 + LineHeight(FontBody, BodyScale) * 2.f + SX(40.f);
		const float MX = (W - MW) * 0.5f;
		const float MY = (H - MH) * 0.5f;
		DrawRect(FLinearColor(0.02f, 0.02f, 0.04f, 0.96f), MX, MY, MW, MH);
		DrawRect(FLinearColor(0.75f, 0.12f, 0.08f, 0.95f), MX, MY, MW, HeaderH);

		CenterText(TEXT("EG GAMES"), MY + SX(6.f), FontBody, BodyScale, FLinearColor(1.f, 0.85f, 0.7f));
		CenterText(CigText::Get(TEXT("title.name")), MY + SX(6.f) + LineHeight(FontBody, BodyScale) * 1.1f, FontBody, HeadScale, GWhite);

		const FString Items[] = { CigText::Get(TEXT("menu.continue")), CigText::Get(TEXT("menu.settings")), CigText::Get(TEXT("menu.save")), CigText::Get(TEXT("menu.quitgame")) };
		float Y = MY + HeaderH + SX(20.f);
		for (int32 i = 0; i < UE_ARRAY_COUNT(Items); ++i)
		{
			const bool bSel = GM->PauseMenuCursor == i;
			if (bSel)
			{
				DrawRect(FLinearColor(0.9f, 0.55f, 0.1f, 0.4f), MX + SX(36.f), Y - SX(5.f), MW - SX(72.f), ItemH - SX(10.f));
			}
			CenterText(FString::Printf(TEXT("%s%s%s"), bSel ? TEXT("> ") : TEXT(""), *Items[i], bSel ? TEXT(" <") : TEXT("")),
				Y, FontBody, HeadScale, bSel ? GGold : GWhite);
			Y += ItemH;
		}
		CenterText(CigText::Get(TEXT("pause.hint")), MY + MH - LineHeight(FontBody, BodyScale) * 1.6f, FontBody, BodyScale, GDim);
	}

	if (Days->Phase == ECigPhase::Intro)
	{
		DrawTitleScreen(GM);
	}
	else if (Days->Phase == ECigPhase::Summary)
	{
		const float BodyLH = LineHeight(FontBody, BodyScale);
		const float PH = LineHeight(FontBody, TitleScale) + BodyLH * 8.f + SX(60.f);
		const float PW = SX(680.f);
		const float PX = (W - PW) * 0.5f;
		const float PY = (H - PH) * 0.5f;
		DrawRect(FLinearColor(0.02f, 0.01f, 0.005f, 0.88f), PX, PY, PW, PH);

		float Y = PY + SX(28.f);
		CenterText(CigText::Format(TEXT("summary.title"), Days->Day), Y, FontBody, TitleScale * 0.85f, GGold);
		Y += LineHeight(FontBody, TitleScale * 0.85f) * 1.35f;
		CenterText(CigText::Format(TEXT("summary.earnings"), Days->DayEarnings), Y, FontBody, HeadScale, GGood);
		Y += LineHeight(FontBody, HeadScale) * 1.2f;
		CenterText(CigText::Format(TEXT("summary.customers"), Days->DayServed, Days->DayMissed), Y, FontBody, BodyScale, GWhite);
		Y += BodyLH * 1.25f;
		CenterText(CigText::Format(TEXT("summary.rent"), Days->LastRent), Y, FontBody, HeadScale, GBad);
		Y += LineHeight(FontBody, HeadScale) * 1.2f;
		if (Eco)
		{
			CenterText(CigText::Format(TEXT("summary.cash"), Eco->Money), Y, FontBody, HeadScale, GWhite);
			Y += LineHeight(FontBody, HeadScale) * 1.25f;
		}
		if (GM->Reviews)
		{
			CenterText(CigText::Format(TEXT("summary.rating"), *Stars(GM->Reviews->ShopScore())), Y, FontBody, BodyScale, GGold);
			Y += BodyLH * 1.6f;
		}
		CenterText(CigText::Get(TEXT("summary.nextday")), Y, FontBody, BodyScale, GDim);
	}
	else if (Days->Phase == ECigPhase::GameOver)
	{
		DrawRect(FLinearColor(0.1f, 0.f, 0.f, 0.88f), 0.f, 0.f, W, H);
		float Y = H * 0.5f - LineHeight(FontBody, TitleScale) * 1.5f;
		CenterText(CigText::Get(TEXT("gameover.title")), Y, FontBody, TitleScale * 1.2f, FLinearColor::Red);
		Y += LineHeight(FontBody, TitleScale * 1.2f) * 1.4f;
		CenterText(CigText::Format(TEXT("gameover.reason"), Days->LastRent, Days->Day), Y, FontBody, HeadScale, GWhite);
		Y += LineHeight(FontBody, HeadScale) * 1.6f;
		CenterText(CigText::Get(TEXT("gameover.restart")), Y, FontBody, HeadScale, GGold);
	}
}
