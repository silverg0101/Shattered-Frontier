#include "Construction/SFConstructionSandboxPalette.h"

#include "Framework/SFHudStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"

USFConstructionPaletteButton::USFConstructionPaletteButton(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{ InitIsFocusable(false); }

namespace
{
// Flat HUD-v1 button: slate fill with a brass edge on hover, so tiles and rows share one look.
void StyleButton(UButton* Button, const FLinearColor& Fill, const FLinearColor& HoverEdge)
{
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = SFHudStyle::KitOrFrame(TEXT("button_normal"), Fill, SFHudStyle::Hex(TEXT("3A4348")), 3.f);
    Style.Hovered = SFHudStyle::KitOrFrame(TEXT("button_hover"), Fill, HoverEdge, 3.f);
    Style.Pressed = SFHudStyle::KitOrFrame(TEXT("button_pressed"), SFHudStyle::Hex(TEXT("2B3B2C"), 1.f), HoverEdge, 3.f);
    Style.NormalPadding = FMargin(0.f); Style.PressedPadding = FMargin(0.f);
    Button->SetStyle(Style);
}
}

int32 USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction Action)
{
    switch (Action)
    {
    case ESFConstructionPaletteAction::Slab: return 0;
    case ESFConstructionPaletteAction::Cube: return 1;
    case ESFConstructionPaletteAction::Door: return 2;
    case ESFConstructionPaletteAction::Slope: return 3;
    default: return INDEX_NONE;
    }
}

const TCHAR* USFConstructionSandboxPalette::TileLabel(int32 Tile)
{
    static const TCHAR* const Labels[TileCount] = {TEXT("Foundation"), TEXT("Wall"), TEXT("Door"), TEXT("Roof")};
    return Labels[FMath::Clamp(Tile, 0, TileCount - 1)];
}

void USFConstructionSandboxPalette::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    using namespace SFHudStyle;

    SetIsFocusable(false);
    CachedStatusText = FText::FromString(TEXT("Ready. Select a piece and placement mode."));

    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PaletteContents"));
    PalettePanel = Panel(WidgetTree, Slate(0.92f), Brass(), FMargin(16.f, 14.f), 6.f);
    PalettePanel->SetVisibility(ESlateVisibility::Visible);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("PaletteScroll"));
    Scroll->AddChild(Contents); PalettePanel->SetContent(Scroll);
    WidgetTree->RootWidget = PalettePanel;

    UTextBlock* Heading = SFHudStyle::Heading(WidgetTree, TEXT("Building"), 24, Ivory());
    Contents->AddChildToVerticalBox(Heading)->SetPadding(FMargin(2.f, 0.f, 2.f, 2.f));
    Contents->AddChildToVerticalBox(Text(WidgetTree, TEXT("Development palette"), 12, Hex(TEXT("A9A593"))))->SetPadding(FMargin(2.f, 0.f, 2.f, 10.f));

    auto AddLabel = [this](UButton* Button, const FString& Text)
    {
        UTextBlock* Label = SFHudStyle::Text(WidgetTree, Text, 13, SFHudStyle::Ivory());
        Label->SetJustification(ETextJustify::Center);
        UBorder* Pad = SFHudStyle::Panel(WidgetTree, FLinearColor::Transparent, FLinearColor::Transparent, FMargin(8.f, 5.f), 0.f);
        Pad->SetContent(Label); Button->SetContent(Pad);
    };
    auto Bind = [this](UButton* Button, const FName& Handler)
    {
        FScriptDelegate ClickHandler; ClickHandler.BindUFunction(this, Handler); Button->OnClicked.Add(ClickHandler);
    };

    // Catalogue: four tiles, icon above label. Slab = Foundation, Cube = Wall, Slope = Roof.
    static const TCHAR* const TileHandlers[TileCount] = {TEXT("HandleSlab"), TEXT("HandleCube"), TEXT("HandleDoor"), TEXT("HandleSlope")};
    static const TCHAR* const TileIcons[TileCount] = {TEXT("build_foundation"), TEXT("build_wall"), TEXT("build_door"), TEXT("build_roof")};
    UUniformGridPanel* Catalogue = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    Catalogue->SetSlotPadding(FMargin(3.f));
    for (int32 Tile = 0; Tile < TileCount; ++Tile)
    {
        UButton* Button = WidgetTree->ConstructWidget<USFConstructionPaletteButton>(USFConstructionPaletteButton::StaticClass(), TileHandlers[Tile]);
        StyleButton(Button, Hex(TEXT("0E171C"), 0.85f), Brass());
        UBorder* Frame = Panel(WidgetTree, FLinearColor::Transparent, Hex(TEXT("3A4348")), FMargin(6.f), 3.f);  // selection frame drawn by SelectTile
        UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconBox->SetWidthOverride(64.f); IconBox->SetHeightOverride(64.f);
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Icons/T_Icon_%s.T_Icon_%s"), TileIcons[Tile], TileIcons[Tile])))
        { Icon->SetBrushFromTexture(Texture, false); }
        IconBox->SetContent(Icon);
        Stack->AddChildToVerticalBox(IconBox)->SetHorizontalAlignment(HAlign_Center);
        UTextBlock* Label = Text(WidgetTree, TileLabel(Tile), 13, Ivory(), true);
        Label->SetJustification(ETextJustify::Center);
        UVerticalBoxSlot* LabelSlot = Stack->AddChildToVerticalBox(Label);
        LabelSlot->SetHorizontalAlignment(HAlign_Center); LabelSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
        Frame->SetContent(Stack); Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
        Button->SetContent(Frame);
        Bind(Button, TileHandlers[Tile]);
        TileFrames.Add(Frame);
        Catalogue->AddChildToUniformGrid(Button, Tile / 2, Tile % 2);
    }
    Contents->AddChildToVerticalBox(Catalogue)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    SelectTile(SelectedTile);

    // Placement cost readout. ponytail: no cost model exists yet (dev stock is unlimited); wire real "have / need" rows here when it does.
    UBorder* Cost = Panel(WidgetTree, Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), FMargin(10.f, 6.f), 3.f);
    UHorizontalBox* CostRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    CostRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Cost"), 13, Brass(), true))->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
    CostRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("None - unlimited dev stock"), 13, Ivory()));
    Cost->SetContent(CostRow);
    Contents->AddChildToVerticalBox(Cost)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

    auto AddActionRow = [&](std::initializer_list<TPair<FString, FName>> Actions)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Contents->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
        for (const TPair<FString, FName>& Action : Actions)
        {
            UButton* Button = WidgetTree->ConstructWidget<USFConstructionPaletteButton>(USFConstructionPaletteButton::StaticClass(), Action.Value);
            StyleButton(Button, Hex(TEXT("0E171C"), 0.85f), Brass());
            AddLabel(Button, Action.Key);
            Bind(Button, Action.Value);
            Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(2.f));
        }
    };

    AddActionRow({{TEXT("Single"), TEXT("HandleSingle")}, {TEXT("Line"), TEXT("HandleLine")},
        {TEXT("Demolish"), TEXT("HandleDemolish")}});
    AddActionRow({{TEXT("Rotate"), TEXT("HandleRotate")}, {TEXT("Surface"), TEXT("HandleSurface")},
        {TEXT("Layer"), TEXT("HandleLayer")}});
    AddActionRow({{TEXT("Layer -"), TEXT("HandleLayerDown")}, {TEXT("Layer +"), TEXT("HandleLayerUp")}});
    AddActionRow({{TEXT("Confirm"), TEXT("HandleConfirm")}, {TEXT("Cancel"), TEXT("HandleCancel")}});
    AddActionRow({{TEXT("F5  Save"), TEXT("HandleSave")}, {TEXT("F9  Load"), TEXT("HandleLoad")}});

    StatusTextBlock = Text(WidgetTree, TEXT(""), 13, Ivory());
    StatusTextBlock->SetText(CachedStatusText);
    StatusTextBlock->SetAutoWrapText(true);
    Contents->AddChildToVerticalBox(StatusTextBlock)->SetPadding(FMargin(3.f, 10.f, 3.f, 3.f));

    UTextBlock* Hints = Text(WidgetTree, TEXT(""), 12, Hex(TEXT("A9A593")));
    HintsTextBlock=Hints;
    Hints->SetText(FText::FromString(TEXT("Unlimited dev stock | isolated plot\nRMB hold: look in either mode\nDemolish: LMB delete\nX / Delete: mode | R rotate | Esc cancel")));
    Hints->SetAutoWrapText(true);
    Contents->AddChildToVerticalBox(Hints)->SetPadding(FMargin(3.f, 2.f, 3.f, 1.f));
}

