#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/CigSavePlacement.h"
#include "CigSaveGame.generated.h"

// A regular customer's record in the save file.
USTRUCT()
struct FCigSaveLoyal
{
	GENERATED_BODY()

	UPROPERTY() int32 Id = 0;
	UPROPERTY() FString Name;
	UPROPERTY() int32 Seed = 0;
	UPROPERTY() uint8 FavSpice = 1;
	UPROPERTY() int32 FavPortion = 1;
	UPROPERTY() uint8 FavToppings = 0;
	UPROPERTY() bool bFavAyran = false;
	UPROPERTY() bool bFavPacked = false;
	UPROPERTY() uint8 FavSide = 0;
	UPROPERTY() int32 Visits = 0;
	UPROPERTY() float Satisfaction = 60.f;
	UPROPERTY() float Trust = 50.f;
	UPROPERTY() int32 LastVisitDay = 0;
	UPROPERTY() float AvgTip = 0.f;
	UPROPERTY() int32 Traits = 0;
	UPROPERTY() int32 RememberedMistakes = 0;
};

USTRUCT()
struct FCigSaveRival
{
	GENERATED_BODY()

	UPROPERTY() FString Name;
	UPROPERTY() float Price = 1.f;
	UPROPERTY() float Quality = 60.f;
	UPROPERTY() float Hygiene = 60.f;
	UPROPERTY() float Popularity = 50.f;
	UPROPERTY() float AdPower = 0.f;
	UPROPERTY() int32 Capacity = 30;
	UPROPERTY() int32 Attitude = 0;
	UPROPERTY() bool bOpen = true;
	UPROPERTY() int32 BadDays = 0;
};

USTRUCT()
struct FCigSaveReview
{
	GENERATED_BODY()

	UPROPERTY() FString Author;
	UPROPERTY() FString Text;
	UPROPERTY() int32 Stars = 3;
	UPROPERTY() int32 Day = 1;

	// Version 12. Zero means a pre-v12 save; migration assigns one.
	UPROPERTY() int32 Id = 0;
};

// Game settings (changed from the settings screen).
USTRUCT()
struct FCigSettings
{
	GENERATED_BODY()

	UPROPERTY() float FOV = 90.f;
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() bool bHeadBob = true;
	UPROPERTY() float MasterVolume = 1.f;
	UPROPERTY() float EffectsVolume = 1.f;
	UPROPERTY() float MusicVolume = 0.7f;
	UPROPERTY() int32 QualityLevel = 2; // 0 low - 3 epic
	UPROPERTY() int32 WindowMode = 0;   // 0 windowed, 1 fullscreen, 2 borderless
	UPROPERTY() int32 ResolutionIndex = 1;

	// --- Accessibility (version 4) ---
	UPROPERTY() float UIScaleMult = 1.f;
	UPROPERTY() bool bScreenFlash = true;
	UPROPERTY() int32 ColorBlindMode = 0;

	// --- Language (version 5) ---
	// Up to version 3 there was a Language field here that was never actually
	// applied; version 4 removed it, and version 5 brought it back once UI text
	// really became translatable. 0 Turkish, 1 English.
	UPROPERTY() int32 Language = 0;

	// --- Key bindings (version 6) ---
	// Keys are stored by name (FKey::GetFName), so a save stays readable even if
	// an engine update shuffles the numeric codes. An empty array = defaults.
	UPROPERTY() TArray<FString> KeyBindings;
};

