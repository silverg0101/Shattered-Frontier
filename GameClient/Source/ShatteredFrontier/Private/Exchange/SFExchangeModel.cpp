#include "Exchange/SFExchangeModel.h"

FString SFGold::Sanitize(const FString& Text)
{
    FString Out;
    for (const TCHAR Char : Text) { if (Char >= TEXT('0') && Char <= TEXT('9')) { Out.AppendChar(Char); } }
    return Out;
}

bool SFGold::Parse(const FString& Text, int64 Max, int64& Out)
{
    Out = 0;
    if (Text.IsEmpty()) { return true; }
    if (Sanitize(Text) != Text || Text.Len() > 10) { return false; }  // sign, point, letters, or beyond int64-safe width
    const int64 Value = FCString::Atoi64(*Text);
    if (Value < 0 || Value > Max) { return false; }
    Out = Value;
    return true;
}

FString SFGold::Format(int64 Amount)
{
    FString Digits = FString::Printf(TEXT("%lld"), FMath::Abs(Amount)), Out;
    for (int32 I = 0; I < Digits.Len(); ++I)
    {
        if (I > 0 && (Digits.Len() - I) % 3 == 0) { Out.AppendChar(TEXT(',')); }
        Out.AppendChar(Digits[I]);
    }
    return Amount < 0 ? TEXT("-") + Out : Out;
}

int64 SFGold::FeeFor(int64 Amount, int32 BasisPoints)
{
    if (Amount <= 0 || BasisPoints <= 0) { return 0; }
    return FMath::Max<int64>(1, (Amount * BasisPoints + 5000) / 10000);
}

FSFTradeViewModel::FSFTradeViewModel(ISFExchangeService& InService, FSFTradeSnapshot Initial, bool bInInitiator)
    : Service(InService), Trade(MoveTemp(Initial)), bInitiator(bInInitiator), GoldText(Trade.Mine.Gold > 0 ? FString::Printf(TEXT("%lld"), Trade.Mine.Gold) : FString())
{}

bool FSFTradeViewModel::SetMyGoldText(const FString& Text)
{
    GoldText = SFGold::Sanitize(Text);
    int64 Value = 0;
    // The most you can offer is what your wallet has free, never more than the global cap.
    bGoldValid = SFGold::Parse(GoldText, FMath::Min(SFGold::MaxAmount, Service.GetWallet().Available), Value);
    if (Trade.State != ESFTradeState::Open || Trade.Mine.bLocked) { GoldText = Trade.Mine.Gold > 0 ? FString::Printf(TEXT("%lld"), Trade.Mine.Gold) : FString(); bGoldValid = true; return false; }
    if (!bGoldValid) { return false; }
    if (Value != Trade.Mine.Gold) { Trade.Mine.Gold = Value; UnlockBoth(); }
    return true;
}

bool FSFTradeViewModel::AddMyItem(const FSFTradeStack& Stack)
{
    if (Trade.State != ESFTradeState::Open || Trade.Mine.bLocked || Stack.Count <= 0) { return false; }
    if (FSFTradeStack* Existing = Trade.Mine.Items.FindByPredicate([&Stack](const FSFTradeStack& S) { return S.ItemId == Stack.ItemId; })) { Existing->Count += Stack.Count; }
    else { Trade.Mine.Items.Add(Stack); }
    UnlockBoth();
    return true;
}

bool FSFTradeViewModel::RemoveMyItem(int32 Index)
{
    if (Trade.State != ESFTradeState::Open || Trade.Mine.bLocked || !Trade.Mine.Items.IsValidIndex(Index)) { return false; }
    Trade.Mine.Items.RemoveAt(Index);
    UnlockBoth();
    return true;
}

bool FSFTradeViewModel::CanLock() const
{
    return Trade.State == ESFTradeState::Open && !Trade.Mine.bLocked && !bPending && bGoldValid && (Trade.Mine.Gold > 0 || !Trade.Mine.Items.IsEmpty() || Trade.Theirs.Gold > 0 || !Trade.Theirs.Items.IsEmpty());
}

bool FSFTradeViewModel::Lock()
{
    if (!CanLock()) { return false; }
    if (!bInitiator) { Trade.Mine.bLocked = true; return true; }  // the partner's lock is local readiness until they accept
    // The initiator's lock is the backend OfferTrade; it takes effect when the server commits.
    bPending = true;
    const TWeakPtr<bool> Token = Alive;
    Service.OfferTrade(Trade, [this, Token](bool bCommitted)
    {
        if (!Token.IsValid()) { return; }
        bPending = false;
        if (bCommitted && Trade.State == ESFTradeState::Open) { Trade.Mine.bLocked = true; }
    });
    return true;
}

bool FSFTradeViewModel::CanAccept() const
{
    return Trade.State == ESFTradeState::Open && !bPending && Trade.Mine.bLocked && Trade.Theirs.bLocked;
}

