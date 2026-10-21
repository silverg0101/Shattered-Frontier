using System.Text.Json;
using System.Runtime.CompilerServices;
using ShatteredFrontier.InventoryLedger;
using InventoryService = ShatteredFrontier.InventoryLedger.InventoryLedger;

namespace ShatteredFrontier.Exchange;

public sealed record ExchangeControl(string Key, string Label, string Unit, decimal Default, decimal Min, decimal Max,
    string Permission, string EffectiveTime);

public static class ExchangeControls
{
    public static readonly IReadOnlyList<ExchangeControl> Keys =
    [
        new("tune.economy.market_listing_fee_rate", "Commodity listing fee", "fraction", .01m, 0m, .10m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_listing_fee_rate_regional", "Regional listing fee", "fraction", .02m, 0m, .10m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_transaction_fee_rate", "Sale fee", "fraction", .05m, 0m, .20m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_expiry_fee_rate", "Expired listing fee", "fraction", .01m, 0m, .10m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_fee_min_copper", "Minimum fee", "copper", 100m, 0m, 10000m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_max_listings_per_character", "Active listing limit", "listings", 100m, 1m, 100m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_listing_durations_hours", "Listing duration", "hours", 48m, 12m, 48m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_min_level", "Minimum listing level", "levels", 10m, 1m, 100m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_price_cap_mult", "Price cap above fair value", "multiplier", 20m, 1m, 50m, "economy.tuning.publish", "new listings"),
        new("tune.economy.market_price_cap_abs_copper", "Minimum price ceiling", "copper", 100000m, 10000m, 10000000m, "economy.tuning.publish", "new listings")
    ];
}

public sealed class ExchangeSettings
{
    private readonly Dictionary<string, decimal> values = ExchangeControls.Keys.ToDictionary(x => x.Key, x => x.Default, StringComparer.Ordinal);
    public ExchangeSettings(IReadOnlyDictionary<string, decimal>? snapshot = null)
    {
        if (snapshot is null) return;
        foreach (var (key, value) in snapshot)
        {
            var control = ExchangeControls.Keys.SingleOrDefault(x => x.Key == key) ?? throw new ArgumentException("Unknown exchange setting.");
            if (value < control.Min || value > control.Max ||
                (control.Unit is "copper" or "listings" or "hours" or "levels" && value != decimal.Truncate(value)) ||
                (control.Unit == "hours" && value is not (12m or 24m or 48m)))
                throw new ArgumentOutOfRangeException(nameof(snapshot), key);
            values[key] = value;
        }
    }
    public decimal Get(string key) => values[key];
    public long Fee(string rateKey, long amount) => Math.Max((long)Get("tune.economy.market_fee_min_copper"),
        checked((long)decimal.Round(Get(rateKey) * amount, 0, MidpointRounding.AwayFromZero)));
}

public sealed record ExchangeReceipt(string OperationId, bool Accepted, string Code, string SubjectId, long Amount = 0, long Fee = 0);
public sealed record CurrencyEntry(string OperationId, string OwnerId, string CurrencyId, long Sequence, long Delta, string FlowClass, string Code);
public sealed record WalletView(long Balance, long Reserved, long Spendable, long Sequence);
public sealed record OfferedItem(string SourceContainer, string EscrowContainer, string DestinationContainer, string DefinitionId, long Quantity);
public sealed record TradeOffer(string CharacterId, OfferedItem? Item, long GoldCopper);
public enum MarketScope { GlobalExchange, Regional }
public sealed record MarketEligibility(bool Tradeable, bool Bound, bool QuestItem, bool Token, string Category,
    int SellerLevel, long FairValueCopper, bool HubAttested, bool SameRealm);

internal sealed class TradeRecord(string id, TradeOffer a, TradeOffer b)
{
    public string Id { get; } = id;
    public TradeOffer A { get; } = a;
    public TradeOffer B { get; } = b;
    public HashSet<string> Locked { get; } = new(StringComparer.Ordinal);
    public HashSet<string> Accepted { get; } = new(StringComparer.Ordinal);
    public string State { get; set; } = "open";
}
internal sealed class ListingRecord(string id, string seller, string sellerContainer, string escrowContainer,
    string item, long quantity, long price, DateTimeOffset expires, MarketScope scope, long listingFee, ExchangeSettings settings)
{
    public string Id { get; } = id;
    public string Seller { get; } = seller;
    public string SellerContainer { get; } = sellerContainer;
    public string EscrowContainer { get; } = escrowContainer;
    public string Item { get; } = item;
    public long Quantity { get; } = quantity;
    public long Price { get; } = price;
    public DateTimeOffset Expires { get; } = expires;
    public MarketScope Scope { get; } = scope;
    public long ListingFee { get; } = listingFee;
    public ExchangeSettings Settings { get; } = settings;
    public string State { get; set; } = "active";
}

