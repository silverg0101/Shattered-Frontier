using Grpc.Core;
using Grpc.Net.Client;
using Google.Protobuf;
using CharacterAppearance;
using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.Progression;
using Npgsql;
using System.Text.Json;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Hosting.Server;
using Microsoft.AspNetCore.Hosting.Server.Features;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using Microsoft.Extensions.DependencyInjection;
using RewardPb = ShatteredFrontier.Rewards.V1;
using ProgressPb = ShatteredFrontier.Progression.V1;
using HousingPb = ShatteredFrontier.Housing.V1;
using GatheringPb = ShatteredFrontier.Gathering.V1;
using WorldPb = ShatteredFrontier.World.V1;
using GroupPb = ShatteredFrontier.Group.V1;
using ExchangePb = ShatteredFrontier.Exchange.V1;

static void Check(bool condition, string message)
{
    if (!condition) throw new Exception(message);
}
static void Code(Action call, StatusCode expected)
{
    try { call(); throw new Exception($"Expected {expected}."); }
    catch (RpcException ex) when (ex.StatusCode == expected) { }
}
var mode = args.Single();
var port = Environment.GetEnvironmentVariable("GAMEPLAY_HOST_PORT") ?? throw new Exception("GAMEPLAY_HOST_PORT required");
var token = Environment.GetEnvironmentVariable("SF_GAMEPLAY_TOKEN") ?? throw new Exception("SF_GAMEPLAY_TOKEN required");
var connection = Environment.GetEnvironmentVariable("GAMEPLAY_LEDGER_CONNECTION") ?? throw new Exception("GAMEPLAY_LEDGER_CONNECTION required");
using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{port}");
var headers = new Metadata { { "authorization", "Bearer " + token } };
var bad = new Metadata { { "authorization", "Bearer wrong" } };
DateTime Deadline() => DateTime.UtcNow.AddSeconds(5);

if (mode is "gear-repair" or "gear-repair-restart")
{
    var exchange = new ExchangePb.Exchange.ExchangeClient(channel);
    using var db = new NpgsqlConnection(connection); db.Open();
    static void Sql(NpgsqlConnection db, string sql)
    { using var cmd = new NpgsqlCommand(sql, db); cmd.ExecuteNonQuery(); }
    static (long Balance, int Current, int Max, int Receipts, int Debits) State(NpgsqlConnection db, string owner, string instance)
    {
        using var cmd = new NpgsqlCommand("""
            SELECT w.balance,i.durability_percent,i.max_durability_percent,
            (SELECT count(*) FROM inventory.repair_receipts WHERE owner_id=$1),
            (SELECT count(*) FROM exchange.wallet_entries WHERE owner_id=$1 AND code='repair_cost')
            FROM exchange.wallets w JOIN inventory.item_instances i ON i.instance_id=$2 WHERE w.owner_id=$1
            """, db);
        cmd.Parameters.AddWithValue(owner); cmd.Parameters.AddWithValue(instance);
        using var row = cmd.ExecuteReader(); Check(row.Read(), "Gear repair state exists.");
        return (row.GetInt64(0),row.GetInt32(1),row.GetInt32(2),Convert.ToInt32(row.GetInt64(3)),Convert.ToInt32(row.GetInt64(4)));
    }
    const string op = "58d518be-d038-4e6b-871d-ab082aac0b21";
    if (mode == "gear-repair")
    {
        Sql(db, """
            INSERT INTO inventory.containers(id,owner_kind,owner_id,kind,capacity) VALUES
            ('ctn_repair_equipped',0,'chr_repair',0,16),('ctn_repair_poor',0,'chr_repair_poor',1,16),
            ('ctn_repair_single',0,'chr_repair_single',1,16);
            INSERT INTO inventory.entries(container_id,definition_id,quantity) VALUES
            ('ctn_repair_equipped','item/hide_cap',1),('ctn_repair_equipped','item/iron_shortsword',1),
            ('ctn_repair_poor','item/hide_cap',1),('ctn_repair_single','item/hide_cap',1);
            INSERT INTO inventory.item_instances(instance_id,container_id,definition_id,durability_percent) VALUES
            ('itm_repair_broken','ctn_repair_equipped','item/hide_cap',0),
            ('itm_repair_worn','ctn_repair_equipped','item/iron_shortsword',40),
            ('itm_repair_poor','ctn_repair_poor','item/hide_cap',0),
            ('itm_repair_single','ctn_repair_single','item/hide_cap',40);
            INSERT INTO exchange.wallets(owner_id,balance) VALUES
            ('chr_repair',5000),('chr_repair_poor',0),('chr_repair_single',5000);
            """);
        var quote = exchange.QuoteRepair(new ExchangePb.RepairQuoteRequest { OwnerId="chr_repair", AllEquipped=true },headers,Deadline());
        Check(quote.ItemCount == 2 && quote.CostCopper > 0, "All equipped quote prices two damaged copies.");
        var request = new ExchangePb.RepairGearRequest { OperationId=op, QuoteId=quote.QuoteId, OwnerId="chr_repair" };
        var receipt = exchange.RepairGear(request,headers,Deadline());
        var state = State(db,"chr_repair","itm_repair_broken");
        Check(receipt.Committed && state.Balance == 5000-quote.CostCopper && state.Debits == 1,
            "Repair fee debited once in the Exchange ledger.");
        Check(state.Current == 98 && state.Max == 98 && State(db,"chr_repair","itm_repair_worn").Current == 100,
            "Broken and worn gear restore to their maximum durability; broken maximum loses two points.");
        var replay = exchange.RepairGear(request,headers,Deadline());
        state = State(db,"chr_repair","itm_repair_broken");
        Check(replay.CostCopper == receipt.CostCopper && state.Debits == 1 && state.Receipts == 1 && state.Balance == 5000-quote.CostCopper,
            "Repair request replay is idempotent.");
        var poor = exchange.QuoteRepair(new ExchangePb.RepairQuoteRequest { OwnerId="chr_repair_poor", InstanceId="itm_repair_poor" },headers,Deadline());
        Code(() => exchange.RepairGear(new ExchangePb.RepairGearRequest { OperationId=Guid.NewGuid().ToString(), QuoteId=poor.QuoteId,
            OwnerId="chr_repair_poor" },headers,Deadline()),StatusCode.FailedPrecondition);
        Check(State(db,"chr_repair_poor","itm_repair_poor").Current == 0 && State(db,"chr_repair_poor","itm_repair_poor").Debits == 0,
            "Insufficient funds refuse repair without changing gear.");
        var single = exchange.QuoteRepair(new ExchangePb.RepairQuoteRequest { OwnerId="chr_repair_single", InstanceId="itm_repair_single" },headers,Deadline());
        exchange.RepairGear(new ExchangePb.RepairGearRequest { OperationId=Guid.NewGuid().ToString(), QuoteId=single.QuoteId,
            OwnerId="chr_repair_single" },headers,Deadline());
        Check(State(db,"chr_repair_single","itm_repair_single").Current == 100,
            "One selected copy can be repaired.");
        Console.WriteLine("PASS gear repair: 5 checks");
    }
    else
    {
        using var cmd = new NpgsqlCommand("SELECT quote_id FROM inventory.repair_receipts WHERE operation_id=$1",db);
        cmd.Parameters.AddWithValue(op); var quoteId = (string?)cmd.ExecuteScalar() ?? throw new Exception("Repair receipt lost on restart.");
        var receipt = exchange.RepairGear(new ExchangePb.RepairGearRequest { OperationId=op, QuoteId=quoteId,
            OwnerId="chr_repair" },headers,Deadline());
        var state = State(db,"chr_repair","itm_repair_broken");
        Check(receipt.Committed && state.Current == 98 && state.Debits == 1 && state.Receipts == 1,
            "PostgreSQL restart preserves the repair and its replay receipt.");
        Console.WriteLine("PASS gear repair restart: 1 check");
    }
    return;
}

if (mode == "raid-rewards")
{
    var group = new GroupPb.GroupService.GroupServiceClient(channel);
    var rewards = new RewardPb.Rewards.RewardsClient(channel);
    var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
    GroupPb.Party MakeParty(string suffix)
    {
        var leader = "chr_raid_" + suffix;
        var created = group.CreateParty(new GroupPb.CreatePartyRequest { IdempotencyKey = "raid/create/" + suffix,
            LeaderCharacterId = leader, AccountId = "acct_raid_" + suffix, RealmId = "realm_dev", DcRegion = "dc_dev" },
            headers, Deadline());
        return group.JoinParty(new GroupPb.JoinPartyRequest { IdempotencyKey = "raid/join/" + suffix,
            PartyId = created.PartyId, ActorCharacterId = leader, CharacterId = leader + "_second",
            AccountId = "acct_raid_" + suffix + "_second", RealmId = "realm_dev" }, headers, Deadline());
    }
    var a = MakeParty("a"); var b = MakeParty("b");
    var raid = group.CreateRaid(new GroupPb.CreateRaidRequest { IdempotencyKey = "raid/establish",
        PartyId = a.PartyId, ActorCharacterId = "chr_raid_a" }, headers, Deadline());
    Check(raid.RaidGroupId.StartsWith("rgp_") && raid.PartyIds.SequenceEqual([a.PartyId]), "Raid creation failed.");
    var link = new GroupPb.AddPartyToRaidRequest { IdempotencyKey = "raid/link-b", RaidGroupId = raid.RaidGroupId,
        PartyId = b.PartyId, ActorCharacterId = "chr_raid_a", JoiningPartyLeaderCharacterId = "chr_raid_b" };
    raid = group.AddPartyToRaid(link, headers, Deadline());
    Check(raid.PartyIds.Count == 2 && raid.MemberCharacterIds.Count == 4 &&
        group.AddPartyToRaid(link, headers, Deadline()).Equals(raid), "Two-party raid or replay failed.");
    var aView = group.GetPartyForCharacter(new GroupPb.CharacterRef { CharacterId = "chr_raid_a_second" }, headers, Deadline());
    var bView = group.GetPartyForCharacter(new GroupPb.CharacterRef { CharacterId = "chr_raid_b_second" }, headers, Deadline());
    Check(aView.RaidGroupId == raid.RaidGroupId && bView.RaidGroupId == raid.RaidGroupId &&
        aView.RaidMemberCharacterIds.Count == 4 && bView.RaidMemberCharacterIds.Count == 4,
        "Raid membership was not visible from both parties.");
    Code(() => group.CreateRaid(new GroupPb.CreateRaidRequest { IdempotencyKey = "raid/duplicate",
        PartyId = b.PartyId, ActorCharacterId = "chr_raid_b" }, headers, Deadline()), StatusCode.FailedPrecondition);
    foreach (var member in new[] { "chr_raid_a_second", "chr_raid_b_second" })
    {
        var container = "ctn_" + member;
        ledger.CreateContainer(new ContainerSpec(container, OwnerKind.Character, member, ContainerKind.Backpack, 100));
        var personal = rewards.GrantDefeatReward(new RewardPb.GrantDefeatRewardRequest {
            DefeatEventId = "defeat/raid-ordinary", KillerCharacterId = "chr_raid_a_second",
            RecipientCharacterId = member, ContainerId = container, ExpectedContainerRevision = 0,
            CreatureId = "npc/boar", TableId = "loot/greenlands_silverbrook_boar" }, headers, Deadline());
        Check(personal.Granted && ledger.Read(container).Quantities.GetValueOrDefault("item/boar_hide") > 0,
            "A raid member in another party missed personal loot.");
    }
    var boss = new RewardPb.CreateSharedDefeatDropRequest { DefeatEventId = "defeat/raid-boss",
        CreatureId = "npc/test_boss", TableId = "loot/boss_greenlands_bandit_captain",
        KillerCharacterId = "chr_raid_a_second", RaidGroupId = raid.RaidGroupId };
    boss.EligibleCharacterIds.AddRange(["chr_raid_a_second", "chr_raid_b_second"]);
    var drop = rewards.CreateSharedDefeatDrop(boss, headers, Deadline());
    Check(drop.RaidGroupId == raid.RaidGroupId && drop.Items.Count == 1,
        "Boss did not snapshot one raid-wide set.");
    var claim = rewards.ClaimSharedDefeatDrop(new RewardPb.ClaimSharedDefeatDropRequest {
        DefeatEventId = drop.DefeatEventId, CharacterId = "chr_raid_b_second",
        ContainerId = "ctn_chr_raid_b_second" }, headers, Deadline());
    Check(claim.ClaimantCharacterId == "chr_raid_b_second" && claim.RaidGroupId == raid.RaidGroupId,
        "Second party could not claim the raid set.");
    var claimedBag = ledger.Read("ctn_chr_raid_b_second");
    Check(claim.Items.All(item => claimedBag.Quantities.GetValueOrDefault(item.DefinitionId) == item.Quantity) &&
        claim.Items.Where(item => GearDefinitions.IsGear(item.DefinitionId)).All(item =>
            claimedBag.Instances!.Count(instance => instance.DefinitionId == item.DefinitionId) == item.Quantity),
        "Boss claim did not persist readable gear copies.");
    Code(() => rewards.ClaimSharedDefeatDrop(new RewardPb.ClaimSharedDefeatDropRequest {
        DefeatEventId = drop.DefeatEventId, CharacterId = "chr_raid_a_second",
        ContainerId = "ctn_chr_raid_a_second" }, headers, Deadline()), StatusCode.AlreadyExists);
    var changed = boss.Clone(); changed.RaidGroupId = "rgp_other";
    Code(() => rewards.CreateSharedDefeatDrop(changed, headers, Deadline()), StatusCode.AlreadyExists);
    Console.WriteLine("PASS raid two-party membership/replay/visibility/personal grants/single boss claim (8)");
    return;
}

