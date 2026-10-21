#include "Exchange/SFExchangeComponent.h"

#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace
{
constexpr int32 ExchangeMaxStacksPerSide = 16;

// Server-only memory of who is trading with whom, so Accept and Cancel can notify the other side.
TMap<FString, TPair<FString, FString>>& ExchangeTradeParties()
{
    static TMap<FString, TPair<FString, FString>> Parties;
    return Parties;
}

bool ExchangeValidStacks(const TArray<FSFExchangeStackWire>& Stacks)
{
    if (Stacks.Num() > ExchangeMaxStacksPerSide) { return false; }
    for (const FSFExchangeStackWire& Stack : Stacks) { if (Stack.ItemId.IsEmpty() || Stack.Count <= 0) { return false; } }
    return true;
}

TArray<FSFTradeStack> ExchangeToStacks(const TArray<FSFExchangeStackWire>& Wire)
{
    TArray<FSFTradeStack> Out;
    for (const FSFExchangeStackWire& Stack : Wire) { Out.Add({Stack.ItemId, FString(), Stack.Count}); }
    return Out;
}

TArray<FSFExchangeStackWire> ExchangeToWire(const TArray<FSFTradeStack>& Stacks)
{
    TArray<FSFExchangeStackWire> Out;
    for (const FSFTradeStack& Stack : Stacks) { FSFExchangeStackWire Wire; Wire.ItemId = Stack.ItemId; Wire.Count = Stack.Count; Out.Add(Wire); }
    return Out;
}
}

USFExchangeComponent::USFExchangeComponent()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    PrimaryComponentTick.bCanEverTick = true;  // only the -ExchangeScenario driver ticks
#endif
}

USFExchangeComponent* USFExchangeComponent::FindOrCreate(APlayerController* Controller)
{
    if (!Controller) { return nullptr; }
    if (USFExchangeComponent* Existing = Controller->FindComponentByClass<USFExchangeComponent>()) { return Existing; }
    if (!Controller->HasAuthority()) { return nullptr; }  // clients receive the replicated component
    USFExchangeComponent* Component = NewObject<USFExchangeComponent>(Controller, TEXT("ExchangeComponent"));
    Component->SetIsReplicated(true);
    Component->RegisterComponent();
    return Component;
}

void USFExchangeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
}

bool USFExchangeComponent::BuildRequest(const FSFExchangeIntent& Intent, const FSFExchangeParties& Parties, int32 FeeBasisPoints, int64 NowUnixMs, FSFGameplayRequest& Out)
{
    auto GoldOk = [](int64 Gold) { return Gold >= 0 && Gold <= SFGold::MaxAmount; };
    switch (Intent.Kind)
    {
    case ESFExchangeIntentKind::Wallet:
        Out = SFExchangeRequests::Wallet(Parties.MyId);
        return !Parties.MyId.IsEmpty();
    case ESFExchangeIntentKind::OfferTrade:
    {
        if (Intent.TradeId.IsEmpty() || Parties.TheirId.IsEmpty() || Parties.TheirId == Parties.MyId) { return false; }
        if (!GoldOk(Intent.MyGold) || !GoldOk(Intent.TheirGold) || !ExchangeValidStacks(Intent.MyItems) || !ExchangeValidStacks(Intent.TheirItems)) { return false; }
        if (Intent.MyGold == 0 && Intent.TheirGold == 0 && Intent.MyItems.IsEmpty() && Intent.TheirItems.IsEmpty()) { return false; }
        FSFTradeSnapshot Trade; Trade.TradeId = Intent.TradeId;
        Trade.Mine.Gold = Intent.MyGold; Trade.Mine.Items = ExchangeToStacks(Intent.MyItems);
        Trade.Theirs.Gold = Intent.TheirGold; Trade.Theirs.Items = ExchangeToStacks(Intent.TheirItems);
        Out = SFExchangeRequests::Offer(Trade, Parties);
        return true;
    }
    case ESFExchangeIntentKind::AcceptTrade:
        if (Intent.TradeId.IsEmpty()) { return false; }
        Out = SFExchangeRequests::Accept(Intent.TradeId, Parties.MyId);
        return true;
    case ESFExchangeIntentKind::CancelTrade:
        if (Intent.TradeId.IsEmpty()) { return false; }
        Out = SFExchangeRequests::Cancel(Intent.TradeId, Parties.MyId);
        return true;
    case ESFExchangeIntentKind::ListItem:
    {
        if (Intent.ListingId.IsEmpty() || Intent.ItemId.IsEmpty() || Intent.Quantity <= 0 || Intent.UnitPrice <= 0 || !GoldOk(Intent.UnitPrice)) { return false; }
        // The fee is the server's: the client's preview is never trusted.
        Out = SFExchangeRequests::List(Parties, Intent.ListingId, Intent.ItemId, Intent.Quantity, Intent.UnitPrice, SFGold::FeeFor(Intent.UnitPrice * Intent.Quantity, FeeBasisPoints), NowUnixMs);
        return true;
    }
    case ESFExchangeIntentKind::BuyListing:
    {
        if (Intent.ListingId.IsEmpty() || Intent.Quantity <= 0 || Intent.UnitPrice <= 0) { return false; }
        FSFListing Listing; Listing.ListingId = Intent.ListingId; Listing.Quantity = Intent.Quantity; Listing.UnitPrice = Intent.UnitPrice; Listing.Revision = Intent.ExpectedRevision;
        Out = SFExchangeRequests::Buy(Listing, Parties.MyId, Parties.MyContainerId, SFGold::FeeFor(Intent.UnitPrice * Intent.Quantity, FeeBasisPoints));
        return true;
    }
    case ESFExchangeIntentKind::CancelListing:
        if (Intent.ListingId.IsEmpty()) { return false; }
        Out = SFExchangeRequests::CancelListing(Intent.ListingId, Parties.MyId);
        return true;
    }
    return false;
}

