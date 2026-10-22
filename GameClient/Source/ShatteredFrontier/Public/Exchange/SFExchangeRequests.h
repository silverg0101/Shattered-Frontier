#pragma once

#include "Exchange/SFExchangeModel.h"
#include "Services/SFGameplayServicesClient.h"

// Builds the typed Gameplay.Host requests for the trading UI's intents (proto/sf/exchange/v1). Pure functions: the server-side
// caller sends them through FSFGameplayServicesClient::Call behind USFGameplayServicesSettings::bExchangeEnabled, and item
// changes ride the backpack's FIFO (USFBackpackComponent::Enqueue) so the container revision stays consistent.
// Operation ids are derived from the trade or listing, so a retried intent is idempotent server-side (exactly-once).
struct FSFExchangeParties
{
    FString MyId, TheirId;                          // character ids
    FString MyContainerId, TheirContainerId;        // backpacks
    FString MyEscrowId, TheirEscrowId;              // escrow containers the offered items move into
};

namespace SFExchangeRequests
{
    SHATTEREDFRONTIER_API FSFGameplayRequest Wallet(const FString& CharacterId);
    // party_a is the initiator; each side's items are delivered from its own escrow to the other side's backpack.
    SHATTEREDFRONTIER_API FSFGameplayRequest Offer(const FSFTradeSnapshot& Trade, const FSFExchangeParties& Parties);
    SHATTEREDFRONTIER_API FSFGameplayRequest Accept(const FString& TradeId, const FString& ActorId);
    SHATTEREDFRONTIER_API FSFGameplayRequest Cancel(const FString& TradeId, const FString& ActorId);
    SHATTEREDFRONTIER_API FSFGameplayRequest List(const FSFExchangeParties& Parties, const FString& ListingId, const FString& ItemId, int32 Quantity, int64 UnitPrice, int64 ListingFee, int64 NowUnixMs, const FString& MarketId = TEXT("market/silverbrook"));
    SHATTEREDFRONTIER_API FSFGameplayRequest Buy(const FSFListing& Listing, const FString& BuyerId, const FString& DestinationContainerId, int64 TransactionFee);
    SHATTEREDFRONTIER_API FSFGameplayRequest CancelListing(const FString& ListingId, const FString& SellerId);
    // True for an Ok reply the server applied: committed, or a "pending" settlement (an open trade offer awaiting the
    // partner, a new listing). Rejected, Unavailable and any other state never count.
    SHATTEREDFRONTIER_API bool Committed(const FSFGameplayResponse& Response);
    SHATTEREDFRONTIER_API FSFWalletView ToWallet(const FSFGameplayResponse& Response);
}