if (mode is "group" or "group-restart")
{
    var group = new GroupPb.GroupService.GroupServiceClient(channel);
    if (mode == "group")
    {
        var created = group.CreateParty(new GroupPb.CreatePartyRequest { IdempotencyKey="group/create", LeaderCharacterId="chr_dev_alpha",
            AccountId="acct_dev_alpha", RealmId="realm_dev", DcRegion="dc_dev" }, headers, Deadline());
        Check(created.PartyId.StartsWith("pty_", StringComparison.Ordinal) && created.MemberCharacterIds.SequenceEqual(["chr_dev_alpha"]), "Create failed.");
        var replay = group.CreateParty(new GroupPb.CreatePartyRequest { IdempotencyKey="group/create", LeaderCharacterId="chr_dev_alpha",
            AccountId="acct_dev_alpha", RealmId="realm_dev", DcRegion="dc_dev" }, headers, Deadline());
        Check(replay.PartyId == created.PartyId, "Create replay changed ID.");
        Code(() => group.CreateParty(new GroupPb.CreatePartyRequest { IdempotencyKey="group/create", LeaderCharacterId="chr_dev_gamma",
            AccountId="acct_dev_gamma", RealmId="realm_dev", DcRegion="dc_dev" }, headers, Deadline()), StatusCode.InvalidArgument);
        Code(() => group.CreateParty(new GroupPb.CreatePartyRequest { IdempotencyKey="group/duplicate", LeaderCharacterId="chr_dev_alpha",
            AccountId="acct_dev_alpha", RealmId="realm_dev", DcRegion="dc_dev" }, headers, Deadline()), StatusCode.FailedPrecondition);
        var joined = group.JoinParty(new GroupPb.JoinPartyRequest { IdempotencyKey="group/join-beta", PartyId=created.PartyId,
            ActorCharacterId="chr_dev_alpha", CharacterId="chr_dev_beta", AccountId="acct_dev_beta", RealmId="realm_dev" }, headers, Deadline());
        Check(joined.MemberCharacterIds.Count == 2 && joined.Version == 1, "Join failed.");
        Code(() => group.JoinParty(new GroupPb.JoinPartyRequest { IdempotencyKey="group/invalid-join", PartyId=created.PartyId,
            ActorCharacterId="chr_dev_beta", CharacterId="chr_dev_gamma", AccountId="acct_dev_gamma", RealmId="realm_dev" }, headers, Deadline()), StatusCode.PermissionDenied);
        joined = group.JoinParty(new GroupPb.JoinPartyRequest { IdempotencyKey="group/join-gamma", PartyId=created.PartyId,
            ActorCharacterId="chr_dev_alpha", CharacterId="chr_dev_gamma", AccountId="acct_dev_gamma", RealmId="realm_dev" }, headers, Deadline());
        Check(joined.MemberCharacterIds.Count == 3, "Third member missing.");
        var promoted = group.PromoteLeader(new GroupPb.MemberActionRequest { IdempotencyKey="group/promote-beta", PartyId=created.PartyId,
            ActorCharacterId="chr_dev_alpha", CharacterId="chr_dev_beta" }, headers, Deadline());
        Check(promoted.LeaderCharacterId == "chr_dev_beta", "Explicit promotion failed.");
        promoted = group.PromoteLeader(new GroupPb.MemberActionRequest { IdempotencyKey="group/promote-alpha", PartyId=created.PartyId,
            ActorCharacterId="chr_dev_beta", CharacterId="chr_dev_alpha" }, headers, Deadline());
        Check(promoted.LeaderCharacterId == "chr_dev_alpha", "Return promotion failed.");
        var assistant = group.AssignAssistant(new GroupPb.MemberActionRequest { IdempotencyKey="group/assistant", PartyId=created.PartyId,
            ActorCharacterId="chr_dev_alpha", CharacterId="chr_dev_beta" }, headers, Deadline());
        Check(assistant.AssistantCharacterId == "chr_dev_beta", "Assistant not assigned.");
        var left = group.LeaveParty(new GroupPb.MemberActionRequest { IdempotencyKey="group/leader-leave", PartyId=created.PartyId,
            ActorCharacterId="chr_dev_alpha", CharacterId="chr_dev_alpha" }, headers, Deadline());
        Check(left.LeaderCharacterId == "chr_dev_beta" && left.MemberCharacterIds.Count == 2, "Assistant succession failed.");
        Code(() => group.KickMember(new GroupPb.MemberActionRequest { IdempotencyKey="group/bad-kick", PartyId=created.PartyId,
            ActorCharacterId="chr_dev_gamma", CharacterId="chr_dev_beta" }, headers, Deadline()), StatusCode.PermissionDenied);
        Code(() => group.CreateParty(new GroupPb.CreatePartyRequest { IdempotencyKey="group/occupied", LeaderCharacterId="chr_dev_gamma",
            AccountId="acct_dev_gamma", RealmId="realm_dev", DcRegion="dc_dev" }, headers, Deadline()), StatusCode.FailedPrecondition);
        using var db = new NpgsqlConnection(connection); db.Open();
        try { using var ddl = new NpgsqlCommand("CREATE TABLE \"group\".forbidden_runtime_ddl(id int)", db); ddl.ExecuteNonQuery(); throw new Exception("Runtime role has DDL privilege."); }
        catch (PostgresException ex) when (ex.SqlState == "42501") { }
        Console.WriteLine("PASS group create/replay/conflict/join/one-party/permissions/promotion/assistant succession/DML-only (10)");
    }
    else
    {
        var beta = group.GetPartyForCharacter(new GroupPb.CharacterRef { CharacterId="chr_dev_beta" }, headers, Deadline());
        Check(beta.MemberCharacterIds.SequenceEqual(["chr_dev_beta", "chr_dev_gamma"]) && beta.LeaderCharacterId == "chr_dev_beta", "Party lost after host restart.");
        var kicked = group.KickMember(new GroupPb.MemberActionRequest { IdempotencyKey="group/kick-gamma", PartyId=beta.PartyId,
            ActorCharacterId="chr_dev_beta", CharacterId="chr_dev_gamma" }, headers, Deadline());
        Check(kicked.MemberCharacterIds.SequenceEqual(["chr_dev_beta"]), "Kick failed.");
        var disbanded = group.LeaveParty(new GroupPb.MemberActionRequest { IdempotencyKey="group/disband", PartyId=beta.PartyId,
            ActorCharacterId="chr_dev_beta", CharacterId="chr_dev_beta" }, headers, Deadline());
        Check(disbanded.State == "disbanded" && disbanded.MemberCharacterIds.Count == 0 &&
            group.GetPartyForCharacter(new GroupPb.CharacterRef { CharacterId="chr_dev_beta" }, headers, Deadline()).PartyId == "", "Last-member disband failed.");
        Console.WriteLine("PASS group restart/restore/kick/disband (4)");
    }
    return;
}

if (mode == "gathering-old-host")
{
    // A host built before HarvestWithIntent still implements Harvest, but its base service returns UNIMPLEMENTED
    // for the new method. Verify the new client method cannot reach that legacy mutation.
    var builder = WebApplication.CreateBuilder();
    builder.WebHost.ConfigureKestrel(options => options.Listen(System.Net.IPAddress.Loopback, 0,
        listen => listen.Protocols = HttpProtocols.Http2));
    builder.Services.AddGrpc();
    await using var app = builder.Build();
    app.MapGrpcService<LegacyGatheringService>();
    await app.StartAsync();
    try
    {
        var address = app.Services.GetRequiredService<IServer>().Features.Get<IServerAddressesFeature>()!.Addresses.Single();
        using var oldChannel = GrpcChannel.ForAddress(address);
        var oldClient = new GatheringPb.Gathering.GatheringClient(oldChannel);
        var request = new GatheringPb.HarvestRequest { OperationId = "old-host/1", CharacterId = "chr_old_host",
            NodeId = "gathering_node/oak_tree", Generation = 1, ContainerId = "ctn_old_host", FollowUpVersion = 1,
            FollowUpPayload = ByteString.CopyFromUtf8("immutable intent"), FollowUpDigest = new string('a', 64) };
        Code(() => oldClient.HarvestWithIntent(request, deadline: Deadline()), StatusCode.Unimplemented);
        Check(LegacyGatheringService.HarvestCalls == 0, "Old host mutated a node through the new method.");
        Check(oldClient.Harvest(request, deadline: Deadline()).Status == "granted" &&
            LegacyGatheringService.HarvestCalls == 1, "Legacy host fixture does not expose the dangerous old method.");
        Console.WriteLine("PASS new gather client fails closed on old host UNIMPLEMENTED; legacy Harvest was not invoked");
    }
    finally { await app.StopAsync(); }
    return;
}

