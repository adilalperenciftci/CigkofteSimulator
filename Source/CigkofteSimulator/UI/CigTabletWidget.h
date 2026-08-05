#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CigTabletWidget.generated.h"

class UVerticalBox;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UBorder;
class ACigkofteGameMode;

// One row of the tablet UI. Tabs no longer draw directly - they produce a list
// of these, and whoever does the drawing reads the same data. Separating data
// from drawing was the precondition for moving to UMG.
struct FCigTabletRow
{
	FString Left;                    // main label
	FString Right;                   // value / price / status
	FLinearColor Color = FLinearColor::White;
	bool bHeader = false;            // is this a section heading
	bool bSelectable = false;        // can it be picked with a number key

	// A ratio bar on the right of the row (relationship, popularity, morale...).
	// Negative = no bar. Needed for parity with the Canvas version.
	float BarFrac = -1.f;
	FLinearColor BarColor = FLinearColor(0.4f, 0.8f, 0.5f);

	bool HasBar() const { return BarFrac >= 0.f; }
};

// The tablet's UMG implementation.
//
// Why UMG: the tablet is the screen with the most text, the most lists and the
// most layout. On Canvas every alignment was computed by hand; a longer
// translation overflowed it, different aspect ratios broke it, and scrolling
// had to be written manually. UScrollBox gets all of that from the engine.
//
// The widget tree is built entirely in C++ - no .uasset. That keeps the layout
// diffable in git and editable without opening the editor.
//
// This is the tablet's only draw path. The Canvas version was removed once all
// ten tabs had moved to data (FCigTabletRow) - maintaining two separate draw
// paths would have cost more than the move gained.
UCLASS()
class UCigTabletWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Refreshes the tab headers and rows. Called when the content changes -
	// opening the tablet, switching tabs, making a purchase - not every frame.
	void RefreshFrom(ACigkofteGameMode* GM);

	// Hands the keyboard to the shop-name field.
	//
	// Driven by a key rather than a click, and it has to be: the game runs with
	// the cursor hidden, so until something calls this there is no way to reach
	// the field at all.
	void FocusShopName();

protected:
	virtual void NativeOnInitialized() override;

private:
	// The shop's name, editable.
	//
	// The only field in the game, and it needs the game to stop reading keys while
	// it has focus - input is polled from raw key state and does not care about
	// focus, so without the gate typing a name also walks the player. GameMode
	// owns that pairing (BeginTextEntry/EndTextEntry); this widget only says when.
	UFUNCTION() void HandleShopNameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY() TObjectPtr<class UEditableTextBox> ShopNameBox;
	// Set on every refresh, so the committed name reaches the shop that drew it.
	TWeakObjectPtr<ACigkofteGameMode> OwningMode;

	// The tree built in C++.
	UPROPERTY() TObjectPtr<UBorder> RootBorder;
	UPROPERTY() TObjectPtr<UVerticalBox> RootBox;
	UPROPERTY() TObjectPtr<UHorizontalBox> TabBar;
	UPROPERTY() TObjectPtr<UScrollBox> RowList;
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;

	// The last tab drawn; avoids rebuilding unnecessarily.
	int32 LastTab = -1;

	void BuildTree();
	void RebuildTabBar(ACigkofteGameMode* GM);
	void RebuildRows(const TArray<FCigTabletRow>& Rows);
};
