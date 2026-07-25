// The competence model behind the staff system.
//
// HataOlasiligi and IsAraligi are the two pure functions the rest of the system
// leans on, so they can be checked without hiring anyone. What matters is the
// shape: paying for a tidy hand must buy fewer mistakes, experience must beat
// exhaustion back, and neither end may run away.
//
// Running headless:
//   UnrealEditor-Cmd <project>.uproject -ExecCmds="Automation RunTests Cigkofte.Staff; Quit" -unattended -nop4 -nullrhi

#include "Misc/AutomationTest.h"
#include "Staff/CigStaffSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffTidinessTest,
	"Cigkofte.Staff.TidinessBuysFewerMistakes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigStaffTidinessTest::RunTest(const FString& /*Parameters*/)
{
	const float Ozensiz = UCigStaffSystem::HataOlasiligi(0.70f, 1, 100.f);
	const float Notr = UCigStaffSystem::HataOlasiligi(1.00f, 1, 100.f);
	const float Titiz = UCigStaffSystem::HataOlasiligi(1.35f, 1, 100.f);

	TestTrue(TEXT("Titizlik arttıkça hata azalmalı"), Titiz < Notr && Notr < Ozensiz);

	// The whole reason a tidy candidate costs more is that the difference is
	// worth paying for; a token gap would make the hiring screen a formality.
	TestTrue(TEXT("Titiz eleman özensizin yarısından az hata yapmalı"), Titiz < Ozensiz * 0.6f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffExperienceTest,
	"Cigkofte.Staff.ExperienceAndFatigue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigStaffExperienceTest::RunTest(const FString& /*Parameters*/)
{
	const float Yeni = UCigStaffSystem::HataOlasiligi(1.f, 1, 100.f);
	const float Deneyimli = UCigStaffSystem::HataOlasiligi(1.f, 4, 100.f);
	TestTrue(TEXT("Deneyim hatayı azaltmalı"), Deneyimli < Yeni);

	// Fatigue only bites in the back half of the day, so a full tank and a
	// half-full one have to behave the same.
	TestEqual(TEXT("Enerji 50 üstünde yorgunluk etkisi olmamalı"),
		UCigStaffSystem::HataOlasiligi(1.f, 1, 60.f), Yeni, 0.0001f);
	TestTrue(TEXT("Tükenmiş eleman daha çok hata yapmalı"),
		UCigStaffSystem::HataOlasiligi(1.f, 1, 5.f) > Yeni);

	// However bad it gets, most ticks still have to be work rather than damage.
	TestTrue(TEXT("Hata olasılığı yarıyı geçmemeli"),
		UCigStaffSystem::HataOlasiligi(0.1f, 1, 0.f) <= 0.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCigStaffPaceTest,
	"Cigkofte.Staff.SpeedShortensTheGap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCigStaffPaceTest::RunTest(const FString& /*Parameters*/)
{
	const float Yavas = UCigStaffSystem::IsAraligi(0.80f, 1);
	const float Notr = UCigStaffSystem::IsAraligi(1.00f, 1);
	const float Hizli = UCigStaffSystem::IsAraligi(1.35f, 1);

	TestTrue(TEXT("Hız arttıkça iş aralığı kısalmalı"), Hizli < Notr && Notr < Yavas);
	TestTrue(TEXT("Seviye de aralığı kısaltmalı"), UCigStaffSystem::IsAraligi(1.f, 5) < Notr);

	// A neutral level-1 hand must still tick at the pace the system was
	// balanced around before speed existed.
	TestEqual(TEXT("Nötr eleman eski 9 saniyelik aralığı korumalı"), Notr, 8.5f, 0.001f);

	// Nobody works instantly, however fast and however senior.
	TestTrue(TEXT("Aralık alt sınırın altına inmemeli"), UCigStaffSystem::IsAraligi(5.f, 10) >= 2.5f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
