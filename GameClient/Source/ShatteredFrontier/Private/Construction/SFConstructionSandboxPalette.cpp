#include "Construction/SFConstructionSandboxPalette.h"

#include "Framework/SFHudStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"

USFConstructionPaletteButton::USFConstructionPaletteButton(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{ InitIsFocusable(false); }

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

// Build Catalogue (docs/Game-Design/concept-art/UI/v5/build-mode.png, IMPLEMENTATION-FIX table 2): a 480 px panel frame,
// title, search, Voxel Objects / Blueprints tabs, category rail beside a 3-column piece grid, material row and required
// materials. The mode and layer actions live on keys (L, X, R, P, PgUp/PgDn); only the four real pieces are populated.
void USFConstructionSandboxPalette::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    using namespace SFHudStyle;

    SetIsFocusable(false);
    CachedStatusText = FText::FromString(TEXT("Valid placement"));
    const FLinearColor Dim = Hex(TEXT("A9A593"));
    const FLinearColor Well = Hex(TEXT("0E171C"), 0.85f);

    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PaletteContents"));
    PalettePanel = KitPanel(WidgetTree, TEXT("panel_frame"), 1.f, Slate(0.94f), Brass(), FMargin(6.f), 6.f);
    PalettePanel->SetVisibility(ESlateVisibility::Visible);
    PalettePanel->SetContent(Contents);
    // The root is a full-viewport canvas (the controller fills the viewport with it) so the panel is placed by percent anchors:
    // 480 px wide, left edge at 2%, top at 4%. The canvas itself is never hit-tested, so clicks outside the panel reach the world.
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PanelSize->SetWidthOverride(480.f); PanelSize->SetContent(PalettePanel);
    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelSize);
    PanelSlot->SetAnchors(FAnchors(0.02f, 0.04f)); PanelSlot->SetAlignment(FVector2D(0.f, 0.f)); PanelSlot->SetAutoSize(true); PanelSlot->SetOffsets(FMargin(0.f));

    // Title row, with the 2 px rule under it.
    UHorizontalBox* Title = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Title->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("icon_structures"), 28.f))->SetVerticalAlignment(VAlign_Center);
    Title->AddChildToHorizontalBox(Heading(WidgetTree, TEXT("Build Catalogue"), 36, Ivory()))->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    Contents->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    USizeBox* Rule = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Rule->SetHeightOverride(2.f); Rule->SetContent(KitPanel(WidgetTree, TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f));
    Contents->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

    // Search: filters the piece grid by name.
    UEditableTextBox* Search = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
    Search->SetHintText(FText::FromString(TEXT("Search objects")));
    FEditableTextBoxStyle SearchStyle = Search->WidgetStyle;
    const FSlateBrush Field = KitOrFrame(TEXT("search_field"), Well, Hex(TEXT("3A4348")), 3.f);
    SearchStyle.BackgroundImageNormal = Field; SearchStyle.BackgroundImageHovered = Field; SearchStyle.BackgroundImageFocused = Field; SearchStyle.BackgroundImageReadOnly = Field;
    SearchStyle.ForegroundColor = FSlateColor(Ivory()); SearchStyle.FocusedForegroundColor = FSlateColor(Ivory());
    SearchStyle.TextStyle.Font = RoleFont(TextFamily(), 18, false);
    SearchStyle.Padding = FMargin(44.f, 4.f, 12.f, 4.f);
    Search->WidgetStyle = SearchStyle;
    FScriptDelegate SearchChanged; SearchChanged.BindUFunction(this, TEXT("HandleSearchChanged")); Search->OnTextChanged.Add(SearchChanged);
    UOverlay* SearchStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    USizeBox* SearchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SearchSize->SetHeightOverride(40.f); SearchSize->SetContent(Search);
    UOverlaySlot* SearchFill = SearchStack->AddChildToOverlay(SearchSize);
    SearchFill->SetHorizontalAlignment(HAlign_Fill); SearchFill->SetVerticalAlignment(VAlign_Fill);
    UWidget* SearchGlyph = KitImage(WidgetTree, TEXT("icon_search"), 20.f);
    SearchGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
    UOverlaySlot* GlyphSlot = SearchStack->AddChildToOverlay(SearchGlyph);
    GlyphSlot->SetHorizontalAlignment(HAlign_Left); GlyphSlot->SetVerticalAlignment(VAlign_Center); GlyphSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
    Contents->AddChildToVerticalBox(SearchStack)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

    // Voxel Objects / Blueprints tabs (Blueprints stays disabled until blueprints exist).
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    for (int32 Index = 0; Index < 2; ++Index)
    {
        UBorder* Tab = KitPanel(WidgetTree, Index == 0 ? TEXT("tab_selected") : TEXT("tab_normal"), 1.f, Well, Hex(TEXT("3A4348")), FMargin(4.f, 0.f), 3.f);
        UTextBlock* Label = Text(WidgetTree, Index == 0 ? TEXT("Voxel Objects") : TEXT("Blueprints"), 20, Index == 0 ? Ivory() : Dim, Index == 0);
        Label->SetJustification(ETextJustify::Center);
        Tab->SetVerticalAlignment(VAlign_Center); Tab->SetContent(Label); Tab->SetVisibility(ESlateVisibility::HitTestInvisible);
        USizeBox* TabSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        TabSize->SetWidthOverride(210.f); TabSize->SetHeightOverride(44.f); TabSize->SetContent(Tab);
        Tabs->AddChildToHorizontalBox(TabSize)->SetPadding(FMargin(Index == 0 ? 0.f : 12.f, 0.f, 0.f, 0.f));
    }
    Contents->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    auto Bind = [this](UButton* Button, const FName& Handler)
    {
        FScriptDelegate ClickHandler; ClickHandler.BindUFunction(this, Handler); Button->OnClicked.Add(ClickHandler);
    };

    // Body: category rail (80 px tiles, Structures is the only populated category) beside the piece grid.
    UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBox* Rail = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    static const TCHAR* const RailIcons[4] = {TEXT("icon_structures"), TEXT("icon_decor"), TEXT("icon_terrain"), TEXT("icon_favourites")};
    static const TCHAR* const RailLabels[4] = {TEXT("Structures"), TEXT("Decor"), TEXT("Terrain"), TEXT("Favourites")};
    for (int32 Category = 0; Category < 4; ++Category)
    {
        UBorder* Entry = KitPanel(WidgetTree, Category == 0 ? TEXT("tab_selected") : TEXT("tab_normal"), 1.f, Well, Hex(TEXT("3A4348")), FMargin(0.f), 3.f);
        UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Stack->AddChildToVerticalBox(KitImage(WidgetTree, RailIcons[Category], 36.f))->SetHorizontalAlignment(HAlign_Center);
        UTextBlock* Label = Text(WidgetTree, RailLabels[Category], 16, Category == 0 ? Ivory() : Dim);
        Label->SetJustification(ETextJustify::Center);
        Stack->AddChildToVerticalBox(Label)->SetHorizontalAlignment(HAlign_Center);
        Entry->SetVerticalAlignment(VAlign_Center); Entry->SetContent(Stack); Entry->SetVisibility(ESlateVisibility::HitTestInvisible);
        USizeBox* RailBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RailBox->SetWidthOverride(80.f); RailBox->SetHeightOverride(80.f); RailBox->SetContent(Entry);
        Rail->AddChildToVerticalBox(RailBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    }
    Body->AddChildToHorizontalBox(Rail)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));

    // Piece grid: 3 columns of 100 px slots, 12 px gap, thumbnail above the caption. Slab = Foundation, Cube = Wall, Slope = Roof.
    static const TCHAR* const TileHandlers[TileCount] = {TEXT("HandleSlab"), TEXT("HandleCube"), TEXT("HandleDoor"), TEXT("HandleSlope")};
    static const TCHAR* const TileIcons[TileCount] = {TEXT("icon_timber_floor"), TEXT("icon_timber_wall"), TEXT("icon_wooden_door"), TEXT("icon_timber_roof")};
    UUniformGridPanel* Catalogue = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    Catalogue->SetSlotPadding(FMargin(6.f)); Catalogue->SetMinDesiredSlotWidth(100.f); Catalogue->SetMinDesiredSlotHeight(100.f);
    for (int32 Tile = 0; Tile < TileCount; ++Tile)
    {
        UButton* Button = WidgetTree->ConstructWidget<USFConstructionPaletteButton>(USFConstructionPaletteButton::StaticClass(), TileHandlers[Tile]);
        FButtonStyle Flat = Button->GetStyle();
        Flat.Normal = Flat.Hovered = Flat.Pressed = FSlateNoResource();
        Flat.NormalPadding = FMargin(0.f); Flat.PressedPadding = FMargin(0.f);
        Button->SetStyle(Flat);
        UBorder* Frame = KitPanel(WidgetTree, TEXT("slot_catalogue_normal"), 1.f, Well, Hex(TEXT("3A4348")), FMargin(0.f), 3.f);  // selection state drawn by SelectTile
        UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Stack->AddChildToVerticalBox(KitImage(WidgetTree, TileIcons[Tile], 70.f))->SetHorizontalAlignment(HAlign_Center);
        UTextBlock* Label = Text(WidgetTree, TileLabel(Tile), 16, Ivory());
        Label->SetJustification(ETextJustify::Center);
        Stack->AddChildToVerticalBox(Label)->SetHorizontalAlignment(HAlign_Center);
        Frame->SetVerticalAlignment(VAlign_Center); Frame->SetHorizontalAlignment(HAlign_Center); Frame->SetContent(Stack); Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
        Button->SetContent(Frame);
        Bind(Button, TileHandlers[Tile]);
        TileFrames.Add(Frame); TileButtons.Add(Button);
        USizeBox* TileBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());  // fixed 100 px cell so labels never widen a column
        TileBox->SetWidthOverride(100.f); TileBox->SetHeightOverride(100.f); TileBox->SetContent(Button);
        Catalogue->AddChildToUniformGrid(TileBox, Tile / 3, Tile % 3);
    }
    // The other cells of the 3 x 4 grid are empty slots until more pieces exist.
    for (int32 Cell = TileCount; Cell < 12; ++Cell)
    {
        USizeBox* EmptyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        EmptyBox->SetWidthOverride(100.f); EmptyBox->SetHeightOverride(100.f);
        EmptyBox->SetContent(KitPanel(WidgetTree, TEXT("slot_catalogue_normal"), 0.6f, Well, Hex(TEXT("3A4348")), FMargin(0.f), 3.f));
        Catalogue->AddChildToUniformGrid(EmptyBox, Cell / 3, Cell % 3);
    }
    Body->AddChildToHorizontalBox(Catalogue)->SetVerticalAlignment(VAlign_Top);
    Contents->AddChildToVerticalBox(Body)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    SelectTile(SelectedTile);

    // Material row. ponytail: one material until a materials model exists.
    UBorder* Material = KitPanel(WidgetTree, TEXT("dropdown_field"), 1.f, Well, Hex(TEXT("3A4348")), FMargin(8.f, 0.f), 3.f);
    UHorizontalBox* MaterialRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    MaterialRow->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("icon_timber_board"), 28.f))->SetVerticalAlignment(VAlign_Center);
    MaterialRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Material:"), 18, Brass()))->SetPadding(FMargin(12.f, 0.f, 8.f, 0.f));
    UHorizontalBoxSlot* NameSlot = MaterialRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Rough Timber"), 18, Ivory()));
    NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    MaterialRow->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("row_chevron"), 18.f))->SetVerticalAlignment(VAlign_Center);
    Material->SetVerticalAlignment(VAlign_Center); Material->SetContent(MaterialRow); Material->SetVisibility(ESlateVisibility::HitTestInvisible);
    USizeBox* MaterialSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    MaterialSize->SetHeightOverride(40.f); MaterialSize->SetContent(Material);
    Contents->AddChildToVerticalBox(MaterialSize)->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));

    // Required materials. ponytail: no cost model exists yet (dev stock is unlimited); replace the row with have / need rows when it does.
    UBorder* Cost = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Cost->SetBrush(Frame(Well, Hex(TEXT("3A4348")), 3.f)); Cost->SetPadding(FMargin(14.f, 10.f));
    UVerticalBox* CostStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    CostStack->AddChildToVerticalBox(Text(WidgetTree, TEXT("Required Materials"), 20, Ivory()))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    UHorizontalBox* CostRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UHorizontalBoxSlot* CostName = CostRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Unlimited dev stock"), 18, Ivory()));
    CostName->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CostRow->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("status_tick"), 18.f))->SetVerticalAlignment(VAlign_Center);
    CostStack->AddChildToVerticalBox(CostRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    UHorizontalBox* StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    StatusRow->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("status_tick"), 18.f))->SetVerticalAlignment(VAlign_Center);
    StatusTextBlock = Text(WidgetTree, TEXT(""), 18, Ivory());
    StatusTextBlock->SetText(CachedStatusText);
    StatusTextBlock->SetAutoWrapText(true);
    UHorizontalBoxSlot* StatusSlot = StatusRow->AddChildToHorizontalBox(StatusTextBlock);
    StatusSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f)); StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CostStack->AddChildToVerticalBox(StatusRow);
    Cost->SetContent(CostStack);
    Contents->AddChildToVerticalBox(Cost);
}

