using System.Text.Json;
using Npgsql;
using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.InventoryLedger.Postgres;

namespace ShatteredFrontier.Housing.Postgres;

public sealed class HousingUnavailableException(string message, Exception inner) : Exception(message, inner);
public sealed class HousingSchemaCompatibilityException(string message) : Exception(message);
public sealed class HousingRejectedException(string message) : InvalidOperationException(message);

public sealed record HousingClaim(string Id, string OwnerId, string State, long Revision, long PermissionRevision,
    DateTimeOffset FundedUntil, DateTimeOffset? GraceEnds, DateTimeOffset? NoticeEnds,
    IReadOnlyDictionary<string, int> Grants);
public sealed record HousingCell(int X, int Y, int Z);
public sealed record HousingPlacement(string OperationId, string Fingerprint, string ClaimId, string ActorId,
    string StructureId, HousingCell Cell, string ContentId, string MaterialId, long MaterialQuantity,
    string ContainerId, long ExpectedContainerRevision, string State, long Revision, LedgerReceipt? ConsumeReceipt);
public sealed record HousingPlaceRequest(string OperationId, string Fingerprint, string ClaimId, string ActorId,
    long ExpectedPermissionRevision, long ExpectedStructureRevision, string StructureId, HousingCell Cell,
    string ContentId, string MaterialId, long MaterialQuantity, string ContainerId, long ExpectedContainerRevision);
public sealed record HousingRemoveRequest(string OperationId, string Fingerprint, string ClaimId, string ActorId,
    string StructureId, long ExpectedPermissionRevision, long ExpectedStructureRevision,
    long ExpectedContainerRevision, int RefundPercent);
public sealed record HousingRemoval(string OperationId, string Fingerprint, HousingPlacement Placement,
    string State, long Revision, long RefundQuantity, long ExpectedContainerRevision, LedgerReceipt? RefundReceipt);

// Each housing call commits its receipt and revision in one transaction. The external inventory
// debit is identified by operation_id + "/consume"; recovery only trusts a durable ledger receipt.
public sealed class PostgresHousingStore(string connectionString)
{
    private static NpgsqlCommand Cmd(NpgsqlConnection c, NpgsqlTransaction t, string sql, params object[] args)
    {
        var cmd = new NpgsqlCommand(sql, c, t);
        foreach (var arg in args) cmd.Parameters.AddWithValue(arg);
        return cmd;
    }

    private T Run<T>(Func<NpgsqlConnection, NpgsqlTransaction, T> action)
    {
        try
        {
            using var c = new NpgsqlConnection(connectionString); c.Open();
            using var t = c.BeginTransaction();
            // ponytail: This global lock serializes all claims; upgrade to a per-claim key using hashtext(claim_id).
            using (var advisory = Cmd(c, t, "SELECT pg_advisory_xact_lock(519761, 2)")) advisory.ExecuteNonQuery();
            try
            {
                using var check = Cmd(c, t, "SELECT version FROM housing.schema_version WHERE singleton=true FOR SHARE");
                if (check.ExecuteScalar() is not int version || version is not (3 or 4))
                    throw new HousingSchemaCompatibilityException("Unsupported housing schema version.");
            }
            catch (PostgresException ex) when (ex.SqlState is "42P01" or "3F000" or "42501")
            { throw new HousingSchemaCompatibilityException("Housing schema marker missing or inaccessible."); }
            var result = action(c, t); t.Commit(); return result;
        }
        catch (PostgresException ex) when (ex.SqlState.StartsWith("22", StringComparison.Ordinal) || ex.SqlState.StartsWith("23", StringComparison.Ordinal))
        { throw new HousingRejectedException($"Housing operation rejected ({ex.SqlState})."); }
        catch (Exception ex) when (ex is NpgsqlException or TimeoutException or System.IO.IOException)
        { throw new HousingUnavailableException("Housing store unavailable; retry the same operation ID.", ex); }
    }

