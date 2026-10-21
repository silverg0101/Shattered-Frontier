#pragma once

#include "Blueprint/UserWidget.h"
#include "SFInventoryPanel.generated.h"

class UBorder;
class UImage;
class UInventoryItemDefinition;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class USFGatheringPilotComponent;

// Category tabs; Category on an entry is never All.
enum class ESFInventoryTab : uint8 { All, Materials, Gear, Consumables };

// One merged stack row with the item-definition data the grid and tooltip show.
struct FSFInventoryEntry
{
    FText Name;
    int32 Count = 0;
    ESFInventoryTab Category = ESFInventoryTab::Materials;
    FText Description;
    float UnitWeight = 0.f;
    bool bEquipped = false;
    TSoftObjectPtr<UTexture2D> Icon;
};

struct FSFInventoryTooltip { FString Title, Description, TypeLine, WeightLine; };

// Local-only view of the owner's replicated pilot inventory; it sends nothing to the server.
UCLASS()
class SHATTEREDFRONTIER_API USFInventoryPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 GridColumns = 8;
    static constexpr int32 GridRows = 5;
    static constexpr int32 TabCount = 4;

    void Configure(USFGatheringPilotComponent* InPilot);
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

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    void Refresh();
    void StyleTabs();
    void RefreshMainHand(const TArray<FSFInventoryEntry>& All);
    TArray<FSFInventoryEntry> CollectEntries() const;

    ESFInventoryTab ActiveTab = ESFInventoryTab::All;
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> TabBorders;
    UPROPERTY(Transient) TObjectPtr<UBorder> MainHandFrame;
    UPROPERTY(Transient) TObjectPtr<UImage> MainHandIcon;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> SlotFrames;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> SlotTips;
    UPROPERTY(Transient) TArray<TObjectPtr<UImage>> SlotIcons;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotNames;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotCounts;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TipTitles;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TipDescriptions;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TipTypes;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TipWeights;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WeightText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WeightPercent;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> WeightBar;
};