void USFConstructionSandboxPalette::HandleSearchChanged(const FText& Text)
{
    const FString Query = Text.ToString().TrimStartAndEnd();
    for (int32 Tile = 0; Tile < TileButtons.Num(); ++Tile)
    {
        const bool bShown = Query.IsEmpty() || FString(TileLabel(Tile)).Contains(Query, ESearchCase::IgnoreCase);
        TileButtons[Tile]->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
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
        TileFrames[Index]->SetBrush(SFHudStyle::KitOrFrame(bSelected ? TEXT("slot_catalogue_selected") : TEXT("slot_catalogue_normal"),
            bSelected ? SFHudStyle::Hex(TEXT("2B3B2C"), 1.f) : SFHudStyle::Hex(TEXT("0E171C"), 0.85f),
            bSelected ? SFHudStyle::Brass() : SFHudStyle::Hex(TEXT("3A4348")), 3.f));
    }
}

void USFConstructionSandboxPalette::ConfigureNetworkPresentation()
{
    // Presentation capability subset only. The controller retains all actions
    // and authority; the standalone palette keeps its complete default layout.
    for (const FName Name : {FName(TEXT("HandleConfirm")),FName(TEXT("HandleSave")),FName(TEXT("HandleLoad"))})
    { if (auto* Button=WidgetTree->FindWidget(Name)) { Button->SetVisibility(ESlateVisibility::Collapsed); } }
}

void USFConstructionSandboxPalette::SetBuildModeActive(bool bActive)
{
    SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);  // the viewport-filling canvas must not catch world clicks
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