    public HousingClaim? Claim(string id) => Run((c,t) => ReadClaim(c,t,id));
    public HousingClaim? ReplayClaim(string operationId, string fingerprint) => Run((c,t) =>
    {
        RequireGameplaySchema(c,t);
        using var cmd = Cmd(c,t,"SELECT fingerprint,receipt::text FROM housing.claim_operations WHERE operation_id=$1",operationId);
        using var r = cmd.ExecuteReader();
        if (!r.Read()) return null;
        if (r.GetString(0) != fingerprint) throw new HousingRejectedException("Operation ID conflict.");
        return JsonSerializer.Deserialize<HousingClaim>(r.GetString(1)) ?? throw new HousingRejectedException("Invalid claim receipt.");
    });
    private static void RequireGameplaySchema(NpgsqlConnection c, NpgsqlTransaction t)
    {
        using var check = Cmd(c,t,"SELECT version FROM housing.schema_version WHERE singleton=true");
        if (check.ExecuteScalar() is not int version || version != 4)
            throw new HousingSchemaCompatibilityException("Housing gameplay schema version 4 required.");
    }

    public HousingClaim ReserveClaim(string operationId, string fingerprint, string claimId, string ownerId,
        double centerX, double centerY, int sideMetres, int prepaidDays, long expectedWorldRevision,
        int maxSideMetres, int spacingMetres, DateTimeOffset now) => Run((c,t) =>
    {
        RequireGameplaySchema(c,t);
        using (var replay = Cmd(c,t,"SELECT fingerprint,receipt::text FROM housing.claim_operations WHERE operation_id=$1",operationId))
        using (var r = replay.ExecuteReader()) if (r.Read())
        {
            if (r.GetString(0) != fingerprint) throw new HousingRejectedException("Operation ID conflict.");
            return JsonSerializer.Deserialize<HousingClaim>(r.GetString(1)) ?? throw new HousingRejectedException("Invalid claim receipt.");
        }
        if (string.IsNullOrWhiteSpace(operationId) || operationId.Length > 128 ||
            !claimId.StartsWith("clm_",StringComparison.Ordinal) || string.IsNullOrWhiteSpace(ownerId) ||
            !double.IsFinite(centerX) || !double.IsFinite(centerY) ||
            sideMetres is < 10 or > 40 || sideMetres > maxSideMetres || spacingMetres is < 5 or > 20 || prepaidDays is < 1 or > 30)
            throw new HousingRejectedException("Invalid claim request.");
        using (var rev = Cmd(c,t,"SELECT count(*) FROM housing.claim_geometry"))
            if (rev.ExecuteScalar() is not long count || count != expectedWorldRevision)
                throw new HousingRejectedException("Stale world revision.");
        using (var entitlement = Cmd(c,t,"SELECT count(*) FROM housing.claims WHERE owner_id=$1 AND state NOT IN ('Archived','Reclaimed')",ownerId))
            if (entitlement.ExecuteScalar() is long count && count != 0)
                throw new HousingRejectedException("Claim entitlement already used.");
        using (var overlap = Cmd(c,t,"SELECT g.center_x,g.center_y,g.side_metres FROM housing.claim_geometry g JOIN housing.claims c ON c.id=g.claim_id WHERE c.state NOT IN ('Archived','Reclaimed')"))
        using (var r = overlap.ExecuteReader()) while (r.Read())
            if (Math.Abs(r.GetDouble(0)-centerX) < (r.GetInt32(2)+sideMetres)/2.0+spacingMetres &&
                Math.Abs(r.GetDouble(1)-centerY) < (r.GetInt32(2)+sideMetres)/2.0+spacingMetres)
                throw new HousingRejectedException("Claim spacing conflict.");
        var claim = new HousingClaim(claimId,ownerId,"Active",0,0,now.AddDays(prepaidDays),null,null,new Dictionary<string,int>());
        using (var insert = Cmd(c,t,"INSERT INTO housing.claims VALUES ($1,$2,'Active',0,0,$3,NULL,NULL,'{}'::jsonb)",claimId,ownerId,claim.FundedUntil)) insert.ExecuteNonQuery();
        using (var geometry = Cmd(c,t,"INSERT INTO housing.claim_geometry VALUES ($1,$2,$3,$4)",claimId,centerX,centerY,sideMetres)) geometry.ExecuteNonQuery();
        using (var revision = Cmd(c,t,"INSERT INTO housing.claim_structure_revisions(claim_id) VALUES ($1)",claimId)) revision.ExecuteNonQuery();
        using (var receipt = Cmd(c,t,"INSERT INTO housing.claim_operations VALUES ($1,$2,$3,0,$4::jsonb)",operationId,fingerprint,claimId,JsonSerializer.Serialize(claim))) receipt.ExecuteNonQuery();
        return claim;
    });
    private static HousingClaim? ReadClaim(NpgsqlConnection c, NpgsqlTransaction t, string id)
    {
        using var cmd = Cmd(c,t,"SELECT id,owner_id,state,revision,permission_revision,funded_until,grace_ends,notice_ends,grants::text FROM housing.claims WHERE id=$1",id);
        using var r = cmd.ExecuteReader();
        if (!r.Read()) return null;
        return new(r.GetString(0),r.GetString(1),r.GetString(2),r.GetInt64(3),r.GetInt64(4),r.GetFieldValue<DateTimeOffset>(5),
            r.IsDBNull(6)?null:r.GetFieldValue<DateTimeOffset>(6),r.IsDBNull(7)?null:r.GetFieldValue<DateTimeOffset>(7),
            JsonSerializer.Deserialize<Dictionary<string,int>>(r.GetString(8)) ?? new());
    }

