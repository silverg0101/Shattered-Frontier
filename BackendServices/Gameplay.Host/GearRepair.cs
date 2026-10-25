using System.Text.Json;
using Npgsql;
using ShatteredFrontier.Exchange;
using ShatteredFrontier.Exchange.Postgres;

// Release-declared controls. A shared configuration publisher may replace the snapshot
// at the next quote; a quote freezes its calculated copper amount for 60 seconds.
internal sealed record GearRepairTuning(
    int RepairK = 1300, decimal CommonRarityMultiplier = 0.6m,
    decimal BrokenSurcharge = 1.25m, decimal NewbieMultiplier = 0.5m,
    decimal EconomyScaling = 1m, decimal NpcPriceMultiplier = 1m,
    int BrokenMaxLossPercent = 2, int DurabilityFloorPercent = 60,
    int FallbackItemLevel = 20, int QuoteLifetimeSeconds = 60)
{
    public const string Permission = "gameplay_tuning_operator";
    public const string ApplyOn = "restart";
    public const string EffectiveTime = "next_repair_quote";
    public const string RepairKKey = "tune.item.repair_k"; // copper per budget point
    public const string CommonRarityKey = "tune.item.repair_rarity_mult.common";
    public const string BrokenSurchargeKey = "tune.item.broken_repair_surcharge";
    public const string NewbieMultiplierKey = "tune.item.repair_newbie_mult";
    public const string EconomyScalingKey = "tune.economy.repair_scaling";
    public const string BrokenMaxLossKey = "tune.item.broken_max_dur_loss"; // percent of base maximum
    public const string DurabilityFloorKey = "tune.item.dur_floor_fraction"; // percent of base maximum
    public void Validate()
    {
        if (RepairK is < 1 or > 100000 || CommonRarityMultiplier is < 0.1m or > 3m ||
            BrokenSurcharge is < 1m or > 3m || NewbieMultiplier is < 0.1m or > 1m ||
            EconomyScaling is < 0.8m or > 1.5m || NpcPriceMultiplier is < 0.75m or > 1.25m ||
            BrokenMaxLossPercent is < 0 or > 10 || DurabilityFloorPercent is < 50 or > 100 ||
            FallbackItemLevel is < 0 or > 475 || QuoteLifetimeSeconds is < 1 or > 60)
            throw new ArgumentOutOfRangeException(nameof(GearRepairTuning));
    }
}

internal sealed record GearRepairQuote(string Id, long CostCopper, int ItemCount, DateTimeOffset ExpiresAt);
internal sealed record GearRepairReceipt(string OperationId, long CostCopper, int ItemCount);

