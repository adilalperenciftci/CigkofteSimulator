#include "CigFloatText.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"

ACigFloatText::ACigFloatText()
{
	PrimaryActorTick.bCanEverTick = true;

	TextComp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	SetRootComponent(TextComp);
	TextComp->SetMobility(EComponentMobility::Movable);
	TextComp->SetHorizontalAlignment(EHTA_Center);
}

void ACigFloatText::Init(const FString& Text, const FColor& Color, float WorldSize)
{
	TextComp->SetText(FText::FromString(Text));
	TextComp->SetTextRenderColor(Color);
	TextComp->SetWorldSize(WorldSize);
}

void ACigFloatText::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AddActorWorldOffset(FVector(0.f, 0.f, 70.f * DeltaSeconds));

	// Always faces the player
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - GetActorLocation();
		SetActorRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
	}

	Life -= DeltaSeconds;
	if (Life <= 0.f)
	{
		Destroy();
	}
}
