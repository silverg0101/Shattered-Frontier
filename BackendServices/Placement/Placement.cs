using System.Globalization;
using System.Text;
using ClaimTerritory;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.InventoryLedger;
using Ledger = ShatteredFrontier.InventoryLedger.InventoryLedger;

namespace ShatteredFrontier.Placement;

public enum PieceKind { Foundation, Wall, Door, Roof }
public enum PlacementState { Pending, Committed, Compensated, Rejected }
public enum PlacementReason { None, Invalid, PermissionDenied, StaleRevision, InsufficientMaterials, Occupied, Missing, OperationConflict, Capacity, Compensated }
public enum RemovalKind { Undo, Voluntary, Destruction }
public sealed record Cell(int X, int Y, int Z);
public sealed record Structure(string Id, string ClaimId, PieceKind Kind, string ContentId, Cell Cell,
    string MaterialId, long MaterialQuantity, string ContributorContainerId, string BeneficiaryId, long Revision,
    PlacementRefundSettings RefundSettings);
public sealed record PlacementReceipt(string OperationId, PlacementState State, PlacementReason Reason,
    long Revision, Structure? Structure = null, long MaterialDelta = 0);
public sealed record PlaceCommand(string OperationId, string ClaimId, string Actor, string ContainerId,
    long ExpectedContainerRevision, long ExpectedClaimRevision, long ExpectedStructureRevision,
    string StructureId, PieceKind Kind, string ContentId, Cell Cell);
public sealed record RemoveCommand(string OperationId, string ClaimId, string Actor, string ContainerId,
    long ExpectedContainerRevision, long ExpectedClaimRevision, long ExpectedStructureRevision,
    string StructureId, RemovalKind Kind);

// Effective realm snapshot: publish through the shared config workflow with economy tier 3.
// Percent, integer units, bounds 0..100; captured on placement and used for that generation.
public sealed record PlacementRefundSettings(int UndoPercent = 100, int VoluntaryPercent = 50, int DestructionPercent = 25)
{
    public const string Permission = "config-service economy tier 3";
    public void Validate()
    {
        if (UndoPercent is < 0 or > 100 || VoluntaryPercent is < 0 or > 100 || DestructionPercent is < 0 or > 100)
            throw new ArgumentOutOfRangeException(nameof(PlacementRefundSettings));
    }
    public int For(RemovalKind kind) => kind switch
    {
        RemovalKind.Undo => UndoPercent, RemovalKind.Voluntary => VoluntaryPercent,
        RemovalKind.Destruction => DestructionPercent, _ => throw new ArgumentOutOfRangeException(nameof(kind))
    };
}

// In-memory coordinator. The gate serializes its own operations; production needs durable
// prepare/decision/completion and claim/inventory fences before cross-process use.
public sealed class PlacementService
{
    private readonly object gate = new();
    private readonly ClaimService claims;
    private readonly Ledger ledger;
    private readonly Dictionary<PieceKind, (string ContentId, string MaterialId, long Quantity)> costs;
    private readonly Dictionary<string, Structure> structures = new(StringComparer.Ordinal);
    private readonly Dictionary<string, (string Request, PlacementReceipt Receipt)> operations = new(StringComparer.Ordinal);
    private readonly Dictionary<string, long> revisions = new(StringComparer.Ordinal);
    private PlacementRefundSettings settings;

    public PlacementService(ClaimService claims, Ledger ledger, RecipeCatalog recipes,
        PlacementRefundSettings? settings = null)
    {
        this.claims = claims; this.ledger = ledger;
        this.settings = settings ?? new(); this.settings.Validate();
        costs = new();
        foreach (var kind in Enum.GetValues<PieceKind>())
        {
            var name = kind.ToString().ToLowerInvariant();
            var recipe = recipes.Get($"recipe/{name}_material_bundle");
            if (recipe.Outputs.Count != 1 || recipe.Outputs[0].DefinitionId != $"item/{name}_material_bundle")
                throw new ArgumentException($"Invalid {name} construction recipe output.", nameof(recipes));
            costs.Add(kind, ($"piece/{name}", recipe.Outputs[0].DefinitionId, recipe.Outputs[0].Quantity));
        }
    }

