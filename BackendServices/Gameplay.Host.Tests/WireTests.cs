using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.CraftingContent;
using RewardPb = ShatteredFrontier.Rewards.V1;
using ProgressPb = ShatteredFrontier.Progression.V1;
using HousingPb = ShatteredFrontier.Housing.V1;

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

if (mode.EndsWith("-outage", StringComparison.Ordinal))
{
    if (mode == "rewards-outage")
        Code(() => new RewardPb.Rewards.RewardsClient(channel).GrantDefeatReward(new RewardPb.GrantDefeatRewardRequest
        {
            DefeatEventId = "defeat/outage", KillerCharacterId = "chr_rewards", RecipientCharacterId = "chr_rewards",
            ContainerId = "ctn_rewards", TableId = "loot/greenlands_silverbrook_boar"
        }, headers, deadline: Deadline()), StatusCode.Unavailable);
    else if (mode == "progression-outage")
        Code(() => new ProgressPb.Progression.ProgressionClient(channel).GetCharacterState(
            new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers,
            deadline: Deadline()), StatusCode.Unavailable);
    else if (mode == "housing-outage")
        Code(() => new HousingPb.Housing.HousingClient(channel).GetClaim(
            new HousingPb.GetClaimRequest { ClaimId = "clm_test" }, headers,
            deadline: Deadline()), StatusCode.Unavailable);
    Console.WriteLine($"PASS {mode} UNAVAILABLE mapping");
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
        Check(result.Granted && result.LedgerOperationId == "defeat/test-boar", "Reward restart replay failed.");
    }
    else if (mode == "progression-replay")
    {
        var result = new ProgressPb.Progression.ProgressionClient(channel).ApplyReceipt(new ProgressPb.ApplyReceiptRequest
        {
            SourceReceiptId = "harvest/test-oak", Kind = "gather", CharacterId = "chr_progression",
            Payload = new ProgressPb.ActionPayload { SkillId = "skill/logging", Difficulty = 1, ActionSeconds = 6, Success = true },
            Quest = new ProgressPb.QuestEvidence { QuestId = "quest/frontier_founder", StepId = "harvest_oak", Kind = "gather",
                TargetId = "item/oak_log", NodeId = "gathering_node/oak_tree", Quantity = 1,
                OccurredAtUnixMs = 1792540800000 }
        }, headers, deadline: Deadline());
        Check(result.QuestAccepted && result.QuestRevision == 1, "Progression restart replay failed.");
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
    long progressionRevision = 0, questRevision = 0;
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
    var first = client.ApplyReceipt(request, headers, deadline: Deadline());
    var replay = client.ApplyReceipt(request, headers, deadline: Deadline());
    Check(first.Equals(replay) && first.QuestAccepted && first.ActiveStepId == "mine_iron", "Progression or quest replay differs.");
    var state = client.GetCharacterState(new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers, deadline: Deadline());
    Check(state.QuestRevision == 1 && state.ActiveStepId == "mine_iron", "Quest state missing.");
    request.SourceReceiptId = "harvest/stale";
    Code(() => client.ApplyReceipt(request, headers, deadline: Deadline()), StatusCode.FailedPrecondition);
    request.SourceReceiptId = "bad/kind"; request.Kind = "unknown";
    Code(() => client.ApplyReceipt(request, headers, deadline: Deadline()), StatusCode.InvalidArgument);
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


