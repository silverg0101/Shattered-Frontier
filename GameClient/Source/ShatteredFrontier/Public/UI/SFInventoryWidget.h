#pragma once

#include "UI/SFUiWidgets.h"
#include "SFInventoryWidget.generated.h"

class UImage;
class UInventoryItemDefinition;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class USFGatheringPilotComponent;

// Category tabs; Category on an entry is never All.
enum class ESFInventoryTab : uint8 { All, Materials, Gear, Consumables, Misc };

// One merged stack row with the item-definition data the grid and tooltip show.
struct FSFInventoryEntry
{
    FText Name;
    FString DefinitionId;
    int32 Count = 0;
    ESFInventoryTab Category = ESFInventoryTab::Materials;
    FText Description;
    float UnitWeight = 0.f;
    bool bEquipped = false;
    TSoftObjectPtr<UTexture2D> Icon;
};

struct FSFInventoryTooltip { FString Title, Description, RarityLine, TypeLine, WeightLine; };

// Player inventory (WBP_Inventory, docs/blueprints/menus.md section 0), a window on the shared kit: WBP_WindowFrame (emblem, title,
// separators) with a WBP_TabBar, a WBP_ScrollList holding the grid, a SidePanel tooltip and a footer. C++ owns the data (merged stacks, tabs,
// weight, gold, tooltip text) and the one server call (a recipe scroll on right click); the WBP owns layout and look. Local view of the
// owner replicated pilot inventory. Esc closes it (USFWindowStack); there is no close button.
// Binds: SlotGrid (UniformGridPanel or WrapBox, required; C++ fills it with one WBP_ItemSlot per backpack slot, GridColumns wide; it sits
// in a WBP_ScrollList). Optional: TabBar (USFTabBarWidget: All / Materials / Gear / Consumables / Misc), MainHandSlot (the equipped tool),
// WeightText, WeightPercentText, WeightBar (ProgressBar: fraction of the weight limit), Gold (USFGoldDisplayWidget), EmptyText,
// Tooltip (any widget: the hovered item, else the selected one) with TipIcon, TipTitle, TipRarity, TipType, TipQuantity, TipWeight,
// TipDescription. A click selects a slot; select and hover frames are the slot's own.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFInventoryWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 GridColumns = 5;
    static constexpr int32 MaxSlots = 64;
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Inventory/WBP_Inventory.WBP_Inventory_C");

    void Configure(USFGatheringPilotComponent* InPilot);
    // Shows these entries: SlotCount cells in SlotGrid, the active tab's stacks in order, weight line when MaxKg >= 0 (0 = unlimited).
    void Present(const TArray<FSFInventoryEntry>& All, int32 SlotCount, float TotalKg, float MaxKg);
    void SelectTab(ESFInventoryTab Tab);
    int32 CellCount() const { return Cells.Num(); }
    USFSlotWidget* GetCell(int32 Index) const { return Cells.IsValidIndex(Index) ? Cells[Index].Get() : nullptr; }
    USFSlotWidget* GetMainHand() const { return MainHandSlot; }
    FText GetWeightLine() const;
    FText GetWeightPercentLine() const;
    FString GetSelectedName() const { return SelectedKey; }
    void SelectCell(int32 Index);

    // Merges stacks that share a name, drops empty entries and sorts by name.
    static TArray<TPair<FText,int32>> BuildRows(const TArray<TPair<FText,int32>>& Items);
    static FString FormatRows(const TArray<TPair<FText,int32>>& Rows);
    static FString TabLabel(ESFInventoryTab Tab);
    // Gear = equippable (tools, weapons, armour), Consumables = usable/eat/consume, Misc = bundles and scrolls (by definition id),
    // otherwise Materials. Read from the definition, never from display names.
    static ESFInventoryTab CategoryOf(const UInventoryItemDefinition& Definition);
    static FSFInventoryEntry MakeEntry(const UInventoryItemDefinition& Definition, const FString& FallbackName, int32 Count, bool bEquipped);
    // Merges stacks that share a name and equipped state, drops empty entries and sorts by name.
    static TArray<FSFInventoryEntry> BuildEntries(const TArray<FSFInventoryEntry>& Items);
    static TArray<FSFInventoryEntry> FilterByTab(const TArray<FSFInventoryEntry>& Items, ESFInventoryTab Tab);
    static FSFInventoryTooltip BuildTooltip(const FSFInventoryEntry& Entry);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFSlotWidget> SlotClass;  // empty = WBP_ItemSlot
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(18, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> SlotGrid;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabBarWidget> TabBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFSlotWidget> MainHandSlot;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WeightText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WeightPercentText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> WeightBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFGoldDisplayWidget> Gold;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Tooltip;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> TipIcon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipRarity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipType;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipQuantity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipWeight;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipDescription;

private:
    UFUNCTION() void HandleTab(int32 Index);
    UFUNCTION() void HandleSlotClicked(USFSelectableWidget* Source);
    void EnsureCells(int32 Count);
    void UpdateTooltip();
    TArray<FSFInventoryEntry> CollectEntries(int32& OutSlots) const;

    ESFInventoryTab ActiveTab = ESFInventoryTab::All;
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> Cells;
    TArray<FSFInventoryEntry> ShownItems;  // the filtered entries the grid shows, by cell index
    FString SelectedKey;                   // name of the selected stack; survives refreshes, defaults to the first shown
    FSFInventoryEntry MainHandEntry;
    bool bHasMainHand = false;
};
