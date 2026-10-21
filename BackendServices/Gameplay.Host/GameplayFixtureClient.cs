using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.Exchange.Postgres;
using Pb = ShatteredFrontier.Housing.V1;
using ProgressPb = ShatteredFrontier.Progression.V1;
using ExchangePb = ShatteredFrontier.Exchange.V1;

// Exercised only by the owned Test-GameplayHosts fixture against its disposable cluster.
internal static class GameplayFixtureClient
{
    public static void Run(string mode)
    {
        static string Required(string key) => Environment.GetEnvironmentVariable(key) ?? throw new Exception($"{key} required");
        using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{Required("GAMEPLAY_HOST_PORT")}");
        var client = new Pb.Housing.HousingClient(channel);
        var headers = new Metadata { { "authorization", "Bearer " + Required("SF_GAMEPLAY_TOKEN") } };
        var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(Required("GAMEPLAY_LEDGER_CONNECTION")));
        long Revision() => ledger.Read("ctn_housing").Revision;
        long Quantity() => ledger.Read("ctn_housing").Quantities.GetValueOrDefault("item/wall_material_bundle");
        Pb.PlacementReply Place(string op, string structure, long structureRevision, long? containerRevision = null)
        {
            var request = new Pb.PlaceRequest { OperationId=op, ClaimId="clm_test", ActorId="chr_housing",
                ExpectedPermissionRevision=0, ExpectedStructureRevision=structureRevision, StructureId=structure,
                ContentId="piece/wall", MaterialId="item/wall_material_bundle", MaterialQuantity=1,
                ContainerId="ctn_housing", ExpectedContainerRevision=containerRevision ?? Revision(), CellX=checked((int)structureRevision+10) };
            return client.Place(request, headers, deadline: DateTime.UtcNow.AddSeconds(10));
        }
        Pb.PlacementReply Remove(string op, string structure, long structureRevision, long containerRevision) =>
            client.Remove(new Pb.RemoveRequest { OperationId=op, ClaimId="clm_test", ActorId="chr_housing",
                StructureId=structure, ExpectedPermissionRevision=0, ExpectedStructureRevision=structureRevision,
                ExpectedContainerRevision=containerRevision, RemovalKind="undo" },
                headers, deadline: DateTime.UtcNow.AddSeconds(10));
        static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
        switch (mode)
        {
            case "fixture-exchange-seed":
                var exchangeStore = new PostgresExchangeStore(Required("GAMEPLAY_EXCHANGE_CONNECTION"));
                foreach (var actor in new[] { "chr_exchange_seller", "chr_exchange_a", "chr_exchange_b" })
                    ledger.CreateContainer(new ContainerSpec("ctn_" + actor, OwnerKind.Character, actor, ContainerKind.Backpack, 100));
                ledger.CreateContainer(new ContainerSpec("ctn_trade_escrow", OwnerKind.Character, "chr_exchange_seller", ContainerKind.Escrow, 100));
                ledger.CreateContainer(new ContainerSpec("ctn_list_escrow", OwnerKind.Character, "chr_exchange_seller", ContainerKind.Escrow, 100));
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
                var tradeClient = new ExchangePb.Exchange.ExchangeClient(channel);
                var offer = tradeClient.OfferTrade(new ExchangePb.TradeOfferRequest { OperationId = "exchange/offer", TradeId = "trd_wire",
                    PartyA = "chr_exchange_seller", PartyB = "chr_exchange_a", GoldB = 10,
                    Deliveries = { new ExchangePb.ItemDelivery { EscrowId = "ctn_trade_escrow", DestinationId = "ctn_chr_exchange_a", ItemId = "item/iron_ore", Quantity = 1 } } }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(offer.State == "pending", "Trade offer was not pending.");
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
                var market = new ExchangePb.Exchange.ExchangeClient(channel);
                var listed = market.ListItem(new ExchangePb.ListItemRequest { OperationId = "exchange/list", ListingId = "lst_wire",
                    SellerId = "chr_exchange_seller", SourceContainerId = "ctn_chr_exchange_seller", EscrowContainerId = "ctn_list_escrow",
                    ItemId = "item/iron_ore", Quantity = 1, UnitPrice = 20, CreatedAtUnixMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(listed.State == "active", "Listing not active.");
                var start = new ManualResetEventSlim();
                var buys = new[] { "a", "b" }.Select(buyer => Task.Run(() =>
                {
                    start.Wait();
                    try { return market.BuyListing(new ExchangePb.BuyListingRequest { OperationId = "exchange/buy-" + buyer, ListingId = "lst_wire",
                        BuyerId = "chr_exchange_" + buyer, DestinationContainerId = "ctn_chr_exchange_" + buyer,
                        ExpectedRevision = 0, ExpectedUnitPrice = 20 }, headers, deadline: DateTime.UtcNow.AddSeconds(10)).State == "committed"; }
                    catch (RpcException ex) when (ex.StatusCode == StatusCode.FailedPrecondition) { return false; }
                })).ToArray();
                start.Set(); Task.WaitAll(buys);
                Check(buys.Count(t => t.Result) == 1 &&
                    ledger.Read("ctn_chr_exchange_a").Quantities.GetValueOrDefault("item/iron_ore") +
                    ledger.Read("ctn_chr_exchange_b").Quantities.GetValueOrDefault("item/iron_ore") == 2, "Concurrent buyers did not yield one sale.");
                Console.WriteLine("PASS two concurrent buyers yielded one sale");
                break;
            case "fixture-exchange-cancel":
                var cancelClient = new ExchangePb.Exchange.ExchangeClient(channel);
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
                    ItemId = "item/iron_ore", Quantity = 1, UnitPrice = 20, CreatedAtUnixMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                var unlisted = cancelClient.CancelListing(new ExchangePb.ListingActionRequest { OperationId = "exchange/cancel-list",
                    ListingId = "lst_cancel_wire", SellerId = "chr_exchange_seller" }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(unlisted.State == "cancelled" && ledger.Read("ctn_cancel_escrow").Quantities.GetValueOrDefault("item/iron_ore") == 0,
                    "Listing cancellation did not return escrow.");
                Console.WriteLine("PASS trade and listing cancellation returned escrow");
                break;
            case "fixture-exchange-expire-seed":
                var expireClient = new ExchangePb.Exchange.ExchangeClient(channel);
                var bagView = ledger.Read("ctn_chr_exchange_seller");
                ledger.Transfer("exchange/expire-escrow", bagView.Spec.Id, bagView.Revision, "ctn_expire_escrow", 0, "item/iron_ore", 1);
                expireClient.OfferTrade(new ExchangePb.TradeOfferRequest { OperationId = "exchange/offer-expire", TradeId = "trd_expire",
                    PartyA = "chr_exchange_seller", PartyB = "chr_exchange_b", GoldB = 5,
                    Deliveries = { new ExchangePb.ItemDelivery { EscrowId = "ctn_expire_escrow", DestinationId = "ctn_chr_exchange_b", ItemId = "item/iron_ore", Quantity = 1 } } }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Console.WriteLine("PASS expiring trade offer created");
                break;
            case "fixture-exchange-expire-check":
                var expiredClient = new ExchangePb.Exchange.ExchangeClient(channel);
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
                var faultMarket = new ExchangePb.Exchange.ExchangeClient(channel);
                faultMarket.ListItem(new ExchangePb.ListItemRequest { OperationId = "exchange/list-fault", ListingId = "lst_fault",
                    SellerId = "chr_exchange_seller", SourceContainerId = "ctn_chr_exchange_seller", EscrowContainerId = "ctn_fault_escrow",
                    ItemId = "item/iron_ore", Quantity = 1, UnitPrice = 20, CreatedAtUnixMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() }, headers, deadline: DateTime.UtcNow.AddSeconds(10));
                try { faultMarket.BuyListing(new ExchangePb.BuyListingRequest { OperationId = "exchange/buy-fault", ListingId = "lst_fault",
                    BuyerId = "chr_exchange_b", DestinationContainerId = "ctn_chr_exchange_b", ExpectedRevision = 0, ExpectedUnitPrice = 20 },
                    headers, deadline: DateTime.UtcNow.AddSeconds(10)); throw new Exception("Exchange fault did not fire."); }
                catch (RpcException) { }
                Console.WriteLine("PASS exchange host killed after delivery");
                break;
            case "fixture-exchange-replay":
                var replayMarket = new ExchangePb.Exchange.ExchangeClient(channel);
                var sale = replayMarket.BuyListing(new ExchangePb.BuyListingRequest { OperationId = "exchange/buy-fault", ListingId = "lst_fault",
                    BuyerId = "chr_exchange_b", DestinationContainerId = "ctn_chr_exchange_b", ExpectedRevision = 0, ExpectedUnitPrice = 20 },
                    headers, deadline: DateTime.UtcNow.AddSeconds(10));
                Check(sale.State == "committed" && ledger.Read("ctn_chr_exchange_b").Quantities.GetValueOrDefault("item/iron_ore") >= 1,
                    "Exchange settlement did not recover.");
                Console.WriteLine("PASS exchange crash recovered one sale");
                break;
            case "fixture-fault-seed":
                var placed = Place("place/fault", "str_fault", 2);
                Check(placed.State == "Committed" && placed.StructureRevision == 3, "Fault placement failed.");
                try { Remove("remove/fault", "str_fault", 3, Revision()); throw new Exception("Host fault did not fire."); }
                catch (RpcException) { }
                Console.WriteLine("PASS injected host fault after refund");
                break;
            case "fixture-fault-replay":
                var replay = Remove("remove/fault", "str_fault", 3, 4);
                Check(replay.State == "Removed" && replay.StructureRevision == 4 && Quantity() == 1,
                    "Post-crash refund recovery duplicated or lost a result.");
                Console.WriteLine("PASS post-crash refund replay");
                break;
            case "fixture-stale-concurrent":
                var stalePiece = Place("place/stale", "str_stale", 4);
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
                ledger.CreateContainer(new ContainerSpec("ctn_abandon", OwnerKind.Character, "chr_abandon", ContainerKind.Backpack, 100));
                ledger.Grant("seed/abandon", "ctn_abandon", 0, "item/wall_material_bundle", 1, "fixture", "seed/abandon");
                client.ClaimPlot(new Pb.ClaimPlotRequest { OperationId = "claim/abandon", ClaimId = "clm_abandon",
                    OwnerId = "chr_abandon", CenterX = 700, CenterY = 700, SideMetres = 40, PrepaidDays = 1 },
                    headers, deadline: DateTime.UtcNow.AddSeconds(10));
                try
                {
                    client.Place(new Pb.PlaceRequest { OperationId = "place/abandon", ClaimId = "clm_abandon",
                        ActorId = "chr_abandon", StructureId = "str_abandon", ContentId = "piece/wall",
                        MaterialId = "item/wall_material_bundle", MaterialQuantity = 1,
                        ContainerId = "ctn_abandon", ExpectedContainerRevision = 1 },
                        headers, deadline: DateTime.UtcNow.AddSeconds(10));
                    throw new Exception("Placement fault did not fire.");
                }
                catch (RpcException) { }
                Console.WriteLine("PASS abandoned placement reserved");
                break;
            case "fixture-quest-abandon":
            case "fixture-quest-full":
                var full = mode == "fixture-quest-full";
                var suffix = full ? "full" : "abandon";
                var containerId = "ctn_quest_" + suffix;
                var characterId = "chr_quest_" + suffix;
                ledger.CreateContainer(new ContainerSpec(containerId, OwnerKind.Character, characterId, ContainerKind.Backpack, full ? 13 : 100));
                if (full) ledger.Grant("quest-full/fill", containerId, 0, "item/oak_log", 1, "fixture", "quest-full/fill");
                var progress = new ProgressPb.Progression.ProgressionClient(channel);
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

