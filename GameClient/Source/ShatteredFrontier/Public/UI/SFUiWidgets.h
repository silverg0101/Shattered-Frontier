#pragma once

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Framework/SFHudStyle.h"
#include "Types/SlateEnums.h"
#include "SFUiWidgets.generated.h"

class UBorder;
class UButton;
class UComboBoxString;
class UEditableText;
class UImage;
class UNamedSlot;
class UPanelWidget;
class UProgressBar;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UTexture2D;

// Shared UMG parts (docs/blueprints/menus.md "C++ / Widget Blueprint split"). C++ owns data, state, events and the style
// tokens (SFHudStyle: kit pieces, colours, fonts, sizes); the Widget Blueprint (WBP_*) owns layout and look.
// A widget binds the parts it drives with meta=(BindWidget) / BindWidgetOptional: do not rename or delete those in a WBP.
UENUM(BlueprintType)
enum class ESFUiState : uint8 { Normal, Hover, Pressed, Selected, Disabled };

UENUM(BlueprintType)
enum class ESFUiButtonKind : uint8 { Secondary, Primary, Destructive };

// One text role: defaults come from SFHudStyle (ivory EB Garamond); the font itself is resolved at runtime (it is a composite font
// built from the imported OFL faces, never saved into a WBP).
USTRUCT(BlueprintType)
struct SHATTEREDFRONTIER_API FSFUiTextStyle
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") int32 SizePx = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") bool bBold = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") bool bShadow = false;  // 1 px dark shadow and outline for text over art
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") FLinearColor Color = SFHudStyle::Ivory();

    FSFUiTextStyle() {}
    FSFUiTextStyle(int32 InSizePx, const FLinearColor& InColor, bool bInBold = false, bool bInShadow = false)
        : SizePx(InSizePx), bBold(bInBold), bShadow(bInShadow), Color(InColor) {}
    void Apply(UTextBlock* Block) const;
};

// Base of every shared part: applies the kit art, fonts and colours from SFHudStyle on construct (and in the Designer preview).
// Untick bApplyKitStyle on one WBP to style it by hand instead.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFUiWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") bool bApplyKitStyle = true;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativePreConstruct() override;
    virtual void ApplyKitStyle() {}
    // Gives every text block of this widget (including the content it puts into the kit window slots) the project font family where the Designer
    // left the UMG default; sizes and colours the Designer set stay (FSFUiTextStyle::Apply is fill-only).
    void ApplyFamilyToAllText(const FSFUiTextStyle& Style);
};

// Hover and selection behaviour shared by slots, list rows and tiles. Binds, all optional: HoverFrame (shown while the pointer is over an
// unselected item: hover_frame) and SelectFrame (shown while selected: select_frame, gold). Both are Designer-owned overlays; C++ only toggles
// their visibility. OnClicked fires on a left click (the click is only consumed while something is bound to it).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFUiSelectableClicked, class USFSelectableWidget*, Source);

UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFSelectableWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    bool WasLastClickShiftDown() const { return bLastClickShiftDown; }
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFUiSelectableClicked OnClicked;

    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetSelected(bool bInSelected);
    UFUNCTION(BlueprintPure, Category = "SF UI") bool IsSelected() const { return bSelected; }
    UFUNCTION(BlueprintPure, Category = "SF UI") bool IsPointerOver() const { return bPointerOver; }
    // Selection hook for extra visuals in the WBP.
    UFUNCTION(BlueprintNativeEvent, Category = "SF UI") void OnSelectedChanged(bool bNowSelected);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    void RefreshFrames();

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> HoverFrame;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> SelectFrame;
    bool bSelected = false;
    bool bPointerOver = false;
    bool bLastClickShiftDown = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFUiClicked, class USFButtonWidget*, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFUiActivated, class USFButtonWidget*, Source);

