#include "Cooking/CigCookingSystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "Game/CigDaySystem.h"
#include "World/CigWorldBuilder.h"
#include "World/CigkofteStation.h"
#include "Inventory/CigInventorySystem.h"
#include "Economy/CigEconomySystem.h"
#include "Progression/CigProgressionSystem.h"
#include "Progression/CigSkillSystem.h"
#include "Hygiene/CigHygieneSystem.h"
#include "Player/CigkoftePlayerCharacter.h"
#include "Audio/CigAudioSubsystem.h"
#include "Events/CigEventSystem.h"
#include "Core/CigLog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// Names and descriptions stay in code (localization is a separate job), but
	// the balance numbers are overridden from Config/CigRecipes.json when it
	// exists, so ratios, prices and unlocks can be tuned without a rebuild.
	// JSON shape: [{ "index": 0, "priceMult": 1.2, "unlockLevel": 1, ... }, ...]
	void ApplyRecipeOverridesFromJson(TArray<FCigRecipe>& Table)
	{
		const FString Path = FPaths::ProjectDir() / TEXT("Config/CigRecipes.json");
		if (!FPaths::FileExists(Path))
		{
			return; // without the file the C++ defaults are used
		}

		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *Path))
		{
			UE_LOG(LogCig, Warning, TEXT("CigRecipes.json okunamadı; varsayılan tarifler kullanılıyor."));
			return;
		}

		TArray<TSharedPtr<FJsonValue>> Arr;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Arr))
		{
			UE_LOG(LogCig, Warning, TEXT("CigRecipes.json ayrıştırılamadı; varsayılan tarifler kullanılıyor."));
			return;
		}

		auto Flt = [](const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float& Out)
		{
			double D;
			if (O->TryGetNumberField(Key, D)) { Out = (float)D; }
		};
		auto Int = [](const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32& Out)
		{
			double D;
			if (O->TryGetNumberField(Key, D)) { Out = FMath::RoundToInt(D); }
		};

		int32 Applied = 0;
		for (const TSharedPtr<FJsonValue>& V : Arr)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!V.IsValid() || !V->TryGetObject(ObjPtr) || !ObjPtr) { continue; }
			const TSharedPtr<FJsonObject>& O = *ObjPtr;

			double IdxD;
			if (!O->TryGetNumberField(TEXT("index"), IdxD)) { continue; }
			const int32 Idx = FMath::RoundToInt(IdxD);
			if (Idx < 0 || Idx >= Table.Num()) { continue; }

			FCigRecipe& R = Table[Idx];
			Flt(O, TEXT("ratioSu"),          R.RatioSu);
			Flt(O, TEXT("ratioSalca"),       R.RatioSalca);
			Flt(O, TEXT("ratioBaharat"),     R.RatioBaharat);
			Flt(O, TEXT("isotMinFrac"),      R.IsotMinFrac);
			Flt(O, TEXT("isotMaxFrac"),      R.IsotMaxFrac);
			Int(O, TEXT("minKnead"),         R.MinKnead);
			Int(O, TEXT("maxKnead"),         R.MaxKnead);
			Flt(O, TEXT("qualityPotential"), R.QualityPotential);
			Flt(O, TEXT("freshDuration"),    R.FreshDuration);
			Flt(O, TEXT("priceMult"),        R.PriceMult);
			Int(O, TEXT("unlockLevel"),      R.UnlockLevel);
			++Applied;
		}

		UE_LOG(LogCig, Log, TEXT("CigRecipes.json uygulandı: %d tarif geçersiz kılındı."), Applied);
	}

	// The runtime recipe table: built once from the C++ defaults, then updated
	// with the JSON balance values when present. The name pointers refer to
	// static literals, so copying is safe.
	const TArray<FCigRecipe>& RecipeTable()
	{
		static const TArray<FCigRecipe> Table = []()
		{
			static const FCigRecipe Defaults[CigRecipeCount] = {
				{ TEXT("Klasik"),      TEXT("Dengeli mahalle usulü"),                0.60f, 0.40f, 0.20f, 0.08f, 0.20f, 12, 24, 90.f,  120.f, 1.00f, 1 },
				{ TEXT("Ekonomik"),    TEXT("Bol bulgur, az masraf"),                0.70f, 0.30f, 0.10f, 0.05f, 0.15f,  9, 20, 72.f,  100.f, 0.85f, 1 },
				{ TEXT("Adıyaman"),    TEXT("Bol salçalı, koyu kıvam"),              0.55f, 0.50f, 0.22f, 0.10f, 0.22f, 14, 26, 95.f,  110.f, 1.10f, 2 },
				{ TEXT("Çok Acılı"),   TEXT("İsot fırtınası, cesurlara"),            0.60f, 0.40f, 0.25f, 0.20f, 0.35f, 12, 24, 92.f,  110.f, 1.15f, 2 },
				{ TEXT("Ev Yapımı"),   TEXT("Anne eli değmiş gibi, uzun dayanır"),   0.62f, 0.38f, 0.18f, 0.08f, 0.18f, 13, 26, 88.f,  160.f, 1.05f, 3 },
				{ TEXT("Premium"),     TEXT("Seçme malzeme, yüksek fiyat"),          0.58f, 0.42f, 0.22f, 0.10f, 0.20f, 16, 28, 100.f, 120.f, 1.30f, 4 },
				{ TEXT("Usta İşi"),    TEXT("Dar tolerans, büyük ödül"),             0.57f, 0.43f, 0.21f, 0.11f, 0.18f, 18, 30, 100.f, 120.f, 1.45f, 5 },
				{ TEXT("Gizli Tarif"), TEXT("Eski ustanın mirası"),                  0.59f, 0.41f, 0.23f, 0.10f, 0.21f, 16, 30, 110.f, 140.f, 1.60f, 99 }
			};
			TArray<FCigRecipe> T(Defaults, CigRecipeCount);
			ApplyRecipeOverridesFromJson(T);
			return T;
		}();
		return Table;
	}
}