void USFExchangeComponent::Send(FSFExchangeIntent Intent, ISFExchangeService::FDone Done)
{
    Intent.Nonce = NextNonce++;
    Pending.Add(Intent.Nonce, MoveTemp(Done));
    ServerIntent(Intent);
}

void USFExchangeComponent::ServerIntent_Implementation(const FSFExchangeIntent& Intent)
{
    ExecuteIntent(Intent);
}

void USFExchangeComponent::Reply(uint32 Nonce, bool bCommitted, const FString& Message, const FSFWalletView& InWallet)
{
    Wallet = InWallet;  // the server keeps the last wallet it told the client, for server-side callers
    OnServerResult.Broadcast(Nonce, bCommitted);
    FSFExchangeResultWire Result; Result.Nonce = Nonce; Result.bCommitted = bCommitted; Result.Message = Message;
    Result.Balance = InWallet.Balance; Result.Reserved = InWallet.Reserved; Result.Available = InWallet.Available;
    ClientResult(Result);
}

void USFExchangeComponent::ClientResult_Implementation(const FSFExchangeResultWire& Result)
{
    Wallet = {Result.Balance, Result.Reserved, Result.Available};
    if (ISFExchangeService::FDone* Found = Pending.Find(Result.Nonce))
    {
        ISFExchangeService::FDone Done = MoveTemp(*Found);
        Pending.Remove(Result.Nonce);
        if (Done) { Done(Result.bCommitted); }
    }
}

void USFExchangeComponent::ClientTradeEvent_Implementation(ESFExchangeTradeEvent Event, const FString& TradeId, const FSFExchangeIntent& Offer)
{
    OnTradeEvent.Broadcast(Event, TradeId, Offer);
}

USFExchangeComponent* USFExchangeComponent::FindPartner(const FString& CharacterId) const
{
    const UWorld* World = GetWorld();
    if (!World) { return nullptr; }
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Other = It->Get();
        USFBackpackComponent* Pack = Other ? Other->FindComponentByClass<USFBackpackComponent>() : nullptr;
        if (Pack && Pack->GetCharacterId() == CharacterId) { return USFExchangeComponent::FindOrCreate(Other); }
    }
    return nullptr;
}

void USFExchangeComponent::NotifyPartner(const FString& PartnerId, ESFExchangeTradeEvent Event, const FSFExchangeIntent& Intent)
{
    if (USFExchangeComponent* Partner = FindPartner(PartnerId)) { Partner->ClientTradeEvent(Event, Intent.TradeId, Intent); }
}