// One process gate serializes wallet state, receipts, and the InventoryLedger escrow operation.
public sealed class ExchangeService(InventoryService inventory, ExchangeSettings? settings = null)
{
    private readonly object gate = new();
    private readonly ExchangeSettings settings = settings ?? new();
    private readonly Dictionary<string, (long Balance, long Reserved, long Sequence)> wallets = new(StringComparer.Ordinal);
    private readonly Dictionary<string, (string Fingerprint, ExchangeReceipt Receipt)> operations = new(StringComparer.Ordinal);
    private readonly Dictionary<string, TradeRecord> trades = new(StringComparer.Ordinal);
    private readonly Dictionary<string, ListingRecord> listings = new(StringComparer.Ordinal);
    private readonly List<CurrencyEntry> ledger = [];
    public IReadOnlyList<CurrencyEntry> CurrencyLedger { get { lock (gate) return ledger.ToArray(); } }
    public WalletView Wallet(string owner, string currency = "currency/gold")
    {
        if (!ValidWallet(owner, currency)) throw new ArgumentException("Invalid registry wallet owner or currency.");
        lock (gate) { var w = wallets.GetValueOrDefault(Key(owner, currency)); return new(w.Balance, w.Reserved, w.Balance - w.Reserved, w.Sequence); }
    }
    public string TradeState(string id) { lock (gate) return trades[id].State; }
    public string ListingState(string id) { lock (gate) return listings[id].State; }

    public ExchangeReceipt Credit(string op, string owner, string currency, long amount, string source) => Command(op, new { owner, currency, amount, source }, () =>
    {
        if (!ValidWallet(owner, currency)) return Reject(op, "invalid_wallet", owner);
        if (amount <= 0 || source is not ("quest_reward" or "gm_grant" or "starter_kit" or "crown_topup")) return Reject(op, "invalid_credit", owner);
        if (currency == "currency/crowns" && source != "crown_topup") return Reject(op, "invalid_source", owner);
        if (currency != "currency/crowns" && source == "crown_topup") return Reject(op, "invalid_source", owner);
        if (!CanCredit(owner, currency, amount)) return Reject(op, "overflow", owner);
        Post(op, owner, currency, amount, "faucet", source);
        return Ok(op, owner, amount);
    });