const FCigRecipe& UCigCookingSystem::Recipe(int32 Index)
{
	const TArray<FCigRecipe>& Table = RecipeTable();
	return Table[FMath::Clamp(Index, 0, Table.Num() - 1)];
}

void UCigCookingSystem::TargetCounts(const FCigRecipe& R, int32 OutCounts[(int32)ECigIngredient::COUNT])
{
	constexpr int32 BulgurBase = 5;
	OutCounts[(int32)ECigIngredient::Bulgur] = BulgurBase;
	OutCounts[(int32)ECigIngredient::Su] = FMath::Max(1, FMath::RoundToInt(BulgurBase * R.RatioSu));
	OutCounts[(int32)ECigIngredient::Salca] = FMath::Max(1, FMath::RoundToInt(BulgurBase * R.RatioSalca));
	OutCounts[(int32)ECigIngredient::Baharat] = FMath::Max(1, FMath::RoundToInt(BulgurBase * R.RatioBaharat));

	// Isot: the count that lands in the middle of the recipe's range
	const float MidFrac = (R.IsotMinFrac + R.IsotMaxFrac) * 0.5f;
	const int32 Others = OutCounts[0] + OutCounts[1] + OutCounts[2] + OutCounts[3];
	OutCounts[(int32)ECigIngredient::Isot] = FMath::Clamp(FMath::RoundToInt(MidFrac * Others / (1.f - MidFrac)), 1, 6);
}

FString UCigCookingSystem::HumanRecipeMix(const FCigRecipe& R)
{
	int32 Counts[(int32)ECigIngredient::COUNT];
	TargetCounts(R, Counts);
	return CigText::Format(TEXT("cooking.mix"),
		Counts[(int32)ECigIngredient::Bulgur], Counts[(int32)ECigIngredient::Su],
		Counts[(int32)ECigIngredient::Salca], Counts[(int32)ECigIngredient::Baharat],
		Counts[(int32)ECigIngredient::Isot]);
}

FString UCigCookingSystem::HumanIsotLevel(const FCigRecipe& R)
{
	const float Mid = (R.IsotMinFrac + R.IsotMaxFrac) * 0.5f;
	if (Mid < 0.10f) return CigText::Get(TEXT("cooking.isot.light"));
	if (Mid <= 0.20f) return CigText::Get(TEXT("cooking.isot.medium"));
	return CigText::Get(TEXT("cooking.isot.strong"));
}

