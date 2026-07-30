#include "Economy/CigEconomySystem.h"
#include "Core/CigText.h"
#include "Game/CigkofteGameMode.h"
#include "Game/CigEventBus.h"
#include "World/CigWorldBuilder.h"
#include "Progression/CigProgressionSystem.h"
#include "Vehicles/CigCar.h"
#include "Core/CigLog.h"

const FCigSupplier& UCigEconomySystem::Supplier(int32 Index)
{
	static const FCigSupplier Suppliers[CigSupplierCount] = {
		{ TEXT("Mahalle Toptancısı"), TEXT("Dengeli fiyat, tanıdık yüz"),      1.00f, 1.00f, 25.f, 0.95f },
		{ TEXT("Ucuz Depo"),          TEXT("Ucuz ama kalitesi şüpheli"),       0.70f, 0.78f, 35.f, 0.85f },
		{ TEXT("Premium Üretici"),    TEXT("Pahalı, kalitesi tartışılmaz"),    1.60f, 1.35f, 30.f, 0.97f },
		{ TEXT("Yerel Çiftçi"),       TEXT("Taze ürün, yavaş teslimat"),       1.10f, 1.20f, 45.f, 0.90f },
		{ TEXT("Hızlı Teslimatçı"),   TEXT("Şimşek hızı, orta kalite"),        1.30f, 0.95f, 10.f, 0.92f }
	};
	return Suppliers[FMath::Clamp(Index, 0, CigSupplierCount - 1)];
}

namespace
{
	const TCHAR* GSupplierKeys[CigSupplierCount] = {
		TEXT("toptanci"), TEXT("ucuzdepo"), TEXT("premium"), TEXT("ciftci"), TEXT("hizli")
	};

	FString SupplierText(int32 Index, const TCHAR* Suffix, const TCHAR* Fallback)
	{
		const int32 i = FMath::Clamp(Index, 0, CigSupplierCount - 1);
		const FString Key = FString::Printf(TEXT("supplier.%s.%s"), GSupplierKeys[i], Suffix);
		const FString Value = CigText::Get(*Key);
		return Value == Key ? FString(Fallback) : Value;
	}
}

FString UCigEconomySystem::SupplierName(int32 Index)
{
	return SupplierText(Index, TEXT("name"), Supplier(Index).Name);
}

FString UCigEconomySystem::SupplierDesc(int32 Index)
{
	return SupplierText(Index, TEXT("desc"), Supplier(Index).Desc);
}

bool UCigEconomySystem::TrySpend(int32 Cost)
{
	if (Money < Cost)
	{
		return false;
	}
	Money -= Cost;
	return true;
}

void UCigEconomySystem::Earn(int32 Amount)
{
	Money += Amount;
	if (GM && GM->Progression)
	{
		GM->Progression->TotalEarned += FMath::Max(0, Amount);
	}
}

void UCigEconomySystem::OnDayEnd(int32 Day)
{
	if (HasUpgrade(ECigUpgrade::YeniSube))
	{
		Money += 250;
		GM->AddMessage(CigText::Get(TEXT("msg.economy.branchincome")), FLinearColor(0.4f, 1.f, 0.4f));
	}
}

// ---------------------------------------------------------------- pricing policy

float UCigEconomySystem::PolicyPriceMult() const
{
	static const float Mults[3] = { 0.8f, 1.f, 1.25f };
	return Mults[FMath::Clamp(PricePolicy, 0, 2)];
}

FString UCigEconomySystem::PricePolicyName() const
{
	switch (PricePolicy)
	{
	case 0: return CigText::Get(TEXT("economy.policy.cheap"));
	case 2: return CigText::Get(TEXT("economy.policy.expensive"));
	default: return CigText::Get(TEXT("economy.policy.normal"));
	}
}

void UCigEconomySystem::CyclePricePolicy()
{
	PricePolicy = (PricePolicy + 1) % 3;
	if (GM)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.policy"), *PricePolicyName()), FLinearColor(0.6f, 0.9f, 1.f));
	}
}

// ---------------------------------------------------------------- station upgrades

int32 UCigEconomySystem::GloveCost() const
{
	return GloveLevel == 0 ? 250 : 550;
}

int32 UCigEconomySystem::IsotCost() const
{
	return IsotLevel == 0 ? 300 : 650;
}

int32 UCigEconomySystem::AdCost() const
{
	return 150 + 100 * AdCount;
}

