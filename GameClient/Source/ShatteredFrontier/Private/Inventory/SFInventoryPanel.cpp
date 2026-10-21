#include "Inventory/SFInventoryPanel.h"

#include "Gathering/SFGatheringPilot.h"
#include "Framework/SFHudStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void USFInventoryPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;

    UBorder* Frame = Panel(WidgetTree, Slate(0.92f), Brass(), FMargin(24.f, 20.f), 6.f);
    WidgetTree->RootWidget = Frame;
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Frame->SetContent(Contents);
    auto AddRow = [Contents](UWidget* Child, const FMargin& Pad) { Contents->AddChildToVerticalBox(Child)->SetPadding(Pad); };

    // Title row: brass diamond stand-in for the stag crest, serif-style heading, close hint.
    UHorizontalBox* Title = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    USizeBox* CrestBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    CrestBox->SetWidthOverride(22.f); CrestBox->SetHeightOverride(22.f);
    UBorder* Crest = Panel(WidgetTree, Hex(TEXT("0E171C"), 1.f), Brass(), FMargin(0.f), 3.f);
    Crest->SetRenderTransformAngle(45.f); CrestBox->SetContent(Crest);
    UHorizontalBoxSlot* CrestSlot = Title->AddChildToHorizontalBox(CrestBox);
    CrestSlot->SetVerticalAlignment(VAlign_Center); CrestSlot->SetPadding(FMargin(4.f, 0.f, 16.f, 0.f));
    Title->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Inventory"), 28, Ivory(), true))->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* HintSlot = Title->AddChildToHorizontalBox(Text(WidgetTree, TEXT("I to close"), 12, Hex(TEXT("A9A593"))));
    HintSlot->SetHorizontalAlignment(HAlign_Right); HintSlot->SetVerticalAlignment(VAlign_Center); HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    AddRow(Title, FMargin(0.f, 0.f, 0.f, 10.f));

    // One "All" tab; category tabs need item categories the pilot items do not have yet.
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UBorder* AllTab = Panel(WidgetTree, Hex(TEXT("2B3B2C"), 1.f), Brass(), FMargin(22.f, 6.f), 3.f);
    AllTab->SetContent(Text(WidgetTree, TEXT("All"), 14, Ivory(), true));
    Tabs->AddChildToHorizontalBox(AllTab);
    AddRow(Tabs, FMargin(0.f, 0.f, 0.f, 12.f));

    // 5 x 5 slot grid. ponytail: fixed 25 cells, no scrolling; pilot capacity is 8 slots, raise rows if that grows.
    UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    Grid->SetSlotPadding(FMargin(3.f));
    for (int32 Index = 0; Index < GridColumns * GridRows; ++Index)
    {
        USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Cell->SetWidthOverride(78.f); Cell->SetHeightOverride(78.f);
        Cell->SetContent(Panel(WidgetTree, Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), FMargin(4.f), 3.f));
        UBorder* CellFrame = Cast<UBorder>(Cell->GetContent());
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        CellFrame->SetContent(Stack);
        UTextBlock* Name = Text(WidgetTree, TEXT(""), 11, Ivory());
        Name->SetAutoWrapText(true); Name->SetJustification(ETextJustify::Center);
        UOverlaySlot* NameSlot = Stack->AddChildToOverlay(Name);
        NameSlot->SetHorizontalAlignment(HAlign_Fill); NameSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Count = Text(WidgetTree, TEXT(""), 15, Ivory(), true);
        UOverlaySlot* CountSlot = Stack->AddChildToOverlay(Count);
        CountSlot->SetHorizontalAlignment(HAlign_Right); CountSlot->SetVerticalAlignment(VAlign_Bottom);
        SlotNames.Add(Name); SlotCounts.Add(Count);
        Grid->AddChildToUniformGrid(Cell, Index / GridColumns, Index % GridColumns);
    }
    AddRow(Grid, FMargin(0.f, 0.f, 0.f, 6.f));
    EmptyText = Text(WidgetTree, TEXT("Your pack is empty."), 13, Hex(TEXT("A9A593")));
    AddRow(EmptyText, FMargin(4.f, 0.f, 0.f, 6.f));

    // Footer: weight readout, moss bar and percentage.
    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    WeightText = Text(WidgetTree, TEXT(""), 14, Ivory());
    UHorizontalBoxSlot* WeightSlot = Footer->AddChildToHorizontalBox(WeightText);
    WeightSlot->SetVerticalAlignment(VAlign_Center); WeightSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
    USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    BarBox->SetHeightOverride(10.f);
    WeightBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    StyleBar(WeightBar, Moss());
    BarBox->SetContent(WeightBar);
    UHorizontalBoxSlot* BarSlot = Footer->AddChildToHorizontalBox(BarBox);
    BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); BarSlot->SetVerticalAlignment(VAlign_Center);
    WeightPercent = Text(WidgetTree, TEXT(""), 14, Ivory());
    UHorizontalBoxSlot* PercentSlot = Footer->AddChildToHorizontalBox(WeightPercent);
    PercentSlot->SetVerticalAlignment(VAlign_Center); PercentSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
    AddRow(Footer, FMargin(0.f, 6.f, 0.f, 0.f));
    Refresh();
}

