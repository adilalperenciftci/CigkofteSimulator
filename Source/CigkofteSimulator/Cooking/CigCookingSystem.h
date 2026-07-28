#pragma once

#include "CoreMinimal.h"
#include "Game/CigSystem.h"
#include "Core/CigkofteTypes.h"
#include "CigCookingSystem.generated.h"

// A playable recipe definition. Ratios are normalised against bulgur.
struct FCigRecipe
{
	const TCHAR* Name;
	const TCHAR* Desc;
	float RatioSu;
	float RatioSalca;
	float RatioBaharat;
	float IsotMinFrac;      // ideal isot band as a fraction of the total
	float IsotMaxFrac;
	int32 MinKnead;         // ideal range of kneading strokes
	int32 MaxKnead;
	float QualityPotential; // quality ceiling
	float FreshDuration;    // freshness lifetime in seconds
	float PriceMult;        // sale multiplier
	int32 UnlockLevel;      // 99 = special lock (secret recipe)
};

constexpr int32 CigRecipeCount = 8;
constexpr int32 CigSecretRecipeIndex = 7;

// Servings a finished batch yields. One number, because the ball on the counter
// has to shrink at the same rate the batch is actually used up - the visual read
// it as a literal 6 while FinishDough set it, and the two could drift apart
// without anything failing.
constexpr int32 CigDoughServings = 6;

// The isot fraction the visual treats as "as hot as it gets". Taken from the
// hottest recipe's upper band (Çok Acılı, 0.35): a bowl is never a third isot,
// so normalising against 1 would leave every batch looking equally mild.
constexpr float CigIsotVisualMax = 0.35f;

// One prepared batch of dough.
struct FCigDough
{
	int32 Servings = 0;
	float Quality = 0.f;
	ECigSpice Spice = ECigSpice::Orta;
	int32 Recipe = 0;
	float Freshness = 100.f;

	bool IsValid() const { return Servings > 0; }
};

// The bowl, kneading, recipes, freshness and the fridge.
UCLASS()
class UCigCookingSystem : public UCigSystem
{
	GENERATED_BODY()

public:
	virtual void UpdateSystem(float DeltaSeconds) override;

	static const FCigRecipe& Recipe(int32 Index);

	// --- Human recipe wording (the UI never shows raw ratios) ---
	// e.g. "5 measures bulgur - 3 water - 2 paste - 1 spice"
	static FString HumanRecipeMix(const FCigRecipe& R);
	// e.g. "Medium" (Mild/Medium/Strong)
	static FString HumanIsotLevel(const FCigRecipe& R);
	// Target counts for the bowl (based on 5 measures of bulgur), for the UI's "3/5".
	static void TargetCounts(const FCigRecipe& R, int32 OutCounts[(int32)ECigIngredient::COUNT]);

	// --- Bowl & kneading ---
	void AddIngredient(ECigIngredient I);
	void KneadPress();
	void DumpAll();
	int32 BowlTotal() const;
	ECigSpice SpiceFromBowl() const;
	float QualityFromBowl() const;

	// --- Using the dough ---
	// Consumes N units and returns the effective quality including freshness (-1 if none).
	float UseServings(int32 N);
	float EffectiveQuality() const;

	// What is on the counter, as the station needs to draw it. Public and pure
	// so the derivation can be checked without a world (Tests/CigDoughVisualTests.cpp).
	struct FCigDoughVisual CurrentVisual() const;

	// --- Fridge ---
	void FridgeInteract();

	// --- Recipes ---
	void CycleRecipe();
	bool IsRecipeUnlocked(int32 Index) const;
	void UnlockSecretRecipe();

	int32 Bowl[(int32)ECigIngredient::COUNT] = { 0, 0, 0, 0, 0 };
	float KneadProgress = 0.f;
	int32 KneadCount = 0;
	int32 CurrentRecipe = 0;
	bool bSecretRecipeUnlocked = false;

	FCigDough Dough;        // the active dough on the counter
	FCigDough FridgeDough;  // the spare in the fridge

	static constexpr int32 BowlCapacity = 18;

private:
	double LastKneadTime = -100.0;
	// Freshness falls every frame, so the dough has to be redrawn on a clock and
	// not only when the player touches it. Four times a second is under the eye's
	// threshold for a slow fade and keeps FindStation off the per-frame path.
	float VisualRefreshTimer = 0.f;
	static constexpr float VisualRefreshInterval = 0.25f;

	void FinishDough();
	void UpdateDoughVisual();
	float FreshnessDecayMult(bool bInFridge) const;
};
