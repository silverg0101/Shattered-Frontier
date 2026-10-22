#pragma once

#include "Exchange/SFExchangeModel.h"

// In-memory ISFExchangeService: records what the UI asked for and commits unless bCommit is false. Used by the
// view-model tests and by the -ExchangeUiDemo screens (F6 trade, F7 market) until the real client is wired.
class FSFFakeExchange : public ISFExchangeService
{
public:
    FSFWalletView Wallet{1240, 0, 1240};
    bool bCommit = true;
    int32 Offers = 0, Accepts = 0, Cancels = 0, Lists = 0, Buys = 0;
    int64 LastBuyFee = 0, LastListFee = 0;
    FSFWalletView GetWallet() override { return Wallet; }
    void OfferTrade(const FSFTradeSnapshot&, FDone Done) override { ++Offers; Done(bCommit); }
    void AcceptTrade(const FString&, FDone Done) override { ++Accepts; Done(bCommit); }
    void CancelTrade(const FString&, FDone Done) override { ++Cancels; Done(true); }
    void ListItem(const FString&, int32, int64, int64 Fee, FDone Done) override { ++Lists; LastListFee = Fee; if (bCommit) { Wallet.Available -= Fee; } Done(bCommit); }
    void BuyListing(const FSFListing& L, int64 Fee, FDone Done) override { ++Buys; LastBuyFee = Fee; if (bCommit) { Wallet.Available -= L.UnitPrice * L.Quantity + Fee; } Done(bCommit); }
    void CancelListing(const FString&, FDone Done) override { Done(true); }

    // Demo content in the concept's shape (HUD-v1/hud-trade.png and hud-market.png).
    static FSFTradeSnapshot DemoTrade(double ExpiresAtSeconds)
    {
        FSFTradeSnapshot Trade; Trade.TradeId = TEXT("demo-trade"); Trade.ExpiresAtSeconds = ExpiresAtSeconds;
        Trade.Mine.PlayerName = TEXT("Your offer"); Trade.Theirs.PlayerName = TEXT("Rowan");
        Trade.Theirs.Items = {{TEXT("item/iron_ingot"), TEXT("Iron Ingot"), 6}, {TEXT("item/boar_hide"), TEXT("Boar Hide"), 3}};
        Trade.Theirs.Gold = 50; Trade.Theirs.bLocked = false;
        return Trade;
    }
    static TArray<FSFListing> DemoListings()
    {
        auto Make = [](const TCHAR* Id, const TCHAR* Name, ESFInventoryTab Category, int32 Quantity, int64 Price, const TCHAR* Seller, int32 TimeLeft = 0)
        { FSFListing L; L.ListingId = Id; L.ItemId = FString(TEXT("item/")) + FString(Name).ToLower().Replace(TEXT(" "), TEXT("_")); L.ItemName = Name; L.Category = Category; L.Quantity = Quantity; L.UnitPrice = Price; L.SellerName = Seller; L.TimeLeftSeconds = TimeLeft; return L; };
        return {Make(TEXT("l1"), TEXT("Oak Log"), ESFInventoryTab::Materials, 24, 5, TEXT("Rowan"), 41280), Make(TEXT("l2"), TEXT("Iron Ingot"), ESFInventoryTab::Materials, 6, 40, TEXT("Edda"), 32400),
            Make(TEXT("l3"), TEXT("Rough Timber"), ESFInventoryTab::Materials, 8, 18, TEXT("Hale"), 13440), Make(TEXT("l4"), TEXT("Boar Hide"), ESFInventoryTab::Materials, 3, 25, TEXT("Rowan"), 61200),
            Make(TEXT("l5"), TEXT("Iron Axe"), ESFInventoryTab::Gear, 1, 200, TEXT("Edda"), 41280), Make(TEXT("l6"), TEXT("Stew"), ESFInventoryTab::Consumables, 4, 12, TEXT("Mara"), 3600)};
    }
};