    public ExchangeReceipt OpenTrade(string op, string id, TradeOffer a, TradeOffer b) => Command(op, new { id, a, b }, () =>
    {
        if (!id.StartsWith("trd_", StringComparison.Ordinal) || a.CharacterId == b.CharacterId ||
            (a.Item is null && b.Item is null && a.GoldCopper == 0 && b.GoldCopper == 0) ||
            !ValidOffer(a) || !ValidOffer(b) || trades.ContainsKey(id)) return Reject(op, "invalid_trade", id);
        trades.Add(id, new(id, a, b)); return Ok(op, id);
    });
    public ExchangeReceipt LockTrade(string op, string id, string actor) => Command(op, new { id, actor }, () =>
    {
        if (!trades.TryGetValue(id, out var trade) || trade.State != "open" || trade.Locked.Contains(actor)) return Reject(op, "not_open", id);
        var offer = Side(trade, actor); if (offer is null) return Reject(op, "not_party", id);
        if (Wallet(actor).Spendable < offer.GoldCopper) return Reject(op, "insufficient_gold", id);
        if (offer.Item is { } item)
        {
            if (!Owns(item.SourceContainer, actor) || !Owns(item.EscrowContainer, actor) || !Owns(item.DestinationContainer, Other(trade, actor).CharacterId))
                return Reject(op, "invalid_containers", id);
            try { inventory.Transfer(op + ":escrow", item.SourceContainer, inventory.Read(item.SourceContainer).Revision,
                item.EscrowContainer, inventory.Read(item.EscrowContainer).Revision, item.DefinitionId, item.Quantity); }
            catch (LedgerRejectedException) { return Reject(op, "inventory_rejected", id); }
        }
        Reserve(actor, offer.GoldCopper); trade.Locked.Add(actor);
        if (trade.Locked.Count == 2) trade.State = "locked";
        return Ok(op, id);
    });
    public ExchangeReceipt AcceptTrade(string op, string id, string actor) => Command(op, new { id, actor }, () =>
    {
        if (!trades.TryGetValue(id, out var trade) || trade.State != "locked" || !trade.Locked.Contains(actor)) return Reject(op, "not_locked", id);
        if (trade.Accepted.Contains(actor)) return Reject(op, "already_accepted", id);
        if (trade.Accepted.Count == 0) { trade.Accepted.Add(actor); return Ok(op, id); }
        if (!CanCredit(trade.A.CharacterId, "currency/gold", trade.B.GoldCopper) ||
            !CanCredit(trade.B.CharacterId, "currency/gold", trade.A.GoldCopper)) return Reject(op, "overflow", id);
        var moves = new[] { trade.A.Item, trade.B.Item }.Where(x => x is not null).Select(x => x!).Select(x =>
            new EscrowDelivery(x.EscrowContainer, inventory.Read(x.EscrowContainer).Revision,
                x.DestinationContainer, inventory.Read(x.DestinationContainer).Revision, x.DefinitionId, x.Quantity)).ToArray();
        try { if (moves.Length > 0) inventory.DeliverEscrowBatch(op + ":settle", moves); }
        catch (LedgerRejectedException) { return Reject(op, "inventory_rejected", id); }
        SettleGold(op, trade.A.CharacterId, trade.B.CharacterId, trade.A.GoldCopper, "trade_send", "trade_receive");
        SettleGold(op, trade.B.CharacterId, trade.A.CharacterId, trade.B.GoldCopper, "trade_send", "trade_receive");
        trade.State = "completed"; trade.Accepted.Add(actor); return Ok(op, id);
    });
    public ExchangeReceipt CancelTrade(string op, string id, string actor) => Command(op, new { id, actor }, () =>
    {
        if (!trades.TryGetValue(id, out var trade) || Side(trade, actor) is null || trade.State is "completed" or "cancelled") return Reject(op, "not_cancellable", id);
        var returns = new[] { trade.A, trade.B }.Where(x => trade.Locked.Contains(x.CharacterId) && x.Item is not null)
            .Select(x => x.Item!).Select(x => new EscrowDelivery(x.EscrowContainer, inventory.Read(x.EscrowContainer).Revision,
                x.SourceContainer, inventory.Read(x.SourceContainer).Revision, x.DefinitionId, x.Quantity)).ToArray();
        try { if (returns.Length > 0) inventory.DeliverEscrowBatch(op + ":return", returns); }
        catch (LedgerRejectedException) { return Reject(op, "inventory_rejected", id); }
        foreach (var offer in new[] { trade.A, trade.B }) if (trade.Locked.Contains(offer.CharacterId))
        {
            Reserve(offer.CharacterId, -offer.GoldCopper);
        }
        trade.State = "cancelled"; return Ok(op, id);
    });

