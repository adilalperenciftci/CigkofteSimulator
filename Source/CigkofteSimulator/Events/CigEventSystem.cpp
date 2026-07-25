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
#include "Progression/CigProgressionSystem.h"

const FCigEventDef& UCigEventSystem::Def(int32 Index)
{
	//                     Name                  Start                                                            End                                      MinDay Odds  Time  Spawn Pat.  Price Deliv. Stock Extra
	static const FCigEventDef Defs[CigEventDefCount] = {
		{ TEXT("Okul Çıkışı"), TEXT("Okul dağıldı! Öğrenci akını geliyor."), TEXT("Öğrenciler dağıldı."), 1 },
		{ TEXT("Maç Günü"), TEXT("Bugün derbi var! Maç öncesi herkes dürüm istiyor."), TEXT("Maç başladı, sokaklar boşaldı."), 0 },
		{ TEXT("Yağmur"), TEXT("Yağmur bastırdı. Sokakta müşteri az, paket servis çok."), TEXT("Yağmur dindi."), 0 },
		{ TEXT("Sıcak Hava"), TEXT("Kavurucu sıcak! Ayran satışları patlayacak, hamur çabuk bozulur."), TEXT("Hava serinledi."), 0 },
		{ TEXT("İsot Zammı"), TEXT("İsot fiyatlarına zam geldi! Stok maliyetleri arttı."), TEXT("İsot fiyatları normale döndü."), 0 },
		{ TEXT("Tedarik Gecikmesi"), TEXT("Yollarda sorun var, kuryeler geç kalıyor."), TEXT("Tedarik normale döndü."), 0 },
		{ TEXT("Elektrik Kesintisi"), TEXT("Elektrik kesildi! Buzdolabı çalışmıyor."), TEXT("Elektrik geldi."), 7 },
		{ TEXT("Belediye Denetimi"), TEXT("Belediye bugün denetim yapıyor. Müfettiş yolda olabilir."), TEXT("Denetim günü bitti."), 2 },
		{ TEXT("Ünlü Müşteri"), TEXT("Ünlü biri mahallede görüldü! VIP kapıda olabilir."), TEXT("Ünlü ayrıldı."), 3 },
		{ TEXT("Fenomen Ziyareti"), TEXT("Yemek fenomeni çekim için geliyor!"), TEXT("Fenomen çekimini bitirdi."), 4 },
		{ TEXT("Rakip Kampanyası"), TEXT("Rakipler bugün kampanya yapıyor, müşteri az."), TEXT("Rakip kampanyası bitti."), 0 },
		{ TEXT("Sokak Festivali"), TEXT("Sokak festivali! Mahalle kaynıyor."), TEXT("Festival sona erdi."), 0 },
		{ TEXT("Toplu Sipariş"), TEXT("Şirketten toplu sipariş geldi! Büyük paket servis fırsatı."), TEXT("Toplu sipariş süresi doldu."), 5 },
		{ TEXT("Malzeme Kıtlığı"), TEXT("Toptancıda kıtlık! Bazı stoklar yarıya düştü."), TEXT("Kıtlık aşıldı."), 6 }
	};
	return Defs[FMath::Clamp(Index, 0, CigEventDefCount - 1)];
}

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
	Active.Empty();

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
	Active.Empty();
	TopluSiparisiSonuclandir(Day);
}

FCigTopluSonuc UCigEventSystem::TopluSiparisSonucu(int32 Yapilan, int32 Istenen)
{
	// Delivering short of the order is not one failure but two different ones:
	// nearly making it costs goodwill, missing it badly costs the reputation the
	// shop was trading on when it took the job.
	constexpr float KilPayiEsigi = 0.8f;

	FCigTopluSonuc S;
	if (Istenen <= 0)
	{
		return S;
	}

	const float Oran = (float)Yapilan / (float)Istenen;
	if (Oran >= 1.f)
	{
		S.OdulOrani = 1.f;
		S.ItibarFarki = 8.f;
	}
	else if (Oran >= KilPayiEsigi)
	{
		S.OdulOrani = 0.6f;
		S.ItibarFarki = -3.f;
	}
	else
	{
		S.OdulOrani = 0.f;
		S.ItibarFarki = -12.f;
	}
	return S;
}

void UCigEventSystem::TopluSiparisTeklifEt(int32 Day)
{
	const UCigProgressionSystem* Prog = GM ? GM->Progression.Get() : nullptr;
	if (!Prog || TopluSiparis.bTeklifVar || TopluSiparis.bKabulEdildi)
	{
		return;
	}

	const FCigEventRow& Row = CigBalance::Event(EventTopluSiparis);
	if (Day < Row.MinGun || !Rng().Chance(Row.Sans))
	{
		return;
	}

	// Three days' notice is what makes this a decision rather than a surprise:
	// enough time to stock up, not enough to coast.
	constexpr int32 IhbarGunu = 3;
	constexpr int32 AdetTabani = 8;
	constexpr int32 UcretCarpani = 55;

	TopluSiparis = FCigTopluSiparis();
	TopluSiparis.bTeklifVar = true;
	TopluSiparis.TeslimGunu = Day + IhbarGunu;
	TopluSiparis.IstenenAdet = AdetTabani + Prog->Level * 2 + Rng().RandRange(0, 4);
	TopluSiparis.Odul = TopluSiparis.IstenenAdet * UcretCarpani;

	GM->AddMessage(CigText::Format(TEXT("msg.event.bulk.offer"),
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

	GM->AddMessage(CigText::Format(TEXT("msg.event.bulk.accepted"),
		TopluSiparis.IstenenAdet, TopluSiparis.TeslimGunu), FLinearColor(0.5f, 1.f, 0.5f));
}

void UCigEventSystem::TopluSiparisiReddet()
{
	if (!TopluSiparis.bTeklifVar)
	{
		return;
	}

	TopluSiparis = FCigTopluSiparis();
	GM->AddMessage(CigText::Get(TEXT("msg.event.bulk.declined")), FLinearColor(0.8f, 0.8f, 0.8f));
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

	if (Kazanc > 0 && GM->Economy)
	{
		GM->Economy->Earn(Kazanc);
	}
	Prog->AddRep(Sonuc.ItibarFarki);

	GM->AddMessage(CigText::Format(TEXT("msg.event.bulk.result"),
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
	const FCigEventDef& D = Def(DefIndex);

	FCigActiveEvent E;
	E.DefIndex = DefIndex;
	E.TimeLeft = CigBalance::Event(DefIndex).Sure;
	Active.Add(E);

	if (GM)
	{
		GM->AddMessage(FString::Printf(TEXT("OLAY: %s"), D.StartMsg), FLinearColor(1.f, 0.85f, 0.4f));
	}
	ApplySpecialStart(D);
	UE_LOG(LogCig, Log, TEXT("Olay başladı: %s"), D.Name);
}

void UCigEventSystem::ApplySpecialStart(const FCigEventDef& D)
{
	if (!GM)
	{
		return;
	}
	switch (D.SpecialType)
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
	const FCigEventDef& D = Def(Active[ActiveIndex].DefIndex);
	if (GM)
	{
		GM->AddMessage(D.EndMsg, FLinearColor(0.8f, 0.8f, 0.7f));
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
		if (FCString::Strcmp(Def(E.DefIndex).Name, TEXT("Tedarik Gecikmesi")) == 0)
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
		if (Def(E.DefIndex).SpecialType == 7)
		{
			return true;
		}
	}
	return false;
}
