#pragma once

#include "UI/SFUiWidgets.h"
#include "SFConstructionSandboxPalette.generated.h"

class UButton;
class UImage;
class UPanelWidget;
class UTextBlock;

UENUM()
enum class ESFConstructionPaletteAction : uint8
{
    Cube,
    Slab,
    Slope,
    Door,
    Single,
    Line,
    Demolish,
    Rotate,
    Surface,
    Layer,
    LayerDown,
    LayerUp,
    Confirm,
    Cancel,
    Save,
    Load
};

DECLARE_DELEGATE_OneParam(FSFConstructionPaletteActionDelegate, ESFConstructionPaletteAction);

// Build catalogue (WBP_BuildCatalogue, docs/blueprints/menus.md section 0): a WBP_WindowFrame (emblem icon_window_build, title "Build
// Catalogue") with a search field, the Voxel Objects / Blueprints tabs, a tile grid (WBP_ItemSlot cells in a WBP_ScrollList, the selected one
// in the gold select frame), a separator, the material dropdown and the selected object's detail (preview slot, name, favourite star, cost,
// placement check). In a player controller it is a window of USFWindowStack while build mode is on (Esc leaves build mode); the mode and layer
// actions live on keys (L, X, R, P, PgUp/PgDn) and only the four real pieces are populated.
// Binds (required): TileGrid (UniformGridPanel or WrapBox in a WBP_ScrollList, left empty: C++ fills it with one WBP_ItemSlot per piece, 3 wide).
// Optional: SearchField, TabBar (Voxel Objects / Blueprints), EmptyText, MaterialDropdown, DetailSlot (WBP_ItemSlot), DetailName,
// FavouriteButton (Button) with FavouriteOn / FavouriteOff (the filled and outline star), CostText, StatusText, StatusOk (the green check).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFConstructionSandboxPalette : public USFUiWidget
{
    GENERATED_BODY()

public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Build/WBP_BuildCatalogue.WBP_BuildCatalogue_C");
    static constexpr int32 GridColumns = 3;

    // The catalogue widget for this controller, or null (logged) when the WBP has not been authored (tools/ui/create_widgets.py).
    static USFConstructionSandboxPalette* Create(APlayerController* Owner);

    FSFConstructionPaletteActionDelegate OnAction;

    // Catalogue tiles in display order; the palette's shape actions map onto them (Cube = Wall, Slab = Foundation, Slope = Roof).
    static constexpr int32 TileCount = 4;
    static int32 TileForAction(ESFConstructionPaletteAction Action);  // INDEX_NONE for non-shape actions
    static const TCHAR* TileLabel(int32 Tile);

    void ConfigureNetworkPresentation();
    // Build mode on = the window is open (the controller's window stack when it has one, else shown), off = closed (it starts closed).
    void SetBuildModeActive(bool bActive);
    void SetStatusText(const FText& InStatusText);
    // The green check shows for a valid placement and hides otherwise (the status line says "Valid placement" or why not).
    void SetPlacementValid(bool bValid);
    bool IsPointerOverPalette() const;
    int32 GetSelectedTile() const { return SelectedTile; }
    int32 CellCount() const { return Cells.Num(); }
    USFSlotWidget* GetCell(int32 Index) const { return Cells.IsValidIndex(Index) ? Cells[Index].Get() : nullptr; }
    // The tile click as the mouse does it (the capture and tests use it): selects the tile and dispatches its shape action.
    void ClickTile(int32 Tile);

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());
    UPROPERTY(EditAnywhere, Category = "SF Style") TArray<FSlateBrush> TabIcons;  // Voxel Objects, Blueprints (kit icon_tab_voxel / icon_tab_blueprints, set in the WBP)

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> TileGrid;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTextFieldWidget> SearchField;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabBarWidget> TabBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFDropdownWidget> MaterialDropdown;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFSlotWidget> DetailSlot;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailName;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UButton> FavouriteButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> FavouriteOn;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> FavouriteOff;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CostText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> StatusOk;

private:
    UFUNCTION() void HandleTileClicked(USFSelectableWidget* Source);
    UFUNCTION() void HandleSearchChanged(const FText& Text);
    UFUNCTION() void HandleTab(int32 Index);
    UFUNCTION() void HandleFavourite();

    void Dispatch(ESFConstructionPaletteAction Action);
    void SelectTile(int32 Tile);
    void RefreshDetail();
    void RefreshTiles();

    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> Cells;
    int32 SelectedTile = 1;  // Wall: the default Cube shape
    int32 ActiveTab = 0;     // 0 Voxel Objects, 1 Blueprints (none exist yet)
    FString Search;
    TSet<int32> Favourites;
    FText CachedStatusText;
    bool bStatusValid = true;
    FSlateColor StatusColour;  // the Designer's status colour, restored when a placement is valid again
};
