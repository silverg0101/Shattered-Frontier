#include "Exchange/SFExchangeRequests.h"

namespace
{
FSFGameplayRequest ExchangeBase(ESFGameplayRpc Rpc, const FString& OperationId, const FString& CharacterId)
{
    FSFGameplayRequest Q; Q.Rpc = Rpc; Q.OperationId = OperationId; Q.CharacterId = CharacterId; return Q;
}
}

FSFGameplayRequest SFExchangeRequests::Wallet(const FString& CharacterId)
{
    return ExchangeBase(ESFGameplayRpc::ExchangeWallet, FString(), CharacterId);
}

FSFGameplayRequest SFExchangeRequests::Offer(const FSFTradeSnapshot& Trade, const FSFExchangeParties& Parties)
{
    FSFGameplayRequest Q = ExchangeBase(ESFGameplayRpc::OfferTrade, FString::Printf(TEXT("trade/%s/offer"), *Trade.TradeId), Parties.MyId);
    Q.Exchange.TradeId = Trade.TradeId; Q.Exchange.PartyA = Parties.MyId; Q.Exchange.PartyB = Parties.TheirId;
    Q.Exchange.GoldA = Trade.Mine.Gold; Q.Exchange.GoldB = Trade.Theirs.Gold;
    for (const FSFTradeStack& Stack : Trade.Mine.Items) { Q.Exchange.Deliveries.Add({Parties.MyEscrowId, Parties.TheirContainerId, Stack.ItemId, Stack.Count}); }
    for (const FSFTradeStack& Stack : Trade.Theirs.Items) { Q.Exchange.Deliveries.Add({Parties.TheirEscrowId, Parties.MyContainerId, Stack.ItemId, Stack.Count}); }
    return Q;
}

FSFGameplayRequest SFExchangeRequests::Accept(const FString& TradeId, const FString& ActorId)
{
    FSFGameplayRequest Q = ExchangeBase(ESFGameplayRpc::AcceptTrade, FString::Printf(TEXT("trade/%s/accept/%s"), *TradeId, *ActorId), ActorId);
    Q.Exchange.TradeId = TradeId; Q.Exchange.OfferOperationId = FString::Printf(TEXT("trade/%s/offer"), *TradeId);
    return Q;
}

FSFGameplayRequest SFExchangeRequests::Cancel(const FString& TradeId, const FString& ActorId)
{
    FSFGameplayRequest Q = ExchangeBase(ESFGameplayRpc::CancelTrade, FString::Printf(TEXT("trade/%s/cancel/%s"), *TradeId, *ActorId), ActorId);
    Q.Exchange.TradeId = TradeId; Q.Exchange.OfferOperationId = FString::Printf(TEXT("trade/%s/offer"), *TradeId);
    return Q;
}

FSFGameplayRequest SFExchangeRequests::List(const FSFExchangeParties& Parties, const FString& ListingId, const FString& ItemId, int32 Quantity, int64 UnitPrice, int64 ListingFee, int64 NowUnixMs)
{
    FSFGameplayRequest Q = ExchangeBase(ESFGameplayRpc::ListItem, FString::Printf(TEXT("list/%s"), *ListingId), Parties.MyId);
    Q.Exchange.ListingId = ListingId; Q.Exchange.SellerId = Parties.MyId; Q.Exchange.SourceContainerId = Parties.MyContainerId;
    Q.Exchange.EscrowContainerId = Parties.MyEscrowId; Q.Exchange.ItemId = ItemId; Q.Exchange.Quantity = Quantity;
    Q.Exchange.UnitPrice = UnitPrice; Q.Exchange.Fee = ListingFee; Q.Exchange.CreatedAtUnixMs = NowUnixMs;
    return Q;
}

FSFGameplayRequest SFExchangeRequests::Buy(const FSFListing& Listing, const FString& BuyerId, const FString& DestinationContainerId, int64 TransactionFee)
{
    // The buyer id is part of the operation id: one buyer's retry is idempotent, a second buyer is a different operation.
    FSFGameplayRequest Q = ExchangeBase(ESFGameplayRpc::BuyListing, FString::Printf(TEXT("buy/%s/%s"), *Listing.ListingId, *BuyerId), BuyerId);
    Q.Exchange.ListingId = Listing.ListingId; Q.Exchange.DestinationContainerId = DestinationContainerId;
    Q.Exchange.ExpectedRevision = Listing.Revision; Q.Exchange.ExpectedUnitPrice = Listing.UnitPrice; Q.Exchange.Fee = TransactionFee;
    return Q;
}

FSFGameplayRequest SFExchangeRequests::CancelListing(const FString& ListingId, const FString& SellerId)
{
    FSFGameplayRequest Q = ExchangeBase(ESFGameplayRpc::CancelListing, FString::Printf(TEXT("listing-cancel/%s"), *ListingId), SellerId);
    Q.Exchange.ListingId = ListingId; Q.Exchange.SellerId = SellerId;
    return Q;
}

bool SFExchangeRequests::Committed(const FSFGameplayResponse& Response)
{
    // Gameplay.Host sets committed only when the settlement state is "committed"; an offer it opened replies "pending".
    return Response.Status == ESFGameplayStatus::Ok && (Response.Exchange.bCommitted || Response.Exchange.State == TEXT("pending"));
}

FSFWalletView SFExchangeRequests::ToWallet(const FSFGameplayResponse& Response)
{
    return {Response.Exchange.Balance, Response.Exchange.Reserved, Response.Exchange.Available};
}
