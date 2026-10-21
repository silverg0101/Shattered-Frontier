#include "Exchange/SFExchangeComponent.h"
#include "Exchange/SFExchangeRequests.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExchangeRequestsTest,
    "ShatteredFrontier.Exchange.Requests",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFExchangeRequestsTest::RunTest(const FString& Parameters)
{
    FSFExchangeParties Parties; Parties.MyId=TEXT("char/a"); Parties.TheirId=TEXT("char/b"); Parties.MyContainerId=TEXT("bag/a"); Parties.TheirContainerId=TEXT("bag/b");
    Parties.MyEscrowId=TEXT("escrow/a"); Parties.TheirEscrowId=TEXT("escrow/b");
    FSFTradeSnapshot Trade; Trade.TradeId=TEXT("trade-1");
    Trade.Mine.Gold=30; Trade.Mine.Items={{TEXT("item/oak_log"),TEXT("Oak Log"),4}};
    Trade.Theirs.Gold=0; Trade.Theirs.Items={{TEXT("item/iron_ingot"),TEXT("Iron Ingot"),2}};

    const FSFGameplayRequest Offer=SFExchangeRequests::Offer(Trade,Parties);
    TestTrue(TEXT("Offer rpc and parties"),Offer.Rpc==ESFGameplayRpc::OfferTrade && Offer.Exchange.PartyA==TEXT("char/a") && Offer.Exchange.PartyB==TEXT("char/b"));
    TestTrue(TEXT("Offer gold"),Offer.Exchange.GoldA==30 && Offer.Exchange.GoldB==0);
    if (TestEqual(TEXT("One delivery per stack"),Offer.Exchange.Deliveries.Num(),2))
    {
        TestTrue(TEXT("My item goes from my escrow to their backpack"),Offer.Exchange.Deliveries[0].EscrowId==TEXT("escrow/a") && Offer.Exchange.Deliveries[0].DestinationId==TEXT("bag/b") && Offer.Exchange.Deliveries[0].Quantity==4);
        TestTrue(TEXT("Their item goes from their escrow to my backpack"),Offer.Exchange.Deliveries[1].EscrowId==TEXT("escrow/b") && Offer.Exchange.Deliveries[1].DestinationId==TEXT("bag/a"));
    }
    TestEqual(TEXT("Offer operation id is derived from the trade"),Offer.OperationId,FString(TEXT("trade/trade-1/offer")));
    TestEqual(TEXT("A retried offer reuses the same operation id"),SFExchangeRequests::Offer(Trade,Parties).OperationId,Offer.OperationId);

    const FSFGameplayRequest Accept=SFExchangeRequests::Accept(TEXT("trade-1"),TEXT("char/b"));
    TestTrue(TEXT("Accept references the offer"),Accept.Rpc==ESFGameplayRpc::AcceptTrade && Accept.Exchange.OfferOperationId==Offer.OperationId && Accept.OperationId==TEXT("trade/trade-1/accept/char/b"));
    TestTrue(TEXT("Cancel rpc"),SFExchangeRequests::Cancel(TEXT("trade-1"),TEXT("char/a")).Rpc==ESFGameplayRpc::CancelTrade);

    FSFListing Listing; Listing.ListingId=TEXT("l1"); Listing.Quantity=24; Listing.UnitPrice=5; Listing.Revision=7;
    const FSFGameplayRequest Buy=SFExchangeRequests::Buy(Listing,TEXT("char/b"),TEXT("bag/b"),6);
    TestTrue(TEXT("Buy carries the expected revision, price and fee"),Buy.Exchange.ExpectedRevision==7 && Buy.Exchange.ExpectedUnitPrice==5 && Buy.Exchange.Fee==6 && Buy.Exchange.DestinationContainerId==TEXT("bag/b"));
    TestNotEqual(TEXT("A second buyer is a different operation"),SFExchangeRequests::Buy(Listing,TEXT("char/c"),TEXT("bag/c"),6).OperationId,Buy.OperationId);
    const FSFGameplayRequest List=SFExchangeRequests::List(Parties,TEXT("l9"),TEXT("item/oak_log"),4,30,6,1000);
    TestTrue(TEXT("List moves the item from the backpack into escrow"),List.Rpc==ESFGameplayRpc::ListItem && List.Exchange.SourceContainerId==TEXT("bag/a") && List.Exchange.EscrowContainerId==TEXT("escrow/a") && List.Exchange.UnitPrice==30 && List.Exchange.Fee==6);

    FSFGameplayResponse Reply; Reply.Status=ESFGameplayStatus::Ok; Reply.Exchange.bCommitted=true; Reply.Exchange.Balance=100; Reply.Exchange.Reserved=30; Reply.Exchange.Available=70;
    TestTrue(TEXT("Ok and committed counts"),SFExchangeRequests::Committed(Reply));
    TestEqual(TEXT("Wallet from a reply"),SFExchangeRequests::ToWallet(Reply).Available,(int64)70);
    Reply.Status=ESFGameplayStatus::Unavailable;
    TestFalse(TEXT("Unavailable never counts as committed"),SFExchangeRequests::Committed(Reply));
    Reply.Status=ESFGameplayStatus::Ok; Reply.Exchange.bCommitted=false;
    TestFalse(TEXT("An Ok reply that did not commit does not count"),SFExchangeRequests::Committed(Reply));
    Reply.Exchange.State=TEXT("pending");
    TestTrue(TEXT("An Ok pending settlement (open offer) counts"),SFExchangeRequests::Committed(Reply));
    Reply.Exchange.State=TEXT("cancelled");
    TestFalse(TEXT("An Ok cancelled settlement does not count"),SFExchangeRequests::Committed(Reply));
    Reply.Status=ESFGameplayStatus::Rejected; Reply.Exchange.State=TEXT("pending");
    TestFalse(TEXT("A rejected reply never counts, whatever its state"),SFExchangeRequests::Committed(Reply));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExchangeIntentTest,
    "ShatteredFrontier.Exchange.Intent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFExchangeIntentTest::RunTest(const FString& Parameters)
{
    FSFExchangeParties Parties; Parties.MyId=TEXT("char/a"); Parties.TheirId=TEXT("char/b"); Parties.MyContainerId=TEXT("bag/a"); Parties.TheirContainerId=TEXT("bag/b");
    Parties.MyEscrowId=USFExchangeComponent::EscrowFor(Parties.MyContainerId); Parties.TheirEscrowId=USFExchangeComponent::EscrowFor(Parties.TheirContainerId);
    TestEqual(TEXT("Escrow naming"),Parties.MyEscrowId,FString(TEXT("bag/a:escrow")));
    FSFGameplayRequest Q;
    auto Build=[&](const FSFExchangeIntent& Intent) { return USFExchangeComponent::BuildRequest(Intent,Parties,500,1000,Q); };

    FSFExchangeIntent Offer; Offer.Kind=ESFExchangeIntentKind::OfferTrade; Offer.TradeId=TEXT("t1"); Offer.PartnerId=TEXT("char/b"); Offer.TheirGold=10;
    FSFExchangeStackWire Stack; Stack.ItemId=TEXT("item/oak_log"); Stack.Count=2; Offer.MyItems.Add(Stack);
    TestTrue(TEXT("A valid offer builds"),Build(Offer) && Q.Rpc==ESFGameplayRpc::OfferTrade && Q.Exchange.GoldB==10 && Q.Exchange.Deliveries.Num()==1);
    TestTrue(TEXT("The actor is the server-derived id, not anything the client sent"),Q.CharacterId==TEXT("char/a") && Q.Exchange.PartyA==TEXT("char/a"));
    FSFExchangeIntent Bad=Offer; Parties.TheirId=TEXT("char/a");
    TestFalse(TEXT("Trading with yourself is refused"),Build(Bad));
    Parties.TheirId=TEXT("char/b");
    Bad=Offer; Bad.MyItems.Empty(); Bad.TheirGold=0;
    TestFalse(TEXT("An empty offer is refused"),Build(Bad));
    Bad=Offer; Bad.MyItems[0].Count=0;
    TestFalse(TEXT("A zero-count stack is refused"),Build(Bad));
    Bad=Offer; Bad.TheirGold=SFGold::MaxAmount+1;
    TestFalse(TEXT("Gold above the cap is refused"),Build(Bad));
    Bad=Offer; for (int32 I=0; I<20; ++I) { Bad.MyItems.Add(Stack); }
    TestFalse(TEXT("Too many stacks are refused"),Build(Bad));

    FSFExchangeIntent List; List.Kind=ESFExchangeIntentKind::ListItem; List.ListingId=TEXT("l1"); List.ItemId=TEXT("item/oak_log"); List.Quantity=4; List.UnitPrice=30;
    TestTrue(TEXT("List builds with the server's fee"),Build(List) && Q.Exchange.Fee==6 && Q.Exchange.CreatedAtUnixMs==1000);
    List.UnitPrice=0;
    TestFalse(TEXT("A free listing is refused"),Build(List));

    FSFExchangeIntent Buy; Buy.Kind=ESFExchangeIntentKind::BuyListing; Buy.ListingId=TEXT("l1"); Buy.Quantity=24; Buy.UnitPrice=5; Buy.ExpectedRevision=3;
    TestTrue(TEXT("Buy carries the expected revision and the server's fee"),Build(Buy) && Q.Exchange.ExpectedRevision==3 && Q.Exchange.Fee==6 && Q.Exchange.DestinationContainerId==TEXT("bag/a"));
    FSFExchangeIntent Accept; Accept.Kind=ESFExchangeIntentKind::AcceptTrade;
    TestFalse(TEXT("Accept needs a trade id"),Build(Accept));
    Accept.TradeId=TEXT("t1");
    TestTrue(TEXT("Accept builds"),Build(Accept) && Q.Rpc==ESFGameplayRpc::AcceptTrade);
    FSFExchangeIntent Wallet; Wallet.Kind=ESFExchangeIntentKind::Wallet;
    TestTrue(TEXT("Wallet builds"),Build(Wallet) && Q.Rpc==ESFGameplayRpc::ExchangeWallet && Q.CharacterId==TEXT("char/a"));
    return true;
}
#endif