bool UCigCookingSystem::IsRecipeUnlocked(int32 Index) const
{
	const FCigRecipe& R = Recipe(Index);
	if (R.UnlockLevel >= 99)
	{
		return bSecretRecipeUnlocked;
	}
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	return Prog ? Prog->Level >= R.UnlockLevel : Index == 0;
}

void UCigCookingSystem::UnlockSecretRecipe()
{
	if (!bSecretRecipeUnlocked)
	{
		bSecretRecipeUnlocked = true;
		if (GM)
		{
			GM->AddMessage(CigText::Get(TEXT("msg.cooking.secretunlocked")), FLinearColor(1.f, 0.8f, 1.f));
		}
	}
}

void UCigCookingSystem::CycleRecipe()
{
	for (int32 Step = 1; Step <= CigRecipeCount; ++Step)
	{
		const int32 Next = (CurrentRecipe + Step) % CigRecipeCount;
		if (IsRecipeUnlocked(Next))
		{
			CurrentRecipe = Next;
			const FCigRecipe& R = Recipe(CurrentRecipe);
			if (GM)
			{
				GM->AddMessage(CigText::Format(TEXT("msg.cooking.recipecycled"), R.Name, R.Desc, R.PriceMult), FLinearColor(0.8f, 0.7f, 1.f));
			}
			return;
		}
	}
}

int32 UCigCookingSystem::BowlTotal() const
{
	int32 Total = 0;
	for (int32 V : Bowl)
	{
		Total += V;
	}
	return Total;
}

ECigSpice UCigCookingSystem::SpiceFromBowl() const
{
	const int32 Total = BowlTotal();
	if (Total <= 0)
	{
		return ECigSpice::AzAci;
	}
	const float F = (float)Bowl[(int32)ECigIngredient::Isot] / (float)Total;
	if (F < 0.10f)
	{
		return ECigSpice::AzAci;
	}
	if (F <= 0.18f)
	{
		return ECigSpice::Orta;
	}
	return ECigSpice::CokAci;
}

float UCigCookingSystem::QualityFromBowl() const
{
	const FCigRecipe& R = Recipe(CurrentRecipe);
	const float B = (float)Bowl[(int32)ECigIngredient::Bulgur];
	if (B <= 0.f)
	{
		return 15.f;
	}
	const float Su = (float)Bowl[(int32)ECigIngredient::Su];
	const float Salca = (float)Bowl[(int32)ECigIngredient::Salca];
	const float Baharat = (float)Bowl[(int32)ECigIngredient::Baharat];

	float Err = 0.f;
	Err += FMath::Abs(Su / B - R.RatioSu) / R.RatioSu;
	Err += FMath::Abs(Salca / B - R.RatioSalca) / R.RatioSalca;
	Err += FMath::Abs(Baharat / B - R.RatioBaharat) / FMath::Max(R.RatioBaharat, 0.05f);

	// Extra error when the isot ratio falls outside the recipe's range
	const float IsotFrac = (float)Bowl[(int32)ECigIngredient::Isot] / FMath::Max(1, BowlTotal());
	if (IsotFrac < R.IsotMinFrac)
	{
		Err += (R.IsotMinFrac - IsotFrac) * 3.f;
	}
	else if (IsotFrac > R.IsotMaxFrac)
	{
		Err += (IsotFrac - R.IsotMaxFrac) * 3.f;
	}

	float Q = FMath::Clamp(R.QualityPotential - 40.f * Err, 15.f, R.QualityPotential);

	// Contribution from ingredient quality (depends on the supplier)
	if (GM && GM->Inventory)
	{
		Q *= GM->Inventory->AverageIngredientQuality();
	}
	return FMath::Clamp(Q, 10.f, 110.f);
}