    public HousingClaim SaveClaim(string operationId, string fingerprint, HousingClaim next, long expectedRevision) => Run((c,t) =>
    {
        using (var replay = Cmd(c,t,"SELECT fingerprint,claim_id,receipt::text FROM housing.claim_operations WHERE operation_id=$1",operationId))
        using (var r = replay.ExecuteReader())
            if (r.Read())
            {
                if (r.GetString(0) != fingerprint || r.GetString(1) != next.Id) throw new HousingRejectedException("Operation ID conflict.");
                return JsonSerializer.Deserialize<HousingClaim>(r.GetString(2)) ?? throw new HousingRejectedException("Invalid claim receipt.");
            }
        if (expectedRevision == -1)
        {
            if (next.Revision != 0 || next.PermissionRevision != 0) throw new HousingRejectedException("Invalid initial revision.");
            using var insert = Cmd(c,t,"INSERT INTO housing.claims VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9::jsonb)",
                next.Id,next.OwnerId,next.State,next.Revision,next.PermissionRevision,next.FundedUntil,
                (object?)next.GraceEnds ?? DBNull.Value,(object?)next.NoticeEnds ?? DBNull.Value,JsonSerializer.Serialize(next.Grants));
            insert.ExecuteNonQuery();
            using var rev = Cmd(c,t,"INSERT INTO housing.claim_structure_revisions(claim_id) VALUES ($1)",next.Id); rev.ExecuteNonQuery();
        }
        else
        {
            if (next.Revision != expectedRevision + 1 || next.PermissionRevision < 0) throw new HousingRejectedException("Invalid claim revision.");
            using var update = Cmd(c,t,"UPDATE housing.claims SET state=$1,revision=$2,permission_revision=$3,funded_until=$4,grace_ends=$5,notice_ends=$6,grants=$7::jsonb WHERE id=$8 AND revision=$9 AND owner_id=$10",
                next.State,next.Revision,next.PermissionRevision,next.FundedUntil,(object?)next.GraceEnds ?? DBNull.Value,
                (object?)next.NoticeEnds ?? DBNull.Value,JsonSerializer.Serialize(next.Grants),next.Id,expectedRevision,next.OwnerId);
            if (update.ExecuteNonQuery()!=1) throw new HousingRejectedException("Stale claim revision.");
        }
        using var receipt = Cmd(c,t,"INSERT INTO housing.claim_operations VALUES ($1,$2,$3,$4,$5::jsonb)",operationId,fingerprint,next.Id,next.Revision,JsonSerializer.Serialize(next));
        receipt.ExecuteNonQuery(); return next;
    });