void USFExchangeComponent::ExecuteIntent(const FSFExchangeIntent& Intent)
{
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    const USFGameplayServicesSettings* Settings = GetDefault<USFGameplayServicesSettings>();
    TSharedPtr<FSFGameplayServicesClient> Services = USFBackpackComponent::Services();
    USFBackpackComponent* Pack = Controller ? USFBackpackComponent::FindOrCreate(Controller) : nullptr;
    if (!Settings->bExchangeEnabled || !Services || !Pack) { Reply(Intent.Nonce, false, TEXT("Exchange unavailable"), Wallet); return; }

    FSFExchangeParties Parties;
    Parties.MyId = Pack->GetCharacterId(); Parties.MyContainerId = Pack->GetContainerId(); Parties.MyEscrowId = EscrowFor(Parties.MyContainerId);
    if (Intent.Kind == ESFExchangeIntentKind::OfferTrade)
    {
        USFExchangeComponent* Partner = FindPartner(Intent.PartnerId);
        USFBackpackComponent* PartnerPack = Partner && Partner->GetOwner() ? Partner->GetOwner()->FindComponentByClass<USFBackpackComponent>() : nullptr;
        if (!PartnerPack) { Reply(Intent.Nonce, false, TEXT("Partner is not here"), Wallet); return; }
        Parties.TheirId = PartnerPack->GetCharacterId(); Parties.TheirContainerId = PartnerPack->GetContainerId(); Parties.TheirEscrowId = EscrowFor(Parties.TheirContainerId);
    }
    FSFGameplayRequest Request;
    if (!BuildRequest(Intent, Parties, Settings->ExchangeFeeBasisPoints, FDateTime::UtcNow().ToUnixTimestamp() * 1000, Request))
    { Reply(Intent.Nonce, false, TEXT("Invalid request"), Wallet); return; }

    if (Intent.Kind == ESFExchangeIntentKind::Wallet)
    {
        TWeakObjectPtr<USFExchangeComponent> Weak(this); const uint32 Nonce = Intent.Nonce;
        Services->Call(Request, [Weak, Nonce](const FSFGameplayResponse& Response)
        { if (USFExchangeComponent* Self = Weak.Get()) { Self->Reply(Nonce, Response.Status == ESFGameplayStatus::Ok, FString(), SFExchangeRequests::ToWallet(Response)); } });
        return;
    }

    // Every other intent can move items, so it joins the backpack FIFO under the request's derived (idempotent) operation id.
    TWeakObjectPtr<USFExchangeComponent> Weak(this); TWeakObjectPtr<USFBackpackComponent> WeakPack(Pack);
    TSharedRef<FSFGameplayResponse> Reply_ = MakeShared<FSFGameplayResponse>();
    const FString MyId = Parties.MyId, TheirId = Parties.TheirId;
    FSFBackpackMutation Mutation; Mutation.Label = TEXT("exchange"); Mutation.OperationId = Request.OperationId;
    Mutation.Run = [Request, Services, Reply_](const FString&, int64, TFunction<void(ESFBackpackOutcome, int64)> Report)
    {
        Services->Call(Request, [Reply_, Report](const FSFGameplayResponse& Response)
        {
            *Reply_ = Response;
            Report(Response.Status == ESFGameplayStatus::Ok ? ESFBackpackOutcome::Committed : Response.Status == ESFGameplayStatus::Unavailable ? ESFBackpackOutcome::Unavailable : ESFBackpackOutcome::Rejected, -1);
        });
    };
    Mutation.Finished = [Weak, WeakPack, Reply_, Intent, MyId, TheirId](ESFBackpackOutcome Outcome)
    {
        USFExchangeComponent* Self = Weak.Get();
        if (!Self) { return; }
        const bool bCommitted = Outcome == ESFBackpackOutcome::Committed && SFExchangeRequests::Committed(*Reply_);
        if (bCommitted)
        {
            if (USFBackpackComponent* Owner = WeakPack.Get()) { Owner->Reconcile(); }  // the ledger moved items; rebuild the session bag from it
            if (Intent.Kind == ESFExchangeIntentKind::OfferTrade)
            {
                ExchangeTradeParties().Add(Intent.TradeId, TPair<FString, FString>(MyId, Intent.PartnerId));
                FSFExchangeIntent FromPartner = Intent;  // the partner sees my side as theirs
                Swap(FromPartner.MyGold, FromPartner.TheirGold); Swap(FromPartner.MyItems, FromPartner.TheirItems);
                Self->NotifyPartner(Intent.PartnerId, ESFExchangeTradeEvent::Offered, FromPartner);
            }
            else if (Intent.Kind == ESFExchangeIntentKind::AcceptTrade || Intent.Kind == ESFExchangeIntentKind::CancelTrade)
            {
                if (const TPair<FString, FString>* Parties_ = ExchangeTradeParties().Find(Intent.TradeId))
                { Self->NotifyPartner(Parties_->Key == MyId ? Parties_->Value : Parties_->Key, Intent.Kind == ESFExchangeIntentKind::AcceptTrade ? ESFExchangeTradeEvent::Accepted : ESFExchangeTradeEvent::Cancelled, Intent); }
                ExchangeTradeParties().Remove(Intent.TradeId);
            }
        }
        Self->Reply(Intent.Nonce, bCommitted, bCommitted ? FString() : Reply_->Message, SFExchangeRequests::ToWallet(*Reply_));
    };
    Pack->Enqueue(MoveTemp(Mutation));
}