void UCigCookingSystem::AddIngredient(ECigIngredient I)
{
	UCigInventorySystem* Inv = GM ? GM->Inventory.Get() : nullptr;
	if (!Inv)
	{
		return;
	}
	if (Dough.IsValid())
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.finishdoughfirst")), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	if (!Inv->HasStock((int32)I))
	{
		GM->AddMessage(CigText::Format(TEXT("msg.cooking.ingredientout"), *CigIngredientName(I)), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}
	if (BowlTotal() >= BowlCapacity)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.bowlfull")), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	if (KneadProgress > 0.f)
	{
		KneadProgress = FMath::Max(0.f, KneadProgress * 0.85f);
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.addduringknead")), FLinearColor(1.f, 0.8f, 0.3f));
	}

	Inv->Consume((int32)I, 1);
	Bowl[(int32)I]++;
	Bus().IngredientAdded.Broadcast();
	UpdateDoughVisual();
}

void UCigCookingSystem::KneadPress()
{
	if (Dough.IsValid())
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.doughready")), FLinearColor(0.5f, 1.f, 0.5f));
		return;
	}
	if (BowlTotal() < 6 || Bowl[(int32)ECigIngredient::Bulgur] <= 0)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.notenough")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}
	if (Bowl[(int32)ECigIngredient::Su] <= 0)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.nowater")), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const double Dt = Now - LastKneadTime;
	LastKneadTime = Now;

	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;

	float Gain = 4.f;
	if (Dt >= 0.25 && Dt <= 0.85)
	{
		Gain = 7.f + 2.f * (Eco ? Eco->GloveLevel : 0);
	}
	else if (Dt < 0.15)
	{
		Gain = 2.f; // panic kneading achieves nothing
	}

	if (Eco && Eco->HasUpgrade(ECigUpgrade::IkinciYogurma))
	{
		Gain *= 1.4f;
	}
	if (GM && GM->Skills)
	{
		Gain *= GM->Skills->KneadMult(); // the HizliEl skill
	}

	// A tired player kneads more weakly
	if (ACigkoftePlayerCharacter* Player = Cast<ACigkoftePlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (Player->Energy < 30.f)
		{
			Gain *= 0.7f;
		}
		Player->Energy = FMath::Max(0.f, Player->Energy - 0.8f);
	}

	KneadProgress += Gain;
	KneadCount++;

	if (GM)
	{
		if (UCigAudioSubsystem* Audio = GM->GetGameInstance() ? GM->GetGameInstance()->GetSubsystem<UCigAudioSubsystem>() : nullptr)
		{
			// Pitch tracks how far the batch has come - the dough is done at 100 -
			// so the ear can hear it coming together without watching the bowl.
			Audio->PlayKnead(KneadProgress / 100.f);
		}
		if (ACigkofteStation* Yogurma = GM->WorldBuilder ? GM->WorldBuilder->FindStation(ECigStation::Yogurma) : nullptr)
		{
			Yogurma->PulseDough();
		}
	}

	if (KneadProgress >= 100.f)
	{
		FinishDough();
	}
	UpdateDoughVisual();
}

void UCigCookingSystem::FinishDough()
{
	const FCigRecipe& R = Recipe(CurrentRecipe);
	UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	UCigHygieneSystem* Hyg = GM ? GM->Hygiene.Get() : nullptr;

	float Q = QualityFromBowl() + 4.f * (Eco ? Eco->IsotLevel : 0);
	if (GM && GM->Skills)
	{
		Q *= GM->Skills->DoughQualityMult(); // the MutfakUstasi skill
	}

	// The knead count is judged against the recipe's range
	if (KneadCount > R.MaxKnead)
	{
		const float Over = (float)(KneadCount - R.MaxKnead);
		Q -= Over * 2.f;
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.overknead")), FLinearColor(1.f, 0.8f, 0.3f));
	}
	else if (KneadCount >= R.MinKnead && KneadCount <= R.MaxKnead)
	{
		Q += 4.f; // bonus for hitting the ideal range
	}

	// Dough kneaded in a dirty shop loses quality
	if (Hyg && Hyg->OverallHygiene() < 50.f)
	{
		Q *= 0.85f;
	}

	Dough.Quality = FMath::Clamp(Q, 10.f, 110.f);
	Dough.Spice = SpiceFromBowl();
	Dough.Servings = 6;
	Dough.Recipe = CurrentRecipe;
	Dough.Freshness = 100.f;
	KneadProgress = 100.f;
	KneadCount = 0;

	for (int32& V : Bowl)
	{
		V = 0;
	}

	if (GM)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.cooking.finished"),
			*CigQualityName(Dough.Quality), *CigSpiceName(Dough.Spice), Recipe(Dough.Recipe).Name), FLinearColor(0.4f, 1.f, 0.4f));
		GM->PlaySound(ECigSound::Success);
	Bus().DoughPrepared.Broadcast(Dough.Quality);
		if (GM->WorldBuilder)
		{
			if (ACigkofteStation* Yogurma = GM->WorldBuilder->FindStation(ECigStation::Yogurma))
			{
				GM->WorldBuilder->SpawnFloatText(Yogurma->GetActorLocation() + FVector(0.f, 0.f, 220.f), CigText::Get(TEXT("float.doughready")), FColor(120, 255, 120));
			}
		}
	}
}