    public HousingPlacement Begin(HousingPlaceRequest request, DateTimeOffset now) => Run((c,t) =>
    {
        var existing = ReadPlacement(c,t,request.OperationId);
        if (existing is not null)
        {
            if (existing.Fingerprint != request.Fingerprint) throw new HousingRejectedException("Operation ID conflict.");
            return existing;
        }
        if (string.IsNullOrWhiteSpace(request.OperationId) || request.MaterialQuantity <= 0 || request.ExpectedContainerRevision < 0)
            throw new HousingRejectedException("Invalid placement.");
        var claim = ReadClaim(c,t,request.ClaimId) ?? throw new HousingRejectedException("Claim missing.");
        if ((claim.State is not ("Active" or "Grace")) || (claim.State == "Active" && now >= claim.FundedUntil) ||
            claim.PermissionRevision != request.ExpectedPermissionRevision ||
            (claim.OwnerId != request.ActorId && (!claim.Grants.TryGetValue(request.ActorId,out var rights) || (rights & 1)==0)))
            throw new HousingRejectedException("Build permission denied or stale.");
        using (var revision = Cmd(c,t,"SELECT revision FROM housing.claim_structure_revisions WHERE claim_id=$1",request.ClaimId))
            if (revision.ExecuteScalar() is not long value || value != request.ExpectedStructureRevision)
                throw new HousingRejectedException("Stale structure revision.");
        using var insert = Cmd(c,t,"INSERT INTO housing.placements (operation_id,fingerprint,claim_id,actor_id,structure_id,cell_x,cell_y,cell_z,content_id,material_id,material_quantity,container_id,expected_container_revision,state,revision,consume_receipt) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,'Pending',$14,NULL)",
            request.OperationId,request.Fingerprint,request.ClaimId,request.ActorId,request.StructureId,
            request.Cell.X,request.Cell.Y,request.Cell.Z,request.ContentId,request.MaterialId,request.MaterialQuantity,
            request.ContainerId,request.ExpectedContainerRevision,request.ExpectedStructureRevision);
        insert.ExecuteNonQuery();
        return ReadPlacement(c,t,request.OperationId)!;
    });

    public HousingPlacement? Placement(string operationId) => Run((c,t) => ReadPlacement(c,t,operationId));
    private static HousingPlacement? ReadPlacement(NpgsqlConnection c,NpgsqlTransaction t,string id)
    {
        using var cmd = Cmd(c,t,"SELECT operation_id,fingerprint,claim_id,actor_id,structure_id,cell_x,cell_y,cell_z,content_id,material_id,material_quantity,container_id,expected_container_revision,state,revision,consume_receipt::text FROM housing.placements WHERE operation_id=$1",id);
        using var r = cmd.ExecuteReader(); if (!r.Read()) return null;
        return new(r.GetString(0),r.GetString(1),r.GetString(2),r.GetString(3),r.GetString(4),
            new(r.GetInt32(5),r.GetInt32(6),r.GetInt32(7)),r.GetString(8),r.GetString(9),r.GetInt64(10),
            r.GetString(11),r.GetInt64(12),r.GetString(13),r.GetInt64(14),
            r.IsDBNull(15)?null:JsonSerializer.Deserialize<LedgerReceipt>(r.GetString(15)));
    }

    public IReadOnlyList<HousingPlacement> Structures(string claimId) => Run((c,t) =>
    {
        var ids = new List<string>();
        using (var cmd = Cmd(c,t,"SELECT operation_id FROM housing.placements WHERE claim_id=$1 AND state='Committed' ORDER BY revision",claimId))
        using (var r = cmd.ExecuteReader()) while (r.Read()) ids.Add(r.GetString(0));
        return (IReadOnlyList<HousingPlacement>)ids.Select(id => ReadPlacement(c,t,id)!).ToArray();
    });