    public void SetEffectiveSettings(PlacementRefundSettings next) { next.Validate(); lock (gate) settings = next; }
    public IReadOnlyList<Structure> Structures(string claimId)
    { lock (gate) return structures.Values.Where(x => x.ClaimId == claimId).ToArray(); }
    public long Revision(string claimId) { lock (gate) return revisions.GetValueOrDefault(claimId); }
    public PlacementReceipt? Receipt(string operationId) { lock (gate) return operations.GetValueOrDefault(operationId).Receipt; }

    private PlacementReceipt Execute(string op, string request, Func<PlacementReceipt> action)
    {
        lock (gate)
        {
            if (string.IsNullOrWhiteSpace(op) || op.Length > 128)
                return new(op, PlacementState.Rejected, PlacementReason.Invalid, 0);
            if (operations.TryGetValue(op, out var prior))
                return prior.Request == request ? prior.Receipt : new(op, PlacementState.Rejected, PlacementReason.OperationConflict, prior.Receipt.Revision);
            var result = action();
            operations[op] = (request, result);
            return result;
        }
    }

    private static string Key(params object?[] values)
    {
        var s = new StringBuilder();
        foreach (var value in values)
        {
            var part = Convert.ToString(value, CultureInfo.InvariantCulture) ?? "<null>";
            s.Append(part.Length).Append(':').Append(part).Append('|');
        }
        return s.ToString();
    }
    private PlacementReceipt Reject(string op, string claimId, PlacementReason reason) =>
        new(op, PlacementState.Rejected, reason, revisions.GetValueOrDefault(claimId));

    // The fault is a deterministic test seam after the irreversible ledger consume.
    // Compensation is recorded before returning; duplicate retries return its receipt.
    public PlacementReceipt Place(PlaceCommand command, bool failAfterConsume = false) => Execute(command.OperationId,
        Key("place", command.ClaimId, command.Actor, command.ContainerId, command.ExpectedContainerRevision,
            command.ExpectedClaimRevision, command.ExpectedStructureRevision, command.StructureId,
            command.Kind, command.ContentId, command.Cell?.X, command.Cell?.Y, command.Cell?.Z), () =>
    {
        var op = command.OperationId; var claim = command.ClaimId;
        if (command.Cell is null || !Enum.IsDefined(command.Kind) || !costs.TryGetValue(command.Kind, out var cost) ||
            command.ContentId != cost.ContentId || string.IsNullOrWhiteSpace(command.StructureId) ||
            !command.StructureId.StartsWith("str_", StringComparison.Ordinal) || command.ExpectedStructureRevision < 0)
            return Reject(op, claim, PlacementReason.Invalid);
        var access = claims.Check(claim, command.Actor, ClaimRight.Build);
        if (!access.Allowed) return Reject(op, claim, PlacementReason.PermissionDenied);
        if (access.PermissionRevision != command.ExpectedClaimRevision ||
            revisions.GetValueOrDefault(claim) != command.ExpectedStructureRevision)
            return Reject(op, claim, PlacementReason.StaleRevision);
        if (structures.ContainsKey(command.StructureId) || structures.Values.Any(x => x.ClaimId == claim && x.Cell == command.Cell))
            return Reject(op, claim, PlacementReason.Occupied);
        ContainerView container;
        try { container = ledger.Read(command.ContainerId); }
        catch (LedgerRejectedException) { return Reject(op, claim, PlacementReason.Invalid); }
        if (container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != command.Actor)
            return Reject(op, claim, PlacementReason.PermissionDenied);
        if (container.Revision != command.ExpectedContainerRevision) return Reject(op, claim, PlacementReason.StaleRevision);
        if (container.Quantities.GetValueOrDefault(cost.MaterialId) < cost.Quantity)
            return Reject(op, claim, PlacementReason.InsufficientMaterials);
        try { ledger.Consume(op + "/consume", command.ContainerId, container.Revision, cost.MaterialId, cost.Quantity,
            "construction_material_consumed"); }
        catch (LedgerRejectedException) { return Reject(op, claim, PlacementReason.InsufficientMaterials); }
        var pending = new PlacementReceipt(op, PlacementState.Pending, PlacementReason.None, revisions.GetValueOrDefault(claim));
        operations[op] = (Key("place", claim, command.Actor, command.ContainerId, command.ExpectedContainerRevision,
            command.ExpectedClaimRevision, command.ExpectedStructureRevision, command.StructureId,
            command.Kind, command.ContentId, command.Cell.X, command.Cell.Y, command.Cell.Z), pending);
        // In a durable coordinator the pending record and debit allocation survive a crash.
        var finalAccess = claims.Check(claim, command.Actor, ClaimRight.Build);
        if (failAfterConsume || !finalAccess.Allowed || finalAccess.PermissionRevision != command.ExpectedClaimRevision)
        {
            ledger.Grant(op + "/compensate", command.ContainerId, ledger.Read(command.ContainerId).Revision,
                cost.MaterialId, cost.Quantity, "construction_material_return", op);
            return new(op, PlacementState.Compensated, PlacementReason.Compensated, revisions.GetValueOrDefault(claim));
        }
        var revision = revisions[claim] = checked(revisions.GetValueOrDefault(claim) + 1);
        var piece = new Structure(command.StructureId, claim, command.Kind, command.ContentId, command.Cell,
            cost.MaterialId, cost.Quantity, command.ContainerId, command.Actor, revision, settings);
        structures.Add(piece.Id, piece);
        return new(op, PlacementState.Committed, PlacementReason.None, revision, piece, -cost.Quantity);
    });

