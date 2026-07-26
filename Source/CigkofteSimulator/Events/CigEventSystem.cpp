#include "Events/CigEventSystem.h"
#include "Game/CigkofteGameMode.h"
#include "Customers/CigCustomerSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Delivery/CigDeliverySystem.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigLog.h"
#include "Core/CigBalance.h"
#include "Core/CigText.h"
#include "Economy/CigEconomySystem.h"
#include "Economy/CigSaleSystem.h"
#include "Progression/CigProgressionSystem.h"

namespace
{
	// Builds event.<key>.<suffix> from the row key, lower-cased so the text
	// table reads consistently with the other bal.* keys.
	FString OlayMetni(int32 Index, const TCHAR* Suffix)
	{
		const FString Key = CigBalance::Event(Index).Key.ToLower();
		return CigText::Get(*FString::Printf(TEXT("event.%s.%s"), *Key, Suffix));
	}
}

FString UCigEventSystem::EventName(int32 Index)     { return OlayMetni(Index, TEXT("name")); }
FString UCigEventSystem::EventStartMsg(int32 Index) { return OlayMetni(Index, TEXT("start")); }
FString UCigEventSystem::EventEndMsg(int32 Index)   { return OlayMetni(Index, TEXT("end")); }

bool UCigEventSystem::IsEventActive(int32 DefIndex) const
{
	for (const FCigActiveEvent& E : Active)
	{
		if (E.DefIndex == DefIndex)
		{
			return true;
		}
	}
	return false;
}

void UCigEventSystem::OnDayStart(int32 Day)
{
	// Normally a no-op: the previous day retired everything. Anything that did
	// survive still leaves through the same door rather than disappearing.
	TumOlaylariBitir();

	// Calendar days come first and outside the two-event budget: match day is
	// something the player planned around, so a pair of unlucky dice rolls must
	// not be able to crowd it out.
	for (int32 i = 0; i < CigEventDefCount; ++i)
	{
		const FCigEventRow& Row = CigBalance::Event(i);
		if (Row.TakvimPeriyodu > 0 && Day >= Row.MinGun && Day % Row.TakvimPeriyodu == 0)
		{
			StartEvent(i);
		}
	}

	// At most 2 events per day
	int32 Started = 0;
	for (int32 i = 0; i < CigEventDefCount && Started < 2; ++i)
	{
		const FCigEventRow& Row = CigBalance::Event(i);
		if (Row.TakvimPeriyodu > 0)
		{
			continue;
		}
		if (Day >= Row.MinGun && Rng().Chance(Row.Sans))
		{
			StartEvent(i);
			Started++;
		}
	}

	TopluSiparisTeklifEt(Day);
}

void UCigEventSystem::OnDayEnd(int32 Day)
{
	TumOlaylariBitir();
	TopluSiparisiSonuclandir(Day);
}

int32 UCigEventSystem::TumOlaylariBitir()
{
	const int32 Bitirilen = Active.Num();
	for (int32 i = Active.Num() - 1; i >= 0; --i)
	{
		EndEvent(i);
	}
	return Bitirilen;
}

FCigTopluSonuc UCigEventSystem::TopluSiparisSonucu(int32 Yapilan, int32 Istenen)
{
	// Delivering short of the order is not one failure but two different ones:
	// nearly making it costs goodwill, missing it badly costs the reputation the
	// shop was trading on when it took the job.
	const float KilPayiEsigi = CigBalance::Contract(TEXT("KilPayiEsigi"), 0.80f);

	FCigTopluSonuc S;
	if (Istenen <= 0)
	{
		return S;
	}

	const float Oran = (float)Yapilan / (float)Istenen;
	if (Oran >= 1.f)
	{
		S.OdulOrani = 1.f;
		S.ItibarFarki = CigBalance::Contract(TEXT("TamItibar"), 8.f);
	}
	else if (Oran >= KilPayiEsigi)
	{
		S.OdulOrani = CigBalance::Contract(TEXT("KilPayiOdulOrani"), 0.60f);
		S.ItibarFarki = CigBalance::Contract(TEXT("KilPayiItibar"), -3.f);
	}
	else
	{
		S.OdulOrani = 0.f;
		S.ItibarFarki = CigBalance::Contract(TEXT("BasarisizItibar"), -12.f);
	}
	return S;
}

