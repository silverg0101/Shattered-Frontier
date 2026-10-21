using ClaimTerritory;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.Exchange;
using ShatteredFrontier.GatheringNodes;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.QuestContent;
using ShatteredFrontier.Progression;
using ShatteredFrontier.Rewards;
using ShatteredFrontier.WorldContent;
using InventoryService = ShatteredFrontier.InventoryLedger.InventoryLedger;

namespace ShatteredFrontier.MvpJourney;

public sealed record JourneyResult(string OperationId, LedgerReceipt? Items = null, ProgressResult? Progress = null,
    ClaimTerritory.Receipt? Claim = null, ExchangeReceipt? Trade = null,
    IReadOnlyList<ProgressionResult>? SkillResults = null);

// Release-authored mapping until action difficulty and duration arrive in trusted producer receipts.
public sealed record JourneyPracticeSettings(int Difficulty = 25, double ActionSeconds = 6, int BoarLevel = 1)
{
    public void Validate()
    {
        if (Difficulty is < 0 or > 1100 || !double.IsFinite(ActionSeconds) || ActionSeconds is < 1 or > 60 || BoarLevel is < 1 or > 200)
            throw new ArgumentOutOfRangeException(nameof(JourneyPracticeSettings));
    }
}

// Process-local adapter. The gate covers command replay, model calls and reward delivery.
public sealed class Journey
{
    private readonly object gate = new();
    private readonly Dictionary<string, (string Signature, JourneyResult Result)> commands = new(StringComparer.Ordinal);
    private readonly TimeProvider clock;
    private readonly QuestDefinition quest;
    private readonly RecipeCatalog recipes;
    private readonly BoarRewardService rewards;
    private readonly JourneyPracticeSettings practice;
    private readonly Func<double> progressionRoll;
    private readonly Dictionary<string, CharacterProgression> characters = new(StringComparer.Ordinal);
    public IReadOnlyDictionary<string, CharacterProgression> Characters => characters;
    public LootTable BoarLoot { get; }
    public InventoryService Items { get; }
    public GatheringNodeService Nodes { get; }
    public QuestProgression Quests { get; }
    public ClaimService Claims { get; }
    public ExchangeService Exchange { get; }

    public Journey(string zonePath, string questPath, IEnumerable<string> recipePaths, TimeProvider clock,
        IEnumerable<NodeDefinition> nodeDefinitions, string nodeSeed, string lootPath,
        Func<double>? progressionRoll = null, JourneyPracticeSettings? practice = null)
    {
        this.clock = clock;
        this.progressionRoll = progressionRoll ?? Random.Shared.NextDouble;
        this.practice = practice ?? new();
        this.practice.Validate();
        var zone = ZoneLoader.Load(zonePath);
        quest = QuestLoader.Load(questPath);
        if (quest.ZoneId != zone.Id) throw new ArgumentException("Quest and zone IDs differ.");
        recipes = RecipeCatalog.LoadFiles(recipePaths);
        Items = new InventoryService(new InMemoryInventoryLedgerStore());
        BoarLoot = LootCatalog.LoadFiles([lootPath])["loot/greenlands_silverbrook_boar"];
        rewards = new BoarRewardService(Items, BoarLoot);
        Nodes = new GatheringNodeService(zone, nodeDefinitions, clock, nodeSeed);
        Quests = new QuestProgression(quest);
        Claims = new ClaimService(ProtectedZone.Load(zonePath), clock, _ => false);
        Exchange = new ExchangeService(Items);
    }

    public void RegisterCharacter(string characterId, int capacity)
    {
        lock (gate)
        {
            Items.CreateContainer(new(Bag(characterId), OwnerKind.Character, characterId, ContainerKind.Backpack, capacity));
            Items.CreateContainer(new(Escrow(characterId), OwnerKind.Character, characterId, ContainerKind.Escrow, capacity));
            Quests.Accept(characterId, clock.GetUtcNow());
            characters.Add(characterId, new CharacterProgression(characterId, clock, progressionRoll));
        }
    }

    public static string Bag(string characterId) => $"ctn_{characterId}_bag";
    public static string Escrow(string characterId) => $"ctn_{characterId}_esc";

    private JourneyResult Run(string op, string signature, Func<JourneyResult> body)
    {
        lock (gate)
        {
            if (commands.TryGetValue(op, out var prior))
            {
                if (prior.Signature != signature) throw new InvalidOperationException("Operation ID reused with different command.");
                return prior.Result;
            }
            if (string.IsNullOrWhiteSpace(op)) throw new ArgumentException("Operation ID required.", nameof(op));
            var result = body();
            commands.Add(op, (signature, result));
            return result;
        }
    }

    private ProgressResult? Evidence(string op, string character, string kind, string target, string? node, int quantity)
    {
        // Only the current authored objective receives evidence. Other conversions remain valid inventory operations.
        var active = quest.Steps.FirstOrDefault(s => s.Objectives.Any(o => o.Kind == kind && o.TargetId == target &&
            (o.NodeId is null || o.NodeId == node)));
        if (active is null) return null;
        var progress = Quests.Apply(new(op + ":quest", character, quest.Id, active.Id, kind, target, node, quantity, clock.GetUtcNow()));
        foreach (var grant in progress.Grants)
            Items.Grant(grant.OperationId, Bag(character), Items.Read(Bag(character)).Revision,
                "item/frontier_founder_completion_bundle", 1, "quest_reward", grant.OperationId);
        return progress;
    }