    public PlacementReceipt Remove(RemoveCommand command) => Execute(command.OperationId,
        Key("remove", command.ClaimId, command.Actor, command.ContainerId, command.ExpectedContainerRevision,
            command.ExpectedClaimRevision, command.ExpectedStructureRevision, command.StructureId, command.Kind), () =>
    {
        var op = command.OperationId; var claim = command.ClaimId;
        if (!Enum.IsDefined(command.Kind)) return Reject(op, claim, PlacementReason.Invalid);
        var access = claims.Check(claim, command.Actor, ClaimRight.Build);
        if (!access.Allowed) return Reject(op, claim, PlacementReason.PermissionDenied);
        if (access.PermissionRevision != command.ExpectedClaimRevision ||
            revisions.GetValueOrDefault(claim) != command.ExpectedStructureRevision)
            return Reject(op, claim, PlacementReason.StaleRevision);
        if (!structures.TryGetValue(command.StructureId, out var piece) || piece.ClaimId != claim)
            return Reject(op, claim, PlacementReason.Missing);
        if (command.ContainerId != piece.ContributorContainerId || command.Actor != piece.BeneficiaryId)
            return Reject(op, claim, PlacementReason.PermissionDenied);
        var refund = checked(piece.MaterialQuantity * piece.RefundSettings.For(command.Kind) / 100);
        try
        {
            if (refund > 0) ledger.Grant(op + "/refund", command.ContainerId, command.ExpectedContainerRevision,
                piece.MaterialId, refund, "construction_material_return", op);
            else if (ledger.Read(command.ContainerId).Revision != command.ExpectedContainerRevision)
                return Reject(op, claim, PlacementReason.StaleRevision);
        }
        catch (LedgerRejectedException) { return Reject(op, claim, PlacementReason.Capacity); }
        structures.Remove(piece.Id);
        var revision = revisions[claim] = checked(revisions.GetValueOrDefault(claim) + 1);
        return new(op, PlacementState.Committed, PlacementReason.None, revision, piece, refund);
    });
}