    public ExchangeReceipt List(string op, string id, string seller, string sourceContainer, string escrowContainer,
        string definitionId, long quantity, long unitPriceCopper, MarketScope scope, DateTimeOffset now, MarketEligibility eligibility) =>
        Command(op, new { id, seller, sourceContainer, escrowContainer, definitionId, quantity, unitPriceCopper, scope, now, eligibility }, () =>
    {
        var commodity = eligibility.Category is "material" or "consumable" or "resource";
        if (!id.StartsWith("lst_", StringComparison.Ordinal) || listings.ContainsKey(id) || !eligibility.Tradeable ||
            eligibility.Bound || eligibility.QuestItem || eligibility.Token || !eligibility.HubAttested || !eligibility.SameRealm ||
            eligibility.SellerLevel < settings.Get("tune.economy.market_min_level") || eligibility.FairValueCopper <= 0 ||
            (commodity ? scope != MarketScope.GlobalExchange : scope != MarketScope.Regional) ||
            !seller.StartsWith("chr_", StringComparison.Ordinal) || !definitionId.StartsWith("item/", StringComparison.Ordinal) ||
            quantity <= 0 || unitPriceCopper <= 0 || !Owns(sourceContainer, seller) || !Owns(escrowContainer, seller) ||
            !IsEscrow(escrowContainer) ||
            listings.Values.Count(x => x.Seller == seller && x.State == "active") >= settings.Get("tune.economy.market_max_listings_per_character"))
            return Reject(op, "invalid_listing", id);
        long total; long fee;
        try { total = checked(quantity * unitPriceCopper); fee = settings.Fee(scope == MarketScope.Regional ?
            "tune.economy.market_listing_fee_rate_regional" : "tune.economy.market_listing_fee_rate", total); }
        catch (OverflowException) { return Reject(op, "overflow", id); }
        if (unitPriceCopper >= Math.Max(settings.Get("tune.economy.market_price_cap_mult") * eligibility.FairValueCopper,
            settings.Get("tune.economy.market_price_cap_abs_copper"))) return Reject(op, "price_above_cap", id);
        if (total < settings.Fee("tune.economy.market_transaction_fee_rate", total)) return Reject(op, "price_below_fee", id);
        if (Wallet(seller).Spendable < fee) return Reject(op, "insufficient_gold", id);
        try { inventory.Transfer(op + ":escrow", sourceContainer, inventory.Read(sourceContainer).Revision,
            escrowContainer, inventory.Read(escrowContainer).Revision, definitionId, quantity); }
        catch (LedgerRejectedException) { return Reject(op, "inventory_rejected", id); }
        Post(op, seller, "currency/gold", -fee, "sink", "market_listing_fee");
        var expiry = now.AddHours((double)settings.Get("tune.economy.market_listing_durations_hours"));
        listings.Add(id, new(id, seller, sourceContainer, escrowContainer, definitionId, quantity, unitPriceCopper, expiry, scope, fee, settings));
        return Ok(op, id, fee: fee);
    });
    public ExchangeReceipt Buy(string op, string id, string buyer, string destinationContainer, long expectedUnitPrice, DateTimeOffset now) =>
        Command(op, new { id, buyer, destinationContainer, expectedUnitPrice, now }, () =>
    {
        if (!listings.TryGetValue(id, out var listing) || listing.State != "active" || now >= listing.Expires ||
            listing.Seller == buyer) return Reject(op, "listing_unavailable", id);
        if (expectedUnitPrice != listing.Price || !Owns(destinationContainer, buyer) ||
            inventory.Read(destinationContainer).Spec.Kind == ContainerKind.Escrow) return Reject(op, "invalid_purchase", id);
        long total; long fee;
        try { total = checked(listing.Price * listing.Quantity); fee = listing.Settings.Fee("tune.economy.market_transaction_fee_rate", total); }
        catch (OverflowException) { return Reject(op, "overflow", id); }
        if (fee > total || Wallet(buyer).Spendable < total) return Reject(op, "insufficient_gold", id);
        if (!CanCredit(listing.Seller, "currency/gold", total)) return Reject(op, "overflow", id);
        try { inventory.DeliverEscrowBatch(op + ":settle", [new(listing.EscrowContainer, inventory.Read(listing.EscrowContainer).Revision,
            destinationContainer, inventory.Read(destinationContainer).Revision, listing.Item, listing.Quantity)]); }
        catch (LedgerRejectedException) { return Reject(op, "inventory_rejected", id); }
        Post(op, buyer, "currency/gold", -total, "transfer", "market_purchase");
        Post(op, listing.Seller, "currency/gold", total, "transfer", "market_sale");
        Post(op, listing.Seller, "currency/gold", -fee, "sink", "market_transaction_fee");
        listing.State = "sold"; return Ok(op, id, total, fee);
    });
    public ExchangeReceipt CancelListing(string op, string id, string seller) => Command(op, new { id, seller }, () =>
    {
        if (!listings.TryGetValue(id, out var listing) || listing.State != "active" || listing.Seller != seller) return Reject(op, "not_cancellable", id);
        if (!ReturnListing(op, listing)) return Reject(op, "inventory_rejected", id);
        listing.State = "cancelled"; return Ok(op, id);
    });
    public ExchangeReceipt Expire(string op, string id, DateTimeOffset now) => Command(op, new { id, now }, () =>
    {
        if (!listings.TryGetValue(id, out var listing) || listing.State != "active" || now < listing.Expires) return Reject(op, "not_expired", id);
        long value;
        try { value = checked(listing.Price * listing.Quantity); }
        catch (OverflowException) { return Reject(op, "overflow", id); }
        if (!ReturnListing(op, listing)) return Reject(op, "inventory_rejected", id);
        var fee = Math.Min(Wallet(listing.Seller).Spendable, listing.Settings.Fee("tune.economy.market_expiry_fee_rate", value));
        if (fee > 0) Post(op, listing.Seller, "currency/gold", -fee, "sink", "market_expiry_fee");
        listing.State = "expired"; return Ok(op, id, fee: fee);
    });
    private bool ReturnListing(string op, ListingRecord x)
    {
        try { inventory.Transfer(op + ":return", x.EscrowContainer, inventory.Read(x.EscrowContainer).Revision,
            x.SellerContainer, inventory.Read(x.SellerContainer).Revision, x.Item, x.Quantity); return true; }
        catch (LedgerRejectedException) { return false; }
    }
    private ExchangeReceipt Command<T>(string op, T args, Func<ExchangeReceipt> action, [CallerMemberName] string command = "")
    {
        if (string.IsNullOrWhiteSpace(op) || op.Length > 128) throw new ArgumentException("Invalid operation ID.", nameof(op));
        var fingerprint = JsonSerializer.Serialize(new { command, args });
        lock (gate)
        {
            if (operations.TryGetValue(op, out var prior))
            {
                if (prior.Fingerprint != fingerprint) throw new InvalidOperationException("Conflicting operation ID reuse.");
                return prior.Receipt;
            }
            var receipt = action(); operations.Add(op, (fingerprint, receipt)); return receipt;
        }
    }
    private static ExchangeReceipt Ok(string op, string id, long amount = 0, long fee = 0) => new(op, true, "ok", id, amount, fee);
    private static ExchangeReceipt Reject(string op, string code, string id) => new(op, false, code, id);
    private static string Key(string owner, string currency) => owner + "|" + currency;
    private static bool ValidWallet(string owner, string currency) =>
        currency is ("currency/gold" or "currency/frontier_marks" or "currency/season_tokens" or "currency/crowns") &&
        (currency == "currency/crowns" ? owner.StartsWith("acc_", StringComparison.Ordinal) : owner.StartsWith("chr_", StringComparison.Ordinal));
    private bool CanCredit(string owner, string currency, long amount)
    {
        var w = wallets.GetValueOrDefault(Key(owner, currency)); return amount >= 0 && w.Balance <= long.MaxValue - amount;
    }
    private void Post(string op, string owner, string currency, long delta, string flow, string code)
    {
        if (delta == 0) return;
        var key = Key(owner, currency); var w = wallets.GetValueOrDefault(key);
        wallets[key] = (checked(w.Balance + delta), w.Reserved, checked(w.Sequence + 1));
        ledger.Add(new(op, owner, currency, w.Sequence + 1, delta, flow, code));
    }
    private void Reserve(string owner, long delta)
    {
        var key = Key(owner, "currency/gold"); var w = wallets.GetValueOrDefault(key);
        wallets[key] = (w.Balance, checked(w.Reserved + delta), w.Sequence);
    }
    private void SettleGold(string op, string from, string to, long amount, string debitCode, string creditCode)
    {
        if (amount == 0) return;
        Reserve(from, -amount); Post(op, from, "currency/gold", -amount, "transfer", debitCode);
        Post(op, to, "currency/gold", amount, "transfer", creditCode);
    }
    private static bool ValidOffer(TradeOffer x) => x.CharacterId.StartsWith("chr_", StringComparison.Ordinal) && x.GoldCopper >= 0 &&
        (x.Item is null || x.Item.Quantity > 0 && x.Item.DefinitionId.StartsWith("item/", StringComparison.Ordinal));
    private static TradeOffer? Side(TradeRecord t, string actor) => t.A.CharacterId == actor ? t.A : t.B.CharacterId == actor ? t.B : null;
    private static TradeOffer Other(TradeRecord t, string actor) => t.A.CharacterId == actor ? t.B : t.A;
    private bool Owns(string container, string owner)
    {
        try { var spec = inventory.Read(container).Spec; return spec.OwnerKind == OwnerKind.Character && spec.OwnerId == owner; }
        catch (LedgerRejectedException) { return false; }
    }
    private bool IsEscrow(string container)
    {
        try { return inventory.Read(container).Spec.Kind == ContainerKind.Escrow; }
        catch (LedgerRejectedException) { return false; }
    }
}