// ---- Client service ----

void FSFExchangeClientService::Dispatch(FSFExchangeIntent Intent, FDone Done)
{
    if (USFExchangeComponent* Target = Component.Get()) { Target->Send(MoveTemp(Intent), MoveTemp(Done)); }
    else if (Done) { Done(false); }
}

void FSFExchangeClientService::OfferTrade(const FSFTradeSnapshot& Offer, FDone Done)
{
    FSFExchangeIntent Intent; Intent.Kind = ESFExchangeIntentKind::OfferTrade; Intent.TradeId = Offer.TradeId; Intent.PartnerId = Offer.Theirs.CharacterId;
    Intent.MyGold = Offer.Mine.Gold; Intent.TheirGold = Offer.Theirs.Gold; Intent.MyItems = ExchangeToWire(Offer.Mine.Items); Intent.TheirItems = ExchangeToWire(Offer.Theirs.Items);
    Dispatch(MoveTemp(Intent), MoveTemp(Done));
}

void FSFExchangeClientService::AcceptTrade(const FString& TradeId, FDone Done)
{
    FSFExchangeIntent Intent; Intent.Kind = ESFExchangeIntentKind::AcceptTrade; Intent.TradeId = TradeId;
    Dispatch(MoveTemp(Intent), MoveTemp(Done));
}

void FSFExchangeClientService::CancelTrade(const FString& TradeId, FDone Done)
{
    FSFExchangeIntent Intent; Intent.Kind = ESFExchangeIntentKind::CancelTrade; Intent.TradeId = TradeId;
    Dispatch(MoveTemp(Intent), MoveTemp(Done));
}

void FSFExchangeClientService::ListItem(const FString& ItemId, int32 Quantity, int64 UnitPrice, int64, FDone Done)
{
    FSFExchangeIntent Intent; Intent.Kind = ESFExchangeIntentKind::ListItem; Intent.ListingId = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
    Intent.ItemId = ItemId; Intent.Quantity = Quantity; Intent.UnitPrice = UnitPrice;
    Dispatch(MoveTemp(Intent), MoveTemp(Done));
}

void FSFExchangeClientService::BuyListing(const FSFListing& Listing, int64, FDone Done)
{
    FSFExchangeIntent Intent; Intent.Kind = ESFExchangeIntentKind::BuyListing; Intent.ListingId = Listing.ListingId;
    Intent.Quantity = Listing.Quantity; Intent.UnitPrice = Listing.UnitPrice; Intent.ExpectedRevision = Listing.Revision;
    Dispatch(MoveTemp(Intent), MoveTemp(Done));
}

void FSFExchangeClientService::CancelListing(const FString& ListingId, FDone Done)
{
    FSFExchangeIntent Intent; Intent.Kind = ESFExchangeIntentKind::CancelListing; Intent.ListingId = ListingId;
    Dispatch(MoveTemp(Intent), MoveTemp(Done));
}
