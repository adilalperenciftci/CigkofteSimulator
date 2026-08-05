#include "UI/CigTabletWidget.h"
#include "UI/CigTabletData.h"
#include "Game/CigkofteGameMode.h"
#include "Core/CigText.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/EditableTextBox.h"
#include "World/CigWorldBuilder.h"
#include "Game/CigShopIdentity.h"

void UCigTabletWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildTree();
}

void UCigTabletWidget::BuildTree()
{
	if (!WidgetTree || RootBorder)
	{
		return;
	}

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TabletRoot"));
	RootBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.96f));
	RootBorder->SetPadding(FMargin(24.f));
	WidgetTree->RootWidget = RootBorder;

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TabletBox"));
	RootBorder->SetContent(RootBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TabletTitle"));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.35f)));
	RootBox->AddChildToVerticalBox(TitleText);

	// The shop's name, at the top where a sign would be.
	ShopNameBox = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(), TEXT("TabletShopName"));
	// The rules refuse anything longer anyway; stopping the keystroke is kinder
	// than accepting it and rejecting the result.
	ShopNameBox->SetJustification(ETextJustify::Left);
	ShopNameBox->OnTextCommitted.AddDynamic(this, &UCigTabletWidget::HandleShopNameCommitted);
	RootBox->AddChildToVerticalBox(ShopNameBox);

	TabBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TabletTabs"));
	RootBox->AddChildToVerticalBox(TabBar);

	RowList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TabletRows"));
	// The engine handles scrolling. On Canvas this was a hand-written TabletScroll
	// counter that broke whenever the row height changed.
	if (UVerticalBoxSlot* ListSlot = RootBox->AddChildToVerticalBox(RowList))
	{
		ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UCigTabletWidget::RebuildTabBar(ACigkofteGameMode* GM)
{
	if (!TabBar || !WidgetTree)
	{
		return;
	}
	TabBar->ClearChildren();

	const int32 Active = (int32)GM->TabletTab;
	for (int32 i = 0; i < (int32)ECigTabletTab::COUNT; ++i)
	{
		UTextBlock* Tab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Tab->SetText(FText::FromString(CigTablet::TabName((ECigTabletTab)i)));
		Tab->SetColorAndOpacity(FSlateColor(i == Active
			? FLinearColor(1.f, 0.85f, 0.35f)
			: FLinearColor(0.55f, 0.55f, 0.6f)));
		if (UHorizontalBoxSlot* TabSlot = TabBar->AddChildToHorizontalBox(Tab))
		{
			TabSlot->SetPadding(FMargin(0.f, 6.f, 18.f, 10.f));
		}
	}
}

void UCigTabletWidget::RebuildRows(const TArray<FCigTabletRow>& Rows)
{
	if (!RowList || !WidgetTree)
	{
		return;
	}
	RowList->ClearChildren();

	for (const FCigTabletRow& R : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UTextBlock* Left = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Left->SetText(FText::FromString(R.Left));
		Left->SetColorAndOpacity(FSlateColor(R.Color));
		if (UHorizontalBoxSlot* LeftSlot = Line->AddChildToHorizontalBox(Left))
		{
			// The left column stretches and the right keeps its own width, so a
			// longer translation cannot shift the value column.
			LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		if (!R.Right.IsEmpty())
		{
			UTextBlock* Right = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Right->SetText(FText::FromString(R.Right));
			Right->SetColorAndOpacity(FSlateColor(R.Color));
			Line->AddChildToHorizontalBox(Right);
		}

		if (R.HasBar())
		{
			// On Canvas this was a hand-drawn rectangle with a width fixed in
			// pixels; UProgressBar takes its share from the layout itself.
			UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
			Bar->SetPercent(FMath::Clamp(R.BarFrac, 0.f, 1.f));
			Bar->SetFillColorAndOpacity(R.BarColor);
			if (UHorizontalBoxSlot* BarSlot = Line->AddChildToHorizontalBox(Bar))
			{
				BarSlot->SetPadding(FMargin(12.f, 4.f, 0.f, 4.f));
				BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}

		RowList->AddChild(Line);
	}
}

void UCigTabletWidget::HandleShopNameCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	ACigkofteGameMode* GM = OwningMode.Get();
	if (!GM || !GM->WorldBuilder)
	{
		return;
	}

	// Enter or a click away both end the entry; escape puts the old name back
	// rather than applying whatever was half-typed.
	if (CommitMethod != ETextCommit::OnCleared)
	{
		const ECigShopNameFault Fault = GM->WorldBuilder->SetShopName(Text.ToString());
		if (Fault != ECigShopNameFault::None)
		{
			GM->AddMessage(CigShopIdentity::FaultText(Fault), FLinearColor(1.f, 0.5f, 0.4f));
		}
	}

	// Whatever happened, the box shows what the shop is actually called. A refused
	// name left in the field would read as accepted.
	if (ShopNameBox)
	{
		ShopNameBox->SetText(FText::FromString(GM->WorldBuilder->ShopDisplayName()));
	}
	GM->EndTextEntry();
}

void UCigTabletWidget::FocusShopName()
{
	if (ACigkofteGameMode* GM = OwningMode.Get())
	{
		GM->BeginTextEntry(ShopNameBox);
	}
}

void UCigTabletWidget::RefreshFrom(ACigkofteGameMode* GM)
{
	if (!GM)
	{
		return;
	}
	BuildTree();
	OwningMode = GM;

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(CigText::Get(TEXT("tablet.title"))));
	}

	// Not while it is being typed into: a refresh mid-edit would replace the
	// player's half-written name with what the shop is still called.
	if (ShopNameBox && !GM->bTextEntryActive && GM->WorldBuilder)
	{
		ShopNameBox->SetText(FText::FromString(GM->WorldBuilder->ShopDisplayName()));
	}
	RebuildTabBar(GM);
	RebuildRows(CigTablet::BuildRows(GM, GM->TabletTab));
	LastTab = (int32)GM->TabletTab;
}