FCigTopluTeklif UCigEventSystem::TeklifUret(int32 Seviye, int32 Sapma)
{
	FCigTopluTeklif T;

	// Notice is what makes this a decision rather than a surprise: enough time to
	// stock up, not enough to coast.
	T.IhbarGunu = FMath::Max(1, FMath::RoundToInt(CigBalance::Contract(TEXT("IhbarGunu"), 3.f)));

	const int32 Taban = FMath::RoundToInt(CigBalance::Contract(TEXT("AdetTabani"), 8.f));
	const int32 SeviyeBasina = FMath::RoundToInt(CigBalance::Contract(TEXT("SeviyeBasinaAdet"), 2.f));

	// A contract for zero wraps would pay out in full the moment it was accepted,
	// so the floor is not decoration.
	T.IstenenAdet = FMath::Max(1, Taban + FMath::Max(0, Seviye) * SeviyeBasina + FMath::Max(0, Sapma));
	T.Odul = FMath::RoundToInt(T.IstenenAdet * CigBalance::Contract(TEXT("UcretCarpani"), 55.f));

	return T;
}

void UCigEventSystem::TopluSiparisTeklifEt(int32 Day)
{
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (!Prog || TopluSiparis.bTeklifVar || TopluSiparis.bKabulEdildi)
	{
		return;
	}

	// Its own odds, not the BuyukTeslimat event's. Sharing that row meant one
	// number decided both how often a wedding called and how often a large
	// delivery turned up, and rolling it twice a day made the pair appear
	// together far more often than either was tuned for.
	if (Day < FMath::RoundToInt(CigBalance::Contract(TEXT("MinGun"), 3.f))
		|| !Rng().Chance(CigBalance::Contract(TEXT("TeklifSansi"), 0.12f)))
	{
		return;
	}

	const int32 SapmaTavani = FMath::Max(0, FMath::RoundToInt(CigBalance::Contract(TEXT("AdetSapmasi"), 4.f)));
	const FCigTopluTeklif Teklif = TeklifUret(Prog->Level, Rng().RandRange(0, SapmaTavani));

	TopluSiparis = FCigTopluSiparis();
	TopluSiparis.bTeklifVar = true;
	TopluSiparis.TeslimGunu = Day + Teklif.IhbarGunu;
	TopluSiparis.IstenenAdet = Teklif.IstenenAdet;
	TopluSiparis.Odul = Teklif.Odul;

	GM->AddMessage(CigText::Format(TEXT("msg.contract.offer"),
		TopluSiparis.IstenenAdet, TopluSiparis.TeslimGunu, TopluSiparis.Odul),
		FLinearColor(1.f, 0.85f, 0.5f));
}

void UCigEventSystem::TopluSiparisiKabulEt()
{
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (!TopluSiparis.bTeklifVar || !Prog)
	{
		return;
	}

	TopluSiparis.bTeklifVar = false;
	TopluSiparis.bKabulEdildi = true;
	TopluSiparis.BaslangicServis = Prog->TotalServed;

	GM->AddMessage(CigText::Format(TEXT("msg.contract.accepted"),
		TopluSiparis.IstenenAdet, TopluSiparis.TeslimGunu), FLinearColor(0.5f, 1.f, 0.5f));
}

void UCigEventSystem::TopluSiparisiReddet()
{
	if (!TopluSiparis.bTeklifVar)
	{
		return;
	}

	TopluSiparis = FCigTopluSiparis();
	GM->AddMessage(CigText::Get(TEXT("msg.contract.declined")), FLinearColor(0.8f, 0.8f, 0.8f));
}

void UCigEventSystem::TopluSiparisiSonuclandir(int32 Day)
{
	UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (!Prog)
	{
		return;
	}

	// An offer nobody answered simply expires on the delivery day.
	if (TopluSiparis.bTeklifVar && Day >= TopluSiparis.TeslimGunu)
	{
		TopluSiparis = FCigTopluSiparis();
		return;
	}

	if (!TopluSiparis.bKabulEdildi || Day < TopluSiparis.TeslimGunu)
	{
		return;
	}

	const int32 Yapilan = Prog->TotalServed - TopluSiparis.BaslangicServis;
	const FCigTopluSonuc Sonuc = TopluSiparisSonucu(Yapilan, TopluSiparis.IstenenAdet);
	const int32 Kazanc = FMath::RoundToInt(TopluSiparis.Odul * Sonuc.OdulOrani);

	// A contract payout is takings like any other, so it counts towards the day
	// and towards the best-day record instead of appearing only in the balance.
	if (Kazanc > 0 && GM->Sales)
	{
		GM->Sales->GeliriKaydet(Kazanc);
	}
	Prog->AddRep(Sonuc.ItibarFarki);

	GM->AddMessage(CigText::Format(TEXT("msg.contract.result"),
		Yapilan, TopluSiparis.IstenenAdet, Kazanc),
		Sonuc.ItibarFarki >= 0.f ? FLinearColor(0.5f, 1.f, 0.5f) : FLinearColor(1.f, 0.4f, 0.3f));

	TopluSiparis = FCigTopluSiparis();
}