    public HousingPlacement Reconcile(string operationId, IInventoryLedgerStore ledgerStore) =>
        Run((c,t) =>
        {
            var pending = ReadPlacement(c,t,operationId) ?? throw new HousingRejectedException("Placement missing.");
            if (pending.State != "Pending") return pending;
            // A missing receipt is fenced in the same ledger transaction as the read.
            StoredOperation operation;
            try
            {
                operation = ledgerStore.Execute(s =>
                {
                    var key = operationId + "/consume";
                    if (s.Operations.TryGetValue(key, out var existing)) return existing;
                    var tombstone = new LedgerReceipt(key, LedgerAction.Consume, [], []);
                    var marker = new StoredOperation("housing/compensated", tombstone);
                    s.Operations.Add(key, marker);
                    return marker;
                });
            }
            catch (LedgerStoreUnavailableException ex) { throw new HousingUnavailableException("Inventory store unavailable; retry reconciliation.", ex); }
            catch (LedgerRejectedException ex) { throw new HousingRejectedException($"Inventory reconciliation rejected: {ex.Message}"); }
            var debit = operation.Receipt;
            var compensated = operation.Fingerprint == "housing/compensated" &&
                debit.OperationId == operationId + "/consume" && debit.Action == LedgerAction.Consume &&
                debit.Revisions.Count == 0 && debit.Changes.Count == 0;
            if (!compensated && (debit.OperationId != operationId + "/consume" || debit.Action != LedgerAction.Consume ||
                debit.Revisions.Count != 1 || debit.Revisions[0].ContainerId != pending.ContainerId ||
                debit.Revisions[0].Before != pending.ExpectedContainerRevision ||
                debit.Changes.Count != 1 || debit.Changes[0].ContainerId != pending.ContainerId ||
                debit.Changes[0].DefinitionId != pending.MaterialId || debit.Changes[0].Delta != -pending.MaterialQuantity))
                throw new HousingRejectedException("Ledger receipt does not match reserved materials.");
            var state = compensated ? "Compensated" : "Committed";
            var revision = pending.Revision;
            if (!compensated)
            {
                using var rev = Cmd(c,t,"UPDATE housing.claim_structure_revisions SET revision=revision+1 WHERE claim_id=$1 RETURNING revision",pending.ClaimId);
                revision = rev.ExecuteScalar() is long value ? value : throw new HousingRejectedException("Claim structure revision missing.");
            }
            using var update = Cmd(c,t,"UPDATE housing.placements SET state=$1,revision=$2,consume_receipt=$3::jsonb WHERE operation_id=$4 AND state='Pending'",
                state,revision,compensated ? "null" : JsonSerializer.Serialize(debit),operationId);
            if (update.ExecuteNonQuery()!=1) throw new HousingRejectedException("Placement already decided.");
            return ReadPlacement(c,t,operationId)!;
        });

    public HousingRemoval BeginRemove(HousingRemoveRequest request, DateTimeOffset now) => Run((c,t) =>
    {
        RequireGameplaySchema(c,t);
        using (var replay = Cmd(c,t,"SELECT fingerprint FROM housing.removals WHERE operation_id=$1",request.OperationId))
        {
            var prior = replay.ExecuteScalar() as string;
            if (prior is not null)
            {
                if (prior != request.Fingerprint) throw new HousingRejectedException("Operation ID conflict.");
                return ReadRemoval(c,t,request.OperationId)!;
            }
        }
        if (string.IsNullOrWhiteSpace(request.OperationId) || request.ExpectedContainerRevision < 0 ||
            request.RefundPercent is < 0 or > 100)
            throw new HousingRejectedException("Invalid removal.");
        var claim = ReadClaim(c,t,request.ClaimId) ?? throw new HousingRejectedException("Claim missing.");
        if (claim.State is not ("Active" or "Grace") || (claim.State == "Active" && now >= claim.FundedUntil) ||
            claim.PermissionRevision != request.ExpectedPermissionRevision ||
            (claim.OwnerId != request.ActorId && (!claim.Grants.TryGetValue(request.ActorId,out var rights) || (rights & 1)==0)))
            throw new HousingRejectedException("Build permission denied or stale.");
        using (var rev = Cmd(c,t,"SELECT revision FROM housing.claim_structure_revisions WHERE claim_id=$1",request.ClaimId))
            if (rev.ExecuteScalar() is not long value || value != request.ExpectedStructureRevision)
                throw new HousingRejectedException("Stale structure revision.");
        string placementId;
        using (var lookup = Cmd(c,t,"SELECT operation_id FROM housing.placements WHERE structure_id=$1 AND claim_id=$2 AND state='Committed'",request.StructureId,request.ClaimId))
            placementId = lookup.ExecuteScalar() as string ?? throw new HousingRejectedException("Structure missing.");
        var placement = ReadPlacement(c,t,placementId)!;
        if (placement.ActorId != request.ActorId)
            throw new HousingRejectedException("Only the contributor may remove this structure.");
        var refund = checked(placement.MaterialQuantity * request.RefundPercent / 100);
        using (var insert = Cmd(c,t,"INSERT INTO housing.removals VALUES ($1,$2,$3,'Pending',$4,$5,$6,NULL)",
            request.OperationId,request.Fingerprint,placementId,request.ExpectedStructureRevision,refund,request.ExpectedContainerRevision)) insert.ExecuteNonQuery();
        return ReadRemoval(c,t,request.OperationId)!;
    });