// Hold-to-activate timer (pure, testable): the fill grows while held, fires once when full, drains after release.
struct SHATTEREDFRONTIER_API FSFHoldTimer
{
    float HoldSeconds = 0.8f;
    float DrainSeconds = 0.25f;
    float Progress = 0.f;
    bool bFired = false;
    // Returns true on the call that completes the fill. Held again while draining continues from the current fill.
    bool Advance(float DeltaSeconds, bool bHeld);
};

// Titled frame with a Content slot. Binds: Frame (Border), Title (TextBlock), Content (NamedSlot), all optional.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFPanelFrameWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetTitle(const FText& InTitle);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) FText TitleText;
    UPROPERTY(EditAnywhere, Category = "SF Style") FName FramePiece = TEXT("panel_frame");  // kit element name in DA_HudKit
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TitleStyle = FSFUiTextStyle(36, SFHudStyle::Ivory());

protected:
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Frame;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Title;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UNamedSlot> Content;
};

// Text button (WBP_Button). Binds: Button (required), Label and HoldFill (optional). A Button whose style already carries art (the kit
// WBP_Button) is left alone; otherwise hover, press and disabled brushes come from SFHudStyle::ButtonBrush. OnStateChanged is the hook for
// extra visuals (glow, sound, animation) in the WBP.
// HoldFill (a ProgressBar the WBP puts UNDER the button face: button_hold_fill) shows the hold progress when bHoldToActivate is on: it grows
// while the button is held, OnActivated fires when full, and it drains after release. OnActivated is also the plain click.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFButtonWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFUiClicked OnClicked;       // a click; not raised for hold buttons
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFUiActivated OnActivated;  // a click, or the hold completing

    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetLabel(const FText& InLabel);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetButtonEnabled(bool bEnabled);
    UFUNCTION(BlueprintPure, Category = "SF UI") bool IsButtonEnabled() const;
    // Turns the hold behaviour on or off at runtime (buy, accept) and shows or hides HoldFill with it.
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetHoldMode(bool bHold, float Seconds);
    // Fills HoldFill to a fraction from outside (a running craft); 0 hides it again unless the button is a hold button.
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetProgress(float Fraction);
    // Drives the hold fill by hand (the tick calls it). Returns true when this call fired OnActivated.
    UFUNCTION(BlueprintCallable, Category = "SF UI") bool AdvanceHold(float DeltaSeconds, bool bHeld);
    UFUNCTION(BlueprintPure, Category = "SF UI") float GetHoldProgress() const { return Hold.Progress; }
    UFUNCTION(BlueprintPure, Category = "SF UI") ESFUiState GetState() const { return State; }
    // Visual hook: Normal, Hover, Pressed or Disabled just became current.
    UFUNCTION(BlueprintImplementableEvent, Category = "SF UI") void OnStateChanged(ESFUiState NewState);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) FText LabelText;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF Style") ESFUiButtonKind Kind = ESFUiButtonKind::Secondary;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Hold") bool bHoldToActivate = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Hold", meta = (ClampMin = "0.05", Units = "s")) float HoldSeconds = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Hold", meta = (ClampMin = "0.05", Units = "s")) float HoldDrainSeconds = 0.25f;
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle LabelStyle = FSFUiTextStyle(24, SFHudStyle::Ivory());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle PrimaryLabelStyle = FSFUiTextStyle(26, SFHudStyle::Hex(TEXT("2A1E10")));  // dark ink on brass

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;
    void SetState(ESFUiState NewState);

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UButton> Button;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Label;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> HoldFill;
    ESFUiState State = ESFUiState::Normal;
    FSFHoldTimer Hold;
    bool bHolding = false;

private:
    UFUNCTION() void HandleClicked();
    UFUNCTION() void HandleHovered();
    UFUNCTION() void HandleUnhovered();
    UFUNCTION() void HandlePressed();
    UFUNCTION() void HandleReleased();
};

