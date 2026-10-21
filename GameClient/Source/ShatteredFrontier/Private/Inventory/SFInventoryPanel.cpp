#include "Inventory/SFInventoryPanel.h"

#include "Gathering/SFGatheringPilot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void USFInventoryPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);

    UBorder* Panel=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("InventoryPanel"));
    Panel->SetPadding(FMargin(16.f));
    Panel->SetBrushColor(FLinearColor(0.035f,0.045f,0.055f,0.92f));
    UVerticalBox* Contents=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->SetContent(Contents);
    WidgetTree->RootWidget=Panel;

    auto AddText=[this,Contents](const FString& Text,float Size,const FLinearColor& Color)
    {
        UTextBlock* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text)); Label->SetFontSize(Size); Label->SetColorAndOpacity(FSlateColor(Color)); Label->SetAutoWrapText(true);
        Contents->AddChildToVerticalBox(Label)->SetPadding(FMargin(2.f,3.f)); return Label;
    };
    AddText(TEXT("INVENTORY"),20.f,FLinearColor(0.9f,0.78f,0.48f,1.f));
    AddText(TEXT("Session items | I closes"),12.f,FLinearColor(0.72f,0.76f,0.8f,1.f));
    Rows=AddText(TEXT(""),14.f,FLinearColor(0.92f,0.93f,0.95f,1.f));
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
    if (!Rows || !Pilot || GetVisibility()==ESlateVisibility::Collapsed) { return; }
    Rows->SetText(FText::FromString(FormatRows(BuildRows(Pilot->GetInventoryItems()))));
}