bool FSFTradeViewModel::Accept()
{
    if (!CanAccept()) { return false; }
    bPending = true;
    const TWeakPtr<bool> Token = Alive;
    Service.AcceptTrade(Trade.TradeId, [this, Token](bool bCommitted)
    {
        if (!Token.IsValid()) { return; }
        bPending = false;
        if (bCommitted && Trade.State == ESFTradeState::Open) { Trade.State = ESFTradeState::Completed; }
    });
    return true;
}

bool FSFTradeViewModel::Cancel()
{
    if (Trade.State != ESFTradeState::Open) { return false; }
    Service.CancelTrade(Trade.TradeId, [](bool) {});  // best effort; the window closes either way
    Trade.State = ESFTradeState::Cancelled;
    UnlockBoth();
    return true;
}

void FSFTradeViewModel::OnPartnerOfferChanged(const TArray<FSFTradeStack>& Items, int64 Gold)
{
    if (Trade.State != ESFTradeState::Open) { return; }
    Trade.Theirs.Items = Items; Trade.Theirs.Gold = Gold;
    UnlockBoth();
}

void FSFTradeViewModel::OnPartnerLocked(bool bLocked)
{
    if (Trade.State == ESFTradeState::Open) { Trade.Theirs.bLocked = bLocked; }
}

void FSFTradeViewModel::Tick(double NowSeconds)
{
    if (Trade.State == ESFTradeState::Open && Trade.ExpiresAtSeconds > 0 && NowSeconds >= Trade.ExpiresAtSeconds)
    {
        Trade.State = ESFTradeState::Expired;
        UnlockBoth();
    }
}

FString FSFTradeViewModel::FormatCountdown(double Seconds)
{
    const int32 Whole = FMath::Max(0, FMath::CeilToInt(Seconds));
    return FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60);
}

const TCHAR* FSFTradeViewModel::StatusLine(const FSFTradeSide& Side, ESFTradeState State)
{
    if (State == ESFTradeState::Completed) { return TEXT("Traded"); }
    if (State == ESFTradeState::Cancelled) { return TEXT("Cancelled"); }
    if (State == ESFTradeState::Expired) { return TEXT("Expired"); }
    return Side.bLocked ? TEXT("Locked in") : TEXT("Waiting");
}

TArray<FSFListing> FSFMarketViewModel::Apply(const TArray<FSFListing>& All, const FSFMarketFilter& Filter)
{
    TArray<FSFListing> Out = All.FilterByPredicate([&Filter](const FSFListing& L)
    {
        return L.Quantity > 0 && (Filter.Tab == ESFInventoryTab::All || L.Category == Filter.Tab)
            && (Filter.Search.IsEmpty() || L.ItemName.Contains(Filter.Search, ESearchCase::IgnoreCase));
    });
    Out.StableSort([&Filter](const FSFListing& A, const FSFListing& B)
    {
        switch (Filter.Sort)
        {
        case ESFMarketSort::PriceDescending: return A.UnitPrice > B.UnitPrice;
        case ESFMarketSort::Name: return A.ItemName < B.ItemName;
        default: return A.UnitPrice < B.UnitPrice;
        }
    });
    return Out;
}

TArray<FSFListing> FSFMarketViewModel::Visible() const { return Apply(Listings, FilterState); }

bool FSFMarketViewModel::CanBuy(const FSFListing& Listing) const
{
    return Listing.Quantity > 0 && Listing.UnitPrice > 0 && WalletState.Available >= Listing.UnitPrice * Listing.Quantity + BuyFee(Listing);
}

bool FSFMarketViewModel::Buy(const FSFListing& Listing)
{
    if (!CanBuy(Listing) || bBuying) { return false; }
    bBuying = true;
    const TWeakPtr<bool> Token = Alive; const FString ListingId = Listing.ListingId;
    Service.BuyListing(Listing, BuyFee(Listing), [this, Token, ListingId](bool bCommitted)
    {
        if (!Token.IsValid()) { return; }
        bBuying = false;
        if (!bCommitted) { return; }
        WalletState = Service.GetWallet();
        Listings.RemoveAll([&ListingId](const FSFListing& L) { return L.ListingId == ListingId; });
    });
    return true;
}

bool FSFMarketViewModel::SetSellQuantityText(const FString& Text, int32 MaxQuantity)
{
    int64 Value = 0;
    const bool bOk = SFGold::Parse(SFGold::Sanitize(Text), MaxQuantity, Value) && Value > 0;
    SellQuantity = bOk ? static_cast<int32>(Value) : 0;
    return bOk;
}

bool FSFMarketViewModel::SetSellPriceText(const FString& Text)
{
    int64 Value = 0;
    const bool bOk = SFGold::Parse(SFGold::Sanitize(Text), SFGold::MaxAmount, Value) && Value > 0;
    SellPrice = bOk ? Value : 0;
    return bOk;
}

bool FSFMarketViewModel::ListSelected(const FString& ItemId, int64 AvailableToPayFee)
{
    if (!CanList(AvailableToPayFee)) { return false; }
    const TWeakPtr<bool> Token = Alive;
    Service.ListItem(ItemId, SellQuantity, SellPrice, ListingFee(), [this, Token](bool bCommitted)
    {
        if (Token.IsValid() && bCommitted) { WalletState = Service.GetWallet(); }
    });
    return true;
}
