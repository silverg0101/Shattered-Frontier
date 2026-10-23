#include "Construction/SFConstructionSandboxPalette.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Framework/SFPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "UI/SFItemTooltip.h"
#include "UI/SFWindowStack.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
// Tile -> shape action and the kit icon stem (icon_<stem>); Slab = Foundation, Cube = Wall, Slope = Roof.
constexpr ESFConstructionPaletteAction TileActions[USFConstructionSandboxPalette::TileCount] =
    {ESFConstructionPaletteAction::Slab, ESFConstructionPaletteAction::Cube, ESFConstructionPaletteAction::Door, ESFConstructionPaletteAction::Slope};
const TCHAR* const TileIconStems[USFConstructionSandboxPalette::TileCount] = {TEXT("timber_floor"), TEXT("timber_wall"), TEXT("wooden_door"), TEXT("timber_roof")};
}

USFConstructionSandboxPalette* USFConstructionSandboxPalette::Create(APlayerController* Owner)
{
    const TSubclassOf<USFConstructionSandboxPalette> Class = LoadClass<USFConstructionSandboxPalette>(nullptr, AssetPath);
    if (!Class) { UE_LOG(LogTemp, Error, TEXT("BUILD_CATALOGUE_WBP_MISSING %s: run tools/ui/create_widgets.py"), AssetPath); return nullptr; }
    return CreateWidget<USFConstructionSandboxPalette>(Owner, Class);
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
    SetIsFocusable(false);
    CachedStatusText = FText::FromString(TEXT("Valid placement"));
    if (StatusText) { StatusColour = StatusText->GetColorAndOpacity(); StatusText->SetText(CachedStatusText); }
    if (TabBar)
    {
        FSFTabSpec Voxel(FText::FromString(TEXT("Voxel Objects"))), Blueprints(FText::FromString(TEXT("Blueprints")));
        if (TabIcons.IsValidIndex(0)) { Voxel.Icon = TabIcons[0]; }
        if (TabIcons.IsValidIndex(1)) { Blueprints.Icon = TabIcons[1]; }
        TabBar->SetTabs({Voxel, Blueprints});
        TabBar->OnTabSelected.AddDynamic(this, &USFConstructionSandboxPalette::HandleTab);
    }
    if (SearchField) { SearchField->OnTextChanged.AddDynamic(this, &USFConstructionSandboxPalette::HandleSearchChanged); }
    // ponytail: one material until a materials model exists.
    if (MaterialDropdown) { MaterialDropdown->SetOptions({TEXT("Rough Timber")}, 0); }
    if (FavouriteButton) { FavouriteButton->OnClicked.AddDynamic(this, &USFConstructionSandboxPalette::HandleFavourite); }
    if (CostText)
    {
        // Every pilot piece costs one wall bundle from the backpack (DEC-02); unpaid only under the dev stock fixture. ponytail: one fixed cost;
        // per-piece costs follow BLD-01.
        CostText->SetText(FText::FromString(USFConstructionSandboxSettings::AllowsUnpaidEdits() ? TEXT("Unlimited dev stock") : TEXT("1 Wall Material Bundle")));
    }

    TSubclassOf<USFSlotWidget> SlotClass = LoadClass<USFSlotWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_ItemTile.WBP_ItemTile_C"));
    if (TileGrid && SlotClass)
    {
        UUniformGridPanel* Uniform = Cast<UUniformGridPanel>(TileGrid.Get());
        for (int32 Tile = 0; Tile < TileCount; ++Tile)
        {
            USFSlotWidget* Cell = CreateWidget<USFSlotWidget>(this, SlotClass);
            if (!Cell) { continue; }
            if (Uniform) { UUniformGridSlot* Placed = Uniform->AddChildToUniformGrid(Cell, Tile / GridColumns, Tile % GridColumns); Placed->SetHorizontalAlignment(HAlign_Fill); Placed->SetVerticalAlignment(VAlign_Fill); } else { TileGrid->AddChild(Cell); }
            Cell->SetIcon(SFItemTooltip::ItemTexture(TileIconStems[Tile]));
            Cell->SetKeyText(FText::GetEmpty());
            Cell->SetCaption(FText::FromString(TileLabel(Tile)));
            Cell->SetCount(0);
            Cell->OnClicked.AddDynamic(this, &USFConstructionSandboxPalette::HandleTileClicked);
            Cells.Add(Cell);
        }
    }
    SelectTile(SelectedTile);
}