FString UCigEconomySystem::UpgradeText(ECigStation Type) const
{
	switch (Type)
	{
	case ECigStation::Eldiven:
		return GloveLevel >= 2
			? CigText::Get(TEXT("prompt.glovemax"))
			: CigText::Format(TEXT("prompt.gloveupgrade"), GloveLevel + 1, GloveCost());
	case ECigStation::IsotPlus:
		return IsotLevel >= 2
			? CigText::Get(TEXT("prompt.isotmax"))
			: CigText::Format(TEXT("prompt.isotupgrade"), IsotLevel + 1, IsotCost());
	case ECigStation::Reklam:
		return CigText::Format(TEXT("prompt.advertise"), AdCost());
	default:
		return TEXT("");
	}
}

void UCigEconomySystem::BuyStationUpgrade(ECigStation Type)
{
	int32 Cost = 0;
	switch (Type)
	{
	case ECigStation::Eldiven:
		if (GloveLevel >= 2) { GM->AddMessage(CigText::Get(TEXT("msg.economy.glovemax"))); return; }
		Cost = GloveCost();
		break;
	case ECigStation::IsotPlus:
		if (IsotLevel >= 2) { GM->AddMessage(CigText::Get(TEXT("msg.economy.isotmax"))); return; }
		Cost = IsotCost();
		break;
	case ECigStation::Reklam:
		Cost = AdCost();
		break;
	default:
		return;
	}

	if (!TrySpend(Cost))
	{
		GM->AddMessage(CigText::Format(TEXT("msg.money.need"), Cost), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	switch (Type)
	{
	case ECigStation::Eldiven:
		GloveLevel++;
		GM->AddMessage(CigText::Format(TEXT("msg.economy.glovebought"), GloveLevel), FLinearColor(0.4f, 1.f, 0.4f));
		break;
	case ECigStation::IsotPlus:
		IsotLevel++;
		GM->AddMessage(CigText::Format(TEXT("msg.economy.isotbought"), IsotLevel), FLinearColor(0.4f, 1.f, 0.4f));
		break;
	case ECigStation::Reklam:
		AdCount++;
		if (GM->Progression)
		{
			GM->Progression->AddRep(8.f);
		}
		GM->AddMessage(CigText::Get(TEXT("msg.economy.advertised")), FLinearColor(0.4f, 1.f, 0.4f));
		break;
	default:
		break;
	}
	GM->RequestSave();
}

// ---------------------------------------------------------------- shop upgrades

bool UCigEconomySystem::BuyUpgrade(ECigUpgrade U)
{
	const FCigUpgradeRow& Def = CigUpgradeDef(U);
	if (HasUpgrade(U))
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.alreadyowned"), *CigBalance::UpgradeName((int32)U)));
		return false;
	}
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (Prog && Prog->Level < Def.MinLevel)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.levelneeded"), *CigBalance::UpgradeName((int32)U), Def.MinLevel), FLinearColor(1.f, 0.6f, 0.2f));
		return false;
	}
	if (!TrySpend(Def.Cost))
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.upgradecost"), *CigBalance::UpgradeName((int32)U), Def.Cost), FLinearColor(1.f, 0.4f, 0.3f));
		return false;
	}

	UpgradeOwned[(int32)U] = true;
	if (GM->WorldBuilder)
	{
		GM->WorldBuilder->ApplyUpgradeVisual((int32)U);
	}
	GM->AddMessage(CigText::Format(TEXT("msg.economy.installed"), *CigBalance::UpgradeName((int32)U), *CigBalance::UpgradeDesc((int32)U)), FLinearColor(0.4f, 1.f, 0.4f));
	if (GM->Progression)
	{
		GM->Progression->AddXP(15);
	}
	Bus().UpgradeBought.Broadcast();
	GM->RequestSave();
	return true;
}

// ---------------------------------------------------------------- suppliers

void UCigEconomySystem::CycleSupplier()
{
	CurrentSupplier = (CurrentSupplier + 1) % CigSupplierCount;
	if (GM)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.supplier"),
			*SupplierName(CurrentSupplier), *SupplierDesc(CurrentSupplier)), FLinearColor(0.7f, 0.9f, 1.f));
	}
}

void UCigEconomySystem::CycleIngredientTier()
{
	IngredientTier = (IngredientTier + 1) % 3;
	if (GM)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.ingredientquality"),
			*IngredientTierName(), IngredientTierCostMult(), IngredientTierQualityMult()), FLinearColor(0.8f, 0.95f, 0.7f));
	}
}

FString UCigEconomySystem::IngredientTierName() const
{
	switch (IngredientTier)
	{
	case 0:  return CigText::Get(TEXT("economy.tier.cheap"));
	case 2:  return CigText::Get(TEXT("economy.tier.quality"));
	default: return CigText::Get(TEXT("common.normal"));
	}
}

