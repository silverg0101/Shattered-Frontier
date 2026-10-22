#include "Exchange/SFFakeExchange.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

namespace
{
FSFTradeSnapshot ExchangeTestTrade()
{
    FSFTradeSnapshot Trade; Trade.TradeId = TEXT("trade-1"); Trade.ExpiresAtSeconds = 100.0;
    Trade.Mine.PlayerName = TEXT("You"); Trade.Theirs.PlayerName = TEXT("Rowan");
    return Trade;
}

FSFListing ExchangeTestListing(const TCHAR* Id, const TCHAR* Name, ESFInventoryTab Category, int32 Quantity, int64 Price)
{
    FSFListing L; L.ListingId = Id; L.ItemId = Id; L.ItemName = Name; L.Category = Category; L.Quantity = Quantity; L.UnitPrice = Price; L.SellerName = TEXT("Seller"); return L;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExchangeGoldFieldTest,
    "ShatteredFrontier.Exchange.ViewModel.GoldField",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFExchangeGoldFieldTest::RunTest(const FString& Parameters)
{
    int64 Value = -1;
    TestTrue(TEXT("Empty field is zero"),SFGold::Parse(TEXT(""),1000,Value) && Value==0);
    TestTrue(TEXT("Plain digits"),SFGold::Parse(TEXT("250"),1000,Value) && Value==250);
    TestTrue(TEXT("Exactly the maximum"),SFGold::Parse(TEXT("1000"),1000,Value) && Value==1000);
    TestFalse(TEXT("Above the maximum"),SFGold::Parse(TEXT("1001"),1000,Value));
    TestFalse(TEXT("Negative"),SFGold::Parse(TEXT("-5"),1000,Value));
    TestFalse(TEXT("Decimal"),SFGold::Parse(TEXT("1.5"),1000,Value));
    TestFalse(TEXT("Letters"),SFGold::Parse(TEXT("12a"),1000,Value));
    TestFalse(TEXT("Overlong digit run"),SFGold::Parse(TEXT("99999999999999999999"),SFGold::MaxAmount,Value));
    TestEqual(TEXT("Sanitise strips non-digits"),SFGold::Sanitize(TEXT("-1,2.5x0")),FString(TEXT("1250")));
    TestEqual(TEXT("Format groups thousands"),SFGold::Format(1240),FString(TEXT("1,240")));
    TestEqual(TEXT("Fee rounds up"),SFGold::FeeFor(30,500),(int64)2);
    TestEqual(TEXT("No fee on nothing"),SFGold::FeeFor(0,500),(int64)0);

    FSFFakeExchange Exchange;
    FSFTradeViewModel Model(Exchange,ExchangeTestTrade(),true);
    TestFalse(TEXT("More than the wallet holds is refused"),Model.SetMyGoldText(TEXT("1241")));
    TestFalse(TEXT("Invalid field flagged"),Model.IsGoldValid());
    TestTrue(TEXT("Typing letters is sanitised away"),Model.SetMyGoldText(TEXT("1a2b")));
    TestEqual(TEXT("Field text is digits only"),Model.MyGoldText(),FString(TEXT("12")));
    TestEqual(TEXT("Offer gold follows the field"),Model.Snapshot().Mine.Gold,(int64)12);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExchangeTradeStatesTest,
    "ShatteredFrontier.Exchange.ViewModel.TradeStates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFExchangeTradeStatesTest::RunTest(const FString& Parameters)
{
    FSFFakeExchange Exchange;
    FSFTradeViewModel Model(Exchange,ExchangeTestTrade(),true);
    TestFalse(TEXT("An empty trade cannot be locked"),Model.CanLock());
    TestTrue(TEXT("Add an item"),Model.AddMyItem({TEXT("item/oak_log"),TEXT("Oak Log"),4}));
    TestTrue(TEXT("Merge a second stack of the same item"),Model.AddMyItem({TEXT("item/oak_log"),TEXT("Oak Log"),2}) && Model.Snapshot().Mine.Items.Num()==1 && Model.Snapshot().Mine.Items[0].Count==6);
    TestTrue(TEXT("Lock sends the offer"),Model.Lock() && Exchange.Offers==1 && Model.Snapshot().Mine.bLocked);
    TestEqual(TEXT("Status after locking"),FString(FSFTradeViewModel::StatusLine(Model.Snapshot().Mine,ESFTradeState::Open)),FString(TEXT("Locked in")));
    TestFalse(TEXT("Cannot edit after locking"),Model.AddMyItem({TEXT("item/iron_ore"),TEXT("Iron Ore"),1}));
    TestFalse(TEXT("Cannot accept until the partner locks"),Model.CanAccept());
    Model.OnPartnerLocked(true);
    TestTrue(TEXT("Both locked allows accept"),Model.CanAccept());
    Model.OnPartnerOfferChanged({{TEXT("item/boar_hide"),TEXT("Boar Hide"),1}},50);
    TestFalse(TEXT("A changed partner offer unlocks both"),Model.Snapshot().Mine.bLocked || Model.Snapshot().Theirs.bLocked || Model.CanAccept());
    TestTrue(TEXT("Relock"),Model.Lock());
    Model.OnPartnerLocked(true);
    TestTrue(TEXT("Accept commits"),Model.Accept() && Exchange.Accepts==1 && Model.Snapshot().State==ESFTradeState::Completed);
    TestFalse(TEXT("A finished trade cannot be cancelled"),Model.Cancel());

    FSFFakeExchange Rejecting; Rejecting.bCommit=false;
    FSFTradeViewModel Refused(Rejecting,ExchangeTestTrade(),true);
    Refused.AddMyItem({TEXT("item/oak_log"),TEXT("Oak Log"),1});
    Refused.Lock();
    TestFalse(TEXT("A rejected offer does not lock"),Refused.Snapshot().Mine.bLocked || Refused.IsPending());

    FSFFakeExchange Counter;
    FSFTradeViewModel Partner(Counter,ExchangeTestTrade(),false);
    Partner.OnPartnerOfferChanged({{TEXT("item/oak_log"),TEXT("Oak Log"),3}},0);
    TestTrue(TEXT("The partner locks locally without calling OfferTrade"),Partner.Lock() && Counter.Offers==0);

    FSFFakeExchange Timing;
    FSFTradeViewModel Timed(Timing,ExchangeTestTrade(),true);
    Timed.AddMyItem({TEXT("item/oak_log"),TEXT("Oak Log"),1}); Timed.Lock();
    TestEqual(TEXT("Countdown format"),FSFTradeViewModel::FormatCountdown(108.0),FString(TEXT("01:48")));
    Timed.Tick(99.0);
    TestEqual(TEXT("Still open before expiry"),Timed.Snapshot().State,ESFTradeState::Open);
    Timed.Tick(100.0);
    TestTrue(TEXT("Expires and unlocks"),Timed.Snapshot().State==ESFTradeState::Expired && !Timed.Snapshot().Mine.bLocked);

    FSFFakeExchange Cancelling;
    FSFTradeViewModel Cancelled(Cancelling,ExchangeTestTrade(),true);
    TestTrue(TEXT("Cancel reaches the service"),Cancelled.Cancel() && Cancelling.Cancels==1 && Cancelled.Snapshot().State==ESFTradeState::Cancelled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExchangeMarketTest,
    "ShatteredFrontier.Exchange.ViewModel.Market",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFExchangeMarketTest::RunTest(const FString& Parameters)
{
    FSFFakeExchange Exchange;
    FSFMarketViewModel Market(Exchange);
    Market.SetListings({
        ExchangeTestListing(TEXT("a"),TEXT("Oak Log"),ESFInventoryTab::Materials,24,5),
        ExchangeTestListing(TEXT("b"),TEXT("Iron Axe"),ESFInventoryTab::Gear,1,200),
        ExchangeTestListing(TEXT("c"),TEXT("Iron Ingot"),ESFInventoryTab::Materials,6,40),
        ExchangeTestListing(TEXT("d"),TEXT("Stew"),ESFInventoryTab::Consumables,3,12),
        ExchangeTestListing(TEXT("e"),TEXT("Empty"),ESFInventoryTab::Materials,0,1)});
    TestEqual(TEXT("Empty listings are hidden, others sorted by price"),Market.Visible().Num(),4);
    TestEqual(TEXT("Cheapest first"),Market.Visible()[0].ListingId,FString(TEXT("a")));
    Market.Filter().Sort=ESFMarketSort::PriceDescending;
    TestEqual(TEXT("Dearest first"),Market.Visible()[0].ListingId,FString(TEXT("b")));
    Market.Filter().Tab=ESFInventoryTab::Materials;
    TestEqual(TEXT("Category tab filters"),Market.Visible().Num(),2);
    Market.Filter().Search=TEXT("INGOT");
    if (TestEqual(TEXT("Search is case-insensitive"),Market.Visible().Num(),1)) { TestEqual(TEXT("Search hit"),Market.Visible()[0].ListingId,FString(TEXT("c"))); }
    Market.Filter()=FSFMarketFilter(); Market.Filter().Sort=ESFMarketSort::Name;
    TestEqual(TEXT("Name sort"),Market.Visible()[0].ItemName,FString(TEXT("Iron Axe")));

    const FSFListing Ingots=ExchangeTestListing(TEXT("c"),TEXT("Iron Ingot"),ESFInventoryTab::Materials,6,40);   // 240 + 5% fee 12
    const FSFListing Dear=ExchangeTestListing(TEXT("z"),TEXT("Gold Crown"),ESFInventoryTab::Gear,1,5000);
    TestEqual(TEXT("Buy fee previewed"),Market.BuyFee(Ingots),(int64)12);
    TestTrue(TEXT("Affordable listing"),Market.CanBuy(Ingots));
    TestFalse(TEXT("Unaffordable listing"),Market.CanBuy(Dear));
    TestFalse(TEXT("Cannot buy what you cannot afford"),Market.Buy(Dear) || Exchange.Buys!=0);
    TestTrue(TEXT("Buy commits with the fee and refreshes the wallet"),Market.Buy(Ingots) && Exchange.Buys==1 && Exchange.LastBuyFee==12 && Market.GetWallet().Available==1240-252);
    TestEqual(TEXT("A bought listing leaves the board"),Market.Visible().Num(),3);

    TestFalse(TEXT("Zero quantity is not listable"),Market.SetSellQuantityText(TEXT("0"),10));
    TestFalse(TEXT("More than held is not listable"),Market.SetSellQuantityText(TEXT("11"),10));
    TestFalse(TEXT("A zero price is not listable"),Market.SetSellPriceText(TEXT("0")));
    TestTrue(TEXT("A typed sign is stripped like the gold field"),Market.SetSellPriceText(TEXT("-3")));
    TestTrue(TEXT("Valid draft"),Market.SetSellQuantityText(TEXT("4"),10) && Market.SetSellPriceText(TEXT("30")));
    TestEqual(TEXT("Listing fee preview"),Market.ListingFee(),(int64)1);
    TestFalse(TEXT("Cannot list without the fee"),Market.CanList(0));
    TestTrue(TEXT("List commits"),Market.ListSelected(TEXT("item/oak_log"),100) && Exchange.Lists==1 && Exchange.LastListFee==1);
    return true;
}

namespace
{
// Completes only when told to, like the server round trip.
class FSFDeferredExchange : public ISFExchangeService
{
public:
    TArray<FDone> Waiting;
    FSFWalletView GetWallet() override { return {1240, 0, 1240}; }
    void OfferTrade(const FSFTradeSnapshot&, FDone Done) override { Waiting.Add(MoveTemp(Done)); }
    void AcceptTrade(const FString&, FDone Done) override { Waiting.Add(MoveTemp(Done)); }
    void CancelTrade(const FString&, FDone Done) override { Done(true); }
    void ListItem(const FString&, int32, int64, int64, FDone Done) override { Waiting.Add(MoveTemp(Done)); }
    void BuyListing(const FSFListing&, int64, FDone Done) override { Waiting.Add(MoveTemp(Done)); }
    void CancelListing(const FString&, FDone Done) override { Done(true); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExchangeAsyncTest,
    "ShatteredFrontier.Exchange.ViewModel.Async",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFExchangeAsyncTest::RunTest(const FString& Parameters)
{
    FSFDeferredExchange Exchange;
    {
        FSFTradeViewModel Model(Exchange,ExchangeTestTrade(),true);
        Model.AddMyItem({TEXT("item/oak_log"),TEXT("Oak Log"),1});
        TestTrue(TEXT("Lock is dispatched"),Model.Lock());
        TestTrue(TEXT("It is pending, not yet locked"),Model.IsPending() && !Model.Snapshot().Mine.bLocked);
        TestFalse(TEXT("A second lock while pending is refused"),Model.Lock() || Model.CanLock());
        Exchange.Waiting[0](true);
        TestTrue(TEXT("The server result locks"),!Model.IsPending() && Model.Snapshot().Mine.bLocked);
        Model.OnPartnerLocked(true);
        TestTrue(TEXT("Accept is dispatched"),Model.Accept() && Model.IsPending() && Model.Snapshot().State==ESFTradeState::Open);
        Exchange.Waiting[1](false);
        TestTrue(TEXT("A rejected accept leaves the trade open and ready"),Model.Snapshot().State==ESFTradeState::Open && !Model.IsPending() && Model.CanAccept());
    }
    // A result arriving after the view-model is gone must not touch it.
    {
        FSFDeferredExchange Gone;
        { FSFTradeViewModel Model(Gone,ExchangeTestTrade(),true); Model.AddMyItem({TEXT("item/oak_log"),TEXT("Oak Log"),1}); Model.Lock(); }
        Gone.Waiting[0](true);  // would crash if the callback still used the destroyed model
        TestTrue(TEXT("Late result is ignored"),true);
    }
    FSFMarketViewModel Market(Exchange);
    Market.SetListings({ExchangeTestListing(TEXT("a"),TEXT("Oak Log"),ESFInventoryTab::Materials,2,5)});
    const FSFListing Oak=Market.Visible()[0];
    TestTrue(TEXT("Buy dispatches"),Market.Buy(Oak));
    TestFalse(TEXT("A second buy while one is in flight is refused"),Market.Buy(Oak));
    TestEqual(TEXT("The listing stays until the server commits"),Market.Visible().Num(),1);
    Exchange.Waiting.Last()(true);
    TestEqual(TEXT("It leaves the board once committed"),Market.Visible().Num(),0);
    return true;
}
#endif
