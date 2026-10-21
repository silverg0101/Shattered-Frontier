#pragma once

#include "Components/ActorComponent.h"
#include "Exchange/SFExchangeModel.h"
#include "Exchange/SFExchangeRequests.h"
#include "SFExchangeComponent.generated.h"

class APlayerController;

UENUM()
enum class ESFExchangeIntentKind : uint8 { OfferTrade, AcceptTrade, CancelTrade, ListItem, BuyListing, CancelListing, Wallet };

UENUM()
enum class ESFExchangeTradeEvent : uint8 { Offered, Accepted, Cancelled };

USTRUCT()
struct FSFExchangeStackWire { GENERATED_BODY() UPROPERTY() FString ItemId; UPROPERTY() int32 Count = 0; };

// What a client may ask. It carries no identity, escrow, fee or revision of the server's: the server derives the actor from the
// controller, escrow and backpack ids from its own convention, the fee from settings, and the ledger rejects what the
// backpack cannot cover.
USTRUCT()
struct FSFExchangeIntent
{
    GENERATED_BODY()
    UPROPERTY() ESFExchangeIntentKind Kind = ESFExchangeIntentKind::Wallet;
    UPROPERTY() uint32 Nonce = 0;
    UPROPERTY() FString TradeId;
    UPROPERTY() FString PartnerId;                      // OfferTrade: the other party's character id
    UPROPERTY() int64 MyGold = 0;
    UPROPERTY() int64 TheirGold = 0;
    UPROPERTY() TArray<FSFExchangeStackWire> MyItems;
    UPROPERTY() TArray<FSFExchangeStackWire> TheirItems;
    UPROPERTY() FString ListingId;                      // BuyListing, CancelListing, ListItem (the new listing's id)
    UPROPERTY() FString ItemId;                         // ListItem
    UPROPERTY() int32 Quantity = 0;
    UPROPERTY() int64 UnitPrice = 0;                    // ListItem price, or BuyListing's expected price
    UPROPERTY() int64 ExpectedRevision = 0;             // BuyListing
};

USTRUCT()
struct FSFExchangeResultWire
{
    GENERATED_BODY()
    UPROPERTY() uint32 Nonce = 0;
    UPROPERTY() bool bCommitted = false;
    UPROPERTY() FString Message;
    UPROPERTY() int64 Balance = 0;
    UPROPERTY() int64 Reserved = 0;
    UPROPERTY() int64 Available = 0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSFExchangeServerResultDelegate, uint32 /*Nonce*/, bool /*bCommitted*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSFExchangeTradeEventDelegate, ESFExchangeTradeEvent, const FString& /*TradeId*/, const FSFExchangeIntent& /*the offer, from this player's view*/);

// Client intent -> server RPC -> Gameplay.Host Exchange -> result RPC back. One per player controller. Every request rides the
// controller's backpack FIFO (USFBackpackComponent::Enqueue) so item changes and the cached container revision stay consistent.
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API USFExchangeComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFExchangeComponent();
    static USFExchangeComponent* FindOrCreate(APlayerController* Controller);

    // Client: queue an intent; Done(bCommitted) runs when the server's result returns (false on a dropped link or timeout).
    void Send(FSFExchangeIntent Intent, ISFExchangeService::FDone Done);
    FSFWalletView GetWallet() const { return Wallet; }
    FSFExchangeTradeEventDelegate OnTradeEvent;  // partner-side pushes for the trade window
    FSFExchangeServerResultDelegate OnServerResult;  // server: every intent outcome, for server-side callers and the scenario
    void ServerRun(const FSFExchangeIntent& Intent) { ExecuteIntent(Intent); }  // server-side entry (scenario, tests)

    // Server: the Gameplay.Host request for an intent, or false when the intent is malformed. Pure, so it is unit tested.
    static bool BuildRequest(const FSFExchangeIntent& Intent, const FSFExchangeParties& Parties, int32 FeeBasisPoints, int64 NowUnixMs, FSFGameplayRequest& Out);
    // Escrow container for a backpack: "<backpack>:escrow" (the ledger creates it with the backpack).
    static FString EscrowFor(const FString& BackpackContainerId) { return BackpackContainerId + TEXT(":escrow"); }

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;  // -ExchangeScenario driver
#endif

private:
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickScenario();
    int32 ScenarioStep = 0;
    uint32 ScenarioWaitNonce = 0;
    FSFWalletView WalletBeforeReplay;
#endif
    UFUNCTION(Server, Reliable) void ServerIntent(const FSFExchangeIntent& Intent);
    UFUNCTION(Client, Reliable) void ClientResult(const FSFExchangeResultWire& Result);
    UFUNCTION(Client, Reliable) void ClientTradeEvent(ESFExchangeTradeEvent Event, const FString& TradeId, const FSFExchangeIntent& Offer);
    void ExecuteIntent(const FSFExchangeIntent& Intent);
    void Reply(uint32 Nonce, bool bCommitted, const FString& Message, const FSFWalletView& InWallet);
    USFExchangeComponent* FindPartner(const FString& CharacterId) const;
    void NotifyPartner(const FString& PartnerId, ESFExchangeTradeEvent Event, const FSFExchangeIntent& Intent);

    uint32 NextNonce = 1;
    TMap<uint32, ISFExchangeService::FDone> Pending;
    FSFWalletView Wallet;
};

// ISFExchangeService over the component: what the trade window and market board use against a live server.
class SHATTEREDFRONTIER_API FSFExchangeClientService : public ISFExchangeService
{
public:
    explicit FSFExchangeClientService(USFExchangeComponent* InComponent) : Component(InComponent) {}
    FSFWalletView GetWallet() override { return Component.IsValid() ? Component->GetWallet() : FSFWalletView(); }
    void OfferTrade(const FSFTradeSnapshot& Offer, FDone Done) override;
    void AcceptTrade(const FString& TradeId, FDone Done) override;
    void CancelTrade(const FString& TradeId, FDone Done) override;
    void ListItem(const FString& ItemId, int32 Quantity, int64 UnitPrice, int64 ListingFee, FDone Done) override;
    void BuyListing(const FSFListing& Listing, int64 TransactionFee, FDone Done) override;
    void CancelListing(const FString& ListingId, FDone Done) override;
private:
    void Dispatch(FSFExchangeIntent Intent, FDone Done);
    TWeakObjectPtr<USFExchangeComponent> Component;
};