float UCigEconomySystem::IngredientTierCostMult() const
{
	switch (IngredientTier)
	{
	case 0:  return 0.68f;
	case 2:  return 1.45f;
	default: return 1.f;
	}
}

float UCigEconomySystem::IngredientTierQualityMult() const
{
	switch (IngredientTier)
	{
	case 0:  return 0.84f;
	case 2:  return 1.16f;
	default: return 1.f;
	}
}

float UCigEconomySystem::RelationDiscount(int32 SupplierIdx) const
{
	const float R = SupplierRelation[FMath::Clamp(SupplierIdx, 0, CigSupplierCount - 1)];
	if (R >= 90.f) return 0.15f;
	if (R >= 60.f) return 0.10f;
	if (R >= 30.f) return 0.05f;
	return 0.f;
}

float UCigEconomySystem::SupplierPriceMult() const
{
	return Supplier(CurrentSupplier).PriceMult * (1.f - RelationDiscount(CurrentSupplier));
}

float UCigEconomySystem::SupplierQuality() const
{
	return Supplier(CurrentSupplier).Quality;
}

float UCigEconomySystem::SupplierDeliverTime() const
{
	float T = Supplier(CurrentSupplier).DeliverTime;
	if (SupplierRelation[CurrentSupplier] >= 60.f)
	{
		T *= 0.8f; // a friendly supplier gives priority
	}
	return T;
}

float UCigEconomySystem::SupplierReliability() const
{
	return Supplier(CurrentSupplier).Reliability;
}

void UCigEconomySystem::AddSupplierRelation(float Delta)
{
	float& R = SupplierRelation[CurrentSupplier];
	const float Old = R;
	R = FMath::Clamp(R + Delta, 0.f, 100.f);
	if (GM && Old < 30.f && R >= 30.f)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.relation5"), *SupplierName(CurrentSupplier)), FLinearColor(0.5f, 1.f, 0.7f));
	}
	else if (GM && Old < 60.f && R >= 60.f)
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.relation10"), *SupplierName(CurrentSupplier)), FLinearColor(0.5f, 1.f, 0.7f));
	}
}

// ---------------------------------------------------------------- house & car

void UCigEconomySystem::BuyHouse()
{
	constexpr int32 HousePrice = 5000;
	if (bOwnHouse)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.economy.houseowned")));
		return;
	}
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (Prog && Prog->Level < 5)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.economy.houselevel")), FLinearColor(1.f, 0.6f, 0.2f));
		return;
	}
	if (!TrySpend(HousePrice))
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.housecost"), HousePrice), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}

	bOwnHouse = true;
	if (GM->WorldBuilder)
	{
		GM->WorldBuilder->SetHouseOwned();
		GM->WorldBuilder->SpawnFloatText(GM->WorldBuilder->HousePos + FVector(-300.f, 0.f, 400.f), CigText::Get(TEXT("float.houseisyours")), FColor(100, 255, 120), 50.f);
	}
	GM->AddMessage(CigText::Get(TEXT("msg.economy.housebought")), FLinearColor(0.4f, 1.f, 0.4f));
	if (GM->Progression)
	{
		GM->Progression->AddXP(30);
	}
	GM->RequestSave();
}

void UCigEconomySystem::RefuelCar()
{
	ACigCar* Car = GM ? GM->PlayerCar.Get() : nullptr;
	if (!Car)
	{
		return;
	}
	if (Car->Fuel >= 99.f)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.economy.fuelfull")));
		return;
	}
	const int32 Cost = FMath::CeilToInt((100.f - Car->Fuel) * 1.2f);
	if (!TrySpend(Cost))
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.fuelcost"), Cost), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}
	Car->Fuel = 100.f;
	GM->AddMessage(CigText::Format(TEXT("msg.economy.refueled"), Cost), FLinearColor(0.6f, 0.9f, 1.f));
}

void UCigEconomySystem::RepairCar()
{
	ACigCar* Car = GM ? GM->PlayerCar.Get() : nullptr;
	if (!Car)
	{
		return;
	}
	if (Car->Damage <= 1.f)
	{
		GM->AddMessage(CigText::Get(TEXT("msg.economy.carhealthy")));
		return;
	}
	const int32 Cost = FMath::CeilToInt(Car->Damage * 3.f);
	if (!TrySpend(Cost))
	{
		GM->AddMessage(CigText::Format(TEXT("msg.economy.repaircost"), Cost), FLinearColor(1.f, 0.4f, 0.3f));
		return;
	}
	Car->Damage = 0.f;
	GM->AddMessage(CigText::Format(TEXT("msg.economy.repaired"), Cost), FLinearColor(0.6f, 0.9f, 1.f));
}
