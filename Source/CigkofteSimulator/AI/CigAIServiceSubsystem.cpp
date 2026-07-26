#include "AI/CigAIServiceSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Core/CigLog.h"

namespace
{
	// A cheap, fast model is the right fit for one-line in-game NPC replies;
	// together with the daily budget it keeps cost low. Changed in one place.
	static const TCHAR* CigAIModel = TEXT("claude-haiku-4-5");
	static const TCHAR* CigAIEndpoint = TEXT("https://api.anthropic.com/v1/messages");
	static const TCHAR* CigAIVersion = TEXT("2023-06-01");
}

void UCigAIServiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Key from the environment only. Empty means AI off and the offline fallback runs.
	ApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("ANTHROPIC_API_KEY"));
	ApiKey.TrimStartAndEndInline();

	UE_LOG(LogCig, Log, TEXT("AI diyalog servisi: %s"),
		IsAIEnabled() ? TEXT("etkin (anahtar bulundu)") : TEXT("kapalı (offline yedek)"));
}

void UCigAIServiceSubsystem::Deinitialize()
{
	Cache.Empty();
	Super::Deinitialize();
}

bool UCigAIServiceSubsystem::IsAIEnabled() const
{
	return !ApiKey.IsEmpty() && RequestsToday < MaxRequestsPerDay;
}

void UCigAIServiceSubsystem::ResetDailyBudget()
{
	RequestsToday = 0;
}

FString UCigAIServiceSubsystem::Sanitize(const FString& Raw)
{
	FString S = Raw;
	S.TrimStartAndEndInline();
	S.ReplaceInline(TEXT("\r"), TEXT(" "));
	S.ReplaceInline(TEXT("\n"), TEXT(" "));
	// Strip the outer quotes if it is wrapped in them
	if (S.Len() >= 2 && S.StartsWith(TEXT("\"")) && S.EndsWith(TEXT("\"")))
	{
		S = S.Mid(1, S.Len() - 2);
	}
	if (S.Len() > 160)
	{
		S = S.Left(157) + TEXT("...");
	}
	return S;
}

FString UCigAIServiceSubsystem::BuildRequestBody(const FCigDialogueContext& Context) const
{
	const TCHAR* MoodTr =
		Context.Mood() == FCigDialogueContext::EMood::Delighted ? TEXT("çok memnun") :
		Context.Mood() == FCigDialogueContext::EMood::Satisfied ? TEXT("memnun") :
		Context.Mood() == FCigDialogueContext::EMood::Mixed     ? TEXT("kararsız") :
		Context.Mood() == FCigDialogueContext::EMood::Unhappy   ? TEXT("hoşnutsuz") :
		                                                          TEXT("kızgın");

	// An accuracy percentage says the order was wrong; it does not say what was
	// wrong, so the line comes back generically disappointed. Naming the mistake
	// is what lets the customer complain about the thing they were handed.
	const FString Hatalar = Context.MistakeSummary();
	const FString HataMetni = Hatalar.IsEmpty()
		? FString(TEXT(" Sipariş istendiği gibi geldi."))
		: FString::Printf(TEXT(" Siparişte hata var: %s."), *Hatalar);

	FString UserText = FString::Printf(
		TEXT("Müşteri profili: %s.%s%s Sipariş doğruluğu %%%.0f, kalite %%%.0f, tezgah hijyeni %%%.0f.%s ")
		TEXT("Bekleme durumu: %s. Genel ruh hali: %s. Bu müşterinin çiğköfteciye söyleyeceği TEK bir Türkçe cümle yaz."),
		*Context.TraitSummary(),
		Context.bVIP ? TEXT(" (VIP)") : TEXT(""),
		Context.bRegular ? *FString::Printf(TEXT(" Müdavim (%d ziyaret, %d kötü anı)."), Context.PastVisits, Context.RememberedMistakes) : TEXT(""),
		Context.Accuracy, Context.Quality, Context.Hygiene,
		*HataMetni,
		Context.PatienceFrac < 0.25f ? TEXT("çok bekledi") : TEXT("makul sürede alındı"),
		MoodTr);

	const FString System = TEXT("Sen bir Türk mahalle çiğköftecisine gelen müşterisin. ")
		TEXT("Sadece kısa, doğal, tek cümlelik bir tepki ver. Tırnak, açıklama veya isim kullanma.");

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), CigAIModel);
	Root->SetNumberField(TEXT("max_tokens"), 64);
	Root->SetStringField(TEXT("system"), System);

	TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("role"), TEXT("user"));
	Msg->SetStringField(TEXT("content"), UserText);

	TArray<TSharedPtr<FJsonValue>> Messages;
	Messages.Add(MakeShared<FJsonValueObject>(Msg));
	Root->SetArrayField(TEXT("messages"), Messages);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}

