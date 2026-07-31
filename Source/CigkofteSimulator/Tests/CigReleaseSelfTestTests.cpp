#include "Misc/AutomationTest.h"
#include "Game/CigReleaseSelfTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigReleaseSelfTestOutputArgumentParsing,
	"Cigkofte.ReleaseSelfTest.OutputArgumentParsing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigReleaseSelfTestOutputArgumentParsing::RunTest(const FString&)
{
	using CigReleaseSelfTest::EOutputArgumentState;

	auto Parse = [](const TCHAR* CommandLine, FString& Value, FString& Error)
	{
		return CigReleaseSelfTest::ParseOutputArgument(CommandLine, Value, Error);
	};
	auto ExpectInvalid = [this, &Parse](const TCHAR* Label, const TCHAR* CommandLine)
	{
		FString Value;
		FString Error;
		const EOutputArgumentState State = Parse(CommandLine, Value, Error);
		TestTrue(Label, State == EOutputArgumentState::Invalid);
		TestTrue(FString::Printf(TEXT("%s: hata aciklanmali"), Label), !Error.IsEmpty());
	};

	FString Value;
	FString Error;
	TestTrue(TEXT("Arguman yoksa fallback secilmeli"),
		Parse(TEXT("-CigReleaseSelfTest -nullrhi"), Value, Error) == EOutputArgumentState::NotProvided);
	TestTrue(TEXT("Arguman yokken deger bos"), Value.IsEmpty());

	ExpectInvalid(TEXT("Acik bos deger"), TEXT("-CigReleaseSelfTestOut="));
	ExpectInvalid(TEXT("Tirnakli bos deger"), TEXT("-CigReleaseSelfTestOut=\"\""));
	ExpectInvalid(TEXT("Yalniz bosluk degeri"), TEXT("-CigReleaseSelfTestOut=\"   \""));
	ExpectInvalid(TEXT("Esittir olmadan"), TEXT("-CigReleaseSelfTestOut"));
	ExpectInvalid(TEXT("Ayni tekrar"),
		TEXT("-CigReleaseSelfTestOut=one.txt -CigReleaseSelfTestOut=one.txt"));
	ExpectInvalid(TEXT("Celisen tekrar"),
		TEXT("-CigReleaseSelfTestOut=one.txt -CigReleaseSelfTestOut=two.txt"));
	ExpectInvalid(TEXT("Bos ve dolu tekrar"),
		TEXT("-CigReleaseSelfTestOut= -CigReleaseSelfTestOut=two.txt"));
	ExpectInvalid(TEXT("Kapanmayan tirnak"), TEXT("-CigReleaseSelfTestOut=\"C:/Temp/report.txt"));

	Value.Reset();
	Error.Reset();
	TestTrue(TEXT("Benzer isim eslesmemeli"),
		Parse(TEXT("-NotCigReleaseSelfTestOut=wrong.txt"), Value, Error)
			== EOutputArgumentState::NotProvided);
	TestTrue(TEXT("Baska argumanin degerindeki metin eslesmemeli"),
		Parse(TEXT("-Other=\"CigReleaseSelfTestOut=wrong.txt\""), Value, Error)
			== EOutputArgumentState::NotProvided);

	Value.Reset();
	Error.Reset();
	TestTrue(TEXT("Bosluklu tirnakli yol kabul edilmeli"),
		Parse(TEXT("-CigReleaseSelfTestOut=\"C:/Temp Folder/report.txt\""), Value, Error)
			== EOutputArgumentState::Provided);
	TestEqual(TEXT("Tirnaklar kaldirilmali, bosluk korunmali"), Value,
		FString(TEXT("C:/Temp Folder/report.txt")));

	Value.Reset();
	Error.Reset();
	TestTrue(TEXT("Goreli yol kabul edilmeli"),
		Parse(TEXT("-cigreleaseselftestout=Reports/self-test.txt"), Value, Error)
			== EOutputArgumentState::Provided);
	TestEqual(TEXT("Buyuk-kucuk harf duyarsiz exact anahtar"), Value,
		FString(TEXT("Reports/self-test.txt")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
