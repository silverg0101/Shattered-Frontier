#pragma once

#include "Blueprint/UserWidget.h"
#include "Exchange/SFExchangeModel.h"
#include "SFExchangeWidgets.generated.h"

class UBorder;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

// Direct trade window (concept: HUD-v1/hud-trade.png). Display and input only: every decision lives in FSFTradeViewModel.
UCLASS()
class SHATTEREDFRONTIER_API USFTradeWindow : public UUserWidget
{
    GENERATED_BODY()
public:
    void Bind(TSharedPtr<FSFTradeViewModel> InModel) { Model = InModel; Refresh(); }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    UFUNCTION() void HandleGoldChanged(const FText& Text);
    void Refresh();
    void FillSide(const FSFTradeSide& Side, UTextBlock* Name, UTextBlock* Status, UVerticalBox* Items, FString& Signature);

    TSharedPtr<FSFTradeViewModel> Model;
    FString MySignature, TheirSignature;  // item rows are rebuilt only when these change
    UPROPERTY(Transient) TObjectPtr<UTextBlock> MyName;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TheirName;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> MyStatus;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TheirStatus;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TheirGold;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Countdown;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> MyItems;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> TheirItems;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> GoldBox;
    UPROPERTY(Transient) TObjectPtr<UBorder> LockButton;
    UPROPERTY(Transient) TObjectPtr<UBorder> AcceptButton;
    UPROPERTY(Transient) TObjectPtr<UBorder> CancelButton;
};

// Market board (concept: HUD-v1/hud-market.png): category tabs, search, sort, listing rows with Buy, and a sell panel.
UCLASS()
class SHATTEREDFRONTIER_API USFMarketBoard : public UUserWidget
{
    GENERATED_BODY()
public:
    void Bind(TSharedPtr<FSFMarketViewModel> InModel) { Model = InModel; Rebuild(); }
    // The item the sell panel offers; the real wiring passes the selected backpack stack.
    void SetSellItem(const FString& ItemId, const FString& Name, int32 MaxQuantity);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    UFUNCTION() void HandleSearchChanged(const FText& Text);
    UFUNCTION() void HandleQuantityChanged(const FText& Text);
    UFUNCTION() void HandlePriceChanged(const FText& Text);
    void Rebuild();
    void StyleTabs();

    TSharedPtr<FSFMarketViewModel> Model;
    FString SellItemId, SellItemName;
    int32 SellMaxQuantity = 0;
    FString RowSignature;
    TArray<FString> RowListingIds;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> TabBorders;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> BuyButtons;
    UPROPERTY(Transient) TObjectPtr<UBorder> SortButton;
    UPROPERTY(Transient) TObjectPtr<UBorder> ListButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SortLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WalletText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FeeText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SellItemText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Rows;
};
