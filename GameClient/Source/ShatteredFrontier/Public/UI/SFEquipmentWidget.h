#pragma once

#include "GameFramework/Actor.h"
#include "UI/SFInventoryWidget.h"
#include "SFEquipmentWidget.generated.h"

class APawn;
class UImage;
class UPanelWidget;
class UPointLightComponent;
class UProgressBar;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UTextBlock;
class UTextureRenderTarget2D;
class USFGatheringPilotComponent;

enum class ESFEquipmentTab : uint8 { All, Weapons, Armor, Accessories };
enum class ESFEquipDrop : uint8 { None, Equip, Unequip };

// The character preview's off-world studio (client only, never replicated): copies of the local pawn's meshes and everything attached to
// its body (the equipped-item visuals), three fixed lights on their own lighting channel, and a scene capture that renders ONLY this actor
// (show-only list, no sky, fog, sun, GI or post) into RT_EquipmentPreview with alpha. Spawned when the Equipment window opens, destroyed
// when it closes, so it captures only while the window is open and lights the same way every time.
UCLASS(NotPlaceable, Transient)
class SHATTEREDFRONTIER_API ASFEquipmentPreviewStudio : public AActor
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* TargetPath = TEXT("/Game/Frontier/UI/Equipment/RT_EquipmentPreview.RT_EquipmentPreview");
    static constexpr const TCHAR* MaterialPath = TEXT("/Game/Frontier/UI/Equipment/M_EquipmentPreview.M_EquipmentPreview");
    static const FVector StudioLocation;  // 2 km under the world origin: nothing else is there, and the capture shows only this actor anyway

    ASFEquipmentPreviewStudio();
    // Rebuilds the copy from Pawn (its body mesh, materials, attached meshes) and frames the camera on it; live pose via leader pose.
    void CopyFrom(const APawn* Pawn);
    void AddYaw(float Degrees);
    UTextureRenderTarget2D* GetTarget() const { return Target; }
    int32 CopiedMeshCount() const { return Copies.Num(); }

private:
    UPROPERTY() TObjectPtr<USceneComponent> Turntable;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> Copies;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Target;
};

// Equipment window (WBP_Equipment, docs/blueprints/menus.md section 0) on WBP_WindowFrame: the paper doll (left Head, Chest, Hands, Legs,
// Feet; right MainHand, OffHand, Back, Neck, Ring) around the character preview, then tabs and a grid of the EQUIPPABLE bag items,
// then weight and gold. C++ owns the data, the drag / double-click requests (server validated, USFGatheringPilotComponent::RequestEquip /
// RequestUnequip), the preview studio and its rotation; the WBP owns layout and look.
// Binds: ItemGrid (WrapBox or UniformGridPanel, required; C++ fills it with WBP_ItemSlot cells, GridColumns wide). Optional, by name: <Slot>Slot for each
// equipment slot (USFSlotWidget: WBP_EquipSlot, ghost shown while empty, a click selects it), Preview (Image whose Designer brush is
// M_EquipmentPreview, which samples RT_EquipmentPreview; drag rotates the character),
// GridScroll (the grid's WBP_ScrollList: dropping a paper-doll item there unequips it), TabBar (All / Weapons / Armor / Accessories),
// WeightText, WeightPercentText, WeightBar, Gold, EmptyText, Tooltip + TipIcon, TipTitle, TipRarity, TipType, TipQuantity, TipWeight, TipDescription.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFEquipmentWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 GridColumns = 7;
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Equipment/WBP_Equipment.WBP_Equipment_C");

    void Configure(USFGatheringPilotComponent* InPilot);
    // Bag = every bag entry (only equippable, unequipped ones show in the grid); Equipped = the entry in each equipment slot.
    void Present(const TArray<FSFInventoryEntry>& Bag, const TMap<FName, FSFInventoryEntry>& Equipped, int32 SlotCount, float TotalKg, float MaxKg);
    void SelectTab(ESFEquipmentTab Tab);
    static FString TabLabel(ESFEquipmentTab Tab);
    // Weapons = MainHand / OffHand, Accessories = Neck / Ring, Armor = the rest.
    static bool InTab(FName SlotName, ESFEquipmentTab Tab);
    // The drop rule: a bag item onto the doll slot of its own equip slot equips; a doll item onto a grid unequips; anything else does nothing.
    static ESFEquipDrop ResolveDrop(const FSoftObjectPath& Item, FName ItemSlot, FName FromEquipSlot, FName TargetSlot, bool bOnGrid);
    USFSlotWidget* GetDollSlot(FName SlotName) const { return DollSlots.FindRef(SlotName); }
    int32 CellCount() const { return Cells.Num(); }
    int32 ShownCount() const { return ShownItems.Num(); }
    USFSlotWidget* GetCell(int32 Index) const { return Cells.IsValidIndex(Index) ? Cells[Index].Get() : nullptr; }
    FText GetWeightLine() const;
    ASFEquipmentPreviewStudio* GetStudio() const { return Studio; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFSlotWidget> SlotClass;  // empty = WBP_ItemSlot
    UPROPERTY(EditAnywhere, Category = "SF") float RotateDegreesPerPixel = 0.5f;
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(18, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> ItemGrid;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> GridScroll;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> Preview;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabBarWidget> TabBar;
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
    UFUNCTION() void HandleVisibility(ESlateVisibility InVisibility);
    UFUNCTION() void HandleCellDoubleClicked(USFSelectableWidget* Source);
    UFUNCTION() void HandleDollDoubleClicked(USFSelectableWidget* Source);
    UFUNCTION() void HandleDollClicked(USFSelectableWidget* Source);
    void EnsureCells(int32 Count);
    void UpdateTooltip();
    void OpenStudio();
    void CloseStudio();
    FName DollSlotAt(const FVector2D& ScreenPos) const;
    void Collect(TArray<FSFInventoryEntry>& OutBag, TMap<FName, FSFInventoryEntry>& OutEquipped, int32& OutSlots) const;

    ESFEquipmentTab ActiveTab = ESFEquipmentTab::All;
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> Cells;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<USFSlotWidget>> DollSlots;
    UPROPERTY(Transient) TObjectPtr<ASFEquipmentPreviewStudio> Studio;
    TArray<FSFInventoryEntry> ShownItems;
    TMap<FName, FSFInventoryEntry> EquippedItems;
    FString EquippedSignature;  // rebuild the preview copy when the equipped set changes
    bool bRotating = false;
    float LastRotateX = 0.f;
};
