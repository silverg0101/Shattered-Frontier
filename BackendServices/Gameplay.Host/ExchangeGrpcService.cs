using Grpc.Core;
using ShatteredFrontier.Exchange.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using Npgsql;
using Pb = ShatteredFrontier.Exchange.V1;

internal sealed class ExchangeGrpcService(PostgresExchangeStore exchange, PostgresInventoryLedgerStore inventoryStore,
    GameplayRecoverySettings recoverySettings) : Pb.Exchange.ExchangeBase
{
    private InventoryLedger Ledger => new(inventoryStore);
    private static void Require(bool condition, string message)
    { if (!condition) throw new GameplayRejectedException(message, StatusCode.InvalidArgument); }
    private static Pb.ExchangeReply Reply(ShatteredFrontier.Exchange.ExchangeReceipt receipt, string state) => new()
    { OperationId = receipt.OperationId, SubjectId = receipt.SubjectId, State = state,
      Committed = receipt.Accepted, Amount = receipt.Amount, Fee = receipt.Fee };
    private static Pb.ExchangeReply Reply(ShatteredFrontier.Exchange.Postgres.DurableSettlement settlement)
    {
        var result = Reply(settlement.Receipt, settlement.State);
        result.Committed = settlement.State == "committed";
        return result;
    }
    private bool TradeExpired(string operationId)
    {
        using var connection = new NpgsqlConnection(Environment.GetEnvironmentVariable("GAMEPLAY_EXCHANGE_CONNECTION"));
        connection.Open();
        using var command = new NpgsqlCommand("SELECT created_at FROM exchange.settlements WHERE operation_id=$1", connection);
        command.Parameters.AddWithValue(operationId);
        using var reader = command.ExecuteReader();
        return reader.Read() && reader.GetFieldValue<DateTimeOffset>(0) <= DateTimeOffset.UtcNow - recoverySettings.TradeOfferLifetime;
    }
    private ContainerView Owned(string id, string actor, ContainerKind? kind = null)
    {
        var view = Ledger.Read(id);
        if (view.Spec.OwnerKind != OwnerKind.Character || view.Spec.OwnerId != actor ||
            (kind is not null && view.Spec.Kind != kind.Value))
            throw new GameplayRejectedException("Container ownership or kind mismatch.", StatusCode.PermissionDenied);
        return view;
    }
    private EscrowDelivery Delivery(Pb.ItemDelivery request, string a, string b)
    {
        var from = Ledger.Read(request.EscrowId);
        Require(from.Spec.Kind == ContainerKind.Escrow && (from.Spec.OwnerId == a || from.Spec.OwnerId == b), "Invalid trade escrow.");
        var recipient = from.Spec.OwnerId == a ? b : a;
        var to = Owned(request.DestinationId, recipient);
        Require(request.Quantity > 0 && !string.IsNullOrWhiteSpace(request.ItemId), "Invalid trade item.");
        return new(request.EscrowId, from.Revision, request.DestinationId, to.Revision, request.ItemId, request.Quantity);
    }
    public override Task<Pb.WalletReply> Wallet(Pb.WalletRequest request, ServerCallContext context) => GrpcRun.Call(context, () =>
    {
        Require(request.OwnerId.StartsWith("chr_", StringComparison.Ordinal), "Invalid character.");
        var w = exchange.Wallet(request.OwnerId);
        return new Pb.WalletReply { Balance = w.Balance, Reserved = w.Reserved, Available = w.Spendable, Revision = w.Sequence };
    });
    public override Task<Pb.ExchangeReply> OfferTrade(Pb.TradeOfferRequest request, ServerCallContext context) => GrpcRun.Call(context, () =>
    {
        Require(request.PartyA.StartsWith("chr_", StringComparison.Ordinal) && request.PartyB.StartsWith("chr_", StringComparison.Ordinal) &&
            request.PartyA != request.PartyB && request.Deliveries.Count > 0, "Invalid trade parties or items.");
        var existing = exchange.Settlement(request.OperationId);
        if (existing is not null)
        {
            Require(existing.Kind == "trade" && existing.SubjectId == request.TradeId &&
                existing.Deliveries.Count == request.Deliveries.Count &&
                existing.Deliveries.Zip(request.Deliveries).All(pair => pair.First.FromId == pair.Second.EscrowId &&
                    pair.First.ToId == pair.Second.DestinationId && pair.First.DefinitionId == pair.Second.ItemId &&
                    pair.First.Quantity == pair.Second.Quantity), "Trade replay conflicts with original offer.");
            return Reply(exchange.BeginTrade(request.OperationId, request.TradeId, request.PartyA, request.PartyB,
                request.GoldA, request.GoldB, existing.Deliveries));
        }
        var moves = request.Deliveries.Select(d => Delivery(d, request.PartyA, request.PartyB)).ToArray();
        return Reply(exchange.BeginTrade(request.OperationId, request.TradeId, request.PartyA, request.PartyB,
            request.GoldA, request.GoldB, moves));
    });
    public override Task<Pb.ExchangeReply> AcceptTrade(Pb.TradeActionRequest request, ServerCallContext context) => GrpcRun.Call(context, () =>
    {
        var pending = exchange.Settlement(request.OfferOperationId) ?? throw new GameplayRejectedException("Trade offer missing.", StatusCode.NotFound);
        Require(pending.Kind == "trade" && pending.SubjectId == request.TradeId, "Trade receipt mismatch.");
        if (pending.State == "pending")
        {
            if (TradeExpired(request.OfferOperationId))
                throw new GameplayRejectedException("Trade offer expired; escrow return is pending.", StatusCode.FailedPrecondition);
            Require(pending.Deliveries.Any(d => Ledger.Read(d.ToId).Spec.OwnerId == request.ActorId), "Actor is not a trade recipient.");
            var moves = pending.Deliveries.Select(d => d with { FromRevision = Ledger.Read(d.FromId).Revision, ToRevision = Ledger.Read(d.ToId).Revision }).ToArray();
            Ledger.DeliverEscrowBatch(request.OfferOperationId + "/settle", moves);
            if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_EXCHANGE_DELIVERY") == request.OfferOperationId)
                Environment.FailFast("Injected post-exchange delivery fault.");
        }
        return Reply(exchange.Reconcile(request.OfferOperationId, inventoryStore));
    });
    public override Task<Pb.ExchangeReply> CancelTrade(Pb.TradeActionRequest request, ServerCallContext context) => GrpcRun.Call(context, () =>
    {
        var settlement = exchange.Settlement(request.OfferOperationId) ?? throw new GameplayRejectedException("Trade settlement missing.", StatusCode.NotFound);
        Require(settlement.Deliveries.Any(d => Ledger.Read(d.FromId).Spec.OwnerId == request.ActorId ||
            Ledger.Read(d.ToId).Spec.OwnerId == request.ActorId), "Actor is not a trade party.");
        var returns = settlement.Deliveries.Select(d =>
        {
            var escrow = Ledger.Read(d.FromId);
            var bag = inventoryStore.Execute(s => s.Containers.Values.Single(c => c.Spec.OwnerId == escrow.Spec.OwnerId && c.Spec.Kind == ContainerKind.Backpack).Spec.Id);
            return new EscrowDelivery(d.FromId, 0, bag, 0, d.DefinitionId, d.Quantity);
        }).ToArray();
        return Reply(exchange.CancelTrade(request.OperationId, request.TradeId, request.ActorId, returns, inventoryStore), "cancelled");
    });
    public override Task<Pb.ExchangeReply> ListItem(Pb.ListItemRequest request, ServerCallContext context) => GrpcRun.Call(context, () =>
    {
        var source = Owned(request.SourceContainerId, request.SellerId);
        var escrow = Owned(request.EscrowContainerId, request.SellerId, ContainerKind.Escrow);
        Require(request.Quantity > 0 && request.UnitPrice > 0 && request.SourceContainerId != request.EscrowContainerId, "Invalid listing item.");
        var intent = exchange.BeginListing(request.OperationId, request.ListingId, request.SellerId, source.Spec.Id, escrow.Spec.Id,
            request.ItemId, request.Quantity, request.UnitPrice, request.ListingFee, DateTimeOffset.FromUnixTimeMilliseconds(request.CreatedAtUnixMs));
        if (intent.Code == "pending")
        {
            try { Ledger.Transfer(request.OperationId + "/escrow", source.Spec.Id, source.Revision, escrow.Spec.Id, escrow.Revision, request.ItemId, request.Quantity); }
            catch (LedgerRejectedException) { /* Reconciliation fences an absent or conflicting transfer. */ }
            if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_LISTING_ESCROW") == request.OperationId)
                Environment.FailFast("Injected post-listing escrow fault.");
        }
        var receipt = exchange.ReconcileListing(request.OperationId, inventoryStore);
        return Reply(receipt, receipt.Accepted ? "active" : "compensated");
    });
    public override Task<Pb.ExchangeReply> BuyListing(Pb.BuyListingRequest request, ServerCallContext context) => GrpcRun.Call(context, () =>
    {
        var previous = exchange.Settlement(request.OperationId);
        if (previous is not null)
        {
            exchange.BeginPurchase(request.OperationId, request.ListingId, request.BuyerId,
                request.ExpectedRevision, request.ExpectedUnitPrice, request.TransactionFee, previous.Deliveries);
            return Reply(exchange.Reconcile(request.OperationId, inventoryStore));
        }
        var listing = exchange.Listing(request.ListingId) ?? throw new GameplayRejectedException("Listing missing.", StatusCode.NotFound);
        var destination = Owned(request.DestinationContainerId, request.BuyerId);
        var escrow = Ledger.Read(listing.EscrowId);
        var pending = exchange.BeginPurchase(request.OperationId, request.ListingId, request.BuyerId,
            request.ExpectedRevision, request.ExpectedUnitPrice, request.TransactionFee,
            [new EscrowDelivery(listing.EscrowId, escrow.Revision, destination.Spec.Id, destination.Revision, listing.ItemId, listing.Quantity)]);
        Ledger.DeliverEscrowBatch(request.OperationId + "/settle", pending.Deliveries);
        if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_EXCHANGE_DELIVERY") == request.OperationId)
            Environment.FailFast("Injected post-exchange delivery fault.");
        return Reply(exchange.Reconcile(request.OperationId, inventoryStore));
    });
    public override Task<Pb.ExchangeReply> CancelListing(Pb.ListingActionRequest request, ServerCallContext context) => GrpcRun.Call(context, () =>
        Reply(exchange.CancelListing(request.OperationId, request.ListingId, request.SellerId, inventoryStore), "cancelled"));
}