// The versioned main save file. Missing fields keep their defaults on load.
UCLASS()
class UCigSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 SaveVersion = 1;

	// --- Day & economy ---
	UPROPERTY() int32 Day = 1;
	UPROPERTY() int32 Money = 400;
	UPROPERTY() int32 PricePolicy = 1;
	UPROPERTY() int32 GloveLevel = 0;
	UPROPERTY() int32 IsotLevel = 0;
	UPROPERTY() int32 AdCount = 0;
	UPROPERTY() bool bOwnHouse = false;
	UPROPERTY() TArray<bool> UpgradeOwned;
	UPROPERTY() int32 CurrentSupplier = 0;
	UPROPERTY() TArray<float> SupplierRelation;
	UPROPERTY() int32 IngredientTier = 1; // 0 cheap, 1 normal, 2 premium

	// --- Pricing (version 7) ---
	// The player's markup per product, not the list price: the list price is
	// balance data and may legitimately change under an existing save, whereas
	// the markup is a decision the player made and must survive.
	UPROPERTY() TArray<float> UrunCarpanlari;

	// --- Skills & prestige ---
	UPROPERTY() int32 SkillPoints = 0;
	UPROPERTY() int32 SkillSpent = 0;
	UPROPERTY() TArray<int32> SkillRanks;
	UPROPERTY() int32 PrestigeCount = 0;

	// --- Achievements ---
	UPROPERTY() TArray<bool> AchievementsUnlocked;

	// --- Progression ---
	UPROPERTY() int32 XP = 0;
	UPROPERTY() int32 Level = 1;
	UPROPERTY() float Rep = 50.f;
	UPROPERTY() int32 TotalServed = 0;
	UPROPERTY() int32 TotalDeliveries = 0;
	UPROPERTY() int32 TotalEarned = 0;
	UPROPERTY() int32 BestDayEarnings = 0;
	UPROPERTY() int32 TotalAngryCustomers = 0;
	UPROPERTY() int32 TotalPerfectOrders = 0;

	// --- Stock ---
	UPROPERTY() TArray<int32> Stock;
	UPROPERTY() TArray<float> StockQuality;
	UPROPERTY() int32 Garnish = 0;

	// --- Kitchen ---
	UPROPERTY() int32 CurrentRecipe = 0;
	UPROPERTY() bool bSecretRecipeUnlocked = false;

	// --- Hygiene ---
	UPROPERTY() float CounterDirt = 10.f;
	UPROPERTY() float TrashFill = 20.f;
	UPROPERTY() float DishPile = 0.f;

	// --- Customers ---
	UPROPERTY() TArray<FCigSaveLoyal> Loyals;
	UPROPERTY() int32 NextLoyalId = 1;

	// --- Rivals & reviews ---
	UPROPERTY() TArray<FCigSaveRival> Rivals;
	UPROPERTY() TArray<FCigSaveReview> Reviews;
	UPROPERTY() float FoodScore = 3.f;
	UPROPERTY() float ServiceScore = 3.f;
	UPROPERTY() float PriceScore = 3.f;
	UPROPERTY() float HygieneScore = 3.f;
	UPROPERTY() float AtmosphereScore = 2.5f;

	// --- Apprentice ---
	UPROPERTY() bool bApprenticeHired = false;
	UPROPERTY() FString ApprenticeName;
	UPROPERTY() int32 ApprenticeLevel = 1;
	UPROPERTY() int32 ApprenticeXP = 0;
	UPROPERTY() float ApprenticeMorale = 70.f;
	UPROPERTY() int32 ApprenticeSalary = 100;
	UPROPERTY() uint8 ApprenticeTask = 0;
	UPROPERTY() uint8 ApprenticeSpec = 0;
	UPROPERTY() int32 ApprenticeDaysSinceRaise = 0;
	UPROPERTY() bool bApprenticeWantsRaise = false;

	// Traits the apprentice was hired with (version 8). The candidate pool is
	// deliberately not saved: walk-ins are regenerated each morning anyway.
	UPROPERTY() int32 ApprenticeArketip = 0;
	UPROPERTY() float ApprenticeHiz = 1.f;
	UPROPERTY() float ApprenticeTitizlik = 1.f;
	UPROPERTY() float ApprenticeGulerYuz = 1.f;
	UPROPERTY() int32 ApprenticeOdenmemisGun = 0;
	UPROPERTY() int32 StaffTransferTeklifi = 0;

	// How many ticks of each job this apprentice has worked (version 16).
	//
	// Not bookkeeping: it is what decides the specialism earned at level 3, so
	// without it an apprentice saved before that comes back having forgotten what
	// they spent their days doing, and the reward for how the player used them is
	// then settled by whatever they happen to do next. It went unnoticed because
	// nothing reports it - the specialism simply arrives wrong.
	//
	// An array rather than five fields because the job list is an enum that has
	// grown once already, and a save that reads short is the migration below.
	UPROPERTY() TArray<int32> ApprenticeTaskCounts;

	// --- Bulk order (version 9) ---
	// An offer only matters until its delivery day, but it has to survive a
	// save in between: the three days' notice is the whole decision.
	UPROPERTY() bool bTopluTeklifVar = false;
	UPROPERTY() bool bTopluKabulEdildi = false;
	UPROPERTY() int32 TopluTeslimGunu = 0;
	UPROPERTY() int32 TopluIstenenAdet = 0;
	UPROPERTY() int32 TopluOdul = 0;
	UPROPERTY() int32 TopluBaslangicServis = 0;

	// --- Inspection (version 10) ---
	// The bribe count is the one that matters: it is what makes the next envelope
	// riskier, so losing it would quietly forgive a whole history.
	UPROPERTY() int32 RuhsatBitisGunu = 0;
	UPROPERTY() int32 BasarisizDenetim = 0;
	UPROPERTY() int32 RusvetSayisi = 0;
	UPROPERTY() int32 KalanKapaliGun = 0;

	// --- Social (version 11) ---
	// Only the follower count persists. The daily post allowance, the campaign
	// and the viral roll all belong to a single day and are rebuilt each morning.
	UPROPERTY() int32 Takipci = 0;

	// Version 12: the pending reply is held by review ID, and the counter has to
	// persist so a reloaded shop cannot reissue an ID that is still in the list.
	UPROPERTY() int32 NextReviewId = 1;
	UPROPERTY() int32 YanitlanacakYorumId = 0;

	// --- Cat ---
	UPROPERTY() FString CatName = TEXT("Pamuk");
	UPROPERTY() float CatFood = 70.f;
	UPROPERTY() float CatAttention = 60.f;

	// --- Vehicle ---
	UPROPERTY() float CarFuel = 100.f;
	UPROPERTY() float CarDamage = 0.f;

	// --- Quests & story ---
	UPROPERTY() uint8 StoryStage = 0;
	UPROPERTY() int32 StoryProgress = 0;
	UPROPERTY() bool bTutorialActive = true;
	UPROPERTY() uint8 TutorialStep = 0;

	// --- Shop layout (version 13) ---
	// The installed placements, authored inputs only: the derived consequence is
	// recomputed by policy on load, so storing it would be a second authority.
	// Transient placements are absent on purpose - a delivery crate belongs to the
	// delivery state that already persists, and writing it here would restore two.
	UPROPERTY() TArray<FCigSavePlacement> InstalledLayout;

	// Whether InstalledLayout means anything.
	//
	// The field it disambiguates is the array above, which reads back empty both
	// for a save written before layout was persisted and for a shop the player has
	// genuinely emptied. Those need opposite handling - the first falls back to the
	// authored default, the second is honoured - and nothing else in the file can
	// tell them apart once MigrateV12ToV13 has moved the version forward.
	UPROPERTY() bool bLayoutPersisted = false;

	// --- Shop identity (version 14) ---
	// Empty means the shop was never named and shows its default. No companion
	// flag is needed here, unlike the layout: an empty name is not a state a
	// player can choose - CigShopIdentity refuses it - so empty has exactly one
	// meaning.
	//
	// Not translated, and deliberately not a text key. A name given in Turkish
	// stays that name when the interface is switched to English.
	UPROPERTY() FString ShopName;

	// --- The back room (version 15) ---
	//
	// Placements the player took off the floor in build mode. Persisted because
	// the alternative is a trap: removal would be permanent the moment the game
	// saved, and this project has no shop to buy furniture back from, so a table
	// put away for a minute would be gone for good.
	//
	// No companion flag, unlike InstalledLayout. An empty back room has one
	// meaning - nothing is stored - and a save that predates this field reads back
	// empty, which is the same thing and is true of it.
	UPROPERTY() TArray<FCigSavePlacement> StoredLayout;

	// --- Randomness ---
	// The session's starting seed (the number quoted in bug reports) and the
	// stream's current position. Both are written so a load does not replay the
	// same day from the start.
	UPROPERTY() int32 RngInitialSeed = 0;
	UPROPERTY() int32 RngStateSeed = 0;

	// --- Settings ---
	UPROPERTY() FCigSettings Settings;
};