void UCigCookingSystem::DumpAll()
{
	if (BowlTotal() == 0 && !Dough.IsValid())
	{
		return;
	}
	for (int32& V : Bowl)
	{
		V = 0;
	}
	KneadProgress = 0.f;
	KneadCount = 0;
	Dough = FCigDough();
	UpdateDoughVisual();
	if (GM)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.dumped")), FLinearColor(0.8f, 0.8f, 0.8f));
	}
}

float UCigCookingSystem::EffectiveQuality() const
{
	if (!Dough.IsValid())
	{
		return -1.f;
	}
	// Stale dough drags the quality down
	const float FreshMult = FMath::Lerp(0.45f, 1.f, FMath::Clamp(Dough.Freshness / 100.f, 0.f, 1.f));
	return Dough.Quality * FreshMult;
}

float UCigCookingSystem::UseServings(int32 N)
{
	if (Dough.Servings < N)
	{
		return -1.f;
	}
	const float Q = EffectiveQuality();
	Dough.Servings -= N;
	if (Dough.Servings <= 0)
	{
		const int32 UsedRecipe = Dough.Recipe;
		Dough = FCigDough();
		Dough.Recipe = UsedRecipe;
		KneadProgress = 0.f;
	}
	UpdateDoughVisual();
	return Q;
}

void UCigCookingSystem::FridgeInteract()
{
	// Put the counter's dough in the fridge, or take out / swap the one inside.
	if (Dough.IsValid() && !FridgeDough.IsValid())
	{
		FridgeDough = Dough;
		Dough = FCigDough();
		KneadProgress = 0.f;
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.fridge.in")), FLinearColor(0.6f, 0.85f, 1.f));
	}
	else if (!Dough.IsValid() && FridgeDough.IsValid())
	{
		Dough = FridgeDough;
		FridgeDough = FCigDough();
		KneadProgress = 100.f;
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.fridge.out")), FLinearColor(0.6f, 0.85f, 1.f));
	}
	else if (Dough.IsValid() && FridgeDough.IsValid())
	{
		Swap(Dough, FridgeDough);
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.fridge.swap")), FLinearColor(0.6f, 0.85f, 1.f));
	}
	else
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.fridge.empty")), FLinearColor(0.8f, 0.8f, 0.8f));
	}
	UpdateDoughVisual();
}

float UCigCookingSystem::FreshnessDecayMult(bool bInFridge) const
{
	if (bInFridge && GM && GM->Events && GM->Events->IsFridgeBroken())
	{
		return 1.2f; // no power, the fridge is warming up
	}
	float Mult = bInFridge ? 0.35f : 1.f;
	const UCigEconomySystem* Eco = GM ? GM->Economy.Get() : nullptr;
	if (bInFridge && Eco && Eco->HasUpgrade(ECigUpgrade::BuyukBuzdolabi))
	{
		Mult = 0.12f;
	}
	return Mult;
}