// Tab (WBP_Tab): a button that can be Selected. The plate brushes and the optional icon belong to the WBP (class defaults of WBP_Tab, or per
// instance); a brush without art is left alone, and without SelectedBrush the HUD tab_selected moss plate is used. Extra optional bind:
// TabIcon (Image, collapses without IconBrush art; the WBP puts it above or left of the label). Same other binds as USFButtonWidget.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFTabWidget : public USFButtonWidget
{
    GENERATED_BODY()
public:
    USFTabWidget() { LabelStyle = FSFUiTextStyle(20, SFHudStyle::Ivory()); }
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetSelected(bool bInSelected);
    UFUNCTION(BlueprintPure, Category = "SF UI") bool IsSelected() const { return bSelected; }
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetIcon(const FSlateBrush& Brush);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") FSlateBrush NormalBrush;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") FSlateBrush HoverBrush;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") FSlateBrush SelectedBrush;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") FSlateBrush IconBrush;

protected:
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> TabIcon;
    bool bSelected = false;
};

// One tab to build: its label and optional icon.
USTRUCT(BlueprintType)
struct SHATTEREDFRONTIER_API FSFTabSpec
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF") FText Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF") FSlateBrush Icon;
    FSFTabSpec() {}
    explicit FSFTabSpec(const FText& InLabel) : Label(InLabel) {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFTabBarSelected, int32, Index);

// A row of tabs (WBP_TabBar). Binds: TabRow (HorizontalBox or any panel, required). SetTabs builds one TabClass (default WBP_Tab) per spec;
// clicking one selects it (the others deselect) and raises OnTabSelected(index). Every menu with tabs uses this one bar.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFTabBarWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFTabBarSelected OnTabSelected;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFTabWidget> TabClass;  // empty = WBP_Tab
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF Style") FMargin TabPadding = FMargin(6.f, 0.f);

    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetTabs(const TArray<FSFTabSpec>& Specs);
    // Selects a tab; bNotify also raises OnTabSelected.
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SelectTab(int32 Index, bool bNotify = false);
    UFUNCTION(BlueprintPure, Category = "SF UI") int32 GetSelectedIndex() const { return Selected; }
    UFUNCTION(BlueprintPure, Category = "SF UI") int32 NumTabs() const { return Tabs.Num(); }
    USFTabWidget* GetTab(int32 Index) const { return Tabs.IsValidIndex(Index) ? Tabs[Index].Get() : nullptr; }

protected:
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> TabRow;

private:
    UFUNCTION() void HandleTab(USFButtonWidget* Source);
    UPROPERTY(Transient) TArray<TObjectPtr<USFTabWidget>> Tabs;
    int32 Selected = INDEX_NONE;
};

// An item dragged out of a slot (inventory / equipment grids, paper-doll slots). The drop target asks the server; it never moves items itself.
UCLASS()
class SHATTEREDFRONTIER_API USFItemDragOperation : public UDragDropOperation
{
    GENERATED_BODY()
public:
    FSoftObjectPath Item;   // a bag item (its definition)
    FName ItemSlot;         // that item's equip slot
    FName FromEquipSlot;    // set when the drag left an equipped paper-doll slot
    int32 FromHotbar = INDEX_NONE;  // set when the drag left a hotbar slot (windows ignore it; off the bar it clears the slot)
    FString DefinitionId, InstanceId;
    int32 Quantity = 0;
    bool bFromLoot = false, bFromInventory = false;
    virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;
    virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
    UPROPERTY(Transient) TObjectPtr<class USFWorldDropTarget> WorldDropTarget;
};

// Input-only viewport surface behind the HUD/windows, present only during a bag
// drag. A rejected UI drop keeps its UI hit path and never reaches this sibling.
UCLASS(Transient)
class SHATTEREDFRONTIER_API USFWorldDropTarget : public UUserWidget
{
    GENERATED_BODY()
    friend class FSFLootWorkflowNetcheck;
    friend class FSFLootWorldReleaseTest;
public:
    void Configure(USFItemDragOperation* InOperation) { Operation = InOperation; }
    void Cancel() { Operation.Reset(); RemoveFromParent(); }
protected:
    virtual void NativeOnInitialized() override;
    virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* InOperation) override;