void UCigEventSystem::TriggerEvent(int32 DefIndex)
{
	StartEvent(FMath::Clamp(DefIndex, 0, CigEventDefCount - 1));
}

void UCigEventSystem::StartEvent(int32 DefIndex)
{
	FCigActiveEvent E;
	E.DefIndex = DefIndex;
	E.TimeLeft = CigBalance::Event(DefIndex).Sure;
	Active.Add(E);

	if (GM)
	{
		GM->AddMessage(FString::Printf(TEXT("OLAY: %s"), *EventStartMsg(DefIndex)), FLinearColor(1.f, 0.85f, 0.4f));
	}
	ApplySpecialStart(CigBalance::Event(DefIndex).OzelTur);
	UE_LOG(LogCig, Log, TEXT("Olay başladı: %s"), *EventName(DefIndex));
}

void UCigEventSystem::ApplySpecialStart(int32 OzelTur)
{
	if (!GM)
	{
		return;
	}
	switch (OzelTur)
	{
	case 1: // öğrenci patlaması
		if (GM->Customers)
		{
			for (int32 i = 0; i < 2; ++i)
			{
				if (GM->Customers->Queue.Num() < GM->Customers->MaxQueue())
				{
					GM->Customers->SpawnCustomer();
				}
			}
		}
		break;
	case 2: // denetim
		if (GM->Customers)
		{
			GM->Customers->InspectorTimer = Rng().FRandRange(20.f, 60.f);
		}
		break;
	case 3: // VIP
		if (GM->Customers && GM->Customers->Queue.Num() < GM->Customers->MaxQueue())
		{
			GM->Customers->SpawnCustomer(true, false);
		}
		break;
	case 4: // fenomen
		if (GM->Customers && GM->Customers->Queue.Num() < GM->Customers->MaxQueue())
		{
			GM->Customers->SpawnCustomer(false, true);
		}
		break;
	case 5: // toplu sipariş
		if (GM->Delivery)
		{
			GM->Delivery->SpawnBulkOrder();
		}
		break;
	case 6: // kıtlık
		if (GM->Inventory)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				const int32 Item = Rng().RandRange(0, CigStockCount - 1);
				GM->Inventory->Stock[Item] /= 2;
			}
		}
		break;
	default:
		break;
	}
}

void UCigEventSystem::EndEvent(int32 ActiveIndex)
{
	if (GM)
	{
		GM->AddMessage(EventEndMsg(Active[ActiveIndex].DefIndex), FLinearColor(0.8f, 0.8f, 0.7f));
	}
	Active.RemoveAt(ActiveIndex);
}

void UCigEventSystem::UpdateSystem(float DeltaSeconds)
{
	for (int32 i = Active.Num() - 1; i >= 0; --i)
	{
		if (Active[i].TimeLeft > 0.f)
		{
			Active[i].TimeLeft -= DeltaSeconds;
			if (Active[i].TimeLeft <= 0.f)
			{
				EndEvent(i);
			}
		}
	}
}

float UCigEventSystem::SpawnMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= CigBalance::Event(E.DefIndex).SpawnCarpani;
	}
	return M;
}

float UCigEventSystem::PatienceMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= CigBalance::Event(E.DefIndex).SabirCarpani;
	}
	return M;
}

float UCigEventSystem::PriceMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= CigBalance::Event(E.DefIndex).FiyatCarpani;
	}
	return M;
}

float UCigEventSystem::DeliveryMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= CigBalance::Event(E.DefIndex).TeslimatCarpani;
	}
	return M;
}

float UCigEventSystem::StockCostMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= CigBalance::Event(E.DefIndex).StokCarpani;
	}
	// The supply delay event affects lead time rather than stock price; see SupplyDelayMult.
	return M;
}

float UCigEventSystem::SupplyDelayMult() const
{
	for (const FCigActiveEvent& E : Active)
	{
		if (E.DefIndex == EventTedarikGecikmesi)
		{
			return 2.f;
		}
	}
	return 1.f;
}

bool UCigEventSystem::IsFridgeBroken() const
{
	for (const FCigActiveEvent& E : Active)
	{
		if (CigBalance::Event(E.DefIndex).OzelTur == 7)
		{
			return true;
		}
	}
	return false;
}
