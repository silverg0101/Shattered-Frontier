using Grpc.Core;
using Grpc.Net.Client;
using Grpc.Core.Interceptors;
using Google.Protobuf;
using System.Text.Json;
using System.Security.Cryptography;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.Exchange.Postgres;
using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.Housing.Postgres;
using Pb = ShatteredFrontier.Housing.V1;
using ProgressPb = ShatteredFrontier.Progression.V1;
using ExchangePb = ShatteredFrontier.Exchange.V1;
using InventoryPb = ShatteredFrontier.InventoryLedger.V1;

// Exercised only by the owned Test-GameplayHosts fixture against its disposable cluster.
internal static class GameplayFixtureClient
{
    private sealed class ReceiptLedger(string path) : Interceptor
    {
        private static readonly object Gate = new();
        public override TResponse BlockingUnaryCall<TRequest, TResponse>(TRequest request, ClientInterceptorContext<TRequest, TResponse> context, BlockingUnaryCallContinuation<TRequest, TResponse> continuation)
        {
            var response = continuation(request, context);
            Record(request, response, context.Method.Name);
            return response;
        }
        private void Record<TRequest, TResponse>(TRequest request, TResponse response, string rpc)
        {
            if (request is not IMessage input || response is not IMessage output) return;
            string Field(IMessage message, string name) => message.Descriptor.FindFieldByName(name) is { } field ? field.Accessor.GetValue(message)?.ToString() ?? "" : "";
            var operation = Field(input, "operation_id");
            if (operation.Length == 0) operation = Field(input, "source_receipt_id");
            if (operation.Length == 0) return;
            var revision = Field(output, "structure_revision");
            if (revision.Length == 0) revision = Field(output, "quest_revision");
            if (revision.Length == 0) revision = Field(output, "container_revision");
            if (revision.Length == 0) revision = Field(output, "revision");
            var receiptId = Field(output, "operation_id");
            if (receiptId.Length == 0) receiptId = operation;
            var line = JsonSerializer.Serialize(new { operationId = operation, rpc, receiptId, revision, acknowledgedAtUtc = DateTimeOffset.UtcNow });
            lock (Gate) File.AppendAllText(path, line + Environment.NewLine);
        }
    }
    public static void Run(string mode)
    {
        static string Required(string key) => Environment.GetEnvironmentVariable(key) ?? throw new Exception($"{key} required");
        using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{Required("GAMEPLAY_HOST_PORT")}");
        var invoker = Environment.GetEnvironmentVariable("REHEARSAL_RECEIPT_LEDGER") is { Length: > 0 } path
            ? channel.Intercept(new ReceiptLedger(path)) : channel.CreateCallInvoker();
        var client = new Pb.Housing.HousingClient(invoker);
        var headers = new Metadata { { "authorization", "Bearer " + Required("SF_GAMEPLAY_TOKEN") } };
        var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(Required("GAMEPLAY_LEDGER_CONNECTION")));
        long Revision() => ledger.Read("ctn_housing").Revision;
        long Quantity() => ledger.Read("ctn_housing").Quantities.GetValueOrDefault("item/wall_material_bundle");
        long StructureRevision() => new PostgresHousingStore(Required("GAMEPLAY_LEDGER_CONNECTION")).StructureSnapshot("clm_test").Revision;
        long PermissionRevision() => new PostgresHousingStore(Required("GAMEPLAY_LEDGER_CONNECTION")).Claim("clm_test")!.PermissionRevision;
        Pb.PlacementReply Place(string op, string structure, long structureRevision, long? containerRevision = null)
        {
            var request = new Pb.PlaceRequest { OperationId=op, ClaimId="clm_test", ActorId="chr_housing",
                ExpectedPermissionRevision=PermissionRevision(), ExpectedStructureRevision=structureRevision, StructureId=structure,
                ContentId="piece/wall", MaterialId="item/wall_material_bundle", MaterialQuantity=1,
                ContainerId="ctn_housing", ExpectedContainerRevision=containerRevision ?? Revision(), CellX=checked((int)structureRevision+10) };
            return client.Place(request, headers, deadline: DateTime.UtcNow.AddSeconds(10));
        }
        Pb.PlacementReply Remove(string op, string structure, long structureRevision, long containerRevision) =>
            client.Remove(new Pb.RemoveRequest { OperationId=op, ClaimId="clm_test", ActorId="chr_housing",
                StructureId=structure, ExpectedPermissionRevision=PermissionRevision(), ExpectedStructureRevision=structureRevision,
                ExpectedContainerRevision=containerRevision, RemovalKind="undo" },
                headers, deadline: DateTime.UtcNow.AddSeconds(10));
        static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
        switch (mode)
        {
            case "fixture-outbox-seed":
            case "fixture-outbox-replay":
            case "fixture-outbox-conflict":
            case "fixture-outbox-omit":
            {
                using var inventoryChannel = GrpcChannel.ForAddress($"http://127.0.0.1:{Required("GAMEPLAY_LEDGER_PORT")}");
                var inventory = new InventoryPb.InventoryLedger.InventoryLedgerClient(inventoryChannel);
                var character = "chr_int06";
                var container = inventory.EnsureContainer(new InventoryPb.EnsureContainerRequest { Spec = new InventoryPb.ContainerSpec {
                    Id = "ctn_int06", OwnerKind = InventoryPb.OwnerKind.Character, OwnerId = character,
                    Kind = InventoryPb.ContainerKind.Backpack, Capacity = 100 } }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                new ProgressPb.Progression.ProgressionClient(invoker).ApplyReceipt(new ProgressPb.ApplyReceiptRequest { SourceReceiptId = "int06/accept/1", Kind = "quest", CharacterId = character,
                    Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder", Kind = "accept", OccurredAtUnixMs = 1792540800000 } }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                var request = new ProgressPb.ApplyReceiptRequest { SourceReceiptId = "int06/gather/1", CharacterId = character,
                    Kind = "gather", Payload = new ProgressPb.ActionPayload { SkillId = "skill/logging", Difficulty = 1,
                        ActionSeconds = mode == "fixture-outbox-conflict" ? 7 : 6, Success = true },
                    Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder", StepId = "harvest_oak",
                        Kind = "gather", TargetId = "item/oak_log", NodeId = "gathering_node/oak_tree",
                        Quantity = 1, OccurredAtUnixMs = 1792540800000 } };
                var bytes = request.ToByteArray();
                var grant = new InventoryPb.GrantRequest { OperationId = request.SourceReceiptId, ContainerId = "ctn_int06",
                    ExpectedRevision = mode == "fixture-outbox-seed" ? container.Revision : 0,
                    DefinitionId = "item/oak_log", Quantity = 1, Source = "fixture", CausationId = "int06/gather/1",
                    FollowUpIntent = new InventoryPb.FollowUpIntent { PayloadVersion = 1,
                        Payload = ByteString.CopyFrom(bytes), PayloadDigest = Convert.ToHexStringLower(SHA256.HashData(bytes)) } };
                if (mode == "fixture-outbox-omit") grant.FollowUpIntent = null;
                if (mode == "fixture-outbox-seed")
                {
                    var missing = grant.Clone(); missing.FollowUpIntent = null;
                    try { inventory.GrantWithIntent(missing, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                        throw new Exception("Intent-required RPC accepted a missing intent."); }
                    catch (RpcException ex) when (ex.StatusCode == StatusCode.InvalidArgument) { }
                }
                try
                {
                    var receipt = mode == "fixture-outbox-omit"
                        ? inventory.Grant(grant, headers, deadline: DateTime.UtcNow.AddSeconds(10))
                        : inventory.GrantWithIntent(grant, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                    Check(mode is not ("fixture-outbox-conflict" or "fixture-outbox-omit") && receipt.OperationId == grant.OperationId, "Outbox grant result mismatch.");
                }
                catch (RpcException ex) when (mode is "fixture-outbox-conflict" or "fixture-outbox-omit" && ex.StatusCode == StatusCode.AlreadyExists) { }
                Console.WriteLine("PASS " + mode);
                break;
            }
            case "fixture-outbox-check":
            {
                var state = new ProgressPb.Progression.ProgressionClient(invoker).GetCharacterState(
                    new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_int06" }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(state.LifetimeXp > 0 && state.ActiveStepId == "mine_iron",
                    $"Outbox effect mismatch: xp={state.LifetimeXp} step={state.ActiveStepId}");
                Check(new PostgresInventoryLedgerStore(Required("GAMEPLAY_LEDGER_CONNECTION")).OutboxCounts() == (0, 0), "Outbox not drained.");
                Console.WriteLine($"PASS INT-06 XP={state.LifetimeXp} quest oak=1");
                break;
            }
            case "fixture-rehearsal-schema-baseline":
            case "fixture-rehearsal-schema-newer":
                var schema = Required("REHEARSAL_SCHEMA");
                var newer = mode.EndsWith("newer", StringComparison.Ordinal);
                void CheckSchema(Action read, Type exceptionType)
                {
                    try { read(); if (newer) throw new Exception($"{schema} newer schema accepted."); }
                    catch (Exception ex) when (newer && ex.GetType() == exceptionType) { }
                }
                switch (schema)
                {
                    case "inventory": CheckSchema(() => ledger.Read("ctn_journey"), typeof(LedgerSchemaCompatibilityException)); break;
                    case "character_state": CheckSchema(() => new PostgresCharacterStateStore(Required("GAMEPLAY_CHARACTER_CONNECTION")).ReadProgression("chr_journey"), typeof(CharacterSchemaCompatibilityException)); break;
                    case "housing": CheckSchema(() => new PostgresHousingStore(Required("GAMEPLAY_HOUSING_CONNECTION")).Claim("clm_journey"), typeof(HousingSchemaCompatibilityException)); break;
                    case "exchange": CheckSchema(() => new PostgresExchangeStore(Required("GAMEPLAY_EXCHANGE_CONNECTION")).Wallet("chr_exchange_a"), typeof(ExchangeSchemaCompatibilityException)); break;
                    default: throw new Exception("Unknown rehearsal schema.");
                }
                Console.WriteLine($"PASS {schema} schema {(newer ? "newer refused" : "current accepted")}");
                break;
            case "fixture-rehearsal-after-backup":
                new PostgresExchangeStore(Required("GAMEPLAY_EXCHANGE_CONNECTION"))
                    .Credit("rehearsal/post-backup-credit", "chr_exchange_a", 1, "starter_kit");
                File.AppendAllText(Required("REHEARSAL_RECEIPT_LEDGER"), JsonSerializer.Serialize(new {
                    operationId = "rehearsal/post-backup-credit", rpc = "Exchange.Credit", receiptId = "rehearsal/post-backup-credit",
                    revision = "", acknowledgedAtUtc = DateTimeOffset.UtcNow }) + Environment.NewLine);
                Console.WriteLine("PASS post-backup wallet write");
                break;
            case "fixture-exchange-seed":
                var exchangeStore = new PostgresExchangeStore(Required("GAMEPLAY_EXCHANGE_CONNECTION"));
                foreach (var actor in new[] { "chr_exchange_seller", "chr_exchange_a", "chr_exchange_b" })
                    ledger.CreateContainer(new ContainerSpec("ctn_" + actor, OwnerKind.Character, actor, ContainerKind.Backpack, 100));
                ledger.CreateContainer(new ContainerSpec("ctn_trade_escrow", OwnerKind.Character, "chr_exchange_seller", ContainerKind.Escrow, 100));
                ledger.CreateContainer(new ContainerSpec("ctn_fault_escrow", OwnerKind.Character, "chr_exchange_seller", ContainerKind.Escrow, 100));
                ledger.CreateContainer(new ContainerSpec("ctn_cancel_escrow", OwnerKind.Character, "chr_exchange_seller", ContainerKind.Escrow, 100));
                ledger.CreateContainer(new ContainerSpec("ctn_expire_escrow", OwnerKind.Character, "chr_exchange_seller", ContainerKind.Escrow, 100));
                ledger.Grant("exchange/seed", "ctn_chr_exchange_seller", 0, "item/iron_ore", 5, "fixture", "exchange/seed");
                ledger.Transfer("exchange/trade-escrow", "ctn_chr_exchange_seller", 1, "ctn_trade_escrow", 0, "item/iron_ore", 1);
                exchangeStore.Credit("exchange/credit-a", "chr_exchange_a", 100, "starter_kit");
                exchangeStore.Credit("exchange/credit-b", "chr_exchange_b", 100, "starter_kit");
                Console.WriteLine("PASS exchange wallets and inventory seeded");
                break;
            case "fixture-exchange-trade":
                var tradeClient = new ExchangePb.Exchange.ExchangeClient(invoker);
                var offer = tradeClient.OfferTrade(new ExchangePb.TradeOfferRequest { OperationId = "exchange/offer", TradeId = "trd_wire",
                    PartyA = "chr_exchange_seller", PartyB = "chr_exchange_a", GoldB = 10,
                    Deliveries = { new ExchangePb.ItemDelivery { EscrowId = "ctn_trade_escrow", DestinationId = "ctn_chr_exchange_a", ItemId = "item/iron_ore", Quantity = 1 } } }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(offer.State == "pending", "Trade offer was not pending.");
                Check(tradeClient.ListTrades(new ExchangePb.ListTradesRequest { CharacterId = "chr_exchange_a", PageSize = 10 }, headers,
                    deadline: DateTime.UtcNow.AddSeconds(10)).Trades.Any(x => x.TradeId == "trd_wire"), "Pending trade read missing.");
                var accepted = tradeClient.AcceptTrade(new ExchangePb.TradeActionRequest { OperationId = "exchange/accept", OfferOperationId = "exchange/offer",
                    TradeId = "trd_wire", ActorId = "chr_exchange_a" }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(accepted.State == "committed" && ledger.Read("ctn_chr_exchange_a").Quantities.GetValueOrDefault("item/iron_ore") == 1,
                    "Trade did not deliver exactly once.");
                Check(tradeClient.Wallet(new ExchangePb.WalletRequest { OwnerId = "chr_exchange_a" }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).Balance == 90 &&
                    tradeClient.Wallet(new ExchangePb.WalletRequest { OwnerId = "chr_exchange_seller" }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).Balance == 10,
                    "Trade wallet transfer incorrect.");
                Console.WriteLine("PASS two-character trade over gRPC");
                break;
            case "fixture-exchange-market":
                var market = new ExchangePb.Exchange.ExchangeClient(invoker);
                foreach (var (caseKey, item, marketId, quantity, price, refusal) in new[]
                {
                    ("gear", "item/iron_axe", "market/silverbrook", 1L, 8L, "invalid_listing"),
                    ("wrong-market", "item/iron_ore", "market/eastford", 1L, 8L, "invalid_listing"),
                    ("quantity", "item/iron_ore", "market/silverbrook", 101L, 8L, "quantity_above_limit"),
                    ("cap", "item/iron_ore", "market/silverbrook", 1L, 101L, "price_above_cap"),
                    ("fee", "item/iron_ore", "market/silverbrook", 1L, 1L, "price_below_fee")
                })
                {
                    try
                    {
                        market.ListItem(new ExchangePb.ListItemRequest { OperationId = "exchange/refuse-" + caseKey, ListingId = "lst_refuse_" + caseKey,
                            SellerId = "chr_exchange_seller", SourceContainerId = "ctn_chr_exchange_seller", EscrowContainerId = "ctn_chr_exchange_seller:escrow",
                            ItemId = item, Quantity = quantity, UnitPrice = price, MarketId = marketId }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                        throw new Exception("Market accepted " + caseKey);
                    }
                    catch (RpcException ex) when (ex.Status.Detail == refusal) { }
                }
                var listed = market.ListItem(new ExchangePb.ListItemRequest { OperationId = "exchange/list", ListingId = "lst_wire",
                    SellerId = "chr_exchange_seller", SourceContainerId = "ctn_chr_exchange_seller", EscrowContainerId = "ctn_chr_exchange_seller:escrow",
                    ItemId = "item/iron_ore", Quantity = 1, UnitPrice = 20, MarketId = "market/silverbrook", CreatedAtUnixMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(listed.State == "active" && listed.Fee == 1, "Listing fee or state incorrect.");
                Check(ledger.Read("ctn_chr_exchange_seller:escrow").Spec.Kind == ContainerKind.Escrow &&
                    market.ListListings(new ExchangePb.ListListingsRequest { MarketId = "market/silverbrook", PageSize = 10 }, headers,
                        deadline: DateTime.UtcNow.AddSeconds(10)).Listings.Any(x => x.Id == "lst_wire"), "Host did not create and list escrow.");
                try
                {
                    market.ListListings(new ExchangePb.ListListingsRequest { MarketId = "market/eastford", PageSize = 10 }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                    throw new Exception("Unknown market exposed listings.");
                }
                catch (RpcException ex) when (ex.Status.Detail == "invalid_market") { }
                var start = new ManualResetEventSlim();
                var buys = new[] { "a", "b" }.Select(buyer => Task.Run(() =>
                {
                    start.Wait();
                    try { return market.BuyListing(new ExchangePb.BuyListingRequest { OperationId = "exchange/buy-" + buyer, ListingId = "lst_wire",
                        BuyerId = "chr_exchange_" + buyer, DestinationContainerId = "ctn_chr_exchange_" + buyer,
                        ExpectedRevision = 0, ExpectedUnitPrice = 20, MarketId = "market/silverbrook" }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).State == "committed"; }
                    catch (RpcException ex) when (ex.StatusCode == StatusCode.FailedPrecondition) { return false; }
                })).ToArray();
                start.Set(); Task.WaitAll(buys);
                Check(buys.Count(t => t.Result) == 1 &&
                    ledger.Read("ctn_chr_exchange_a").Quantities.GetValueOrDefault("item/iron_ore") +
                    ledger.Read("ctn_chr_exchange_b").Quantities.GetValueOrDefault("item/iron_ore") == 2 &&
                    market.Wallet(new ExchangePb.WalletRequest { OwnerId = "chr_exchange_seller" }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).Balance == 28,
                    "Concurrent buyers or 1%/5% fees incorrect.");
                Console.WriteLine("PASS two concurrent buyers yielded one sale");
                break;
            case "fixture-exchange-cancel":
                var cancelClient = new ExchangePb.Exchange.ExchangeClient(invoker);
                var sellerBag = ledger.Read("ctn_chr_exchange_seller");
                ledger.Transfer("exchange/cancel-escrow", sellerBag.Spec.Id, sellerBag.Revision, "ctn_cancel_escrow", 0, "item/iron_ore", 1);
                cancelClient.OfferTrade(new ExchangePb.TradeOfferRequest { OperationId = "exchange/offer-cancel", TradeId = "trd_cancel",
                    PartyA = "chr_exchange_seller", PartyB = "chr_exchange_b", GoldB = 5,
                    Deliveries = { new ExchangePb.ItemDelivery { EscrowId = "ctn_cancel_escrow", DestinationId = "ctn_chr_exchange_b", ItemId = "item/iron_ore", Quantity = 1 } } }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                var cancelled = cancelClient.CancelTrade(new ExchangePb.TradeActionRequest { OperationId = "exchange/cancel-trade",
                    OfferOperationId = "exchange/offer-cancel", TradeId = "trd_cancel", ActorId = "chr_exchange_seller" }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(cancelled.State == "cancelled" && ledger.Read("ctn_cancel_escrow").Quantities.GetValueOrDefault("item/iron_ore") == 0,
                    "Trade cancellation did not return escrow.");
                cancelClient.ListItem(new ExchangePb.ListItemRequest { OperationId = "exchange/list-cancel", ListingId = "lst_cancel_wire",
                    SellerId = "chr_exchange_seller", SourceContainerId = "ctn_chr_exchange_seller", EscrowContainerId = "ctn_cancel_escrow",
                    ItemId = "item/iron_ore", Quantity = 1, UnitPrice = 20, MarketId = "market/silverbrook", CreatedAtUnixMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                try
                {
                    cancelClient.CancelListing(new ExchangePb.ListingActionRequest { OperationId = "exchange/cancel-wrong-market",
                        ListingId = "lst_cancel_wire", SellerId = "chr_exchange_seller", MarketId = "market/eastford" }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                    throw new Exception("Cross-market cancellation accepted.");
                }
                catch (RpcException ex) when (ex.Status.Detail == "invalid_market") { }
                var unlisted = cancelClient.CancelListing(new ExchangePb.ListingActionRequest { OperationId = "exchange/cancel-list",
                    ListingId = "lst_cancel_wire", SellerId = "chr_exchange_seller", MarketId = "market/silverbrook" }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(unlisted.State == "cancelled" && ledger.Read("ctn_cancel_escrow").Quantities.GetValueOrDefault("item/iron_ore") == 0,
                    "Listing cancellation did not return escrow.");
                Console.WriteLine("PASS trade and listing cancellation returned escrow");
                break;
            case "fixture-exchange-expire-seed":
                var expireClient = new ExchangePb.Exchange.ExchangeClient(invoker);
                var bagView = ledger.Read("ctn_chr_exchange_seller");
                ledger.Transfer("exchange/expire-escrow", bagView.Spec.Id, bagView.Revision, "ctn_expire_escrow", 0, "item/iron_ore", 1);
                expireClient.OfferTrade(new ExchangePb.TradeOfferRequest { OperationId = "exchange/offer-expire", TradeId = "trd_expire",
                    PartyA = "chr_exchange_seller", PartyB = "chr_exchange_b", GoldB = 5,
                    Deliveries = { new ExchangePb.ItemDelivery { EscrowId = "ctn_expire_escrow", DestinationId = "ctn_chr_exchange_b", ItemId = "item/iron_ore", Quantity = 1 } } }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Console.WriteLine("PASS expiring trade offer created");
                break;
            case "fixture-exchange-expire-check":
                var expiredClient = new ExchangePb.Exchange.ExchangeClient(invoker);
                try
                {
                    var expired = expiredClient.AcceptTrade(new ExchangePb.TradeActionRequest { OperationId = "exchange/accept-expired",
                        OfferOperationId = "exchange/offer-expire", TradeId = "trd_expire", ActorId = "chr_exchange_b" },
                        headers, deadline: DateTime.UtcNow.AddSeconds(10));
                    Check(expired.State == "cancelled" && !expired.Committed, "Expired trade was accepted.");
                }
                catch (RpcException ex) when (ex.StatusCode == StatusCode.FailedPrecondition) { }
                Console.WriteLine("PASS expired trade offer rejected at accept");
                break;
            case "fixture-exchange-fault":
                var faultMarket = new ExchangePb.Exchange.ExchangeClient(invoker);
                faultMarket.ListItem(new ExchangePb.ListItemRequest { OperationId = "exchange/list-fault", ListingId = "lst_fault",
                    SellerId = "chr_exchange_seller", SourceContainerId = "ctn_chr_exchange_seller", EscrowContainerId = "ctn_fault_escrow",
                    ItemId = "item/iron_ore", Quantity = 1, UnitPrice = 20, MarketId = "market/silverbrook", CreatedAtUnixMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                try { faultMarket.BuyListing(new ExchangePb.BuyListingRequest { OperationId = "exchange/buy-fault", ListingId = "lst_fault",
                    BuyerId = "chr_exchange_b", DestinationContainerId = "ctn_chr_exchange_b", ExpectedRevision = 0, ExpectedUnitPrice = 20, MarketId = "market/silverbrook" },
                    headers, deadline: DateTime.UtcNow.AddSeconds(10)); throw new Exception("Exchange fault did not fire."); }
                catch (RpcException) { }
                Console.WriteLine("PASS exchange host killed after delivery");
                break;
            case "fixture-exchange-replay":
                var replayMarket = new ExchangePb.Exchange.ExchangeClient(invoker);
                var sale = replayMarket.BuyListing(new ExchangePb.BuyListingRequest { OperationId = "exchange/buy-fault", ListingId = "lst_fault",
                    BuyerId = "chr_exchange_b", DestinationContainerId = "ctn_chr_exchange_b", ExpectedRevision = 0, ExpectedUnitPrice = 20, MarketId = "market/silverbrook" },
                    headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(sale.State == "committed" && ledger.Read("ctn_chr_exchange_b").Quantities.GetValueOrDefault("item/iron_ore") >= 1,
                    "Exchange settlement did not recover.");
                Console.WriteLine("PASS exchange crash recovered one sale");
                break;
            case "fixture-fault-seed":
                var priorStructureRevision = StructureRevision();
                var placed = Place("place/fault", "str_fault", priorStructureRevision);
                Check(placed.State == "Committed" && placed.StructureRevision == priorStructureRevision + 1, "Fault placement failed.");
                try { Remove("remove/fault", "str_fault", placed.StructureRevision, Revision()); throw new Exception("Host fault did not fire."); }
                catch (RpcException) { }
                Console.WriteLine("PASS injected host fault after refund");
                break;
            case "fixture-fault-replay":
                var placeAcknowledgement = File.ReadLines(Required("REHEARSAL_RECEIPT_LEDGER"))
                    .Select(line => JsonDocument.Parse(line)).First(doc => doc.RootElement.GetProperty("operationId").GetString() == "place/fault");
                var originalStructureRevision = long.Parse(placeAcknowledgement.RootElement.GetProperty("revision").GetString()!);
                var replay = Remove("remove/fault", "str_fault", originalStructureRevision, Revision()-1);
                Check(replay.State == "Removed" && replay.StructureRevision == StructureRevision() && Quantity() == 1,
                    "Post-crash refund recovery duplicated or lost a result.");
                Console.WriteLine("PASS post-crash refund replay");
                break;
            case "fixture-stale-concurrent":
                var stalePiece = Place("place/stale", "str_stale", StructureRevision());
                var actual = Revision();
                var stale = Remove("remove/stale", "str_stale", stalePiece.StructureRevision, actual-1);
                Check(stale.State == "Compensated" && Quantity() == 0, "Stale refund was not compensated.");
                var corrected = Remove("remove/stale-retry", "str_stale", stalePiece.StructureRevision, actual);
                Check(corrected.State == "Removed" && Quantity() == 1, "New operation could not retry removal.");
                var concurrentPiece = Place("place/concurrent-seed", "str_concurrent_seed", corrected.StructureRevision);
                var before = Revision();
                var gate = new ManualResetEventSlim();
                var placeTask = Task.Run(() => { gate.Wait(); try { return Place("place/concurrent-race", "str_concurrent_race", concurrentPiece.StructureRevision, before).State; } catch (RpcException ex) when (ex.StatusCode == StatusCode.FailedPrecondition) { return "Compensated"; } });
                var removeTask = Task.Run(() => { gate.Wait(); try { return Remove("remove/concurrent-race", "str_concurrent_seed", concurrentPiece.StructureRevision, before).State; } catch (RpcException ex) when (ex.StatusCode == StatusCode.FailedPrecondition) { return "Compensated"; } });
                gate.Set(); Task.WaitAll(placeTask, removeTask);
                var placeWon = placeTask.Result == "Committed";
                var removeWon = removeTask.Result == "Removed";
                Check(placeWon != removeWon && Quantity() == (removeWon ? 1 : 0), "Concurrent place/remove broke conservation.");
                Console.WriteLine("PASS stale revision retry and concurrent place/remove");
                break;
            case "fixture-place-abandon":
            case "fixture-place-decided":
                // The host dies after the placement intent (abandon) or after its commit decision (decided).
                var fault = mode == "fixture-place-abandon" ? "abandon" : "decided";
                ledger.CreateContainer(new ContainerSpec($"ctn_{fault}", OwnerKind.Character, $"chr_{fault}", ContainerKind.Backpack, 100));
                ledger.Grant($"seed/{fault}", $"ctn_{fault}", 0, "item/wall_material_bundle", 1, "fixture", $"seed/{fault}");
                client.ClaimPlot(new Pb.ClaimPlotRequest { OperationId = $"claim/{fault}", ClaimId = $"clm_{fault}",
                    OwnerId = $"chr_{fault}", CenterX = fault == "abandon" ? 700 : 820, CenterY = 700, SideMetres = 40, PrepaidDays = 1 },
                    headers, deadline: DateTime.UtcNow.AddSeconds(10));
                try
                {
                    client.Place(new Pb.PlaceRequest { OperationId = $"place/{fault}", ClaimId = $"clm_{fault}",
                        ActorId = $"chr_{fault}", StructureId = $"str_{fault}", ContentId = "piece/wall",
                        MaterialId = "item/wall_material_bundle", MaterialQuantity = 1,
                        ContainerId = $"ctn_{fault}", ExpectedContainerRevision = 1 },
                        headers, deadline: DateTime.UtcNow.AddSeconds(10));
                    throw new Exception("Placement fault did not fire.");
                }
                catch (RpcException) { }
                Console.WriteLine($"PASS {fault} placement interrupted");
                break;
            case "fixture-quest-abandon":
            case "fixture-quest-full":
                var full = mode == "fixture-quest-full";
                var suffix = full ? "full" : "abandon";
                var containerId = "ctn_quest_" + suffix;
                var characterId = "chr_quest_" + suffix;
                ledger.CreateContainer(new ContainerSpec(containerId, OwnerKind.Character, characterId, ContainerKind.Backpack, full ? 13 : 100));
                if (full) ledger.Grant("quest-full/fill", containerId, 0, "item/oak_log", 1, "fixture", "quest-full/fill");
                var progress = new ProgressPb.Progression.ProgressionClient(invoker);
                progress.ApplyReceipt(new ProgressPb.ApplyReceiptRequest { SourceReceiptId = $"quest-{suffix}/accept", Kind = "quest", CharacterId = characterId,
                    Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder", Kind = "accept", OccurredAtUnixMs = 1792540800000 } }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                var steps = new (string Step, string Kind, string Target, string Node)[] {
                    ("harvest_oak","gather","item/oak_log","gathering_node/oak_tree"),
                    ("mine_iron","gather","item/iron_ore",""),
                    ("refine_materials","craft","item/rough_timber",""),
                    ("refine_materials","craft","item/iron_ingot",""),
                    ("place_wall","build","structure_tag/wall",""),
                    ("defeat_boar","defeat","npc/boar","") };
                for (var i = 0; i < steps.Length; i++)
                {
                    var step = steps[i];
                    var request = new ProgressPb.ApplyReceiptRequest { SourceReceiptId = $"quest-{suffix}/{i}",
                        Kind = "quest", CharacterId = characterId,
                        ExpectedQuestRevision = i,
                        Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder",
                            StepId = step.Step, Kind = step.Kind, TargetId = step.Target, NodeId = step.Node,
                            Quantity = 1, OccurredAtUnixMs = 1792540800000 + i * 1000 } };
                    try
                    {
                        var result = progress.ApplyReceipt(request, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                        if (i == steps.Length - 1) throw new Exception(full ? "Full backpack was accepted." : "Quest fault did not fire.");
                        Check(result.QuestAccepted, "Quest setup step rejected.");
                    }
                    catch (RpcException ex) when (i == steps.Length - 1 && (!full || ex.StatusCode == StatusCode.FailedPrecondition)) { }
                }
                Console.WriteLine(full ? "PASS full backpack rejects payout" : "PASS abandoned reward grant committed");
                break;
            case "fixture-quest-free":
                var view = ledger.Read("ctn_quest_full");
                ledger.Consume("quest-full/free", "ctn_quest_full", view.Revision, "item/oak_log", 1, "fixture");
                Console.WriteLine("PASS reward backpack space freed");
                break;
            default: throw new ArgumentException("Unknown fixture mode.");
        }
    }
}