    private ProgressionResult Practice(string receiptId, string character, ReceiptKind kind, string skillId, ulong context = 0) =>
        characters[character].Apply(new(receiptId, character, kind, true, skillId, practice.Difficulty,
            practice.ActionSeconds, context));

    public JourneyResult Harvest(string op, string character, string nodeId, int generation) =>
        Run(op, $"harvest|{character}|{nodeId}|{generation}", () =>
        {
            var harvested = Nodes.Harvest(new(op, character, nodeId, generation));
            if (harvested.Status != HarvestStatus.Granted) return new(op);
            var item = harvested.ResourceId!;
            var bag = Bag(character);
            var receipt = Items.Grant(op + ":items", bag, Items.Read(bag).Revision, item, harvested.Quantity, "gathering", op);
            var node = Nodes.Snapshot().Single(n => n.Id == nodeId);
            var definition = node.DefinitionId;
            var granted = receipt.Changes.Single(c => c.ContainerId == bag && c.DefinitionId == item && c.Delta > 0);
            var skill = node.Kind == ResourceKind.Wood ? ProgressionRules.Logging : ProgressionRules.Mining;
            var gain = Practice(receipt.OperationId, character, ReceiptKind.GatheringHarvest, skill);
            return new(op, receipt, Evidence(op, character, "gather", item, definition, checked((int)granted.Delta)),
                SkillResults: [gain]);
        });

    public JourneyResult Craft(string op, string character, string recipeId, int count = 1) =>
        Run(op, $"craft|{character}|{recipeId}|{count}", () =>
        {
            var recipe = recipes.Get(recipeId);
            var bag = Bag(character);
            var receipt = new CraftAttemptService(recipes, Items).Execute(new(op + ":items", bag,
                Items.Read(bag).Revision, recipeId, recipe.Station, count));
            ProgressResult? progress = null;
            var gains = new List<ProgressionResult>();
            foreach (var output in receipt.Changes.Where(c => c.ContainerId == bag && c.Delta > 0))
            {
                gains.Add(Practice(receipt.OperationId + ":" + output.DefinitionId, character, ReceiptKind.CraftConvert,
                    ProgressionRules.Smithing));
                progress = Evidence(op + ":" + output.DefinitionId, character, "craft", output.DefinitionId, null,
                    checked((int)output.Delta)) ?? progress;
            }
            return new(op, receipt, progress, SkillResults: gains);
        });

    public JourneyResult Claim(string op, string character, string claimId, Point center) =>
        Run(op, $"claim|{character}|{claimId}|{center}", () =>
            new(op, Claim: Claims.Reserve(op, Claims.Revision, claimId, character, center, new ClaimSettings().SideMetres)));

    public JourneyResult PlaceWall(string op, string character, string claimId) =>
        Run(op, $"wall|{character}|{claimId}", () =>
        {
            if (!Claims.Check(claimId, character, ClaimRight.Build).Allowed) throw new InvalidOperationException("Build denied.");
            var bag = Bag(character);
            var receipt = Items.Consume(op + ":items", bag, Items.Read(bag).Revision,
                "item/wall_material_bundle", 1, "construction");
            return new(op, receipt, Evidence(op, character, "build", "structure_tag/wall", null, 1));
        });

    // Simulated authoritative combat receipt for this in-process test slice.
    public JourneyResult DefeatBoar(string op, string character) =>
        Run(op, $"boar|{character}", () =>
        {
            var bag = Bag(character);
            var reward = rewards.Grant(new(op, character, character, bag, Items.Read(bag).Revision, false, false));
            if (reward.Decision != RewardDecision.Granted || reward.Receipt is null)
                throw new InvalidOperationException("Simulated credited boar defeat was ineligible.");
            var gain = characters[character].Apply(new(op, character, ReceiptKind.EnemyDefeat, true,
                EnemyLevel: practice.BoarLevel));
            return new(op, reward.Receipt, Evidence(op, character, "defeat", "npc/boar", null, 1),
                SkillResults: [gain]);
        });

    public JourneyResult TradeOpen(string op, string tradeId, TradeOffer a, TradeOffer b) =>
        Run(op, $"open|{tradeId}|{a}|{b}", () => new(op, Trade: Exchange.OpenTrade(op, tradeId, a, b)));
    public JourneyResult TradeLock(string op, string tradeId, string actor) =>
        Run(op, $"lock|{tradeId}|{actor}", () => new(op, Trade: Exchange.LockTrade(op, tradeId, actor)));
    public JourneyResult TradeAccept(string op, string tradeId, string actor) =>
        Run(op, $"accept|{tradeId}|{actor}", () => new(op, Trade: Exchange.AcceptTrade(op, tradeId, actor)));
}