private:
    TWeakObjectPtr<USFItemDragOperation> Operation;
};

// Slot or tile (bag, equipment, hotbar, catalogue; menu slots are WBP_ItemSlot). Binds, all optional: Frame (Border), Icon (Image),
// KeyLabel, CountLabel (TextBlocks), Ghost (Image shown only while Icon is empty: the paper-doll slot guide), plus HoverFrame / SelectFrame
// from USFSelectableWidget. A slot whose owner set a drag payload (DragItem, DragFromEquipSlot or a hotbar index) can be dragged; OnDoubleClicked fires on a
// left double click.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFSlotWidget : public USFSelectableWidget
{
    GENERATED_BODY()
    friend class FSFLootWorkflowNetcheck;
    friend class FSFLootWorldReleaseTest;
public:
    // The one size of every box a player drags an item into or out of (bag grids, paper doll, trade / market / reward slots, the
    // hotbar), in kit art px: windows show art at 0.5, so 72 px on screen; the HUD hotbar is authored at half (also 72 px).
    static constexpr float ItemSlotArt = 144.f;
    static constexpr float ItemIconArt = 112.f;  // the icon inside it
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetIcon(UTexture2D* Texture);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetKeyText(const FText& Key);
    // The tile caption under the icon (build catalogue); empty collapses it.
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetCaption(const FText& InCaption);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetCount(int32 Count);
    UFUNCTION(BlueprintPure, Category = "SF UI") FText GetCountText() const;
    UFUNCTION(BlueprintPure, Category = "SF UI") bool HasIcon() const;
    // Drag payload, set by the owning window each refresh; clear both to make the slot not draggable.
    void SetDragPayload(const FSoftObjectPath& Item, FName ItemSlot, FName FromEquipSlot) { DragItem = Item; DragItemSlot = ItemSlot; DragFromEquipSlot = FromEquipSlot; }
    void SetInventoryDragPayload(const FString& Id, int32 Count, const FString& Copy, bool bExternal)
    { DragDefinitionId = Id; DragQuantity = Count; DragInstanceId = Copy; bDragFromLoot = bExternal; bDragFromInventory = true; }
    void SetHotbarDrag(int32 Index) { DragFromHotbar = Index; }  // INDEX_NONE: an empty hotbar slot, not draggable
    bool CanDrag() const { return !DragItem.IsNull() || !DragFromEquipSlot.IsNone() || DragFromHotbar != INDEX_NONE; }
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFUiSelectableClicked OnDoubleClicked;
    // The ghost art of an empty paper-doll slot (per instance in the Designer); applied to Ghost.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF Style") FSlateBrush GhostBrush;
    // Count rule: shown from 2 up (tools are single items), empty otherwise; bShowSingleCount shows from 1 up (the bag).
    static FText CountText(int32 Count);

    // The native selection body swaps the frame to SFHudStyle::SlotBrush while bKitFrameSwap is on (the HUD slot). The menu slot
    // (WBP_ItemSlot) turns it off: its Frame brush is the Designer's and HoverFrame / SelectFrame show the state.
    UPROPERTY(EditAnywhere, Category = "SF Style") bool bKitFrameSwap = true;
    UPROPERTY(EditAnywhere, Category = "SF Style") bool bShowSingleCount = false;
    UPROPERTY(EditAnywhere, Category = "SF Style") FName NormalPiece = TEXT("slot_hotbar_normal");  // bag 64 / equipment 72 / hotbar 88 / catalogue 100
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle KeyStyle = FSFUiTextStyle(32, SFHudStyle::Ivory(), false, true);
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle CountStyle = FSFUiTextStyle(24, SFHudStyle::Ivory(), false, true);

protected:
    virtual void ApplyKitStyle() override;
    virtual void OnSelectedChanged_Implementation(bool bNowSelected) override;
    virtual void NativePreConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Frame;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> Icon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> Ghost;
    FSoftObjectPath DragItem;
    FName DragItemSlot;
    FName DragFromEquipSlot;
    int32 DragFromHotbar = INDEX_NONE;
    FString DragDefinitionId, DragInstanceId;
    int32 DragQuantity = 0;
    bool bDragFromLoot = false, bDragFromInventory = false;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> KeyLabel;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountLabel;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CaptionLabel;
};

