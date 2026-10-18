#include "Construction/SFConstructionSandboxPalette.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"

USFConstructionPaletteButton::USFConstructionPaletteButton(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{ InitIsFocusable(false); }

void USFConstructionSandboxPalette::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    SetIsFocusable(false);
    CachedStatusText = FText::FromString(TEXT("Ready. Select a shape and placement mode."));

    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PaletteContents"));
    PalettePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PalettePanel"));
    PalettePanel->SetPadding(FMargin(8.0f));
    PalettePanel->SetBrushColor(FLinearColor(0.035f, 0.045f, 0.055f, 0.94f));
    PalettePanel->SetVisibility(ESlateVisibility::Visible);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("PaletteScroll"));
    Scroll->AddChild(Contents); PalettePanel->SetContent(Scroll);
    WidgetTree->RootWidget = PalettePanel;

    UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PaletteHeading"));
    Heading->SetText(FText::FromString(TEXT("CONSTRUCTION | DEV")));
    Heading->SetFontSize(18.0f);
    Heading->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.78f, 0.48f, 1.0f)));
    Heading->SetAutoWrapText(true);
    Contents->AddChildToVerticalBox(Heading)->SetPadding(FMargin(2.0f, 1.0f, 2.0f, 5.0f));

    auto AddActionRow = [this, Contents](std::initializer_list<TPair<FString, FName>> Actions)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Contents->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));
        for (const TPair<FString, FName>& Action : Actions)
        {
            UButton* Button = WidgetTree->ConstructWidget<USFConstructionPaletteButton>(USFConstructionPaletteButton::StaticClass());
            UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            Label->SetText(FText::FromString(Action.Key));
            Label->SetFontSize(13.0f);
            Label->SetJustification(ETextJustify::Center);
            Label->SetAutoWrapText(true);
            Button->AddChild(Label);
            FScriptDelegate ClickHandler;
            ClickHandler.BindUFunction(this, Action.Value);
            Button->OnClicked.Add(ClickHandler);
            Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(2.0f));
        }
    };

    AddActionRow({{TEXT("Cube"), TEXT("HandleCube")}, {TEXT("Slab"), TEXT("HandleSlab")},
        {TEXT("Slope"), TEXT("HandleSlope")}, {TEXT("Door"), TEXT("HandleDoor")}});
    AddActionRow({{TEXT("Single"), TEXT("HandleSingle")}, {TEXT("Line"), TEXT("HandleLine")},
        {TEXT("Demolish"), TEXT("HandleDemolish")}});
    AddActionRow({{TEXT("Rotate"), TEXT("HandleRotate")}, {TEXT("Surface"), TEXT("HandleSurface")},
        {TEXT("Layer"), TEXT("HandleLayer")}});
    AddActionRow({{TEXT("Layer -"), TEXT("HandleLayerDown")}, {TEXT("Layer +"), TEXT("HandleLayerUp")}});
    AddActionRow({{TEXT("Confirm"), TEXT("HandleConfirm")}, {TEXT("Cancel"), TEXT("HandleCancel")}});
    AddActionRow({{TEXT("F5  Save"), TEXT("HandleSave")}, {TEXT("F9  Load"), TEXT("HandleLoad")}});

    StatusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PaletteStatus"));
    StatusTextBlock->SetText(CachedStatusText);
    StatusTextBlock->SetFontSize(13.0f);
    StatusTextBlock->SetAutoWrapText(true);
    Contents->AddChildToVerticalBox(StatusTextBlock)->SetPadding(FMargin(3.0f, 7.0f, 3.0f, 3.0f));

    UTextBlock* Hints = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PaletteHints"));
    Hints->SetText(FText::FromString(TEXT("Unlimited dev stock | isolated plot\nRMB hold: look in either mode\nDemolish: LMB delete\nX / Delete: mode | R rotate | Esc cancel")));
    Hints->SetFontSize(12.0f);
    Hints->SetAutoWrapText(true);
    Contents->AddChildToVerticalBox(Hints)->SetPadding(FMargin(3.0f, 2.0f, 3.0f, 1.0f));
}

void USFConstructionSandboxPalette::SetStatusText(const FText& InStatusText)
{
    CachedStatusText = InStatusText;
    if (StatusTextBlock)
    {
        StatusTextBlock->SetText(CachedStatusText);
    }
}

bool USFConstructionSandboxPalette::IsPointerOverPalette() const
{
    return PalettePanel
        && PalettePanel->GetVisibility() == ESlateVisibility::Visible
        && PalettePanel->IsHovered();
}

FReply USFConstructionSandboxPalette::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    return FReply::Handled();
}

FReply USFConstructionSandboxPalette::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    return FReply::Handled();
}

void USFConstructionSandboxPalette::Dispatch(ESFConstructionPaletteAction Action)
{
    OnAction.ExecuteIfBound(Action);
}

void USFConstructionSandboxPalette::HandleCube() { Dispatch(ESFConstructionPaletteAction::Cube); }
void USFConstructionSandboxPalette::HandleSlab() { Dispatch(ESFConstructionPaletteAction::Slab); }
void USFConstructionSandboxPalette::HandleSlope() { Dispatch(ESFConstructionPaletteAction::Slope); }
void USFConstructionSandboxPalette::HandleDoor() { Dispatch(ESFConstructionPaletteAction::Door); }
void USFConstructionSandboxPalette::HandleSingle() { Dispatch(ESFConstructionPaletteAction::Single); }
void USFConstructionSandboxPalette::HandleLine() { Dispatch(ESFConstructionPaletteAction::Line); }
void USFConstructionSandboxPalette::HandleDemolish() { Dispatch(ESFConstructionPaletteAction::Demolish); }
void USFConstructionSandboxPalette::HandleRotate() { Dispatch(ESFConstructionPaletteAction::Rotate); }
void USFConstructionSandboxPalette::HandleSurface() { Dispatch(ESFConstructionPaletteAction::Surface); }
void USFConstructionSandboxPalette::HandleLayer() { Dispatch(ESFConstructionPaletteAction::Layer); }
void USFConstructionSandboxPalette::HandleLayerDown() { Dispatch(ESFConstructionPaletteAction::LayerDown); }
void USFConstructionSandboxPalette::HandleLayerUp() { Dispatch(ESFConstructionPaletteAction::LayerUp); }
void USFConstructionSandboxPalette::HandleConfirm() { Dispatch(ESFConstructionPaletteAction::Confirm); }
void USFConstructionSandboxPalette::HandleCancel() { Dispatch(ESFConstructionPaletteAction::Cancel); }
void USFConstructionSandboxPalette::HandleSave() { Dispatch(ESFConstructionPaletteAction::Save); }
void USFConstructionSandboxPalette::HandleLoad() { Dispatch(ESFConstructionPaletteAction::Load); }
