#pragma once

#include "UI/SFUiWidgets.h"
#include "SFInventoryWidget.generated.h"

class UImage;
class UInventoryItemDefinition;
class UPanelWidget;
class UTextBlock;
class USFGatheringPilotComponent;

// Category tabs; Category on an entry is never All.
enum class ESFInventoryTab : uint8 { All, Materials, Gear, Consumables };

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

struct FSFInventoryTooltip { FString Title, Description, TypeLine, WeightLine; };

// Player inventory (WBP_Inventory, docs/blueprints/menus.md section 0). C++ owns the data (merged stacks, tabs, weight, gold, tooltip text) and
// the one server call (a recipe scroll on right click); the WBP owns layout and look. Local view of the owner's replicated pilot inventory.
// Binds: SlotGrid (UniformGridPanel or WrapBox, required; C++ fills it with one WBP_Slot per backpack slot). Optional: TabAll, TabMaterials,
// TabTools, TabFood (WBP_Tab), MainHandSlot (WBP_Slot, the equipped tool), CloseButton (WBP_Button), WeightText, GoldText, GoldRow, EmptyText,
// Tooltip (any widget, shown for the hovered slot) with TipIcon, TipTitle, TipType, TipQuantity, TipWeight, TipDescription.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFInventoryWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 GridColumns = 8;
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

    // Merges stacks that share a name, drops empty entries and sorts by name.
    static TArray<TPair<FText,int32>> BuildRows(const TArray<TPair<FText,int32>>& Items);
    static FString FormatRows(const TArray<TPair<FText,int32>>& Rows);
    static FString TabLabel(ESFInventoryTab Tab);
    // Gear = equippable, Consumables = usable/eat/consume, otherwise Materials. Read from the definition, never from names.
    static ESFInventoryTab CategoryOf(const UInventoryItemDefinition& Definition);
    static FSFInventoryEntry MakeEntry(const UInventoryItemDefinition& Definition, const FString& FallbackName, int32 Count, bool bEquipped);
    // Merges stacks that share a name and equipped state, drops empty entries and sorts by name.
    static TArray<FSFInventoryEntry> BuildEntries(const TArray<FSFInventoryEntry>& Items);
    static TArray<FSFInventoryEntry> FilterByTab(const TArray<FSFInventoryEntry>& Items, ESFInventoryTab Tab);
    static FSFInventoryTooltip BuildTooltip(const FSFInventoryEntry& Entry);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFSlotWidget> SlotClass;  // empty = WBP_Slot
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(18, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> SlotGrid;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabWidget> TabAll;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabWidget> TabMaterials;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabWidget> TabTools;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabWidget> TabFood;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFSlotWidget> MainHandSlot;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> CloseButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WeightText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GoldText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> GoldRow;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Tooltip;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> TipIcon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipType;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipQuantity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipWeight;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipDescription;

private:
    UFUNCTION() void HandleTab(USFButtonWidget* Source);
    UFUNCTION() void HandleClose(USFButtonWidget* Source);
    void EnsureCells(int32 Count);
    void UpdateTooltip();
    TArray<FSFInventoryEntry> CollectEntries(int32& OutSlots) const;

    ESFInventoryTab ActiveTab = ESFInventoryTab::All;
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> Cells;
    TArray<FSFInventoryEntry> ShownItems;  // the filtered entries the grid shows, by cell index
    FSFInventoryEntry MainHandEntry;
    bool bHasMainHand = false;
};