// List row (quest objective, toast card, recipe or market row). Binds, all optional: Frame (Border), Icon (Image), Caption, Label, Value
// (TextBlocks), Marker (checkbox group), DoneMark (tick shown when done), Rule (hairline shown for headings), plus HoverFrame / SelectFrame.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFListRowWidget : public USFSelectableWidget
{
    GENERATED_BODY()
public:
    // bHeading: bold title style, no checkbox, hairline Rule. bDone shows the DoneMark.
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetEntry(const FText& InLabel, const FText& InValue, bool bDone = false, bool bHeading = false);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetCaption(const FText& InCaption);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetRowIcon(const FSlateBrush& Brush);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetTint(const FLinearColor& LabelColor, const FLinearColor& ValueColor);

    // The native selection body swaps the frame to SFHudStyle::RowBrush (or FramePiece) while bKitFrameSwap is on.
    UPROPERTY(EditAnywhere, Category = "SF Style") bool bKitFrameSwap = true;
    UPROPERTY(EditAnywhere, Category = "SF Style") FName FramePiece;  // kit element for the frame; none = SFHudStyle::RowBrush
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle LabelStyle = FSFUiTextStyle(20, SFHudStyle::Ivory());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle ValueStyle = FSFUiTextStyle(20, SFHudStyle::Ivory());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle CaptionStyle = FSFUiTextStyle(18, SFHudStyle::Brass());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle HeadingStyle = FSFUiTextStyle(24, SFHudStyle::Ivory(), true);

protected:
    virtual void ApplyKitStyle() override;
    virtual void OnSelectedChanged_Implementation(bool bNowSelected) override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Frame;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> Icon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Caption;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Label;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Value;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Marker;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> DoneMark;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Rule;
    bool bHeadingShown = false;
};

// Kit list row (WBP_KitRow: row_back, icon well, name, up to two text cells, price, an action button, chevron, gold select / dim hover
// frame). One parent for the market listing, recipe, ingredient and trade rows. Binds, all optional on top of USFListRowWidget: Cell1, Cell2
// (TextBlocks inside the column containers Cell1Box, Cell2Box; a container collapses while its text is empty), PriceBox (the coin and Value;
// collapsed by SetPrice(empty)), Action (WBP_Button inside ActionBox), Chevron, IconBox. Value is the price or count text.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFKitRowWidget : public USFListRowWidget
{
    GENERATED_BODY()
public:
    // Empty text collapses that column.
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetCells(const FText& InCell1, const FText& InCell2);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetPrice(const FText& InPrice);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void ShowAction(bool bShown);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void ShowChevron(bool bShown);
    // The item art in the icon well (null collapses the icon); a runtime state tint of Value (have / need, affordability) is data.
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetIconTexture(UTexture2D* Texture);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetValueTint(const FLinearColor& Color);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetCellTint(const FLinearColor& Color);  // Cell1 (a have / need count)
    USFButtonWidget* GetAction() const { return Action; }
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle CellStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Cell1;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Cell2;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Cell1Box;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Cell2Box;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> PriceBox;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> ActionBox;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> Action;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Chevron;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> IconBox;
};

// ---- shared menu kit v7 parts (docs/dependency-map/ui.md "Shared menu kit v7"). Each has ONE parent WBP in /Game/Frontier/UI/Widgets/Common/.

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFStepperChanged, int32, NewValue);