void USFConstructionSandboxPalette::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

void USFConstructionSandboxPalette::HandleTileClicked(USFSelectableWidget* Source)
{
    const int32 Tile = Cells.IndexOfByPredicate([Source](const TObjectPtr<USFSlotWidget>& Cell) { return Cell == Source; });
    if (Tile != INDEX_NONE) { ClickTile(Tile); }
}

void USFConstructionSandboxPalette::ClickTile(int32 Tile)
{
    if (Tile >= 0 && Tile < TileCount) { Dispatch(TileActions[Tile]); }
}

void USFConstructionSandboxPalette::HandleSearchChanged(const FText& Text) { Search = Text.ToString().TrimStartAndEnd(); RefreshTiles(); }
void USFConstructionSandboxPalette::HandleTab(int32 Index) { ActiveTab = Index; RefreshTiles(); }

void USFConstructionSandboxPalette::HandleFavourite()
{
    if (Favourites.Contains(SelectedTile)) { Favourites.Remove(SelectedTile); } else { Favourites.Add(SelectedTile); }
    RefreshDetail();
}

// The tile grid shows the matching pieces of the Voxel Objects tab (the Blueprints tab has none yet).
void USFConstructionSandboxPalette::RefreshTiles()
{
    int32 Shown = 0;
    for (int32 Tile = 0; Tile < Cells.Num(); ++Tile)
    {
        const bool bShown = ActiveTab == 0 && (Search.IsEmpty() || FString(TileLabel(Tile)).Contains(Search, ESearchCase::IgnoreCase));
        Cells[Tile]->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Shown += bShown ? 1 : 0;
    }
    if (EmptyText)
    {
        EmptyText->SetText(FText::FromString(ActiveTab == 1 ? TEXT("No blueprints yet.") : TEXT("No matching objects.")));
        EmptyText->SetVisibility(Shown == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void USFConstructionSandboxPalette::RefreshDetail()
{
    if (DetailName) { DetailName->SetText(FText::FromString(TileLabel(SelectedTile))); }
    if (DetailSlot)
    {
        DetailSlot->SetIcon(SFItemTooltip::ItemTexture(TileIconStems[FMath::Clamp(SelectedTile, 0, TileCount - 1)]));
        DetailSlot->SetKeyText(FText::GetEmpty());
        DetailSlot->SetCount(0);
    }
    const bool bFavourite = Favourites.Contains(SelectedTile);
    if (FavouriteOn) { FavouriteOn->SetVisibility(bFavourite ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (FavouriteOff) { FavouriteOff->SetVisibility(bFavourite ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
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
    for (int32 Index = 0; Index < Cells.Num(); ++Index) { Cells[Index]->SetSelected(Index == SelectedTile); }
    RefreshDetail();
    RefreshTiles();
}

void USFConstructionSandboxPalette::ConfigureNetworkPresentation()
{
    // Presentation capability subset only. The controller retains all actions and authority; the standalone palette keeps its complete layout.
}

void USFConstructionSandboxPalette::SetBuildModeActive(bool bActive)
{
    if (ASFPlayerController* Player = Cast<ASFPlayerController>(GetOwningPlayer()))
    {
        if (bActive) { Player->GetWindowStack()->Open(this); } else { Player->GetWindowStack()->Close(this); }
        return;
    }
    SetVisibility(bActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void USFConstructionSandboxPalette::SetPlacementValid(bool bValid)
{
    bStatusValid = bValid;
    if (StatusOk) { StatusOk->SetVisibility(bValid ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (StatusText) { StatusText->SetColorAndOpacity(bValid ? StatusColour : FSlateColor(SFHudStyle::Crimson())); }  // state tint
}

void USFConstructionSandboxPalette::SetStatusText(const FText& InStatusText)
{
    CachedStatusText = InStatusText;
    if (StatusText) { StatusText->SetText(CachedStatusText); }
}

bool USFConstructionSandboxPalette::IsPointerOverPalette() const
{
    return GetVisibility() != ESlateVisibility::Collapsed && FSlateApplication::IsInitialized()
        && GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

FReply USFConstructionSandboxPalette::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    return FReply::Handled();  // a click on the window never reaches the world behind it
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