void USFConstructionSandboxPalette::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // Keys 1-4 select Cube, Slab, Slope and Door in the controller; mirror that here so the frame follows the keyboard too.
    if (APlayerController* Controller = GetOwningPlayer())
    {
        if (Controller->WasInputKeyJustPressed(EKeys::One)) { SelectTile(1); }
        else if (Controller->WasInputKeyJustPressed(EKeys::Two)) { SelectTile(0); }
        else if (Controller->WasInputKeyJustPressed(EKeys::Three)) { SelectTile(3); }
        else if (Controller->WasInputKeyJustPressed(EKeys::Four)) { SelectTile(2); }
    }
}

void USFConstructionSandboxPalette::SelectTile(int32 Tile)
{
    SelectedTile = Tile;
    for (int32 Index = 0; Index < TileFrames.Num(); ++Index)
    {
        const bool bSelected = Index == SelectedTile;
        TileFrames[Index]->SetBrush(SFHudStyle::Frame(bSelected ? SFHudStyle::Hex(TEXT("2B3B2C"), 1.f) : FLinearColor::Transparent,
            bSelected ? SFHudStyle::Brass() : SFHudStyle::Hex(TEXT("3A4348")), 3.f));
    }
}

void USFConstructionSandboxPalette::ConfigureNetworkPresentation()
{
    // Presentation capability subset only. The controller retains all actions
    // and authority; the standalone palette keeps its complete default layout.
    for (const FName Name : {FName(TEXT("HandleConfirm")),FName(TEXT("HandleSave")),FName(TEXT("HandleLoad"))})
    { if (auto* Button=WidgetTree->FindWidget(Name)) { Button->SetVisibility(ESlateVisibility::Collapsed); } }
    if (HintsTextBlock)
    { HintsTextBlock->SetText(FText::FromString(TEXT("RMB hold: look | E: door\nX / Delete: demolition mode\nLMB: place or demolish | L: line\nR: rotate | Escape: cancel start\nP: surface/layer | PgUp/PgDn: level\nUnlimited dev stock"))); }
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
    const int32 Tile = TileForAction(Action);
    if (Tile != INDEX_NONE) { SelectTile(Tile); }
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