if (mode == "world-slots")
{
    var client = new WorldPb.WorldSlots.WorldSlotsClient(channel);
    var tick = new WorldPb.AdvanceSlotsRequest { Command = "tick", HostileSpawning = true };
    Code(() => client.Advance(tick, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    Code(() => client.Advance(new() { Command = "defeat", GroupId = "other", HostileSpawning = true },
        headers, deadline: Deadline()), StatusCode.InvalidArgument);
    var initial = client.Advance(tick, headers, deadline: Deadline());
    Check(initial.Slots.Count == 10 && initial.Slots.All(s => s.Active && s.PointId.Length > 0 && s.Cycle == 0),
        "Initial boar slots missing.");
    var first = initial.Slots[0];
    var defeated = client.Advance(new() { Command = "defeat", GroupId = first.GroupId,
        SlotIndex = first.SlotIndex, PlayersInArea = 1, HostileSpawning = true }, headers, deadline: Deadline());
    Check(defeated.Slots.Single(s => s.GroupId == first.GroupId && s.SlotIndex == first.SlotIndex).Cycle == 1,
        "Defeat did not advance the slot cycle.");
    Check(!defeated.Slots.Single(s => s.GroupId == first.GroupId && s.SlotIndex == first.SlotIndex).Active,
        "Defeated slot respawned before its deadline.");
    var disabled = client.Advance(new() { Command = "tick", HostileSpawning = false }, headers, deadline: Deadline());
    Check(disabled.Slots.All(s => !s.Active), "Hostile spawn switch did not disable slots.");
    Console.WriteLine("PASS world-slots wire auth, initial groups, defeat cycle, hostile switch");
    return;
}

if (mode == "gathering")
{
    var client = new GatheringPb.Gathering.GatheringClient(channel);
    new InventoryLedger(new PostgresInventoryLedgerStore(connection)).CreateContainer(
        new ContainerSpec("ctn_gathering", OwnerKind.Character, "chr_gathering", ContainerKind.Backpack, 100));
    const string id = "zone/greenlands_silverbrook/eastwood_iron_ore/0";
    GatheringPb.NodeReply Node() => client.GetNode(new() { NodeId = id }, headers, deadline: Deadline());
    var initial = Node();
    Check(initial.Generation == 1 && initial.Charges == 5 && initial.GrowthStage == "mature", "Initial iron node state missing.");
    Code(() => client.ListZoneNodes(new() { ZoneId = "zone/greenlands_silverbrook" }, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    Code(() => client.ListZoneNodes(new() { ZoneId = "zone/unknown" }, headers, deadline: Deadline()), StatusCode.NotFound);
    var listed = client.ListZoneNodes(new() { ZoneId = "zone/greenlands_silverbrook" }, headers, deadline: Deadline());
    Check(listed.Nodes.Count > 0 && listed.Nodes.Single(n => n.NodeId == id).Equals(initial) &&
        initial.Active && initial.PoolId == "eastwood_iron_ore" && initial.PointId.Length > 0 &&
        double.IsFinite(initial.PositionXM) && double.IsFinite(initial.PositionYM), "Initial placement list differs from GetNode.");
    Code(() => client.GetNode(new() { NodeId = "zone/unknown/node/0" }, headers, deadline: Deadline()), StatusCode.NotFound);
    GatheringPb.HarvestRequest Request(string operation, int generation)
    {
        var number = operation.StartsWith("gathering/mature/", StringComparison.Ordinal) ? int.Parse(operation[17..]) :
            operation == "gathering/young/1" ? 5 : operation == "gathering/young/2" ? 6 : 0;
        var follow = new ProgressPb.ApplyReceiptRequest { SourceReceiptId = operation, CharacterId = "chr_gathering", Kind = "gather",
            ExpectedProgressionRevision = number,
            Payload = new ProgressPb.ActionPayload { SkillId = "skill/mining", Difficulty = 1, ActionSeconds = 1, Success = true } };
        var bytes = follow.ToByteArray();
        return new GatheringPb.HarvestRequest { OperationId = operation, CharacterId = "chr_gathering", NodeId = id,
            Generation = generation, ContainerId = "ctn_gathering", FollowUpVersion = 1,
            FollowUpPayload = ByteString.CopyFrom(bytes), FollowUpDigest = Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(bytes)) };
    }
    GatheringPb.HarvestReply Harvest(string operation, int generation) => client.HarvestWithIntent(Request(operation, generation), headers, deadline: Deadline());
    Code(() => client.Harvest(new() { OperationId = "legacy", CharacterId = "chr_gathering", NodeId = id, Generation = 1 },
        headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    for (var i = 0; i < 5; i++)
    {
        var result = Harvest($"gathering/mature/{i}", 1);
        Check(result.Status == "granted" && result.Quantity == 1 && result.ResourceId == "item/iron_ore" &&
            result.Node.Charges == 4 - i, "Mature harvest or charge count differs.");
    }
    var last = Harvest("gathering/mature/4", 1);
    Check(last.Node.RegrowAtUnixMs > DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(), "Backend respawn deadline missing.");
    Check(!last.Node.Active && !Node().Active &&
        !client.ListZoneNodes(new() { ZoneId = "zone/greenlands_silverbrook" }, headers, deadline: Deadline())
            .Nodes.Single(n => n.NodeId == id).Active, "Depleted slot remains active.");
    Check(Harvest("gathering/mature/4", 1).Equals(last), "Idempotent harvest replay differs.");
    var conflict = Request("gathering/mature/4", 1); conflict.CharacterId = "chr_other";
    Code(() => client.HarvestWithIntent(conflict,
        headers, deadline: Deadline()), StatusCode.InvalidArgument);
    Check(Harvest("gathering/depleted", 1).Status == "depleted", "Depleted node accepted another harvest.");
    var until = DateTime.UtcNow.AddSeconds(20);
    GatheringPb.NodeReply young;
    do { Thread.Sleep(100); young = Node(); } while (young.GrowthStage != "young" && DateTime.UtcNow < until);
    Check(young.Generation == 2 && young.Charges == 2 && young.MaxCharges == 5, "Immature stage did not reduce charges to two.");
    Check(young.Active && young.PointId != initial.PointId &&
        client.ListZoneNodes(new() { ZoneId = "zone/greenlands_silverbrook" }, headers, deadline: Deadline())
            .Nodes.Single(n => n.NodeId == id).PointId == young.PointId, "Rotated point was not listed.");
    Check(Harvest("gathering/mature/4", 1).Equals(last), "Old harvest replay changed after growth.");
    Check(Harvest("gathering/stale", 1).Status == "stale_generation", "Old generation accepted.");
    Check(Harvest("gathering/young/1", 2).Node.Charges == 1, "First immature charge differs.");
    var youngLast = Harvest("gathering/young/2", 2);
    Check(youngLast.Status == "granted" && youngLast.Node.Charges == 0 &&
        youngLast.Node.RegrowAtUnixMs > DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(), "Immature node exceeded stage charges.");
    Console.WriteLine("PASS gathering RPC immature stage has 2/5 charges, depletion, backend respawn and replay");
    return;
}

if (mode is "gathering-backup-seed" or "gathering-replay" or "gathering-fault" or "gathering-fault-replay" or
    "gathering-grant-fault" or "gathering-grant-replay")
{
    var client = new GatheringPb.Gathering.GatheringClient(channel);
    const string iron = "zone/greenlands_silverbrook/eastwood_iron_ore/0";
    const string oak = "zone/greenlands_silverbrook/westfields_wood/0";
    GatheringPb.HarvestRequest Request(string operation, string character, string node, string container, string skill)
    {
        var follow = new ProgressPb.ApplyReceiptRequest { SourceReceiptId = operation, CharacterId = character, Kind = "gather",
            ExpectedProgressionRevision = operation == "gathering/fault/2" ? 1 : 0,
            Payload = new ProgressPb.ActionPayload { SkillId = skill, Difficulty = 1, ActionSeconds = 1, Success = true } };
        var bytes = follow.ToByteArray();
        return new GatheringPb.HarvestRequest { OperationId = operation, CharacterId = character, NodeId = node,
            Generation = 1, ContainerId = container, FollowUpVersion = 1, FollowUpPayload = ByteString.CopyFrom(bytes),
            FollowUpDigest = Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(bytes)) };
    }
    if (mode == "gathering-replay")
    {
        var before = client.GetNode(new() { NodeId = iron }, headers, deadline: Deadline());
        Check(before.Generation == 2 && before.Charges == 0 && before.GrowthStage == "" &&
            before.NextTransitionUnixMs > DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            "Restart lost the depleted young node or its next emergence deadline.");
        using (var db = new NpgsqlConnection(connection))
        {
            db.Open();
            using var cmd = new NpgsqlCommand("SELECT state::text FROM inventory.gathering_node_state WHERE realm_id='zone/greenlands_silverbrook'", db);
            using var saved = JsonDocument.Parse((string)cmd.ExecuteScalar()!);
            var state = saved.RootElement.EnumerateArray().Single(n => n.GetProperty("Id").GetString() == iron);
            var matureAt = state.GetProperty("RegrowAt").GetDateTimeOffset();
            var respawnSeconds = state.GetProperty("ScheduledRespawnSeconds").GetDouble();
            var emergeAt = matureAt.AddSeconds(-respawnSeconds * .5);
            Check(before.NextTransitionUnixMs == emergeAt.ToUnixTimeMilliseconds(),
                "Restart recalculated the backend emergence deadline instead of using persisted node state.");
        }
        var prior = Request("gathering/mature/4", "chr_gathering", iron, "ctn_gathering", "skill/mining");
        prior.FollowUpPayload = ByteString.CopyFrom(new ProgressPb.ApplyReceiptRequest { SourceReceiptId = prior.OperationId,
            CharacterId = prior.CharacterId, Kind = "gather", ExpectedProgressionRevision = 4,
            Payload = new ProgressPb.ActionPayload { SkillId = "skill/mining", Difficulty = 1, ActionSeconds = 1, Success = true } }.ToByteArray());
        prior.FollowUpDigest = Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(prior.FollowUpPayload.Span));
        var replay = client.HarvestWithIntent(prior, headers, deadline: Deadline());
        Check(replay.Status == "granted" && replay.Node.Generation == 1 && replay.Node.Charges == 0,
            "Restart changed the saved harvest receipt.");
        var emergenceUntil = DateTime.UtcNow.AddSeconds(25);
        GatheringPb.NodeReply emerged;
        do { Thread.Sleep(100); emerged = client.GetNode(new() { NodeId = iron }, headers, deadline: Deadline()); }
        while (emerged.GrowthStage != "young" && DateTime.UtcNow < emergenceUntil);
        Check(emerged.Generation == 3 && emerged.Charges == 2 && emerged.GrowthStage == "young" &&
            emerged.NextTransitionUnixMs > before.NextTransitionUnixMs,
            "Restarted host did not advance depleted young iron from its saved deadline.");
        Check(emerged.Active && emerged.PointId != before.PointId &&
            client.ListZoneNodes(new() { ZoneId = "zone/greenlands_silverbrook" }, headers, deadline: Deadline())
                .Nodes.Single(n => n.NodeId == iron).PointId == emerged.PointId,
            "Restarted host did not retain the rotated placement.");
        Console.WriteLine("PASS depleted young node, saved deadline, receipt and next stage survive PostgreSQL and host restart");
        return;
    }
    if (mode == "gathering-backup-seed")
    {
        new InventoryLedger(new PostgresInventoryLedgerStore(connection)).CreateContainer(
            new ContainerSpec("ctn_gathering_backup", OwnerKind.Character, "chr_gathering_backup", ContainerKind.Backpack, 100));
        var seed = Request("gathering/backup/1", "chr_gathering_backup", oak, "ctn_gathering_backup", "skill/logging");
        var saved = client.HarvestWithIntent(seed, headers, deadline: Deadline());
        Check(saved.Status == "granted" && saved.Node.Charges == 2, "Gathering backup seed did not persist one harvest.");
        Console.WriteLine("PASS gathering backup point has populated node state and harvest receipt");
        return;
    }
    var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
    if (mode == "gathering-fault")
        ledger.CreateContainer(new ContainerSpec("ctn_gathering_fault", OwnerKind.Character, "chr_gathering_fault", ContainerKind.Backpack, 100));
    var request = Request(mode is "gathering-grant-fault" or "gathering-grant-replay" ? "gathering/fault/2" : "gathering/fault/1",
        "chr_gathering_fault", oak, "ctn_gathering_fault", "skill/logging");
    if (mode is "gathering-fault" or "gathering-grant-fault")
    {
        try { client.HarvestWithIntent(request, headers, deadline: Deadline()); throw new Exception("Fault host returned normally."); }
        catch (RpcException ex) when (ex.StatusCode is StatusCode.Unavailable or StatusCode.DeadlineExceeded) { }
        Console.WriteLine(mode == "gathering-fault" ? "PASS injected host death after gathering intent commit" :
            "PASS injected host death after gathering ledger grant");
        return;
    }
    var until = DateTime.UtcNow.AddSeconds(20);
    while (ledger.Receipt(request.OperationId) is null && DateTime.UtcNow < until) Thread.Sleep(100);
    var result = client.HarvestWithIntent(request, headers, deadline: Deadline());
    var expected = mode == "gathering-grant-replay" ? 2 : 1;
    Check(result.Status == "granted" && result.Node.Charges == 3 - expected &&
        ledger.Read(request.ContainerId).Quantities.GetValueOrDefault("item/oak_log") == expected,
        "Crash replay duplicated or lost grant/charge.");
    Check(client.HarvestWithIntent(request, headers, deadline: Deadline()).Equals(result), "Crash replay not idempotent.");
    Console.WriteLine(mode == "gathering-fault-replay" ? "PASS gathering crash replay granted once and consumed one charge" :
        "PASS post-grant crash replay kept one grant per charge");
    return;
}

if (mode.EndsWith("-outage", StringComparison.Ordinal))
{
    if (mode == "rewards-outage")
        Code(() => new RewardPb.Rewards.RewardsClient(channel).GrantDefeatReward(new RewardPb.GrantDefeatRewardRequest
        {
            DefeatEventId = "defeat/outage", KillerCharacterId = "chr_rewards", RecipientCharacterId = "chr_rewards",
            ContainerId = "ctn_rewards", TableId = "loot/greenlands_silverbrook_boar"
        }, headers, deadline: Deadline()), StatusCode.Unavailable);
    else if (mode == "progression-outage")
    {
        Code(() => new ProgressPb.Progression.ProgressionClient(channel).GetCharacterState(
            new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers,
            deadline: Deadline()), StatusCode.Unavailable);
        Code(() => new ProgressPb.Progression.ProgressionClient(channel).SavePosition(new ProgressPb.SavePositionRequest
        { CharacterId = "chr_progression", ExpectedPositionRevision = 1,
            Position = new ProgressPb.CharacterPosition { ZoneId = "silverbrook" } }, headers, deadline: Deadline()), StatusCode.Unavailable);
    }
    else if (mode == "housing-outage")
        Code(() => new HousingPb.Housing.HousingClient(channel).GetClaim(
            new HousingPb.GetClaimRequest { ClaimId = "clm_test" }, headers,
            deadline: Deadline()), StatusCode.Unavailable);
    Console.WriteLine($"PASS {mode} UNAVAILABLE mapping");
    return;
}
if (mode == "appearance")
{
    var client = new ProgressPb.Progression.ProgressionClient(channel);
    var appearance = new Appearance(2, 0, 1, 1, 128, 128, new byte[8], new ushort[4], new byte[4], [], 6);
    byte[] packet = AppearanceCodec.Pack(appearance);
    var create = new ProgressPb.CreateCharacterRequest { CharacterId = "chr_appearance", AppearancePacket = ByteString.CopyFrom(packet), ArchetypeId = "archer" };
    Check(client.CreateCharacter(create, headers, deadline: Deadline()).AppearanceRevision == 1,
        "Appearance create revision missing.");
    var stored = new PostgresCharacterStateStore(Environment.GetEnvironmentVariable("GAMEPLAY_CHARACTER_CONNECTION")!).ReadAppearance(create.CharacterId);
    Check(stored is { Revision: 1 } && stored.Result.SequenceEqual(packet), "Appearance bytes were not stored.");
    var storedCombat = new PostgresCharacterStateStore(Environment.GetEnvironmentVariable("GAMEPLAY_CHARACTER_CONNECTION")!)
        .ReadProgression(create.CharacterId)?.Result.CombatIdentity;
    Check(storedCombat is { Archetype: "archer", Base.Strength: 10, Base.Dexterity: 40, Base.Constitution: 20 },
        "Server-derived Archer allocation was not stored.");
    Check(client.CreateCharacter(create, headers, deadline: Deadline()).AppearanceRevision == 1,
        "Create on a returning gateway login changed the appearance revision.");
    var loaded = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = create.CharacterId }, headers, deadline: Deadline());
    Check(loaded.HasAppearancePacket && loaded.AppearancePacket.ToByteArray().SequenceEqual(packet) && loaded.FaceId == "Aelfwyn",
        "Stored face and packet missing on load.");
    Check(loaded.CombatAttributes is { Archetype: "archer", Strength: 10, Dexterity: 40, Constitution: 20 },
        "Selected archetype attributes were not restored.");
    try
    {
        client.CreateCharacter(new ProgressPb.CreateCharacterRequest { CharacterId = "chr_archetype_invalid",
            AppearancePacket = ByteString.CopyFrom(packet), ArchetypeId = "custom" }, headers, deadline: Deadline());
        throw new Exception("Unknown archetype was accepted.");
    }
    catch (RpcException ex) when (ex.StatusCode == StatusCode.InvalidArgument && ex.Status.Detail == "ARCHETYPE_INVALID") { }
    Check(new PostgresCharacterStateStore(Environment.GetEnvironmentVariable("GAMEPLAY_CHARACTER_CONNECTION")!)
        .ReadProgression("chr_archetype_invalid") is null, "Unknown archetype was stored.");
    var gather = new ProgressPb.ApplyReceiptRequest { SourceReceiptId = "appearance/gather/oak", Kind = "gather",
        ExpectedProgressionRevision = loaded.ProgressionRevision,
        CharacterId = create.CharacterId, Payload = new ProgressPb.ActionPayload { SkillId = "skill/logging",
            Difficulty = 25, ActionSeconds = 6, Success = true } };
    var firstGather = client.ApplyReceipt(gather, headers, deadline: Deadline());
    Check(firstGather.ProgressionApplied && firstGather.SkillGain is >= 0 and <= 1 &&
        client.ApplyReceipt(gather, headers, deadline: Deadline()).Equals(firstGather),
        "Gather receipt did not replay its original server roll.");
    var afterGather = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = create.CharacterId }, headers, deadline: Deadline());
    Check(afterGather.GatheringSkills["skill/logging"] == firstGather.SkillGain &&
        afterGather.Skills["skill/logging"] == firstGather.SkillGain,
        "Gather skill did not round trip through character state.");
    var invalid = (byte[])packet.Clone(); invalid[^1] = 0; // Orlando is not a female-body face.
    try
    {
        client.CreateCharacter(new ProgressPb.CreateCharacterRequest { CharacterId = "chr_appearance_invalid",
            AppearancePacket = ByteString.CopyFrom(invalid), ArchetypeId = "fighter" }, headers, deadline: Deadline());
        throw new Exception("Invalid appearance was accepted.");
    }
    catch (RpcException ex) when (ex.StatusCode == StatusCode.InvalidArgument && ex.Status.Detail == "APPEARANCE_INVALID") { }
    Check(new PostgresCharacterStateStore(Environment.GetEnvironmentVariable("GAMEPLAY_CHARACTER_CONNECTION")!).ReadAppearance("chr_appearance_invalid") is null,
        "Invalid appearance was stored.");
    client.SavePosition(new ProgressPb.SavePositionRequest { CharacterId = "chr_appearance_legacy",
        Position = new ProgressPb.CharacterPosition { ZoneId = "silverbrook" } }, headers, deadline: Deadline());
    var legacy = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_appearance_legacy" }, headers, deadline: Deadline());
    Check(!legacy.HasAppearancePacket && legacy.FaceId == "Orlando" && legacy.Position?.ZoneId == "silverbrook",
        "Legacy character face default missing.");
    Console.WriteLine("PASS appearance and gathering receipt/store/replay (5 cases)");
    return;
}
if (mode.EndsWith("-replay", StringComparison.Ordinal))
{
    if (mode == "rewards-replay")
    {
        var result = new RewardPb.Rewards.RewardsClient(channel).GrantDefeatReward(new RewardPb.GrantDefeatRewardRequest
        {
            DefeatEventId = "defeat/test-boar", KillerCharacterId = "chr_rewards", RecipientCharacterId = "chr_rewards",
            ContainerId = "ctn_rewards", TableId = "loot/greenlands_silverbrook_boar"
        }, headers, deadline: Deadline());
        Check(result.Granted && result.LedgerOperationId == "defeat:16:defeat/test-boar:chr_rewards", "Reward restart replay failed.");
        var shared = new RewardPb.Rewards.RewardsClient(channel).ClaimSharedDefeatDrop(
            new RewardPb.ClaimSharedDefeatDropRequest
            { DefeatEventId = "defeat/test-boss", CharacterId = "chr_rewards_second", ContainerId = "ctn_rewards_second" },
            headers, deadline: Deadline());
        Check(shared.ClaimantCharacterId == "chr_rewards_second" && shared.Items.Count == 1 &&
            shared.LedgerOperationId == "shared-defeat:16:defeat/test-boss", "Boss claim restart replay failed.");
        var pending = new RewardPb.Rewards.RewardsClient(channel).ClaimSharedDefeatDrop(
            new RewardPb.ClaimSharedDefeatDropRequest
            { DefeatEventId = "defeat/test-boss-pending", CharacterId = "chr_rewards", ContainerId = "ctn_shared_full" },
            headers, deadline: Deadline());
        var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
        Check(pending.Items.Count == 1 &&
            ledger.Read("ctn_shared_full").Quantities[pending.Items[0].DefinitionId] == pending.Items[0].Quantity,
            "Pending boss claim did not complete once after restart.");
    }
    else if (mode == "progression-replay")
    {
        var lockState = new ProgressPb.Progression.ProgressionClient(channel).GetCharacterState(
            new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_lock_wire" }, headers, deadline: Deadline());
        Check(lockState.GatheringSkillLocksAuthoritative && lockState.GatheringSkillLocks["skill/logging"] == "down" &&
            lockState.GatheringSkillLockReadyAtUnixMs["skill/logging"] > 0,
            "Gathering lock state missing after host and database restart.");
        var result = new ProgressPb.Progression.ProgressionClient(channel).ApplyReceipt(new ProgressPb.ApplyReceiptRequest
        {
            SourceReceiptId = "harvest/test-oak", Kind = "gather", CharacterId = "chr_progression",
            Payload = new ProgressPb.ActionPayload { SkillId = "skill/logging", Difficulty = 1, ActionSeconds = 6, Success = true },
            Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder", StepId = "harvest_oak", Kind = "gather",
                TargetId = "item/oak_log", NodeId = "gathering_node/oak_tree", Quantity = 1,
                OccurredAtUnixMs = 1792540800000 }
        }, headers, deadline: Deadline());
        Check(result.QuestAccepted && result.QuestRevision == 1, "Progression restart replay failed.");
        var state = new ProgressPb.Progression.ProgressionClient(channel).GetCharacterState(
            new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers, deadline: Deadline());
        Check(state.RestedMinutes >= 118 && state.RestedMinutes <= 121,
            "Rested pool missing after host restart.");
        Check(state.GatheringSkills.TryGetValue("skill/logging", out var logging) &&
            state.Skills["skill/logging"] == logging && logging is >= 0 and <= 1,
            "Gathering skill missing from durable character state.");
        Check(state.PositionRevision == 2 && state.Position?.ZoneId == "silverbrook" && state.Position.X == 11.5 &&
            state.Position.Y == -9.25 && state.Position.Z == 42 && state.Position.Yaw == 90,
            "Position missing after host and PostgreSQL restart.");
        var unstuck = new ProgressPb.CommitUnstuckRequest { OperationId = "00000000-0000-0000-0000-000000000101", CharacterId = "chr_unstuck_wire", CooldownSeconds = 900,
            Position = new() { ZoneId = "silverbrook", X = 220, Y = 0, Z = 40 } };
        Code(() => new ProgressPb.Progression.ProgressionClient(channel).CommitUnstuck(unstuck, headers, deadline: Deadline()),
            StatusCode.FailedPrecondition);
    }
    else if (mode == "housing-replay")
    {
        var client = new HousingPb.Housing.HousingClient(channel);
        var claim = client.ClaimPlot(new HousingPb.ClaimPlotRequest { OperationId = "claim/test", ClaimId = "clm_test",
            OwnerId = "chr_housing", CenterX = 900, CenterY = 900, SideMetres = 40, PrepaidDays = 1 }, headers,
            deadline: Deadline());
        var removed = client.Remove(new HousingPb.RemoveRequest { OperationId = "remove/test", ClaimId = "clm_test",
            ActorId = "chr_housing", StructureId = "str_test_wall", ExpectedPermissionRevision = 0,
            ExpectedStructureRevision = 1, ExpectedContainerRevision = 2, RemovalKind = "undo" }, headers,
            deadline: Deadline());
        Check(claim.State == "Active" && removed.State == "Removed", "Housing restart replay failed.");
        var structures = client.ListStructures(new HousingPb.ListStructuresRequest { ClaimId = "clm_test" }, headers, deadline: Deadline());
        Check(structures.StructureRevision >= 2 && structures.Structures.Count == 0, "Housing restart structure list failed.");
        var grantStructures = client.ListStructures(new HousingPb.ListStructuresRequest { ClaimId = "clm_grants" }, headers, deadline: Deadline());
        Check(grantStructures.StructureRevision == 1 && grantStructures.Structures.Count == 1 &&
            grantStructures.Structures[0].StructureId == "str_target_wall", "Granted target structure lost after restart.");
        var grant = client.Grant(new HousingPb.GrantRequest { OperationId = "grant/test", ClaimId = "clm_grants",
            ActorId = "chr_grants", TargetId = "chr_target", Rights = 1, ExpectedPermissionRevision = 0 }, headers, deadline: Deadline());
        Check(grant.PermissionRevision == 1 &&
            client.GetClaim(new HousingPb.GetClaimRequest { ClaimId = "clm_grants" }, headers, deadline: Deadline()).PermissionRevision == 2,
            "Housing grant receipt or revoke state lost after restart.");
    }
    Console.WriteLine($"PASS {mode} durable replay");
    return;
}
if (mode == "journey")
{
    Console.WriteLine("JOURNEY begin");
    var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
    ledger.CreateContainer(new ContainerSpec("ctn_journey", OwnerKind.Character, "chr_journey", ContainerKind.Backpack, 100));
    ledger.Grant("harvest/journey-oak", "ctn_journey", 0, "item/oak_log", 2, "gathering", "harvest/journey-oak");
    ledger.Grant("harvest/journey-ore", "ctn_journey", 1, "item/iron_ore", 2, "gathering", "harvest/journey-ore");
    var progress = new ProgressPb.Progression.ProgressionClient(channel);
    var housing = new HousingPb.Housing.HousingClient(channel);
    var rewards = new RewardPb.Rewards.RewardsClient(channel);
    // This journey starts with the modest trade skills needed for Common starter goods.
    using (var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("GAMEPLAY_CHARACTER_CONNECTION")))
    {
        db.Open();
        var seeded = new CharacterProgression("chr_journey", TimeProvider.System, () => .5,
            initialSkills: new Dictionary<string, int> { ["skill/carpentry"] = 100, ["skill/smithing"] = 100 }).Snapshot();
        using var cmd = new NpgsqlCommand("INSERT INTO character_state.progression VALUES ('chr_journey',1,$1::jsonb)", db);
        cmd.Parameters.AddWithValue(JsonSerializer.Serialize(seeded)); cmd.ExecuteNonQuery();
    }
    long progressionRevision = 1, questRevision = 0;
    Check(progress.ApplyReceipt(new ProgressPb.ApplyReceiptRequest { SourceReceiptId = "accept/journey", Kind = "quest", CharacterId = "chr_journey",
        Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder", Kind = "accept", OccurredAtUnixMs = 1792540800000 } },
        headers, deadline: Deadline()).QuestAccepted, "Journey accept failed.");
    ProgressPb.ApplyReceiptReply Apply(string id, string kind, string skill, string step, string objectiveKind,
        string target, string node = "")
    {
        var request = new ProgressPb.ApplyReceiptRequest
        {
            SourceReceiptId = id, Kind = kind, CharacterId = "chr_journey",
            ExpectedProgressionRevision = progressionRevision, ExpectedQuestRevision = questRevision,
            Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder", StepId = step,
                Kind = objectiveKind, TargetId = target, NodeId = node, Quantity = 1,
                OccurredAtUnixMs = 1792540800000 + questRevision * 1000 }
        };
        if (kind != "quest") request.Payload = new ProgressPb.ActionPayload { SkillId = skill,
            Difficulty = 1, ActionSeconds = 6, Success = true, EnemyLevel = 1, EnemyTierMultiplier = 1,
            EligiblePartySize = 1 };
        var result = progress.ApplyReceipt(request, headers, deadline: Deadline());
        Check(result.QuestAccepted, $"Journey quest rejected {id}.");
        var replay = progress.ApplyReceipt(request, headers, deadline: Deadline());
        Check(replay.Equals(result), $"Journey replay changed {id}.");
        progressionRevision = result.ProgressionRevision;
        questRevision = result.QuestRevision;
        return result;
    }
    Check(Apply("harvest/journey-oak", "gather", "skill/logging", "harvest_oak", "gather",
        "item/oak_log", "gathering_node/oak_tree").ActiveStepId == "mine_iron", "Oak step failed.");
    Check(Apply("harvest/journey-ore", "gather", "skill/mining", "mine_iron", "gather",
        "item/iron_ore").ActiveStepId == "refine_materials", "Ore step failed.");
    Console.WriteLine("JOURNEY harvested");
    var crafting = new ShatteredFrontier.Crafting.V1.Crafting.CraftingClient(channel);
    void Craft(string op, string recipe, int count)
    {
        var request = new ShatteredFrontier.Crafting.V1.CraftRequest { OperationId = op,
            CharacterId = "chr_journey", ContainerId = "ctn_journey", RecipeId = recipe,
            Count = count, ExpectedContainerRevision = ledger.Read("ctn_journey").Revision };
        var first = crafting.Craft(request, headers, deadline: Deadline());
        Check(first.Equals(crafting.Craft(request, headers, deadline: Deadline())), "Craft replay changed.");
        if (op == "craft/journey-timber")
        {
            request.Count++;
            Code(() => crafting.Craft(request, headers, deadline: Deadline()), StatusCode.AlreadyExists);
            request.Count--;
            request.CharacterId = "chr_other";
            Code(() => crafting.Craft(request, headers, deadline: Deadline()), StatusCode.PermissionDenied);
        }
    }
    Craft("craft/journey-timber", "recipe/rough_timber", 2);
    Apply("craft/journey-timber", "craft", "skill/smithing", "refine_materials", "craft", "item/rough_timber");
    var partial = progress.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_journey" }, headers, deadline: Deadline());
    Check(partial.ActiveStepId == "refine_materials" && partial.ObjectiveCounts["rough_timber"] == 1 &&
        partial.ObjectiveCounts["iron_ingot"] == 0, "Active quest objective counts missing.");
    Craft("craft/journey-ingot", "recipe/iron_ingot", 2);
    Check(Apply("craft/journey-ingot", "craft", "skill/smithing", "refine_materials", "craft",
        "item/iron_ingot").ActiveStepId == "place_wall", "Craft step failed.");
    Craft("craft/journey-nails", "recipe/iron_nails", 1);
    Craft("craft/journey-wall", "recipe/wall_material_bundle", 1);
    Console.WriteLine("JOURNEY crafted");
    var claim = housing.ClaimPlot(new HousingPb.ClaimPlotRequest { OperationId = "claim/journey", ClaimId = "clm_journey",
        OwnerId = "chr_journey", CenterX = -900, CenterY = -900, SideMetres = 40, PrepaidDays = 1,
        ExpectedWorldRevision = 1 }, headers, deadline: Deadline());
    Check(claim.State == "Active", "Journey claim failed.");
    var placement = housing.Place(new HousingPb.PlaceRequest { OperationId = "place/journey-wall", ClaimId = "clm_journey",
        ActorId = "chr_journey", StructureId = "str_journey_wall", ContentId = "piece/wall",
        MaterialId = "item/wall_material_bundle", MaterialQuantity = 1, ContainerId = "ctn_journey",
        ExpectedContainerRevision = ledger.Read("ctn_journey").Revision }, headers, deadline: Deadline());
    Check(placement.State == "Committed", "Journey place failed.");
    Console.WriteLine("JOURNEY placed");
    Check(Apply("place/journey-wall", "quest", "", "place_wall", "build", "structure_tag/wall").ActiveStepId == "defeat_boar",
        "Build step failed.");
    var reward = rewards.GrantDefeatReward(new RewardPb.GrantDefeatRewardRequest { DefeatEventId = "defeat/journey-boar",
        KillerCharacterId = "chr_journey", RecipientCharacterId = "chr_journey", ContainerId = "ctn_journey",
        ExpectedContainerRevision = ledger.Read("ctn_journey").Revision,
        TableId = "loot/greenlands_silverbrook_boar" }, headers, deadline: Deadline());
    Check(reward.Granted, "Journey boar reward failed.");
    Console.WriteLine("JOURNEY rewarded");
    var beforeQuestReward = ledger.Read("ctn_journey");
    var final = Apply("defeat/journey-boar", "defeat", "", "defeat_boar", "defeat", "npc/boar");
    Check(final.QuestCompleted && final.RewardBundleIds.Contains("reward/frontier_founder_completion"),
        "Journey completion failed.");
    var expectedReward = new Dictionary<string, long> { ["item/rough_timber"] = 4,
        ["item/iron_nails"] = 8, ["item/starter_tool_repair_kit"] = 1 };
    Check(final.GrantedItems.Count == expectedReward.Count && final.GrantedItems.All(i =>
        expectedReward.GetValueOrDefault(i.DefinitionId) == i.Quantity), "Quest reply item lines differ.");
    var afterQuestReward = ledger.Read("ctn_journey");
    Check(expectedReward.All(i => afterQuestReward.Quantities.GetValueOrDefault(i.Key) -
        beforeQuestReward.Quantities.GetValueOrDefault(i.Key) == i.Value), "Quest reward was not paid exactly once.");
    Console.WriteLine("PASS journey harvest, craft, place, boar reward, quest completion and replay");
    return;
}
if (mode == "recipes")
{
    var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
    ledger.CreateContainer(new ContainerSpec("ctn_recipe", OwnerKind.Character, "chr_recipe", ContainerKind.Backpack, 100));
    ledger.CreateContainer(new ContainerSpec("ctn_recipe_low", OwnerKind.Character, "chr_recipe_low", ContainerKind.Backpack, 10));
    using (var db = new NpgsqlConnection(connection))
    {
        db.Open();
        var seeded = new CharacterProgression("chr_recipe", TimeProvider.System, () => .5,
            initialSkills: new Dictionary<string, int> { ["skill/carpentry"] = 150, ["skill/smithing"] = 80 }).Snapshot();
        using var cmd = new NpgsqlCommand("INSERT INTO character_state.progression VALUES ('chr_recipe',1,$1::jsonb)", db);
        cmd.Parameters.AddWithValue(JsonSerializer.Serialize(seeded)); cmd.ExecuteNonQuery();
    }
    var exchange = new ShatteredFrontier.Exchange.Postgres.PostgresExchangeStore(connection);
    exchange.Credit("recipe/seed-wallet", "chr_recipe", 500, "gm_grant");
    exchange.Credit("recipe/low-wallet", "chr_recipe_low", 500, "gm_grant");
    var progression = new ProgressPb.Progression.ProgressionClient(channel);
    var crafting = new ShatteredFrontier.Crafting.V1.Crafting.CraftingClient(channel);
    Code(() => crafting.Craft(new ShatteredFrontier.Crafting.V1.CraftRequest { OperationId = "recipe/unknown-craft",
        CharacterId = "chr_recipe", ContainerId = "ctn_recipe", RecipeId = "recipe/worked_timber", Count = 1 },
        headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    var low = new ProgressPb.LearnRecipeRequest { OperationId = "recipe/low-learn", CharacterId = "chr_recipe_low",
        RecipeId = "recipe/worked_timber", Source = "trainer", TrainerNpc = "npc/millwarden_oswin" };
    Code(() => progression.LearnRecipe(low, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    Check(exchange.Wallet("chr_recipe_low").Balance == 500, "low-skill trainer debited wallet");
    var trainer = new ProgressPb.LearnRecipeRequest { OperationId = "recipe/trainer", CharacterId = "chr_recipe",
        RecipeId = "recipe/worked_timber", Source = "trainer", TrainerNpc = "npc/millwarden_oswin" };
    var taught = progression.LearnRecipe(trainer, headers, deadline: Deadline());
    Check(taught.Equals(progression.LearnRecipe(trainer, headers, deadline: Deadline())) && taught.CostCopper == 125,
        "trainer replay or fee");
    Check(exchange.Wallet("chr_recipe").Balance == 375, "trainer charged more than once");
    ledger.GrantBundle("recipe/seed-items", "ctn_recipe", 0,
        [new ItemQuantity("item/rough_timber", 4), new ItemQuantity("item/iron_nails", 2),
         new ItemQuantity("item/recipe_scroll_iron_brackets", 1), new ItemQuantity("item/iron_ingot", 2)], "fixture", "fixture");
    var craft = new ShatteredFrontier.Crafting.V1.CraftRequest { OperationId = "recipe/known-craft",
        CharacterId = "chr_recipe", ContainerId = "ctn_recipe", RecipeId = "recipe/worked_timber",
        ExpectedContainerRevision = 1, Count = 1 };
    var made = crafting.Craft(craft, headers, deadline: Deadline());
    Check(made.Grade == "crude" && made.Changes.Any(c => c.DefinitionId == "item/worked_timber/crude" && c.Delta == 2),
        "below-difficulty craft must commit Crude");
    Check(made.Equals(crafting.Craft(craft, headers, deadline: Deadline())), "graded craft replay changed");
    craft.Count = 2; Code(() => crafting.Craft(craft, headers, deadline: Deadline()), StatusCode.AlreadyExists);
    var scroll = new ProgressPb.LearnRecipeRequest { OperationId = "recipe/scroll", CharacterId = "chr_recipe",
        RecipeId = "recipe/iron_brackets", Source = "scroll", ScrollContainer = "ctn_recipe",
        ExpectedContainerRevision = ledger.Read("ctn_recipe").Revision };
    var learned = progression.LearnRecipe(scroll, headers, deadline: Deadline());
    Check(learned.Equals(progression.LearnRecipe(scroll, headers, deadline: Deadline())), "scroll replay changed");
    Check(ledger.Read("ctn_recipe").Quantities.GetValueOrDefault("item/recipe_scroll_iron_brackets") == 0,
        "scroll not consumed exactly once");
    var state = progression.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_recipe" }, headers, deadline: Deadline());
    Check(state.KnownRecipes.Any(k => k.RecipeId == "recipe/worked_timber") &&
        state.KnownRecipes.Any(k => k.RecipeId == "recipe/iron_brackets"), "learned recipes absent from snapshot");
    ledger.Grant("recipe/second-scroll", "ctn_recipe", ledger.Read("ctn_recipe").Revision,
        "item/recipe_scroll_iron_brackets", 1, "fixture", "fixture");
    scroll.OperationId = "recipe/scroll-again"; scroll.ExpectedContainerRevision = ledger.Read("ctn_recipe").Revision;
    Code(() => progression.LearnRecipe(scroll, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    Check(ledger.Read("ctn_recipe").Quantities.GetValueOrDefault("item/recipe_scroll_iron_brackets") == 1,
        "already-known recipe consumed a second scroll");
    Console.WriteLine("PASS recipe trainer, scroll, grade, replay, and snapshot");
    return;
}
if (mode == "recipes-replay")
{
    var progression = new ProgressPb.Progression.ProgressionClient(channel);
    var state = progression.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_recipe" }, headers, deadline: Deadline());
    Check(state.KnownRecipes.Any(k => k.RecipeId == "recipe/worked_timber") &&
        state.KnownRecipes.Any(k => k.RecipeId == "recipe/iron_brackets"), "learned recipes lost after restart");
    var recovered = progression.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_recipe_crash" }, headers, deadline: Deadline());
    Check(recovered.KnownRecipes.Any(k => k.RecipeId == "recipe/iron_brackets"), "scroll knowledge recovery failed");
    Console.WriteLine("PASS recipe knowledge survived host restart"); return;
}
if (mode == "recipes-crash")
{
    var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
    ledger.CreateContainer(new ContainerSpec("ctn_recipe_crash", OwnerKind.Character, "chr_recipe_crash", ContainerKind.Backpack, 10));
    ledger.Grant("recipe/scroll-crash-seed", "ctn_recipe_crash", 0,
        "item/recipe_scroll_iron_brackets", 1, "fixture", "fixture");
    using (var db = new NpgsqlConnection(connection))
    {
        db.Open();
        var seeded = new CharacterProgression("chr_recipe_crash", TimeProvider.System, () => .5,
            initialSkills: new Dictionary<string, int> { ["skill/smithing"] = 80 }).Snapshot();
        using var cmd = new NpgsqlCommand("INSERT INTO character_state.progression VALUES ('chr_recipe_crash',1,$1::jsonb)", db);
        cmd.Parameters.AddWithValue(JsonSerializer.Serialize(seeded)); cmd.ExecuteNonQuery();
    }
    try
    {
        new ProgressPb.Progression.ProgressionClient(channel).LearnRecipe(new ProgressPb.LearnRecipeRequest
        {
            OperationId = "recipe/scroll-crash", CharacterId = "chr_recipe_crash", RecipeId = "recipe/iron_brackets",
            Source = "scroll", ScrollContainer = "ctn_recipe_crash", ExpectedContainerRevision = 1
        }, headers, deadline: Deadline());
        throw new Exception("Scroll interruption did not stop the host.");
    }
    catch (RpcException ex) when (ex.StatusCode is StatusCode.Unavailable or StatusCode.DeadlineExceeded) { }
    Check(ledger.Read("ctn_recipe_crash").Quantities.GetValueOrDefault("item/recipe_scroll_iron_brackets") == 0,
        "interrupted scroll was not consumed");
    Console.WriteLine("PASS scroll consumed before host interruption"); return;
}

if (mode == "rewards")
{
    var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
    ledger.CreateContainer(new ContainerSpec("ctn_rewards", OwnerKind.Character, "chr_rewards", ContainerKind.Backpack, 100));
    var client = new RewardPb.Rewards.RewardsClient(channel);
    var request = new RewardPb.GrantDefeatRewardRequest
    {
        DefeatEventId = "defeat/test-boar", KillerCharacterId = "chr_rewards", RecipientCharacterId = "chr_rewards",
        ContainerId = "ctn_rewards", ExpectedContainerRevision = 0,
        TableId = "loot/greenlands_silverbrook_boar"
    };
    Code(() => client.GrantDefeatReward(request, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    Code(() => client.GrantDefeatReward(request, headers, deadline: DateTime.UtcNow.AddMilliseconds(-1)), StatusCode.DeadlineExceeded);
    var first = client.GrantDefeatReward(request, headers, deadline: Deadline());
    var replay = client.GrantDefeatReward(request, headers, deadline: Deadline());
    Check(first.Equals(replay) && first.Granted && first.Quantity >= 2, "Reward replay differs.");
    Check(ledger.Read("ctn_rewards").Quantities["item/boar_hide"] == first.Quantity, "Reward duplicated.");
    ledger.CreateContainer(new ContainerSpec("ctn_rewards_full", OwnerKind.Character, "chr_rewards", ContainerKind.Backpack, 4));
    ledger.Grant("reward/full/seed", "ctn_rewards_full", 0, "item/oak_log", 4, "fixture", "reward/full/seed");
    var full = new RewardPb.GrantDefeatRewardRequest
    {
        DefeatEventId = "defeat/full-boar", KillerCharacterId = "chr_rewards", RecipientCharacterId = "chr_rewards",
        ContainerId = "ctn_rewards_full", ExpectedContainerRevision = 1, TableId = "loot/greenlands_silverbrook_boar"
    };
    Code(() => client.GrantDefeatReward(full, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    using (var db = new Npgsql.NpgsqlConnection(connection))
    {
        db.Open();
        using var pending = new Npgsql.NpgsqlCommand("SELECT state FROM inventory.defeat_reward_intents WHERE defeat_event_id='defeat/full-boar'", db);
        Check((string?)pending.ExecuteScalar() == "Pending", "Full backpack lost defeat intent.");
    }
    ledger.Consume("reward/full/free", "ctn_rewards_full", 1, "item/oak_log", 4, "fixture");
    var resumed = client.GrantDefeatReward(full, headers, deadline: Deadline());
    var resumedReplay = client.GrantDefeatReward(full, headers, deadline: Deadline());
    Check(resumed.Equals(resumedReplay) && resumed.Granted &&
        ledger.Read("ctn_rewards_full").Quantities["item/boar_hide"] == resumed.Quantity,
        "Full backpack reward did not resume exactly once.");
    ledger.CreateContainer(new ContainerSpec("ctn_rewards_second", OwnerKind.Character, "chr_rewards_second", ContainerKind.Backpack, 100));
    var second = client.GrantDefeatReward(new RewardPb.GrantDefeatRewardRequest
    {
        DefeatEventId = request.DefeatEventId, KillerCharacterId = "chr_rewards", RecipientCharacterId = "chr_rewards_second",
        ContainerId = "ctn_rewards_second", ExpectedContainerRevision = 0, TableId = request.TableId
    }, headers, deadline: Deadline());
    Check(second.Granted && second.LedgerOperationId != first.LedgerOperationId &&
        ledger.Read("ctn_rewards_second").Quantities["item/boar_hide"] == second.Quantity,
        "Shared kill did not grant a separate personal reward.");
    foreach (var (creature, tableId) in new[] {
        ("npc/wolf", "loot/greenlands_silverbrook_wolf"),
        ("npc/bandit_thug", "loot/greenlands_silverbrook_bandit_thug"),
        ("npc/bandit_archer", "loot/greenlands_silverbrook_bandit_archer") })
    {
        var target = new RewardPb.GrantDefeatRewardRequest {
            DefeatEventId = "defeat/" + creature, CreatureId = creature, TableId = tableId,
            KillerCharacterId = "chr_rewards", RecipientCharacterId = "chr_rewards",
            ContainerId = "ctn_rewards", ExpectedContainerRevision = ledger.Read("ctn_rewards").Revision };
        var before = ledger.Read("ctn_rewards");
        var result = client.GrantDefeatReward(target, headers, deadline: Deadline());
        Check(result.Granted == (result.Items.Count > 0) &&
            client.GrantDefeatReward(target, headers, deadline: Deadline()).Equals(result),
            "Creature personal reward did not replay.");
        var after = ledger.Read("ctn_rewards");
        Check(result.Items.All(item => after.Quantities.GetValueOrDefault(item.DefinitionId) ==
            before.Quantities.GetValueOrDefault(item.DefinitionId) + item.Quantity),
            "Creature reward used the wrong table items.");
        var wrongTable = target.Clone(); wrongTable.TableId = request.TableId;
        Code(() => client.GrantDefeatReward(wrongTable, headers, deadline: Deadline()),
            StatusCode.InvalidArgument);
    }
    var sharedRequest = new RewardPb.CreateSharedDefeatDropRequest
    {
        DefeatEventId = "defeat/test-boss", CreatureId = "npc/test_boss",
        TableId = "loot/boss_greenlands_bandit_captain", KillerCharacterId = "chr_rewards"
    };
    sharedRequest.EligibleCharacterIds.AddRange(["chr_rewards", "chr_rewards_second"]);
    var drop = client.CreateSharedDefeatDrop(sharedRequest, headers, deadline: Deadline());
    Check(drop.Items.Count == 1 && drop.ClaimantCharacterId == "", "Boss did not create one shared set.");
    var wrongScope = new RewardPb.CreateSharedDefeatDropRequest
    { DefeatEventId = request.DefeatEventId, CreatureId = sharedRequest.CreatureId,
        TableId = sharedRequest.TableId, KillerCharacterId = sharedRequest.KillerCharacterId };
    wrongScope.EligibleCharacterIds.Add("chr_rewards");
    Code(() => client.CreateSharedDefeatDrop(wrongScope, headers, deadline: Deadline()), StatusCode.AlreadyExists);
    Code(() => client.GrantDefeatReward(new RewardPb.GrantDefeatRewardRequest
    { DefeatEventId = drop.DefeatEventId, KillerCharacterId = "chr_rewards", RecipientCharacterId = "chr_rewards",
        ContainerId = "ctn_rewards", TableId = request.TableId }, headers, deadline: Deadline()), StatusCode.AlreadyExists);
    Check(client.CreateSharedDefeatDrop(sharedRequest, headers, deadline: Deadline()).Equals(drop),
        "Boss create retry changed its drop.");
    Code(() => client.ClaimSharedDefeatDrop(new RewardPb.ClaimSharedDefeatDropRequest
    { DefeatEventId = drop.DefeatEventId, CharacterId = "chr_uncredited", ContainerId = "ctn_rewards_second" },
        headers, deadline: Deadline()), StatusCode.PermissionDenied);
    Code(() => client.ClaimSharedDefeatDrop(new RewardPb.ClaimSharedDefeatDropRequest
    { DefeatEventId = drop.DefeatEventId, CharacterId = "chr_rewards", ContainerId = "ctn_rewards_second" },
        headers, deadline: Deadline()), StatusCode.PermissionDenied);
    var claimRequest = new RewardPb.ClaimSharedDefeatDropRequest
    { DefeatEventId = drop.DefeatEventId, CharacterId = "chr_rewards_second", ContainerId = "ctn_rewards_second" };
    var claimed = client.ClaimSharedDefeatDrop(claimRequest, headers, deadline: Deadline());
    Check(claimed.ClaimantCharacterId == "chr_rewards_second" && claimed.Items.Count == 1 &&
        ledger.Read("ctn_rewards_second").Quantities[claimed.Items[0].DefinitionId] == claimed.Items[0].Quantity,
        "Boss set was not granted to its claimant.");
    Check(client.ClaimSharedDefeatDrop(claimRequest, headers, deadline: Deadline()).Equals(claimed),
        "Boss claim retry duplicated the set.");
    Code(() => client.ClaimSharedDefeatDrop(new RewardPb.ClaimSharedDefeatDropRequest
    { DefeatEventId = drop.DefeatEventId, CharacterId = "chr_rewards", ContainerId = "ctn_rewards" },
        headers, deadline: Deadline()), StatusCode.AlreadyExists);
    var uniqueRequest = new RewardPb.CreateSharedDefeatDropRequest
    { DefeatEventId = "defeat/test-unique", CreatureId = "npc/test_unique",
        TableId = "loot/unique_greenlands_wolf_alpha", KillerCharacterId = "chr_rewards" };
    uniqueRequest.EligibleCharacterIds.AddRange(["chr_rewards", "chr_rewards_second"]);
    var uniqueDrop = client.CreateSharedDefeatDrop(uniqueRequest, headers, deadline: Deadline());
    Check(uniqueDrop.Items.Any(i => i.DefinitionId == "item/wolf_pelt" && i.Quantity == 2) &&
        uniqueDrop.Items.Any(i => i.DefinitionId == "item/wolf_fang" && i.Quantity == 1),
        "Unique creature did not roll its multi-item set.");
    var beforeUnique = ledger.Read("ctn_rewards");
    var uniqueClaim = client.ClaimSharedDefeatDrop(new RewardPb.ClaimSharedDefeatDropRequest
    { DefeatEventId = uniqueDrop.DefeatEventId, CharacterId = "chr_rewards", ContainerId = "ctn_rewards" },
        headers, deadline: Deadline());
    Check(uniqueClaim.Items.All(i => ledger.Read("ctn_rewards").Quantities[i.DefinitionId] ==
        beforeUnique.Quantities.GetValueOrDefault(i.DefinitionId) + i.Quantity),
        "Unique multi-item set was not granted atomically.");
    var protectedDrop = new RewardPb.CreateSharedDefeatDropRequest
    { DefeatEventId = "defeat/test-boss-protected", CreatureId = "npc/test_boss",
        TableId = sharedRequest.TableId, KillerCharacterId = "chr_rewards", CreatureInProtectedTown = true };
    protectedDrop.EligibleCharacterIds.Add("chr_rewards");
    Check(client.CreateSharedDefeatDrop(protectedDrop, headers, deadline: Deadline()).Items.Count == 0,
        "Protected boss generated loot.");
    Code(() => client.ClaimSharedDefeatDrop(new RewardPb.ClaimSharedDefeatDropRequest
    { DefeatEventId = protectedDrop.DefeatEventId, CharacterId = "chr_rewards", ContainerId = "ctn_rewards" },
        headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    protectedDrop.CreatureInProtectedTown = false;
    Code(() => client.CreateSharedDefeatDrop(protectedDrop, headers, deadline: Deadline()), StatusCode.AlreadyExists);
    ledger.CreateContainer(new ContainerSpec("ctn_shared_full", OwnerKind.Character, "chr_rewards", ContainerKind.Backpack, 1));
    ledger.Grant("shared/full/seed", "ctn_shared_full", 0, "item/oak_log", 1, "fixture", "shared/full/seed");
    var pendingDrop = new RewardPb.CreateSharedDefeatDropRequest
    {
        DefeatEventId = "defeat/test-boss-pending", CreatureId = "npc/test_boss",
        TableId = sharedRequest.TableId, KillerCharacterId = "chr_rewards"
    };
    pendingDrop.EligibleCharacterIds.Add("chr_rewards");
    client.CreateSharedDefeatDrop(pendingDrop, headers, deadline: Deadline());
    Code(() => client.ClaimSharedDefeatDrop(new RewardPb.ClaimSharedDefeatDropRequest
    { DefeatEventId = pendingDrop.DefeatEventId, CharacterId = "chr_rewards", ContainerId = "ctn_shared_full" },
        headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    using (var db = new Npgsql.NpgsqlConnection(connection))
    {
        db.Open();
        using var pending = new Npgsql.NpgsqlCommand("SELECT state FROM inventory.shared_defeat_drops WHERE defeat_event_id='defeat/test-boss-pending'", db);
        Check((string?)pending.ExecuteScalar() == "ClaimPending", "Full backpack lost shared claim intent.");
    }
    ledger.Consume("shared/full/free", "ctn_shared_full", 1, "item/oak_log", 1, "fixture");
    request.KillerInProtectedTown = true;
    Code(() => client.GrantDefeatReward(request, headers, deadline: Deadline()), StatusCode.AlreadyExists);
    request.DefeatEventId = "defeat/bad-table"; request.TableId = "loot/unknown";
    Code(() => client.GrantDefeatReward(request, headers, deadline: Deadline()), StatusCode.InvalidArgument);
}
else if (mode == "progression")
{
    new InventoryLedger(new PostgresInventoryLedgerStore(connection)).CreateContainer(
        new ContainerSpec("ctn_progression", OwnerKind.Character, "chr_progression", ContainerKind.Backpack, 100));
    var client = new ProgressPb.Progression.ProgressionClient(channel);
    var lockInitial = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_lock_wire" }, headers, deadline: Deadline());
    Check(lockInitial.GatheringSkillLocksAuthoritative && lockInitial.GatheringSkillLocks.Count == 6 &&
        lockInitial.GatheringSkillLocks["skill/logging"] == "up", "Gathering lock capability or default state missing.");
    var lockRequest = new ProgressPb.ChangeSkillLockRequest { OperationId = "lock/wire/down-1", CharacterId = "chr_lock_wire",
        ExpectedProgressionRevision = lockInitial.ProgressionRevision, SkillId = "skill/logging", Lock = "down" };
    Code(() => client.ChangeSkillLock(lockRequest, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    var invalidLock = lockRequest.Clone(); invalidLock.OperationId = "lock/wire/invalid"; invalidLock.SkillId = "skill/smithing";
    Code(() => client.ChangeSkillLock(invalidLock, headers, deadline: Deadline()), StatusCode.InvalidArgument);
    var missingOwner = lockRequest.Clone(); missingOwner.OperationId = "lock/wire/missing"; missingOwner.CharacterId = "chr_lock_missing";
    Code(() => client.ChangeSkillLock(missingOwner, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    var lockReply = client.ChangeSkillLock(lockRequest, headers, deadline: Deadline());
    Check(lockReply.ProgressionRevision == lockInitial.ProgressionRevision + 1 &&
        lockReply.ReadyAtUnixMs > DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() &&
        client.ChangeSkillLock(lockRequest, headers, deadline: Deadline()).Equals(lockReply),
        "Gathering lock mutation or idempotent reply missing.");
    lockRequest.Lock = "locked";
    Code(() => client.ChangeSkillLock(lockRequest, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    lockRequest.Lock = "down";
    var lockPractice = client.ApplyReceipt(new ProgressPb.ApplyReceiptRequest { SourceReceiptId = "lock/wire/practice-1",
        CharacterId = "chr_lock_wire", Kind = "gather", ExpectedProgressionRevision = lockReply.ProgressionRevision,
        Payload = new ProgressPb.ActionPayload { SkillId = "skill/logging", Difficulty = 25,
            ActionSeconds = 6, Success = true } }, headers, deadline: Deadline());
    Check(lockPractice.SkillGain == 0 && lockPractice.GatheringSkillLocksAuthoritative &&
        lockPractice.GatheringSkillLocks["skill/logging"] == "down" &&
        lockPractice.GatheringSkillLockReadyAtUnixMs["skill/logging"] == lockReply.ReadyAtUnixMs,
        "Gathering lock did not govern service practice or receipt view.");
    var lockState = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_lock_wire" }, headers, deadline: Deadline());
    Check(lockState.GatheringSkillLocksAuthoritative && lockState.GatheringSkillLocks["skill/logging"] == "down" &&
        lockState.GatheringSkillLockReadyAtUnixMs["skill/logging"] == lockReply.ReadyAtUnixMs,
        "Gathering lock state missing after service restore.");
    var request = new ProgressPb.ApplyReceiptRequest
    {
        SourceReceiptId = "harvest/test-oak", Kind = "gather", CharacterId = "chr_progression",
        Payload = new ProgressPb.ActionPayload { SkillId = "skill/logging", Difficulty = 1, ActionSeconds = 6, Success = true },
        Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder", StepId = "harvest_oak", Kind = "gather",
            TargetId = "item/oak_log", NodeId = "gathering_node/oak_tree", Quantity = 1,
            OccurredAtUnixMs = 1792540800000 }
    };
    Code(() => client.ApplyReceipt(request, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    Code(() => client.ApplyReceipt(request, headers, deadline: DateTime.UtcNow.AddMilliseconds(-1)), StatusCode.DeadlineExceeded);
    ProgressPb.ApplyReceiptReply Quest(string id, string quest, string kind) => client.ApplyReceipt(new ProgressPb.ApplyReceiptRequest { SourceReceiptId = id,
        Kind = "quest", CharacterId = "chr_progression", Quest = new ProgressPb.QuestEvidence { QuestId = quest, Kind = kind, OccurredAtUnixMs = 1792540800000 } }, headers, deadline: Deadline());
    Check(Quest("accept/founder", "quest/frontier_founder", "accept") is { QuestAccepted: true, ActiveStepId: "harvest_oak" }, "Founder accept failed.");
    Check(Quest("accept/founder-2", "quest/frontier_founder", "accept") is { QuestAccepted: false, QuestReason: "duplicate" }, "Duplicate accept not refused.");
    Check(Quest("accept/river", "quest/river_road_timber", "accept") is { QuestAccepted: false, QuestReason: "prerequisite_missing" }, "Prerequisite not enforced.");
    Code(() => Quest("accept/nope", "quest/nope", "accept"), StatusCode.InvalidArgument);
    Check(Quest("accept/mills", "quest/mills_supply", "accept").QuestAccepted, "Mills accept failed.");
    Check(Quest("abandon/mills", "quest/mills_supply", "abandon").QuestAccepted, "Mills abandon failed.");
    Check(Quest("abandon/mills-2", "quest/mills_supply", "abandon") is { QuestAccepted: false, QuestReason: "not_active" }, "Second abandon not refused.");
    Check(Quest("accept/mills-again", "quest/mills_supply", "accept").QuestAccepted, "Re-accept failed.");
    var first = client.ApplyReceipt(request, headers, deadline: Deadline());
    var replay = client.ApplyReceipt(request, headers, deadline: Deadline());
    Check(first.Equals(replay) && first.QuestAccepted && first.ActiveStepId == "mine_iron", "Progression or quest replay differs.");
    var state = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers, deadline: Deadline());
    Check(state.QuestRevision == 1 && state.ActiveStepId == "mine_iron", "Quest state missing.");
    Check(state.Quests.Count == 2 && state.Quests.Single(q => q.QuestId == "quest/mills_supply") is { State: "active", ActiveStepId: "gather_oak", Revision: 2 } &&
        state.Quests.Single(q => q.QuestId == "quest/frontier_founder").State == "active", "Quest log missing.");
    Check(state.Position is null && state.PositionRevision == 0 && state.ObjectiveCounts["iron_ore"] == 0,
        "Unsaved position or active objective defaults invalid.");
    var save = new ProgressPb.SavePositionRequest { CharacterId = "chr_progression", ExpectedPositionRevision = 0,
        Position = new ProgressPb.CharacterPosition { ZoneId = "silverbrook", X = 10, Y = -9.25, Z = 42, Yaw = 90 } };
    Code(() => client.SavePosition(save, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    Code(() => client.SavePosition(save, headers, deadline: DateTime.UtcNow.AddMilliseconds(-1)), StatusCode.DeadlineExceeded);
    Code(() => client.SavePosition(new ProgressPb.SavePositionRequest { Position = save.Position }, headers, deadline: Deadline()), StatusCode.InvalidArgument);
    foreach (var zone in new[] { "", "Upper", "bad-zone", new string('a', 65) })
    {
        save.Position.ZoneId = zone;
        Code(() => client.SavePosition(save, headers, deadline: Deadline()), StatusCode.InvalidArgument);
    }
    save.Position.ZoneId = "silverbrook";
    foreach (var value in new[] { double.NaN, double.PositiveInfinity, double.NegativeInfinity, 10000000.01, -10000000.01 })
    {
        save.Position.X = value;
        Code(() => client.SavePosition(save, headers, deadline: Deadline()), StatusCode.InvalidArgument);
        save.Position.X = 10; save.Position.Y = value;
        Code(() => client.SavePosition(save, headers, deadline: Deadline()), StatusCode.InvalidArgument);
        save.Position.Y = -9.25; save.Position.Z = value;
        Code(() => client.SavePosition(save, headers, deadline: Deadline()), StatusCode.InvalidArgument);
        save.Position.Z = 42;
    }
    foreach (var yaw in new[] { float.NaN, float.PositiveInfinity, float.NegativeInfinity, 360.1f, -360.1f })
    {
        save.Position.Yaw = yaw;
        Code(() => client.SavePosition(save, headers, deadline: Deadline()), StatusCode.InvalidArgument);
    }
    save.Position.Yaw = 90;
    Check(client.SavePosition(save, headers, deadline: Deadline()).PositionRevision == 1, "First position save failed.");
    Code(() => client.SavePosition(save, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    save.ExpectedPositionRevision = 1; save.Position.X = 11.5;
    Check(client.SavePosition(save, headers, deadline: Deadline()).PositionRevision == 2, "Position update failed.");
    state = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers, deadline: Deadline());
    Check(state.PositionRevision == 2 && state.Position?.X == 11.5, "Saved position missing from state.");
    var unstuck = new ProgressPb.CommitUnstuckRequest { OperationId = "00000000-0000-0000-0000-000000000100", CharacterId = "chr_unstuck_wire", CooldownSeconds = 900,
        Position = new() { ZoneId = "silverbrook", X = 200, Y = 0, Z = 40 } };
    Code(() => client.CommitUnstuck(unstuck, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    unstuck.CooldownSeconds = 0;
    Code(() => client.CommitUnstuck(unstuck, headers, deadline: Deadline()), StatusCode.InvalidArgument);
    unstuck.CooldownSeconds = 900;
    Check(client.CommitUnstuck(unstuck, headers, deadline: Deadline()).PositionRevision == 1,
        "Unstuck landing not committed.");
    Check(client.CommitUnstuck(unstuck, headers, deadline: Deadline()).PositionRevision == 1,
        "Same Unstuck operation did not replay.");
    unstuck.OperationId = "00000000-0000-0000-0000-000000000101";
    Code(() => client.CommitUnstuck(unstuck, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    request.SourceReceiptId = "harvest/stale";
    Code(() => client.ApplyReceipt(request, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    request.SourceReceiptId = "bad/kind"; request.Kind = "unknown";
    Code(() => client.ApplyReceipt(request, headers, deadline: Deadline()), StatusCode.InvalidArgument);
    var finalBatch = new ProgressPb.ApplyReceiptRequest { SourceReceiptId = "rest/final/1", Kind = "gather", CharacterId = "chr_progression",
        ExpectedProgressionRevision = state.ProgressionRevision,
        Payload = new ProgressPb.ActionPayload { SkillId = "skill/logging", Difficulty = 25, ActionSeconds = 6,
            Success = true, SessionEnd = true, InRestArea = true } };
    var ended = client.ApplyReceipt(finalBatch, headers, deadline: Deadline());
    Check(ended.ProgressionRevision == state.ProgressionRevision + 1 &&
        client.ApplyReceipt(finalBatch, headers, deadline: Deadline()).Equals(ended),
        "Final practice closes session and replays atomically.");
    Code(() => client.EndSession(new ProgressPb.EndSessionRequest { OperationId = "rest/double-end", CharacterId = "chr_progression",
        ExpectedProgressionRevision = ended.ProgressionRevision, InRestArea = true }, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    using (var db = new NpgsqlConnection(connection))
    {
        db.Open();
        using var cmd = new NpgsqlCommand("UPDATE character_state.progression SET state=jsonb_set(jsonb_set(state,'{LastSessionEndAt}',to_jsonb($1::text)),'{LastRestedActiveMinute}',to_jsonb($2::bigint)) WHERE character_id='chr_progression'", db);
        cmd.Parameters.AddWithValue(DateTimeOffset.UtcNow.AddHours(-4).ToString("O"));
        cmd.Parameters.AddWithValue(DateTimeOffset.UtcNow.AddHours(-4).ToUnixTimeSeconds() / 60);
        Check(cmd.ExecuteNonQuery() == 1, "Offline fixture timestamp missing.");
    }
    var resumed = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers, deadline: Deadline());
    Check(resumed.RestedMinutes >= 119 && resumed.RestedMinutes <= 121 && resumed.ProgressionRevision == ended.ProgressionRevision + 1,
        "Rest pool accrued and exposed on wire.");
    var ordinary = client.ApplyReceipt(new ProgressPb.ApplyReceiptRequest { SourceReceiptId = "rest/ordinary/start",
        Kind = "defeat", CharacterId = "chr_rest_ordinary",
        Payload = new ProgressPb.ActionPayload { EnemyLevel = 1, EnemyTierMultiplier = 1, EligiblePartySize = 1 } },
        headers, deadline: Deadline());
    client.EndSession(new ProgressPb.EndSessionRequest { OperationId = "rest/ordinary/end", CharacterId = "chr_rest_ordinary",
        ExpectedProgressionRevision = ordinary.ProgressionRevision, InRestArea = false }, headers, deadline: Deadline());
    using (var db = new NpgsqlConnection(connection))
    {
        db.Open();
        using var cmd = new NpgsqlCommand("UPDATE character_state.progression SET state=jsonb_set(state,'{LastSessionEndAt}',to_jsonb($1::text)) WHERE character_id='chr_rest_ordinary'", db);
        cmd.Parameters.AddWithValue(DateTimeOffset.UtcNow.AddHours(-4).ToString("O"));
        Check(cmd.ExecuteNonQuery() == 1, "Ordinary offline fixture missing.");
    }
    var ordinaryResumed = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_rest_ordinary" }, headers, deadline: Deadline());
    Check(ordinaryResumed.RestedMinutes >= 59 && ordinaryResumed.RestedMinutes <= 61 &&
        resumed.RestedMinutes >= ordinaryResumed.RestedMinutes * 1.9,
        "Host applies the 1:2 rest-area rate versus 1:4 elsewhere.");
    var reread = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers, deadline: Deadline());
    Check(reread.ProgressionRevision == resumed.ProgressionRevision && reread.RestedMinutes == resumed.RestedMinutes,
        "Repeated state read does not accrue twice.");
    var kill = new ProgressPb.ApplyReceiptRequest { SourceReceiptId = "rest/kill/1", Kind = "defeat", CharacterId = "chr_progression",
        ExpectedProgressionRevision = resumed.ProgressionRevision,
        Payload = new ProgressPb.ActionPayload { EnemyLevel = 1, EnemyTierMultiplier = 1, EligiblePartySize = 1 } };
    var restedReply = client.ApplyReceipt(kill, headers, deadline: Deadline());
    Check(restedReply.XpGain == 10 && Math.Abs(restedReply.RestedMinutes - (resumed.RestedMinutes - 1)) < 1e-9 &&
        client.ApplyReceipt(kill, headers, deadline: Deadline()).Equals(restedReply),
        "Rested XP receipt persists and replays on wire.");
    var spent = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers, deadline: Deadline());
    Check(Math.Abs(spent.RestedMinutes - (resumed.RestedMinutes - 1)) < 1e-9,
        "Rested active minute debit persists on wire.");
    ProgressPb.ApplyReceiptRequest PartyKill(string recipient, string eventId, int referenceLevel) => new()
    {
        SourceReceiptId = eventId, Kind = "defeat", CharacterId = recipient,
        Payload = new ProgressPb.ActionPayload { EnemyLevel = 1, EnemyTierMultiplier = 1,
            EligiblePartySize = 2, ReferencePartyLevel = referenceLevel }
    };
    var partyOne = PartyKill("chr_party_one", "party/boar/1", 1);
    var partyTwo = PartyKill("chr_party_two", "party/boar/1", 1);
    var partyFirst = client.ApplyReceipt(partyOne, headers, deadline: Deadline());
    Check(partyFirst.XpGain == 4 &&
        client.ApplyReceipt(partyTwo, headers, deadline: Deadline()).XpGain == 4 &&
        client.ApplyReceipt(partyOne, headers, deadline: Deadline()).Equals(partyFirst),
        "Party kill XP reaches each eligible character once on the host wire.");
    Check(client.ApplyReceipt(PartyKill("chr_party_grey", "party/boar/grey", 20), headers, deadline: Deadline()).XpGain == 0,
        "Host wire uses highest party level for grey kills.");
    Code(() => client.ApplyReceipt(PartyKill("chr_party_bad", "party/boar/bad", 101), headers, deadline: Deadline()),
        StatusCode.InvalidArgument);
    var end = new ProgressPb.EndSessionRequest { OperationId = "rest/end/empty", CharacterId = "chr_progression",
        ExpectedProgressionRevision = restedReply.ProgressionRevision };
    var emptyEnd = client.EndSession(end, headers, deadline: Deadline());
    Check(emptyEnd.ProgressionRevision == restedReply.ProgressionRevision + 1 &&
        client.EndSession(end, headers, deadline: Deadline()).Equals(emptyEnd), "Empty final batch closes session and replays.");
}
else if (mode == "death")
{
    var client = new ProgressPb.Progression.ProgressionClient(channel);
    var id = Guid.NewGuid().ToString();
    var p = new ProgressPb.CharacterPosition { ZoneId = "silverbrook", X = 120, Y = -40, Z = 100 };
    var respawn = new ProgressPb.CharacterPosition { ZoneId = "silverbrook", X = -35970, Y = -14180, Z = 100 };
    var request = new ProgressPb.ReportDeathRequest { CharacterId = "chr_death", DeathEventId = id,
        OccurredAtUnixMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(), RespawnDelayMs = 4000,
        DeathPosition = p, RespawnPosition = respawn };
    Code(() => client.ReportDeath(request, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    Code(() => client.ReportDeath(request, headers, deadline: DateTime.UtcNow.AddMilliseconds(-1)), StatusCode.DeadlineExceeded);
    var first = client.ReportDeath(request, headers, deadline: Deadline());
    Check(first.IsDead && first.RespawnAtUnixMs == request.OccurredAtUnixMs + 4000, "death recorded with deadline");
    Check(client.ReportDeath(request, headers, deadline: Deadline()).Equals(first), "death event replay stable");
    var state = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = request.CharacterId }, headers, deadline: Deadline());
    Check(state.Death.IsDead && state.Death.RespawnPosition.Equals(respawn) && state.Death.DeathPosition.Equals(p), "death round trip through character state");
    request.RespawnDelayMs = 5000;
    Code(() => client.ReportDeath(request, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    request.RespawnDelayMs = 4000;
    var clear = new ProgressPb.ClearDeathRequest { CharacterId = request.CharacterId, DeathEventId = id };
    Check(!client.ClearDeath(clear, headers, deadline: Deadline()).IsDead && !client.ClearDeath(clear, headers, deadline: Deadline()).IsDead, "respawn clear replay stable");
    Check(!client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = request.CharacterId }, headers, deadline: Deadline()).Death.IsDead, "alive state survives read");
    Check(!client.ReportDeath(request, headers, deadline: Deadline()).IsDead, "old death replay cannot kill again");
    Code(() => client.ClearDeath(new ProgressPb.ClearDeathRequest { CharacterId = request.CharacterId, DeathEventId = Guid.NewGuid().ToString() }, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    request.DeathEventId = Guid.NewGuid().ToString();
    request.OccurredAtUnixMs += 10000;
    Check(client.ReportDeath(request, headers, deadline: Deadline()).IsDead, "second death accepted");
    var resurrect = new ProgressPb.ClearDeathRequest { CharacterId = request.CharacterId,
        DeathEventId = request.DeathEventId, ResurrectAtDeathPosition = true };
    Check(!client.ClearDeath(resurrect, headers, deadline: Deadline()).IsDead &&
        client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = request.CharacterId }, headers, deadline: Deadline()).Position.X == p.X,
        "resurrection clears durably at the death position");
}
else if (mode == "housing")
{
    var client = new HousingPb.Housing.HousingClient(channel);
    var request = new HousingPb.ClaimPlotRequest { OperationId = "claim/test", ClaimId = "clm_test",
        OwnerId = "chr_housing", CenterX = 900, CenterY = 900, SideMetres = 40, PrepaidDays = 1 };
    Code(() => client.ClaimPlot(request, bad, deadline: Deadline()), StatusCode.Unauthenticated);
    Code(() => client.ClaimPlot(request, headers, deadline: DateTime.UtcNow.AddMilliseconds(-1)), StatusCode.DeadlineExceeded);
    var first = client.ClaimPlot(request, headers, deadline: Deadline());
    var replay = client.ClaimPlot(request, headers, deadline: Deadline());
    Check(first.Equals(replay) && first.State == "Active", "Claim replay differs.");
    var read = client.GetClaim(new HousingPb.GetClaimRequest { ClaimId = "clm_test" }, headers, deadline: Deadline());
    Check(read.Equals(first), "Claim read differs.");
    Check(client.GetClaim(new HousingPb.GetClaimRequest { ClaimId = "clm_test", ActorId = "chr_housing" }, headers,
        deadline: Deadline()).ActorPermitted, "Claim owner rest permission missing.");
    Check(!client.GetClaim(new HousingPb.GetClaimRequest { ClaimId = "clm_test", ActorId = "chr_intruder" }, headers,
        deadline: Deadline()).ActorPermitted, "Unpermitted character gained house rest.");
    var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
    ledger.CreateContainer(new ContainerSpec("ctn_housing", OwnerKind.Character, "chr_housing", ContainerKind.Backpack, 100));
    ledger.Grant("craft/wall-bundle", "ctn_housing", 0, "item/wall_material_bundle", 1, "craft", "craft/wall-bundle");
    var place = new HousingPb.PlaceRequest { OperationId = "place/test-wall", ClaimId = "clm_test",
        ActorId = "chr_housing", ExpectedPermissionRevision = 0, ExpectedStructureRevision = 0,
        StructureId = "str_test_wall", ContentId = "piece/wall", MaterialId = "item/wall_material_bundle",
        MaterialQuantity = 1, ContainerId = "ctn_housing", ExpectedContainerRevision = 1 };
    var placed = client.Place(place, headers, deadline: Deadline());
    var placeReplay = client.Place(place, headers, deadline: Deadline());
    Check(placed.Equals(placeReplay) && placed.State == "Committed" && placed.StructureRevision == 1,
        "Placement replay differs.");
    Check(ledger.Read("ctn_housing").Quantities.GetValueOrDefault("item/wall_material_bundle") == 0,
        "Placement duplicated debit.");
    var listed = client.ListStructures(new HousingPb.ListStructuresRequest { ClaimId = "clm_test" }, headers, deadline: Deadline());
    Check(listed.StructureRevision == 1 && listed.Structures.Count == 1 &&
        listed.Structures[0].StructureId == "str_test_wall" && listed.Structures[0].Cell.X == 0 &&
        listed.Structures[0].ContentId == "piece/wall" && listed.Structures[0].Revision == 1,
        "Committed structure list differs.");
    request.OperationId = "claim/invalid"; request.ClaimId = "bad_id";
    Code(() => client.ClaimPlot(request, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    var remove = new HousingPb.RemoveRequest { OperationId = "remove/test", ClaimId = "clm_test",
        ActorId = "chr_housing", StructureId = "str_test_wall", ExpectedPermissionRevision = 0,
        ExpectedStructureRevision = 1, ExpectedContainerRevision = 2, RemovalKind = "undo" };
    var removed = client.Remove(remove, headers, deadline: Deadline());
    var removeReplay = client.Remove(remove, headers, deadline: Deadline());
    Check(removed.Equals(removeReplay) && removed.State == "Removed" && removed.StructureRevision == 2,
        "Removal replay differs.");
    Check(ledger.Read("ctn_housing").Quantities.GetValueOrDefault("item/wall_material_bundle") == 1,
        "Removal refund differs.");
    listed = client.ListStructures(new HousingPb.ListStructuresRequest { ClaimId = "clm_test" }, headers, deadline: Deadline());
    Check(listed.StructureRevision == 2 && listed.Structures.Count == 0, "Removed structure remained listed.");
    client.ClaimPlot(new HousingPb.ClaimPlotRequest { OperationId = "claim/grants", ClaimId = "clm_grants",
        OwnerId = "chr_grants", CenterX = 1200, CenterY = 1200, SideMetres = 40, PrepaidDays = 1 }, headers, deadline: Deadline());
    ledger.CreateContainer(new ContainerSpec("ctn_target", OwnerKind.Character, "chr_target", ContainerKind.Backpack, 100));
    ledger.Grant("seed/target", "ctn_target", 0, "item/wall_material_bundle", 2, "fixture", "seed/target");
    var grantRequest = new HousingPb.GrantRequest { OperationId = "grant/test", ClaimId = "clm_grants",
        ActorId = "chr_grants", TargetId = "chr_target", Rights = 1, ExpectedPermissionRevision = 0 };
    var deniedGrant = grantRequest.Clone(); deniedGrant.OperationId = "grant/denied"; deniedGrant.ActorId = "chr_intruder";
    Code(() => client.Grant(deniedGrant, headers, deadline: Deadline()), StatusCode.PermissionDenied);
    var grantReply = client.Grant(grantRequest, headers, deadline: Deadline());
    Check(grantReply.PermissionRevision == 1 && grantReply.Revision == 1 &&
        client.Grant(grantRequest, headers, deadline: Deadline()).Equals(grantReply), "Grant replay differs.");
    Check(client.GetClaim(new HousingPb.GetClaimRequest { ClaimId = "clm_grants", ActorId = "chr_target" }, headers,
        deadline: Deadline()).ActorPermitted, "Housing grantee rest permission missing.");
    var staleGrant = grantRequest.Clone(); staleGrant.OperationId = "grant/stale";
    Code(() => client.Grant(staleGrant, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    var targetPlace = new HousingPb.PlaceRequest { OperationId = "place/target", ClaimId = "clm_grants",
        ActorId = "chr_target", ExpectedPermissionRevision = 1, ExpectedStructureRevision = 0,
        StructureId = "str_target_wall", ContentId = "piece/wall", MaterialId = "item/wall_material_bundle",
        MaterialQuantity = 1, ContainerId = "ctn_target", ExpectedContainerRevision = 1, CellX = 1, CellY = 2, CellZ = 3 };
    Check(client.Place(targetPlace, headers, deadline: Deadline()).StructureRevision == 1,
        "Target could not place with build right.");
    var revoke = grantRequest.Clone(); revoke.OperationId = "grant/revoke"; revoke.Rights = 0; revoke.ExpectedPermissionRevision = 1;
    Check(client.Grant(revoke, headers, deadline: Deadline()).PermissionRevision == 2, "Revoke failed.");
    targetPlace.OperationId = "place/target-denied"; targetPlace.StructureId = "str_target_denied";
    targetPlace.ExpectedPermissionRevision = 2; targetPlace.ExpectedStructureRevision = 1;
    targetPlace.ExpectedContainerRevision = 2; targetPlace.CellX = 2;
    Code(() => client.Place(targetPlace, headers, deadline: Deadline()), StatusCode.PermissionDenied);
}
else throw new Exception("Unknown test mode.");
Console.WriteLine($"PASS {mode} wire auth, deadline, status, replay");

public sealed class LegacyGatheringService : GatheringPb.Gathering.GatheringBase
{
    public static int HarvestCalls;
    public override Task<GatheringPb.HarvestReply> Harvest(GatheringPb.HarvestRequest request, ServerCallContext context)
    {
        HarvestCalls++;
        return Task.FromResult(new GatheringPb.HarvestReply { Status = "granted" });
    }
}
