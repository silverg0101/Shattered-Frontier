#pragma once

#include "CoreMinimal.h"
#include "Inventory/SFInventoryPanel.h"

// View-model layer for the trading UI. No UObjects and no gRPC here: widgets bind to these classes, and the real
// Exchange client (Services/) or a test fake implements ISFExchangeService. Amounts are whole gold, int64, as in
// proto/sf/exchange/v1.

namespace SFGold
{
    constexpr int64 MaxAmount = 1000000000;
    // Keeps only ASCII digits, so a field can never hold a sign, decimal point or letters.
    SHATTEREDFRONTIER_API FString Sanitize(const FString& Text);
    // True for an empty field (0) or digits within [0, Max]; Out is 0 when false.
    SHATTEREDFRONTIER_API bool Parse(const FString& Text, int64 Max, int64& Out);
    // 1240 -> "1,240"
    SHATTEREDFRONTIER_API FString Format(int64 Amount);
    // Whole-gold fee at a basis-point rate, rounded up so a non-zero price never rides free.
    SHATTEREDFRONTIER_API int64 FeeFor(int64 Amount, int32 BasisPoints);
}

struct FSFTradeStack { FString ItemId; FString Name; int32 Count = 0; };
struct FSFWalletView { int64 Balance = 0, Reserved = 0, Available = 0; };

enum class ESFTradeState : uint8 { Open, Completed, Cancelled, Expired };

struct FSFTradeSide
{
    FString PlayerName;
    TArray<FSFTradeStack> Items;
    int64 Gold = 0;
    bool bLocked = false;
};

struct FSFTradeSnapshot
{
    FString TradeId;
    FSFTradeSide Mine, Theirs;
    double ExpiresAtSeconds = 0;
    ESFTradeState State = ESFTradeState::Open;
};

struct FSFListing
{
    FString ListingId, ItemId, ItemName, SellerName;
    ESFInventoryTab Category = ESFInventoryTab::Materials;  // never All
    int32 Quantity = 0;
    int64 UnitPrice = 0;
    int64 Revision = 0;
};

enum class ESFMarketSort : uint8 { PriceAscending, PriceDescending, Name };

struct FSFMarketFilter
{
    ESFInventoryTab Tab = ESFInventoryTab::All;
    FString Search;
    ESFMarketSort Sort = ESFMarketSort::PriceAscending;
};

// Everything the UI may ask of the Exchange backend. Each call returns true when the server committed it.
class SHATTEREDFRONTIER_API ISFExchangeService
{
public:
    virtual ~ISFExchangeService() = default;
    virtual FSFWalletView GetWallet() = 0;
    virtual bool OfferTrade(const FSFTradeSnapshot& Offer) = 0;
    virtual bool AcceptTrade(const FString& TradeId) = 0;
    virtual bool CancelTrade(const FString& TradeId) = 0;
    virtual bool ListItem(const FString& ItemId, int32 Quantity, int64 UnitPrice, int64 ListingFee) = 0;
    virtual bool BuyListing(const FSFListing& Listing, int64 TransactionFee) = 0;
    virtual bool CancelListing(const FString& ListingId) = 0;
};

// Direct trade window state. Editing your side after locking is refused; editing either side unlocks both, so a partner
// can never accept an offer that changed after they locked it.
class SHATTEREDFRONTIER_API FSFTradeViewModel
{
public:
    FSFTradeViewModel(ISFExchangeService& InService, FSFTradeSnapshot Initial, bool bInInitiator);

    const FSFTradeSnapshot& Snapshot() const { return Trade; }
    const FString& MyGoldText() const { return GoldText; }
    bool IsGoldValid() const { return bGoldValid; }

    // Returns false when the text is invalid or the side is locked; the field text is always sanitised.
    bool SetMyGoldText(const FString& Text);
    bool AddMyItem(const FSFTradeStack& Stack);
    bool RemoveMyItem(int32 Index);

    bool CanLock() const;
    bool Lock();
    bool CanAccept() const;
    bool Accept();
    bool Cancel();

    // Server pushes: the partner changed their offer (unlocks both) or locked/unlocked.
    void OnPartnerOfferChanged(const TArray<FSFTradeStack>& Items, int64 Gold);
    void OnPartnerLocked(bool bLocked);

    void Tick(double NowSeconds);
    double RemainingSeconds(double NowSeconds) const { return FMath::Max(0.0, Trade.ExpiresAtSeconds - NowSeconds); }
    static FString FormatCountdown(double Seconds);  // "01:48"
    static const TCHAR* StatusLine(const FSFTradeSide& Side, ESFTradeState State);

private:
    void UnlockBoth() { Trade.Mine.bLocked = false; Trade.Theirs.bLocked = false; }

    ISFExchangeService& Service;
    FSFTradeSnapshot Trade;
    bool bInitiator;
    FString GoldText;
    bool bGoldValid = true;
};

// Market board state: filtered, searched and sorted listings, affordability, buying and a listing draft.
class SHATTEREDFRONTIER_API FSFMarketViewModel
{
public:
    explicit FSFMarketViewModel(ISFExchangeService& InService, int32 InFeeBasisPoints = 500)
        : Service(InService), FeeBasisPoints(InFeeBasisPoints) { WalletState = Service.GetWallet(); }

    void SetListings(TArray<FSFListing> InListings) { Listings = MoveTemp(InListings); }
    FSFMarketFilter& Filter() { return FilterState; }
    TArray<FSFListing> Visible() const;
    static TArray<FSFListing> Apply(const TArray<FSFListing>& All, const FSFMarketFilter& Filter);

    const FSFWalletView& GetWallet() const { return WalletState; }
    int64 BuyFee(const FSFListing& Listing) const { return SFGold::FeeFor(Listing.UnitPrice * Listing.Quantity, FeeBasisPoints); }
    bool CanBuy(const FSFListing& Listing) const;
    bool Buy(const FSFListing& Listing);

    // Listing draft: quantity and price fields accept digits only and must be positive; the fee is previewed.
    bool SetSellQuantityText(const FString& Text, int32 MaxQuantity);
    bool SetSellPriceText(const FString& Text);
    int64 ListingFee() const { return SFGold::FeeFor(SellPrice * SellQuantity, FeeBasisPoints); }
    bool CanList(int64 AvailableToPayFee) const { return SellQuantity > 0 && SellPrice > 0 && AvailableToPayFee >= ListingFee(); }
    bool ListSelected(const FString& ItemId, int64 AvailableToPayFee);

private:
    ISFExchangeService& Service;
    int32 FeeBasisPoints;
    FSFWalletView WalletState;
    TArray<FSFListing> Listings;
    FSFMarketFilter FilterState;
    int32 SellQuantity = 0;
    int64 SellPrice = 0;
};