    private static HousingRemoval? ReadRemoval(NpgsqlConnection c,NpgsqlTransaction t,string id)
    {
        string fingerprint, placementId, state; long revision, refund, expected; LedgerReceipt? receipt;
        using (var cmd = Cmd(c,t,"SELECT fingerprint,placement_operation_id,state,revision,refund_quantity,expected_container_revision,refund_receipt::text FROM housing.removals WHERE operation_id=$1",id))
        using (var r = cmd.ExecuteReader())
        {
            if (!r.Read()) return null;
            fingerprint=r.GetString(0); placementId=r.GetString(1); state=r.GetString(2);
            revision=r.GetInt64(3); refund=r.GetInt64(4); expected=r.GetInt64(5);
            receipt=r.IsDBNull(6)?null:JsonSerializer.Deserialize<LedgerReceipt>(r.GetString(6));
        }
        return new(id,fingerprint,ReadPlacement(c,t,placementId)!,state,revision,refund,expected,receipt);
    }

    public HousingRemoval CompleteRemove(string operationId, IInventoryLedgerStore ledgerStore) => Run((c,t) =>
    {
        RequireGameplaySchema(c,t);
        var pending = ReadRemoval(c,t,operationId) ?? throw new HousingRejectedException("Removal missing.");
        if (pending.State == "Committed") return pending;
        LedgerReceipt? refundReceipt = null;
        if (pending.RefundQuantity > 0)
        {
            try
            {
                refundReceipt = ledgerStore.Execute(s => s.Operations.TryGetValue(operationId + "/refund",out var op) ? op.Receipt : null);
            }
            catch (LedgerStoreUnavailableException ex) { throw new HousingUnavailableException("Inventory store unavailable; retry removal.",ex); }
            if (refundReceipt is null) throw new HousingRejectedException("Refund receipt pending.");
            if (refundReceipt.OperationId != operationId + "/refund" || refundReceipt.Action != LedgerAction.Grant ||
                refundReceipt.Revisions.Count != 1 || refundReceipt.Revisions[0].ContainerId != pending.Placement.ContainerId ||
                refundReceipt.Revisions[0].Before != pending.ExpectedContainerRevision ||
                refundReceipt.Changes.Count != 1 || refundReceipt.Changes[0].ContainerId != pending.Placement.ContainerId ||
                refundReceipt.Changes[0].DefinitionId != pending.Placement.MaterialId ||
                refundReceipt.Changes[0].Delta != pending.RefundQuantity)
                throw new HousingRejectedException("Refund receipt does not match removal.");
        }
        using (var update = Cmd(c,t,"UPDATE housing.placements SET state='Removed' WHERE operation_id=$1 AND state='Committed'",pending.Placement.OperationId))
            if (update.ExecuteNonQuery()!=1) throw new HousingRejectedException("Structure already removed.");
        long revision;
        using (var rev = Cmd(c,t,"UPDATE housing.claim_structure_revisions SET revision=revision+1 WHERE claim_id=$1 RETURNING revision",pending.Placement.ClaimId))
            revision = rev.ExecuteScalar() is long value ? value : throw new HousingRejectedException("Claim structure revision missing.");
        using (var done = Cmd(c,t,"UPDATE housing.removals SET state='Committed',revision=$1,refund_receipt=$2::jsonb WHERE operation_id=$3 AND state='Pending'",
            revision, refundReceipt is null ? "null" : JsonSerializer.Serialize(refundReceipt),operationId))
            if (done.ExecuteNonQuery()!=1) throw new HousingRejectedException("Removal already decided.");
        return ReadRemoval(c,t,operationId)!;
    });
}