bool UCigAIServiceSubsystem::ParseResponse(const FString& Json, FString& OutLine)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
	if (!Root->TryGetArrayField(TEXT("content"), Content) || !Content || Content->Num() == 0)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Block : *Content)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Block.IsValid() && Block->TryGetObject(Obj) && Obj)
		{
			FString Type;
			if ((*Obj)->TryGetStringField(TEXT("type"), Type) && Type == TEXT("text"))
			{
				FString Text;
				if ((*Obj)->TryGetStringField(TEXT("text"), Text) && !Text.IsEmpty())
				{
					OutLine = Sanitize(Text);
					return !OutLine.IsEmpty();
				}
			}
		}
	}
	return false;
}

void UCigAIServiceSubsystem::RequestLine(const FCigDialogueContext& Context, FCigDialogueDelegate OnComplete)
{
	const FString Key = Context.CacheKey();

	// 1) Cache
	FString Cached;
	if (Cache.TryGet(Key, Cached))
	{
		FCigDialogueResult R;
		R.Line = Cached;
		R.bFromAI = true;
		R.bCached = true;
		OnComplete.ExecuteIfBound(R);
		return;
	}

	// 2) AI off, or budget spent -> offline
	if (!IsAIEnabled())
	{
		Offline.RequestLine(Context, OnComplete);
		return;
	}

	// 3) Async HTTP. Charge the budget now, to bound parallel requests.
	++RequestsToday;

	TWeakObjectPtr<UCigAIServiceSubsystem> WeakThis(this);
	const FCigDialogueContext CtxCopy = Context;

	FHttpModule& Http = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Http.CreateRequest();
	Req->SetURL(CigAIEndpoint);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("content-type"), TEXT("application/json"));
	Req->SetHeader(TEXT("x-api-key"), ApiKey);
	Req->SetHeader(TEXT("anthropic-version"), CigAIVersion);
	Req->SetContentAsString(BuildRequestBody(Context));

	Req->OnProcessRequestComplete().BindLambda(
		[WeakThis, CtxCopy, OnComplete, Key](FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bOk)
		{
			UCigAIServiceSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return; // the subsystem is gone; the game may be shutting down
			}

			FString Line;
			const bool bHttpOk = bOk && Response.IsValid() && Response->GetResponseCode() == 200;
			if (bHttpOk && ParseResponse(Response->GetContentAsString(), Line))
			{
				Self->Cache.Put(Key, Line);
				FCigDialogueResult R;
				R.Line = Line;
				R.bFromAI = true;
				OnComplete.ExecuteIfBound(R);
			}
			else
			{
				// The service failed - drop quietly to offline so the game is unaffected.
				UE_LOG(LogCig, Verbose, TEXT("AI diyalog isteği başarısız (kod %d); offline yedek."),
					Response.IsValid() ? Response->GetResponseCode() : -1);
				Self->Offline.RequestLine(CtxCopy, OnComplete);
			}
		});

	Req->ProcessRequest();
}
