#include "Events/CigEventSystem.h"
#include "Game/CigkofteGameMode.h"
#include "Customers/CigCustomerSystem.h"
#include "Inventory/CigInventorySystem.h"
#include "Delivery/CigDeliverySystem.h"
#include "Core/CigRandomSubsystem.h"
#include "Core/CigLog.h"

const FCigEventDef& UCigEventSystem::Def(int32 Index)
{
	//                     Name                  Start                                                            End                                      MinDay Odds  Time  Spawn Pat.  Price Deliv. Stock Extra
	static const FCigEventDef Defs[CigEventDefCount] = {
		{ TEXT("Okul Çıkışı"),      TEXT("Okul dağıldı! Öğrenci akını geliyor."),                    TEXT("Öğrenciler dağıldı."),                    1, 0.20f, 45.f, 1.8f, 0.8f, 0.95f, 1.f,  1.f,  1 },
		{ TEXT("Maç Günü"),         TEXT("Bugün derbi var! Maç öncesi herkes dürüm istiyor."),       TEXT("Maç başladı, sokaklar boşaldı."),         2, 0.15f, 60.f, 1.6f, 0.7f, 1.1f,  1.2f, 1.f,  0 },
		{ TEXT("Yağmur"),           TEXT("Yağmur bastırdı. Sokakta müşteri az, paket servis çok."),  TEXT("Yağmur dindi."),                          1, 0.18f, -1.f, 0.6f, 1.1f, 1.f,   1.7f, 1.f,  0 },
		{ TEXT("Sıcak Hava"),       TEXT("Kavurucu sıcak! Ayran satışları patlayacak, hamur çabuk bozulur."), TEXT("Hava serinledi."),               2, 0.15f, -1.f, 1.1f, 0.9f, 1.f,   1.f,  1.f,  0 },
		{ TEXT("İsot Zammı"),       TEXT("İsot fiyatlarına zam geldi! Stok maliyetleri arttı."),     TEXT("İsot fiyatları normale döndü."),          3, 0.12f, -1.f, 1.f,  1.f,  1.f,   1.f,  1.5f, 0 },
		{ TEXT("Tedarik Gecikmesi"),TEXT("Yollarda sorun var, kuryeler geç kalıyor."),               TEXT("Tedarik normale döndü."),                 2, 0.12f, -1.f, 1.f,  1.f,  1.f,   1.f,  1.f,  0 },
		{ TEXT("Elektrik Kesintisi"),TEXT("Elektrik kesildi! Buzdolabı çalışmıyor."),                TEXT("Elektrik geldi."),                        3, 0.10f, 50.f, 0.9f, 1.f,  1.f,   1.f,  1.f,  7 },
		{ TEXT("Belediye Denetimi"),TEXT("Belediye bugün denetim yapıyor. Müfettiş yolda olabilir."),TEXT("Denetim günü bitti."),                    2, 0.12f, -1.f, 1.f,  1.f,  1.f,   1.f,  1.f,  2 },
		{ TEXT("Ünlü Müşteri"),     TEXT("Ünlü biri mahallede görüldü! VIP kapıda olabilir."),       TEXT("Ünlü ayrıldı."),                          3, 0.10f, 40.f, 1.2f, 1.f,  1.f,   1.f,  1.f,  3 },
		{ TEXT("Fenomen Ziyareti"), TEXT("Yemek fenomeni çekim için geliyor!"),                      TEXT("Fenomen çekimini bitirdi."),              4, 0.10f, 40.f, 1.1f, 1.f,  1.f,   1.f,  1.f,  4 },
		{ TEXT("Rakip Kampanyası"), TEXT("Rakipler bugün kampanya yapıyor, müşteri az."),            TEXT("Rakip kampanyası bitti."),                2, 0.12f, -1.f, 0.7f, 1.f,  1.f,   1.f,  1.f,  0 },
		{ TEXT("Sokak Festivali"),  TEXT("Sokak festivali! Mahalle kaynıyor."),                     TEXT("Festival sona erdi."),                    4, 0.10f, -1.f, 1.9f, 0.85f,1.15f, 1.3f, 1.f,  0 },
		{ TEXT("Toplu Sipariş"),    TEXT("Şirketten toplu sipariş geldi! Büyük paket servis fırsatı."),TEXT("Toplu sipariş süresi doldu."),          3, 0.12f, 60.f, 1.f,  1.f,  1.f,   1.f,  1.f,  5 },
		{ TEXT("Malzeme Kıtlığı"),  TEXT("Toptancıda kıtlık! Bazı stoklar yarıya düştü."),           TEXT("Kıtlık aşıldı."),                         3, 0.08f, -1.f, 1.f,  1.f,  1.f,   1.f,  1.3f, 6 }
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

	// At most 2 events per day
	int32 Started = 0;
	for (int32 i = 0; i < CigEventDefCount && Started < 2; ++i)
	{
		const FCigEventDef& D = Def(i);
		if (Day >= D.MinDay && Rng().Chance(D.Chance))
		{
			StartEvent(i);
			Started++;
		}
	}
}

void UCigEventSystem::OnDayEnd(int32 Day)
{
	Active.Empty();
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
	E.TimeLeft = D.Duration;
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
		M *= Def(E.DefIndex).SpawnMult;
	}
	return M;
}

float UCigEventSystem::PatienceMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= Def(E.DefIndex).PatienceMult;
	}
	return M;
}

float UCigEventSystem::PriceMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= Def(E.DefIndex).PriceMult;
	}
	return M;
}

float UCigEventSystem::DeliveryMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= Def(E.DefIndex).DeliveryMult;
	}
	return M;
}

float UCigEventSystem::StockCostMult() const
{
	float M = 1.f;
	for (const FCigActiveEvent& E : Active)
	{
		M *= Def(E.DefIndex).StockCostMult;
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