// [-] number [+] (WBP_Stepper). Binds: MinusButton, PlusButton (Buttons, required), ValueText, MinusFill, PlusFill (optional; the fills are
// ProgressBars with the stepper_fill brush: + sweeps its fill left to right, - sweeps it right to left (the WBP sets MinusFill's fill type)).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFStepperWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFStepperChanged OnValueChanged;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) int32 Value = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) int32 MinValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) int32 MaxValue = 99;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ClampMin = "1")) int32 Step = 1;
    UPROPERTY(EditAnywhere, Category = "SF Style", meta = (ClampMin = "0.02", Units = "s")) float SweepSeconds = 0.18f;
    UPROPERTY(EditAnywhere, Category = "SF Style", meta = (ClampMin = "0.0", Units = "s")) float FadeSeconds = 0.12f;
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(36, SFHudStyle::Ivory());

    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetRange(int32 InMin, int32 InMax);
    // Clamps into [MinValue, MaxValue]; raises OnValueChanged only when the value changed. Returns whether it changed.
    UFUNCTION(BlueprintCallable, Category = "SF UI") bool SetValue(int32 NewValue);
    UFUNCTION(BlueprintCallable, Category = "SF UI") bool Increment();  // + plays the left-to-right sweep
    UFUNCTION(BlueprintCallable, Category = "SF UI") bool Decrement();  // - plays the right-to-left sweep
    UFUNCTION(BlueprintPure, Category = "SF UI") int32 GetValue() const { return Value; }
    static int32 ClampValue(int32 InValue, int32 InMin, int32 InMax) { return FMath::Clamp(InValue, InMin, FMath::Max(InMin, InMax)); }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UButton> MinusButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UButton> PlusButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ValueText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> MinusFill;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> PlusFill;

private:
    UFUNCTION() void HandleMinus() { Decrement(); }
    UFUNCTION() void HandlePlus() { Increment(); }
    void Refresh();
    float MinusSweep = -1.f;  // seconds since the sweep started; negative = idle
    float PlusSweep = -1.f;
};

// Horizontal or vertical brass rule (WBP_Separator). Binds: Line (Image, required). The two brushes are class defaults of the WBP.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFSeparatorWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) bool bVertical = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") FSlateBrush HorizontalBrush;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") FSlateBrush VerticalBrush;

protected:
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UImage> Line;
};

// The one scrollbar style (WBP_ScrollList): a ScrollBox styled with scrollbar_track / scrollbar_thumb in the WBP, with a Content slot.
// Put every scrolling list inside an instance of it so one change restyles every menu. Binds: Scroll (ScrollBox, optional), Content (NamedSlot).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFScrollListWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF UI") void ScrollToStart();

protected:
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UScrollBox> Scroll;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UNamedSlot> Content;
};

// Standard menu window (WBP_WindowFrame): split brass frame (4 corners, 4 rails, tiled slate fill), header (banner shield, per-window
// emblem, title, separator), NamedSlots Tabs / Content / SidePanel / Footer and a separator above the Footer. Everything inside is authored
// in art pixels (2x) under the frame's 0.5 ScaleBox. Empty optional slots (Tabs, SidePanel, Footer) collapse with their rule.
// Per instance: TitleText, WindowIcon (the emblem), WindowWidth (the standard width in art pixels; the window grows past it when its content
// needs more) and WindowMinHeight. Binds, all optional: Title, Emblem, WindowSize (SizeBox), Tabs, Content, SidePanel, Footer (NamedSlots),
// FooterRule. There is no close button: Esc closes the top window (USFWindowStack).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFWindowFrameWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetTitle(const FText& InTitle);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetWindowIcon(const FSlateBrush& Brush);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetWindowWidth(float ArtPixels);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) FText TitleText;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) FSlateBrush WindowIcon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ClampMin = "0")) float WindowWidth = 1360.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ClampMin = "0")) float WindowMinHeight = 0.f;
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TitleStyle = FSFUiTextStyle(60, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Title;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> Emblem;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> WindowSize;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UNamedSlot> Tabs;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UNamedSlot> Content;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UNamedSlot> SidePanel;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UNamedSlot> Footer;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> FooterRule;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFTextFieldChanged, const FText&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSFTextFieldCommitted, const FText&, Text, ETextCommit::Type, CommitMethod);

