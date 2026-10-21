using System.Security.Cryptography;
using System.Text;
using Npgsql;
using ShatteredFrontier.TerrainDelta;

namespace ShatteredFrontier.TerrainDelta.Postgres;

public sealed class TerrainSchemaCompatibilityException(string message) : Exception(message);
public sealed class TerrainStoreUnavailableException(string message, Exception inner) : Exception(message, inner);

public sealed class PostgresTerrainDeltaStore(string connectionString)
{
    private static NpgsqlCommand Command(NpgsqlConnection c, NpgsqlTransaction t, string sql, params object[] args)
    {
        var cmd = new NpgsqlCommand(sql, c, t);
        foreach (var arg in args) cmd.Parameters.AddWithValue(arg);
        return cmd;
    }
    private T Run<T>(Func<NpgsqlConnection, NpgsqlTransaction, T> action, bool write)
    {
        try
        {
            using var c = new NpgsqlConnection(connectionString); c.Open();
            using var t = c.BeginTransaction();
            try
            {
                using var check = Command(c,t,"SELECT version FROM terrain_delta.schema_version WHERE singleton=true");
                if (check.ExecuteScalar() is not int version || version != 1)
                    throw new TerrainSchemaCompatibilityException("Unsupported terrain delta schema version.");
            }
            catch (PostgresException ex) when (ex.SqlState is "42P01" or "3F000" or "42501")
            { throw new TerrainSchemaCompatibilityException("Terrain delta schema marker missing or inaccessible."); }
            var result = action(c,t);
            if (write) t.Commit();
            return result;
        }
        catch (PostgresException ex) when (ex.SqlState is "23505" or "23514" or "23503")
        { throw new TerrainConflictException($"Terrain operation conflict ({ex.SqlState})."); }
        catch (Exception ex) when (ex is NpgsqlException or TimeoutException or IOException)
        { throw new TerrainStoreUnavailableException("Terrain delta store unavailable; retry the same operation ID.", ex); }
    }
    private static string Fingerprint(params string[] parts)
    {
        var body = string.Concat(parts.Select(p => p.Length.ToString(System.Globalization.CultureInfo.InvariantCulture) + ":" + p));
        return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(body)));
    }
    private static string N(int value) => value.ToString(System.Globalization.CultureInfo.InvariantCulture);
    private static void LockRegion(NpgsqlConnection c, NpgsqlTransaction t, RegionKey region)
    {
        // Collision can only serialize unrelated regions; it cannot break correctness.
        using var cmd = Command(c,t,"SELECT pg_advisory_xact_lock(hashtextextended($1,0))",$"{region.ZoneId.Length}:{region.ZoneId}:{region.X}:{region.Y}");
        cmd.ExecuteNonQuery();
    }
    private static TerrainReceipt? Replay(NpgsqlConnection c, NpgsqlTransaction t, string opId, string fingerprint)
    {
        using var cmd = Command(c,t,"SELECT fingerprint,zone_id,cell_x,cell_y,revision,tombstone,compensated_operation_id FROM terrain_delta.operations WHERE operation_id=$1",opId);
        using var reader = cmd.ExecuteReader();
        if (!reader.Read()) return null;
        if (reader.GetString(0) != fingerprint) throw new TerrainConflictException("Operation ID conflict.");
        return new TerrainReceipt(opId,new(reader.GetString(1),reader.GetInt32(2),reader.GetInt32(3)),reader.GetInt64(4),reader.GetBoolean(5),reader.IsDBNull(6) ? null : reader.GetString(6));
    }
    private static long Head(NpgsqlConnection c, NpgsqlTransaction t, RegionKey region)
    {
        using (var insert = Command(c,t,"INSERT INTO terrain_delta.regions(zone_id,cell_x,cell_y) VALUES($1,$2,$3) ON CONFLICT DO NOTHING",region.ZoneId,region.X,region.Y)) insert.ExecuteNonQuery();
        using var cmd = Command(c,t,"SELECT revision FROM terrain_delta.regions WHERE zone_id=$1 AND cell_x=$2 AND cell_y=$3",region.ZoneId,region.X,region.Y);
        return (long)cmd.ExecuteScalar()!;
    }
    private static void Advance(NpgsqlConnection c, NpgsqlTransaction t, RegionKey region, long expected)
    {
        using var cmd = Command(c,t,"UPDATE terrain_delta.regions SET revision=revision+1 WHERE zone_id=$1 AND cell_x=$2 AND cell_y=$3 AND revision=$4",region.ZoneId,region.X,region.Y,expected);
        if (cmd.ExecuteNonQuery() != 1) throw new TerrainConflictException("Region revision conflict.");
    }
    public TerrainReceipt Apply(TerrainEdit edit, long expectedRevision)
    {
        TerrainBoundary.Validate(edit);
        if (expectedRevision < 0 || expectedRevision == long.MaxValue) throw new ArgumentOutOfRangeException(nameof(expectedRevision));
        string fp = Fingerprint("apply",edit.OperationId,edit.Region.ZoneId,N(edit.Region.X),N(edit.Region.Y),edit.CharacterId,edit.ClaimId,
            N(edit.Anchor.X),N(edit.Anchor.Y),N(edit.Anchor.Z),edit.Kind.ToString(),N(edit.Magnitude));
        return Run((c,t) =>
        {
            LockRegion(c,t,edit.Region);
            if (Replay(c,t,edit.OperationId,fp) is { } previous) return previous;
            if (Head(c,t,edit.Region) != expectedRevision) throw new TerrainConflictException("Region revision conflict.");
            Advance(c,t,edit.Region,expectedRevision);
            using var cmd = Command(c,t,"INSERT INTO terrain_delta.operations(operation_id,fingerprint,zone_id,cell_x,cell_y,revision,character_id,claim_id,anchor_x,anchor_y,anchor_z,kind,magnitude,tombstone) VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,false)",
                edit.OperationId,fp,edit.Region.ZoneId,edit.Region.X,edit.Region.Y,expectedRevision+1,edit.CharacterId,edit.ClaimId,edit.Anchor.X,edit.Anchor.Y,edit.Anchor.Z,edit.Kind.ToString(),edit.Magnitude);
            cmd.ExecuteNonQuery();
            return new TerrainReceipt(edit.OperationId,edit.Region,expectedRevision+1,false,null);
        },true);
    }
    public TerrainReceipt Compensate(string operationId, string targetOperationId, RegionKey region, string characterId, string claimId, long expectedRevision)
    {
        TerrainBoundary.ValidateId(operationId,nameof(operationId)); TerrainBoundary.ValidateId(targetOperationId,nameof(targetOperationId));
        TerrainBoundary.ValidateRegion(region); TerrainBoundary.ValidateId(characterId,nameof(characterId)); TerrainBoundary.ValidateId(claimId,nameof(claimId));
        if (expectedRevision < 0 || expectedRevision == long.MaxValue) throw new ArgumentOutOfRangeException(nameof(expectedRevision));
        string fp = Fingerprint("compensate",operationId,targetOperationId,region.ZoneId,N(region.X),N(region.Y),characterId,claimId);
        return Run((c,t) =>
        {
            LockRegion(c,t,region);
            if (Replay(c,t,operationId,fp) is { } previous) return previous;
            if (Head(c,t,region) != expectedRevision) throw new TerrainConflictException("Region revision conflict.");
            using (var target = Command(c,t,"SELECT 1 FROM terrain_delta.operations WHERE operation_id=$1 AND zone_id=$2 AND cell_x=$3 AND cell_y=$4 AND character_id=$5 AND claim_id=$6 AND NOT tombstone AND NOT EXISTS (SELECT 1 FROM terrain_delta.operations x WHERE x.compensated_operation_id=$1)",targetOperationId,region.ZoneId,region.X,region.Y,characterId,claimId))
                if (target.ExecuteScalar() is null) throw new TerrainConflictException("Compensation target unavailable or authority mismatch.");
            Advance(c,t,region,expectedRevision);
            using var cmd = Command(c,t,"INSERT INTO terrain_delta.operations(operation_id,fingerprint,zone_id,cell_x,cell_y,revision,character_id,claim_id,compensated_operation_id,tombstone) VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,true)",
                operationId,fp,region.ZoneId,region.X,region.Y,expectedRevision+1,characterId,claimId,targetOperationId);
            cmd.ExecuteNonQuery();
            return new TerrainReceipt(operationId,region,expectedRevision+1,true,targetOperationId);
        },true);
    }
    public RegionListing ListRegion(RegionKey region, long sinceRevision = 0)
    {
        TerrainBoundary.ValidateRegion(region);
        if (sinceRevision < 0) throw new ArgumentOutOfRangeException(nameof(sinceRevision));
        return Run((c,t) =>
        {
            long head;
            using (var h = Command(c,t,"SELECT revision FROM terrain_delta.regions WHERE zone_id=$1 AND cell_x=$2 AND cell_y=$3",region.ZoneId,region.X,region.Y)) head = h.ExecuteScalar() is long value ? value : 0;
            if (sinceRevision > head) throw new TerrainConflictException("Revision exceeds head.");
            using var cmd = Command(c,t,"SELECT operation_id,revision,character_id,claim_id,anchor_x,anchor_y,anchor_z,kind,magnitude,tombstone,compensated_operation_id FROM terrain_delta.operations WHERE zone_id=$1 AND cell_x=$2 AND cell_y=$3 AND revision>$4 ORDER BY revision",region.ZoneId,region.X,region.Y,sinceRevision);
            using var reader = cmd.ExecuteReader();
            var entries = new List<RegionEntry>();
            while (reader.Read())
            {
                var receipt = new TerrainReceipt(reader.GetString(0),region,reader.GetInt64(1),reader.GetBoolean(9),reader.IsDBNull(10) ? null : reader.GetString(10));
                TerrainEdit? edit = receipt.Tombstone ? null : new(receipt.OperationId,region,reader.GetString(2),reader.GetString(3),
                    new(reader.GetInt32(4),reader.GetInt32(5),reader.GetInt32(6)),Enum.Parse<TerrainKind>(reader.GetString(7)),reader.GetInt32(8));
                entries.Add(new(receipt,edit));
            }
            return new RegionListing(head,entries);
        },false);
    }
}