void UCigCookingSystem::UpdateSystem(float DeltaSeconds)
{
	const UCigDaySystem* Days = GM ? GM->Days.Get() : nullptr;
	if (!Days || !Days->IsPlaying())
	{
		return;
	}

	auto Decay = [this, DeltaSeconds](FCigDough& D, bool bInFridge)
	{
		if (!D.IsValid())
		{
			return;
		}
		const float Duration = Recipe(D.Recipe).FreshDuration;
		D.Freshness -= DeltaSeconds * (100.f / FMath::Max(Duration, 10.f)) * FreshnessDecayMult(bInFridge);
		if (D.Freshness <= 0.f)
		{
			D.Freshness = 0.f;
		}
	};
	Decay(Dough, false);
	Decay(FridgeDough, true);

	// Dough that goes fully stale spoils
	if (Dough.IsValid() && Dough.Freshness <= 0.f)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.spoiled.counter")), FLinearColor(1.f, 0.4f, 0.3f));
		Dough = FCigDough();
		KneadProgress = 0.f;
		UpdateDoughVisual();
	}
	if (FridgeDough.IsValid() && FridgeDough.Freshness <= 0.f)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.cooking.spoiled.fridge")), FLinearColor(1.f, 0.4f, 0.3f));
		FridgeDough = FCigDough();
	}

	// The decay above is silent to the world until the station is told. Without
	// this the dough held its colour until the next keypress and then vanished at
	// zero, instead of visibly going off.
	VisualRefreshTimer += DeltaSeconds;
	if (VisualRefreshTimer >= VisualRefreshInterval)
	{
		VisualRefreshTimer = 0.f;
		UpdateDoughVisual();
	}
}

FCigDoughVisual UCigCookingSystem::CurrentVisual() const
{
	// A finished batch is measured against the recipe it was made from, not the
	// one currently selected at the board. Otherwise flipping the selection after
	// kneading changed the colour of dough nobody had touched.
	const FCigRecipe& R = Recipe(Dough.IsValid() ? Dough.Recipe : CurrentRecipe);
	FCigDoughVisual V;

	if (Dough.IsValid())
	{
		// A finished batch: the ball shrinks as it is used and dulls as it sits.
		V.Fill01 = (float)Dough.Servings / (float)CigDoughServings;
		V.Knead01 = 1.f;
		V.Quality01 = Dough.Quality / FMath::Max(R.QualityPotential, 1.f);
		V.Freshness01 = Dough.Freshness / 100.f;
		// The batch carries a spice band rather than a ratio, so the three
		// levels map onto the same scale the bowl is measured against.
		switch (Dough.Spice)
		{
		case ECigSpice::AzAci:  V.Spice01 = 0.15f; break;
		case ECigSpice::Orta:   V.Spice01 = 0.5f;  break;
		default:                V.Spice01 = 1.f;   break;
		}
	}
	else
	{
		// Still in the bowl: what has gone in so far, and how far it has been
		// worked. Freshness does not apply to a batch that does not exist yet.
		const int32 Toplam = BowlTotal();
		V.Fill01 = (float)Toplam / (float)BowlCapacity;
		V.Knead01 = KneadProgress / 100.f;
		V.Quality01 = QualityFromBowl() / FMath::Max(R.QualityPotential, 1.f);
		V.Freshness01 = 1.f;
		V.Spice01 = Toplam > 0
			? ((float)Bowl[(int32)ECigIngredient::Isot] / (float)Toplam) / CigIsotVisualMax
			: 0.f;
	}

	V.Fill01 = FMath::Clamp(V.Fill01, 0.f, 1.f);
	V.Knead01 = FMath::Clamp(V.Knead01, 0.f, 1.f);
	V.Spice01 = FMath::Clamp(V.Spice01, 0.f, 1.f);
	V.Quality01 = FMath::Clamp(V.Quality01, 0.f, 1.f);
	V.Freshness01 = FMath::Clamp(V.Freshness01, 0.f, 1.f);
	return V;
}

void UCigCookingSystem::UpdateDoughVisual()
{
	ACigkofteStation* Yogurma = (GM && GM->WorldBuilder) ? GM->WorldBuilder->FindStation(ECigStation::Yogurma) : nullptr;
	if (!Yogurma)
	{
		return;
	}
	Yogurma->UpdateDough(CurrentVisual());
}