// Search or input field (WBP_SearchField, WBP_InputField share this parent). Binds: Input (EditableText, required). bDigitsOnly drops every
// non-digit as it is typed (quantity and price fields). HintText is the empty-field hint.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFTextFieldWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFTextFieldChanged OnTextChanged;
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFTextFieldCommitted OnTextCommitted;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) FText HintText;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF") bool bDigitsOnly = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF") bool bPassword = false;  // masks the text (the sign-in password field)
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetText(const FText& InText);
    UFUNCTION(BlueprintPure, Category = "SF UI") FText GetText() const;
    static FString DigitsOnly(const FString& In);

protected:
    virtual void NativeOnInitialized() override;
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UEditableText> Input;

private:
    UFUNCTION() void HandleChanged(const FText& InText);
    UFUNCTION() void HandleCommitted(const FText& InText, ETextCommit::Type Method);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFDropdownChanged, int32, Index);

// Dropdown field (WBP_Dropdown). Binds: Combo (ComboBoxString, required). The WBP draws dropdown_field and the chevron around it.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFDropdownWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFDropdownChanged OnSelectionChanged;
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetOptions(const TArray<FString>& Options, int32 SelectedIndex = 0);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetSelectedIndex(int32 Index);
    UFUNCTION(BlueprintPure, Category = "SF UI") int32 GetSelectedIndex() const;

protected:
    virtual void NativeOnInitialized() override;
    virtual void ApplyKitStyle() override;  // selected text and list items get the menu text family (the composite font only exists at runtime)
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UComboBoxString> Combo;

private:
    UFUNCTION() void HandleSelection(FString SelectedItem, ESelectInfo::Type SelectionType);
};

// Gold amount: coin icon + grouped number (WBP_GoldDisplay). Binds: AmountText (TextBlock, required); the WBP draws the coin.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFGoldDisplayWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetAmount(int64 Gold);
    UFUNCTION(BlueprintPure, Category = "SF UI") int64 GetAmount() const { return Amount; }
    static FText FormatAmount(int64 Gold);  // 12345 -> "12,345"
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(36, SFHudStyle::Amber());

protected:
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UTextBlock> AmountText;
    int64 Amount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFToggleChanged, bool, bChecked);

// On / off check box (WBP_Toggle) on USFSelectableWidget: the kit select frame (gold, shown while on) and hover frame come from the base, plus Check
// (Image: the icon_check_ok glyph, shown while on). A click flips it and raises OnToggled; SetChecked changes it quietly unless bNotify.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFToggleWidget : public USFSelectableWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFToggleChanged OnToggled;
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetChecked(bool bInChecked, bool bNotify = false);
    UFUNCTION(BlueprintPure, Category = "SF UI") bool IsChecked() const { return bSelected; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void OnSelectedChanged_Implementation(bool bNowSelected) override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Check;

private:
    UFUNCTION() void HandleClicked(USFSelectableWidget* Source);
};

// One settings row (WBP_SettingRow): a label on the left, the control on the right (a WBP_Dropdown, WBP_Stepper or WBP_Toggle put into Control), a
// separator under it. SetHeading turns it into a group heading (Heading text, no label or control). Binds, all optional: Label, Heading (TextBlocks),
// Control (NamedSlot), RowBody (the label + control line, hidden for a heading), HeadingBox (shown for a heading).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFSettingRowWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetLabel(const FText& InLabel);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetHeading(const FText& InHeading);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetControl(UWidget* InControl);
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle LabelStyle = FSFUiTextStyle(36, SFHudStyle::Ivory());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle HeadingStyle = FSFUiTextStyle(44, SFHudStyle::Brass());

protected:
    virtual void NativeOnInitialized() override;
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Label;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Heading;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UNamedSlot> Control;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> RowBody;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> HeadingBox;
};