// Both schemas must be visible through one connection. This gives the fee, repair and
// idempotent receipt a single WAL commit, including on immediate PostgreSQL restart.
internal sealed class GearRepair(string connectionString, string exchangeConnectionString, GearRepairTuning tuning, string gearCatalogPath)
{
    private readonly Dictionary<string, decimal> slotWeights = LoadSlots(gearCatalogPath);
    private readonly bool coLocated = SameDatabase(connectionString, exchangeConnectionString);
    private static bool SameDatabase(string inventory, string exchange)
    {
        var a = new NpgsqlConnectionStringBuilder(inventory);
        var b = new NpgsqlConnectionStringBuilder(exchange);
        return string.Equals(a.Host,b.Host,StringComparison.OrdinalIgnoreCase) && a.Port==b.Port &&
            string.Equals(a.Database,b.Database,StringComparison.Ordinal);
    }
    private static Dictionary<string, decimal> LoadSlots(string path)
    {
        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        var result = new Dictionary<string, decimal>(StringComparer.Ordinal);
        foreach (var item in doc.RootElement.GetProperty("gear").EnumerateArray())
        {
            var slot = item.GetProperty("slot").GetString();
            decimal weight = slot switch
            {
                "OffHand" => .6m, "Hands" or "Feet" => .75m, "Back" or "Ring" => .55m,
                "Head" or "Chest" or "Legs" or "MainHand" => 1m,
                _ => throw new InvalidOperationException("Unknown repair slot in gear catalog.")
            };
            result.Add(item.GetProperty("definition_id").GetString()!, weight);
        }
        return result;
    }
    private static NpgsqlCommand Cmd(NpgsqlConnection c, NpgsqlTransaction t, string sql, params object[] args)
    {
        var cmd = new NpgsqlCommand(sql, c, t);
        foreach (var arg in args) cmd.Parameters.AddWithValue(arg);
        return cmd;
    }
    private void Schema(NpgsqlConnection c, NpgsqlTransaction t)
    {
        if (!coLocated) throw new ExchangeSchemaCompatibilityException("Repair requires co-located inventory and exchange databases.");
        using var check = Cmd(c, t, "SELECT to_regclass('inventory.repair_quotes') IS NOT NULL AND to_regclass('exchange.wallets') IS NOT NULL AND (SELECT version=3 FROM exchange.schema_version WHERE singleton=true) AND (SELECT version=3 FROM inventory.schema_version WHERE singleton=true)");
        if (check.ExecuteScalar() is not true) throw new ExchangeSchemaCompatibilityException("Repair requires compatible inventory and exchange schemas and the repair migration.");
    }
    private long Cost(string definition, int current, int max, int level)
    {
        if (!slotWeights.TryGetValue(definition, out var weight))
            throw new ExchangeRejectedException("Gear has no repair price definition.");
        var budget = 1m + .075m * tuning.FallbackItemLevel;
        var full = tuning.RepairK * budget * weight * tuning.CommonRarityMultiplier;
        var cost = decimal.Ceiling(full * (max - current) / max *
            (current == 0 ? tuning.BrokenSurcharge : 1m) * tuning.EconomyScaling *
            tuning.NpcPriceMultiplier * (level < 20 ? tuning.NewbieMultiplier : 1m));
        return checked((long)Math.Max(1m, cost));
    }
    public GearRepairQuote Quote(string owner, string instanceId, bool allEquipped, int level)
    {
        if (!owner.StartsWith("chr_", StringComparison.Ordinal) || allEquipped == !string.IsNullOrEmpty(instanceId) || level is < 1 or > 1000)
            throw new ExchangeRejectedException("Invalid repair quote request.");
        using var c = new NpgsqlConnection(connectionString); c.Open();
        using var t = c.BeginTransaction(); Schema(c, t);
        using (var expiry = Cmd(c, t, "DELETE FROM inventory.repair_quotes WHERE expires_at<clock_timestamp()-interval '1 day'")) expiry.ExecuteNonQuery();
        using var query = Cmd(c, t, """
            SELECT i.instance_id,i.definition_id,i.durability_percent,i.max_durability_percent
            FROM inventory.item_instances i JOIN inventory.containers c ON c.id=i.container_id
            WHERE c.owner_kind=0 AND c.owner_id=$1 AND i.durability_percent<i.max_durability_percent
              AND (($2 AND c.kind=0) OR (NOT $2 AND i.instance_id=$3))
            ORDER BY i.instance_id LIMIT 33
            """, owner, allEquipped, instanceId);
        var items = new List<(string Id, int Current, int Max, long Cost)>();
        using (var rows = query.ExecuteReader())
            while (rows.Read()) items.Add((rows.GetString(0), rows.GetInt32(2), rows.GetInt32(3),
                Cost(rows.GetString(1), rows.GetInt32(2), rows.GetInt32(3), level)));
        if (items.Count is 0 or > 32) throw new ExchangeRejectedException("No damaged gear or too many equipped copies.");
        long total = checked(items.Sum(x => x.Cost));
        var quote = new GearRepairQuote("rqt_" + Guid.NewGuid().ToString("N"), total, items.Count,
            DateTimeOffset.UtcNow.AddSeconds(tuning.QuoteLifetimeSeconds));
        using (var insert = Cmd(c, t, "INSERT INTO inventory.repair_quotes(quote_id,owner_id,cost_copper,expires_at,tuning_fingerprint,broken_max_loss_percent,durability_floor_percent) VALUES ($1,$2,$3,$4,$5,$6,$7)",
            quote.Id, owner, total, quote.ExpiresAt, JsonSerializer.Serialize(tuning), tuning.BrokenMaxLossPercent, tuning.DurabilityFloorPercent)) insert.ExecuteNonQuery();
        foreach (var item in items)
            using (var insert = Cmd(c, t, "INSERT INTO inventory.repair_quote_items VALUES ($1,$2,$3,$4,$5)",
                quote.Id, item.Id, item.Current, item.Max, item.Cost)) insert.ExecuteNonQuery();
        t.Commit(); return quote;
    }
    public GearRepairReceipt Repair(string operationId, string quoteId, string owner)
    {
        if (!Guid.TryParse(operationId, out _) || !quoteId.StartsWith("rqt_", StringComparison.Ordinal) ||
            !owner.StartsWith("chr_", StringComparison.Ordinal)) throw new ExchangeRejectedException("Invalid repair request.");
        using var c = new NpgsqlConnection(connectionString); c.Open();
        using var t = c.BeginTransaction(); Schema(c, t);
        using (var lockOp = Cmd(c, t, "SELECT pg_advisory_xact_lock(519761,hashtext($1))", operationId)) lockOp.ExecuteNonQuery();
        using (var prior = Cmd(c, t, "SELECT quote_id,owner_id,cost_copper,item_count FROM inventory.repair_receipts WHERE operation_id=$1", operationId))
        using (var row = prior.ExecuteReader())
            if (row.Read())
            {
                if (row.GetString(0) != quoteId || row.GetString(1) != owner) throw new ExchangeRejectedException("Operation ID conflict.");
                return new GearRepairReceipt(operationId, row.GetInt64(2), row.GetInt32(3));
            }
        using (var conflict = Cmd(c, t, "SELECT EXISTS(SELECT 1 FROM exchange.operations WHERE operation_id=$1) OR EXISTS(SELECT 1 FROM exchange.settlements WHERE operation_id=$1)", operationId))
            if (conflict.ExecuteScalar() is true) throw new ExchangeRejectedException("Operation ID conflict.");
        long total; int maxLoss, floor;
        using (var quote = Cmd(c, t, "SELECT owner_id,cost_copper,expires_at,tuning_fingerprint,broken_max_loss_percent,durability_floor_percent FROM inventory.repair_quotes WHERE quote_id=$1 FOR UPDATE", quoteId))
        using (var row = quote.ExecuteReader())
        {
            if (!row.Read() || row.GetString(0) != owner) throw new ExchangeRejectedException("Repair quote missing or owned by another character.");
            total = row.GetInt64(1);
            if (row.GetFieldValue<DateTimeOffset>(2) < DateTimeOffset.UtcNow) throw new ExchangeRejectedException("Repair quote expired.");
            if (row.GetString(3) != JsonSerializer.Serialize(tuning)) throw new ExchangeRejectedException("Repair tuning changed; request a new quote.");
            maxLoss = row.GetInt32(4); floor = row.GetInt32(5);
        }
        var targets = new List<(string Id, int Prior, int Max)>();
        using (var query = Cmd(c, t, "SELECT instance_id,prior_durability,prior_max FROM inventory.repair_quote_items WHERE quote_id=$1 ORDER BY instance_id", quoteId))
        using (var rows = query.ExecuteReader())
            while (rows.Read()) targets.Add((rows.GetString(0),rows.GetInt32(1),rows.GetInt32(2)));
        if (targets.Count == 0) throw new ExchangeRejectedException("Empty repair quote.");
        foreach (var target in targets)
        {
            using var lockItem = Cmd(c, t, """
                SELECT i.durability_percent,i.max_durability_percent FROM inventory.item_instances i
                JOIN inventory.containers k ON k.id=i.container_id
                WHERE i.instance_id=$1 AND k.owner_kind=0 AND k.owner_id=$2 FOR UPDATE OF i
                """, target.Id, owner);
            using var row = lockItem.ExecuteReader();
            if (!row.Read() || row.GetInt32(0) != target.Prior || row.GetInt32(1) != target.Max)
                throw new ExchangeRejectedException("Gear changed since quote; request a new quote.");
        }
        using (var ensure = Cmd(c, t, "INSERT INTO exchange.wallets(owner_id) VALUES ($1) ON CONFLICT DO NOTHING", owner)) ensure.ExecuteNonQuery();
        long balance, reserved, revision;
        using (var wallet = Cmd(c, t, "SELECT balance,reserved,revision FROM exchange.wallets WHERE owner_id=$1 FOR UPDATE", owner))
        using (var row = wallet.ExecuteReader()) { row.Read(); balance=row.GetInt64(0); reserved=row.GetInt64(1); revision=row.GetInt64(2); }
        if (balance-reserved < total) throw new ExchangeRejectedException("Insufficient spendable gold.");
        using (var debit = Cmd(c, t, "UPDATE exchange.wallets SET balance=$1,revision=$2 WHERE owner_id=$3", balance-total, revision+1, owner)) debit.ExecuteNonQuery();
        using (var entry = Cmd(c, t, "INSERT INTO exchange.wallet_entries VALUES ($1,$2,$3,$4,'sink','repair_cost')", owner, revision+1, operationId, -total)) entry.ExecuteNonQuery();
        foreach (var target in targets)
        {
            var newMax = target.Prior == 0 ? Math.Max(floor, target.Max-maxLoss) : target.Max;
            using var update = Cmd(c, t, "UPDATE inventory.item_instances SET durability_percent=$1,max_durability_percent=$1 WHERE instance_id=$2", newMax, target.Id);
            if (update.ExecuteNonQuery() != 1) throw new ExchangeRejectedException("Repair target vanished.");
        }
        using (var receipt = Cmd(c, t, "INSERT INTO inventory.repair_receipts(operation_id,quote_id,owner_id,cost_copper,item_count) VALUES ($1,$2,$3,$4,$5)",
            operationId, quoteId, owner, total, targets.Count)) receipt.ExecuteNonQuery();
        var exchangeReceipt = new ExchangeReceipt(operationId, true, "ok", owner, total);
        using (var operation = Cmd(c, t, "INSERT INTO exchange.operations(operation_id,fingerprint,receipt) VALUES ($1,$2,$3::jsonb)",
            operationId, JsonSerializer.Serialize(new { command="repair_cost", owner, quoteId, total }), JsonSerializer.Serialize(exchangeReceipt)))
            operation.ExecuteNonQuery();
        t.Commit(); return new GearRepairReceipt(operationId, total, targets.Count);
    }
}