void USFInventoryPanel::Configure(USFGatheringPilotComponent* InPilot)
{
    Pilot=InPilot;
    Refresh();
}

void USFInventoryPanel::NativeTick(const FGeometry& MyGeometry,float InDeltaTime)
{
    Super::NativeTick(MyGeometry,InDeltaTime);
    Refresh();
}

TArray<TPair<FText,int32>> USFInventoryPanel::BuildRows(const TArray<TPair<FText,int32>>& Items)
{
    TMap<FString,int32> Counts;
    for (const TPair<FText,int32>& Item : Items)
    {
        if (Item.Value>0 && !Item.Key.IsEmpty()) { Counts.FindOrAdd(Item.Key.ToString())+=Item.Value; }
    }
    Counts.KeySort([](const FString& A,const FString& B) { return A<B; });
    TArray<TPair<FText,int32>> Out;
    for (const TPair<FString,int32>& Count : Counts) { Out.Emplace(FText::FromString(Count.Key),Count.Value); }
    return Out;
}

FString USFInventoryPanel::FormatRows(const TArray<TPair<FText,int32>>& InRows)
{
    if (InRows.IsEmpty()) { return TEXT("Empty"); }
    TArray<FString> Lines;
    for (const TPair<FText,int32>& Row : InRows) { Lines.Add(FString::Printf(TEXT("%s  x%d"),*Row.Key.ToString(),Row.Value)); }
    return FString::Join(Lines,TEXT("\n"));
}

void USFInventoryPanel::Refresh()
{
    if (!Pilot || SlotNames.Num() != GridColumns * GridRows || GetVisibility()==ESlateVisibility::Collapsed) { return; }
    const TArray<TPair<FText,int32>> Items = BuildRows(Pilot->GetInventoryItems());
    for (int32 Index = 0; Index < SlotNames.Num(); ++Index)
    {
        const bool bFilled = Items.IsValidIndex(Index);
        SlotNames[Index]->SetText(bFilled ? Items[Index].Key : FText::GetEmpty());
        SlotCounts[Index]->SetText(bFilled ? FText::AsNumber(Items[Index].Value) : FText::GetEmpty());
    }
    EmptyText->SetVisibility(Items.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    float Total = 0.f, Max = 0.f;
    if (Pilot->GetInventoryWeight(Total, Max))
    {
        const float Fraction = Max > 0.f ? FMath::Clamp(Total / Max, 0.f, 1.f) : 0.f;
        WeightText->SetText(FText::FromString(Max > 0.f ? FString::Printf(TEXT("Weight  %.1f / %.0f kg"), Total, Max) : FString::Printf(TEXT("Weight  %.1f kg"), Total)));
        WeightBar->SetPercent(Fraction);
        WeightPercent->SetText(FText::FromString(Max > 0.f ? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Fraction * 100.f)) : FString()));
    }
}
