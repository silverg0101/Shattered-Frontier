using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.Progression;
using Npgsql;
using System.Text.Json;
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
        var state = new ProgressPb.Progression.ProgressionClient(channel).GetCharacterState(
            new ProgressPb.GetCharacterStateRequest { CharacterId = "chr_progression" }, headers, deadline: Deadline());
        Check(state.PositionRevision == 2 && state.Position?.ZoneId == "silverbrook" && state.Position.X == 11.5 &&
            state.Position.Y == -9.25 && state.Position.Z == 42 && state.Position.Yaw == 90,
            "Position missing after host and PostgreSQL restart.");
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
