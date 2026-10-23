#pragma once

#include "Exchange/SFExchangeModel.h"
#include "UI/SFItemTooltip.h"
#include "UI/SFUiWidgets.h"
#include "SFExchangeWidgets.generated.h"

class UImage;
class UPanelWidget;
class UTextBlock;

// Direct trade window (WBP_TradeWindow, docs/blueprints/menus.md section 0): a WBP_WindowFrame (emblem icon_window_trade, title "Direct Trade")
// with your offer and theirs side by side (12 WBP_ItemSlot cells each, a gold line, the lock state), Cancel Trade, a review line and the
// hold-to-accept button in the footer. Display and input only: every decision lives in FSFTradeViewModel. Esc hides it (USFWindowStack).
// Binds (required): MySlots, TheirSlots (WrapBox or UniformGridPanel; C++ fills them with one WBP_ItemSlot per offered stack, no empty cells).
// Optional: MyName, TheirName, MyGold (WBP_InputField, digits only), TheirGold (WBP_GoldDisplay), LockButton (WBP_Button: Lock Offer / Offer
// Locked), LockHint, TheirStatus (text, tinted), TheirLockMark, CancelButton, AcceptButton (WBP_Button, hold), StatusText, Tooltip
// with TipIcon, TipTitle, TipRarity, TipType, TipQuantity, TipWeight, TipDescription.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFTradeWindow : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 SlotCount = 12;  // most stacks per side
    static constexpr int32 GridColumns = 4;
    static constexpr double HoldSeconds = 1.0;
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Exchange/WBP_TradeWindow.WBP_TradeWindow_C");

    void Bind(TSharedPtr<FSFTradeViewModel> InModel) { Model = InModel; Refresh(); }
    USFSlotWidget* GetMySlot(int32 Index) const { return MyCells.IsValidIndex(Index) ? MyCells[Index].Get() : nullptr; }
    USFSlotWidget* GetTheirSlot(int32 Index) const { return TheirCells.IsValidIndex(Index) ? TheirCells[Index].Get() : nullptr; }
    USFButtonWidget* GetAcceptButton() const { return AcceptButton; }
    USFButtonWidget* GetLockButton() const { return LockButton; }
    // The lock toggle as a click would do it (the capture and tests call this).
    void ToggleLock() { HandleLock(nullptr); }

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> MySlots;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> TheirSlots;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MyName;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TheirName;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTextFieldWidget> MyGold;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFGoldDisplayWidget> TheirGold;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> LockButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> LockHint;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TheirStatus;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> TheirLockMark;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> CancelButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> AcceptButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Tooltip;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> TipIcon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipRarity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipType;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipQuantity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipWeight;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipDescription;

private:
    UFUNCTION() void HandleGoldChanged(const FText& Text);
    UFUNCTION() void HandleLock(USFButtonWidget* Source);
    UFUNCTION() void HandleCancel(USFButtonWidget* Source);
    UFUNCTION() void HandleAccept(USFButtonWidget* Source);
    void Refresh();
    void EnsureCells(UPanelWidget* Grid, TArray<TObjectPtr<USFSlotWidget>>& Cells, int32 Count);
    void FillCells(const FSFTradeSide& Source, UPanelWidget* Grid, TArray<TObjectPtr<USFSlotWidget>>& Cells, FString& Signature);
    void UpdateTooltip();

    TSharedPtr<FSFTradeViewModel> Model;
    TSubclassOf<USFSlotWidget> SlotClass;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> MyCells;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> TheirCells;
    FString MySignature, TheirSignature;
};

// Market board (WBP_MarketBoard): a WBP_WindowFrame (emblem icon_window_market, title "Market Board") with category tabs, a search field and a
// sort dropdown on top, the listing rows (WBP_KitRow: item, quantity, seller, price, a hold-to-buy button) in a WBP_ScrollList, a "Sell an
// item" side panel (sell slot, quantity and price inputs, fee, List item) and the wallet in the footer. Listings are server data through
// FSFMarketViewModel. Esc hides it (USFWindowStack).
// Binds (required): ListingRows (VerticalBox in the scroll list, left empty: C++ adds one WBP_KitRow per visible listing).
// Optional: TabBar (All / Materials / Gear / Consumables), SearchField, SortDropdown, EmptyText, SellSlot (WBP_ItemSlot), SellEmpty (the
// plus slot shown while no item is offered), SellItemText, QuantityField, PriceField (WBP_InputField, digits only), FeeText, ListButton,
// Gold, Tooltip with TipIcon, TipTitle, TipRarity, TipType, TipQuantity, TipWeight, TipDescription.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFMarketBoard : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr double HoldSeconds = 1.0;
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Exchange/WBP_MarketBoard.WBP_MarketBoard_C");

    void Bind(TSharedPtr<FSFMarketViewModel> InModel) { Model = InModel; Rebuild(); }
    // The item the sell form offers; the real wiring passes the selected backpack stack.
    void SetSellItem(const FString& ItemId, const FString& Name, int32 MaxQuantity);
    int32 RowCount() const { return Rows.Num(); }
    USFKitRowWidget* GetRow(int32 Index) const { return Rows.IsValidIndex(Index) ? Rows[Index].Get() : nullptr; }

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> ListingRows;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabBarWidget> TabBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTextFieldWidget> SearchField;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFDropdownWidget> SortDropdown;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFSlotWidget> SellSlot;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> SellEmpty;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SellItemText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTextFieldWidget> QuantityField;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTextFieldWidget> PriceField;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FeeText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> ListButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFGoldDisplayWidget> Gold;
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
    UFUNCTION() void HandleSearchChanged(const FText& Text);
    UFUNCTION() void HandleSort(int32 Index);
    UFUNCTION() void HandleQuantityChanged(const FText& Text);
    UFUNCTION() void HandlePriceChanged(const FText& Text);
    UFUNCTION() void HandleList(USFButtonWidget* Source);
    UFUNCTION() void HandleRowClicked(USFSelectableWidget* Source);
    UFUNCTION() void HandleBuy(USFButtonWidget* Source);
    void Rebuild();
    void UpdateTooltip();
    const FSFListing* Selected(const TArray<FSFListing>& Visible) const;

    TSharedPtr<FSFMarketViewModel> Model;
    TSubclassOf<USFKitRowWidget> RowClass;
    FString SellItemId, SellItemName;
    int32 SellMaxQuantity = 0;
    FString RowSignature, SelectedListingId;
    TArray<FSFListing> RowListings;  // the listing each row shows, by row index
    UPROPERTY(Transient) TArray<TObjectPtr<USFKitRowWidget>> Rows;
};
