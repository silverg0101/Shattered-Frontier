#pragma once

#include "Blueprint/UserWidget.h"
#include "Exchange/SFExchangeModel.h"
#include "SFExchangeWidgets.generated.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UImage;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UWidget;

// Direct trade window (docs/Game-Design/concept-art/UI/v5/trade.png). Display and input only: every decision lives in
// FSFTradeViewModel. The root fills the viewport (40% dim) and the window sits at 26-74% x 14-78%.
UCLASS()
class SHATTEREDFRONTIER_API USFTradeWindow : public UUserWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 SlotCount = 12;  // 4 x 3 per side
    static constexpr double HoldSeconds = 1.0;

    void Bind(TSharedPtr<FSFTradeViewModel> InModel) { Model = InModel; Refresh(); }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    // Widgets are owned by the WidgetTree, which outlives these pointers.
    struct FSideWidgets
    {
        UTextBlock* Name = nullptr;
        TArray<UImage*> Icons;
        TArray<UTextBlock*> Counts;
        TArray<UTextBlock*> Names;
        TArray<UWidget*> Crosses;  // faint cross shown in empty slots
        bool bMine = false;
        FString Signature;
    };

    UFUNCTION() void HandleGoldChanged(const FText& Text);
    UFUNCTION() void HandleLock();
    UFUNCTION() void HandleCancel();
    UFUNCTION() void HandleClose();
    UFUNCTION() void HandleAcceptPressed();
    UFUNCTION() void HandleAcceptReleased();
    void Refresh();
    void BuildSide(UVerticalBox* Column, FSideWidgets& Side, bool bMine);
    void FillSide(const FSFTradeSide& Source, FSideWidgets& Side);

    TSharedPtr<FSFTradeViewModel> Model;
    FSideWidgets Mine, Theirs;
    double HoldStarted = -1.0;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TheirGold;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TheirStatus;
    UPROPERTY(Transient) TObjectPtr<UImage> TheirStatusIcon;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Countdown;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> GoldBox;
    UPROPERTY(Transient) TObjectPtr<UButton> LockButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LockLabel;
    UPROPERTY(Transient) TObjectPtr<UBorder> LockPlate;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LockHint;
    FSlateBrush LockIdleBrush;
    UPROPERTY(Transient) TObjectPtr<UWidget> LockTick;
    UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;
    UPROPERTY(Transient) TObjectPtr<UButton> AcceptButton;
    UPROPERTY(Transient) TObjectPtr<UImage> AcceptRing;
    float HoldFraction = 0.f;
};

// Market board (docs/Game-Design/concept-art/UI/v5/market-board.png): Browse / My Listings / Sell tabs, a category rail, a
// search field, the listing table and a detail card with a hold-to-buy button; the Sell tab holds the listing form.
UCLASS()
class SHATTEREDFRONTIER_API USFMarketBoard : public UUserWidget
{
    GENERATED_BODY()
public:
    static constexpr double HoldSeconds = 1.0;

    void Bind(TSharedPtr<FSFMarketViewModel> InModel) { Model = InModel; Rebuild(); }
    // The item the sell form offers; the real wiring passes the selected backpack stack.
    void SetSellItem(const FString& ItemId, const FString& Name, int32 MaxQuantity);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    UFUNCTION() void HandleSearchChanged(const FText& Text);
    UFUNCTION() void HandleQuantityChanged(const FText& Text);
    UFUNCTION() void HandlePriceChanged(const FText& Text);
    UFUNCTION() void HandleList();
    UFUNCTION() void HandleClose();
    UFUNCTION() void HandleBuyPressed();
    UFUNCTION() void HandleBuyReleased();
    void Rebuild();
    void Style();
    void ShowTab(int32 Tab);
    const FSFListing* Selected(const TArray<FSFListing>& Visible) const;

    TSharedPtr<FSFMarketViewModel> Model;
    FString SellItemId, SellItemName;
    int32 SellMaxQuantity = 0;
    int32 ActiveTab = 0;  // 0 Browse, 1 My Listings (disabled), 2 Sell
    int32 ActiveRail = 0;  // 0 All Items, 1 Weapons, 2 Materials, 3 Consumables, 4 Tools
    FString RowSignature, SelectedListingId;
    double HoldStarted = -1.0;
    TArray<FString> RowListingIds;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> TopTabs;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> RailRows;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> RowBorders;
    UPROPERTY(Transient) TObjectPtr<UBorder> HeaderItem;
    UPROPERTY(Transient) TObjectPtr<UBorder> HeaderPrice;
    UPROPERTY(Transient) TObjectPtr<UWidget> BrowsePane;
    UPROPERTY(Transient) TObjectPtr<UWidget> SellPane;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FeeText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SellItemText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Rows;
    UPROPERTY(Transient) TObjectPtr<UImage> DetailIcon;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailName;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailQty;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailPrice;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailSeller;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailFee;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailWallet;
    UPROPERTY(Transient) TObjectPtr<UButton> BuyButton;
    UPROPERTY(Transient) TObjectPtr<UImage> BuyRing;
    float HoldFraction = 0.f;
    UPROPERTY(Transient) TObjectPtr<UButton> ListButton;
};
