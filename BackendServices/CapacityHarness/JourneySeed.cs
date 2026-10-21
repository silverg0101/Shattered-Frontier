using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;
using C = ShatteredFrontier.Crafting.V1;
using H = ShatteredFrontier.Housing.V1;
using P = ShatteredFrontier.Progression.V1;
using R = ShatteredFrontier.Rewards.V1;

internal static class JourneySeed
{
    public static void Run()
    {
        var port = Environment.GetEnvironmentVariable("GAMEPLAY_HOST_PORT") ?? throw new Exception("GAMEPLAY_HOST_PORT required");
        var token = Environment.GetEnvironmentVariable("SF_GAMEPLAY_TOKEN") ?? throw new Exception("SF_GAMEPLAY_TOKEN required");
        var connection = Environment.GetEnvironmentVariable("GAMEPLAY_LEDGER_CONNECTION") ?? throw new Exception("GAMEPLAY_LEDGER_CONNECTION required");
        using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{port}");
        var headers = new Metadata { { "authorization", "Bearer " + token } };
        DateTime Deadline() => DateTime.UtcNow.AddSeconds(10);
        var ledger = new InventoryLedger(new PostgresInventoryLedgerStore(connection));
        const string character = "chr_capacity_journey", bag = "ctn_capacity_journey";
        ledger.CreateContainer(new ContainerSpec(bag, OwnerKind.Character, character, ContainerKind.Backpack, 100));
        ledger.Grant("harvest/capacity-oak", bag, 0, "item/oak_log", 2, "gathering", "harvest/capacity-oak");
        ledger.Grant("harvest/capacity-ore", bag, 1, "item/iron_ore", 2, "gathering", "harvest/capacity-ore");
        var progression = new P.Progression.ProgressionClient(channel);
        var crafting = new C.Crafting.CraftingClient(channel);
        var housing = new H.Housing.HousingClient(channel);
        var rewards = new R.Rewards.RewardsClient(channel);
        long progressionRevision = 0, questRevision = 0;
        P.ApplyReceiptReply Apply(string receipt, string kind, string skill, string step, string objectiveKind, string target, string node = "")
        {
            var request = new P.ApplyReceiptRequest
            {
                SourceReceiptId = receipt, Kind = kind, CharacterId = character,
                ExpectedProgressionRevision = progressionRevision, ExpectedQuestRevision = questRevision,
                Quest = new P.QuestEvidence { QuestId = "quest/frontier_founder", StepId = step,
                    Kind = objectiveKind, TargetId = target, NodeId = node, Quantity = 1,
                    OccurredAtUnixMs = 1792540800000 + questRevision * 1000 }
            };
            if (kind != "quest") request.Payload = new P.ActionPayload { SkillId = skill,
                Difficulty = 1, ActionSeconds = 6, Success = true, EnemyLevel = 1,
                EnemyTierMultiplier = 1, EligiblePartySize = 1 };
            var result = progression.ApplyReceipt(request, headers, deadline: Deadline());
            if (!result.QuestAccepted) throw new Exception($"Quest seed rejected {receipt}");
            progressionRevision = result.ProgressionRevision;
            questRevision = result.QuestRevision;
            return result;
        }
        void Craft(string operation, string recipe, int count) => crafting.Craft(new C.CraftRequest
        {
            OperationId = operation, CharacterId = character, ContainerId = bag,
            RecipeId = recipe, Count = count, ExpectedContainerRevision = ledger.Read(bag).Revision
        }, headers, deadline: Deadline());
        Apply("harvest/capacity-oak", "gather", "skill/logging", "harvest_oak", "gather", "item/oak_log", "gathering_node/oak_tree");
        Apply("harvest/capacity-ore", "gather", "skill/mining", "mine_iron", "gather", "item/iron_ore");
        Craft("craft/capacity-timber", "recipe/rough_timber", 2);
        Apply("craft/capacity-timber", "craft", "skill/smithing", "refine_materials", "craft", "item/rough_timber");
        Craft("craft/capacity-ingot", "recipe/iron_ingot", 2);
        Apply("craft/capacity-ingot", "craft", "skill/smithing", "refine_materials", "craft", "item/iron_ingot");
        Craft("craft/capacity-nails", "recipe/iron_nails", 1);
        Craft("craft/capacity-wall", "recipe/wall_material_bundle", 1);
        var claim = housing.ClaimPlot(new H.ClaimPlotRequest { OperationId = "claim/capacity-journey", ClaimId = "clm_capacity_journey",
            OwnerId = character, CenterX = -900, CenterY = -900, SideMetres = 40, PrepaidDays = 1 }, headers, deadline: Deadline());
        if (claim.State != "Active") throw new Exception("Quest seed claim failed");
        var place = housing.Place(new H.PlaceRequest { OperationId = "place/capacity-wall", ClaimId = claim.ClaimId,
            ActorId = character, StructureId = "str_capacity_wall", ContentId = "piece/wall", MaterialId = "item/wall_material_bundle",
            MaterialQuantity = 1, ContainerId = bag, ExpectedContainerRevision = ledger.Read(bag).Revision }, headers, deadline: Deadline());
        if (place.State != "Committed") throw new Exception("Quest seed placement failed");
        Apply("place/capacity-wall", "quest", "", "place_wall", "build", "structure_tag/wall");
        var reward = rewards.GrantDefeatReward(new R.GrantDefeatRewardRequest { DefeatEventId = "defeat/capacity-boar",
            KillerCharacterId = character, RecipientCharacterId = character, ContainerId = bag,
            ExpectedContainerRevision = ledger.Read(bag).Revision, TableId = "loot/greenlands_silverbrook_boar" }, headers, deadline: Deadline());
        if (!reward.Granted) throw new Exception("Quest seed boar reward failed");
        var final = Apply("defeat/capacity-boar", "defeat", "", "defeat_boar", "defeat", "npc/boar");
        if (!final.QuestCompleted || final.GrantedItems.Count == 0) throw new Exception("Quest seed payout failed");
        Console.WriteLine("PASS capacity quest completion seed");
    }
}
