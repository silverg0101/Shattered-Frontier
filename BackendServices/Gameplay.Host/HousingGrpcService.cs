using System.Security.Cryptography;
using System.Text;
using Google.Protobuf;
using Grpc.Core;
using ClaimTerritory;
using ShatteredFrontier.Placement;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.Housing.Postgres;
using ShatteredFrontier.InventoryLedger;
using Pb = ShatteredFrontier.Housing.V1;

internal sealed class HousingGrpcService(PostgresHousingStore store, ProtectedZone zone, ClaimSettings claimSettings,
    InventoryLedger ledger, ShatteredFrontier.InventoryLedger.Postgres.PostgresInventoryLedgerStore ledgerStore,
    RecipeCatalog recipes) : Pb.Housing.HousingBase
{
    public override Task<Pb.ClaimReply> ClaimPlot(Pb.ClaimPlotRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            var fingerprint = Fingerprint(request.ToByteArray());
            var replay = store.ReplayClaim(request.OperationId, fingerprint);
            if (replay is not null) return ToPb(replay);
            claimSettings.Validate();
            var center = new Point(request.CenterX, request.CenterY);
            if (!zone.ContainsSquare(center, request.SideMetres) ||
                zone.HitsProtected(center, request.SideMetres + 2 * claimSettings.SpacingMetres))
                throw new HousingRejectedException("Claim outside eligible territory.", HousingRejectionCode.PermissionDenied);
            var saved = store.ReserveClaim(request.OperationId, fingerprint, request.ClaimId,
                request.OwnerId, request.CenterX, request.CenterY, request.SideMetres,
                request.PrepaidDays, request.ExpectedWorldRevision, claimSettings.SideMetres,
                claimSettings.SpacingMetres, DateTimeOffset.UtcNow);
            return ToPb(saved);
        });

    public override Task<Pb.ClaimReply> GetClaim(Pb.GetClaimRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () => ToPb(store.Claim(request.ClaimId) ??
            throw new InvalidOperationException("Claim missing.")));

    public override Task<Pb.ListStructuresReply> ListStructures(Pb.ListStructuresRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            var snapshot = store.StructureSnapshot(request.ClaimId);
            var reply = new Pb.ListStructuresReply { StructureRevision = snapshot.Revision };
            reply.Structures.AddRange(snapshot.Structures.Select(s => new Pb.Structure
            {
                StructureId = s.StructureId, Cell = new Pb.StructureCell { X = s.Cell.X, Y = s.Cell.Y, Z = s.Cell.Z },
                ContentId = s.ContentId, Revision = s.Revision,
                ShapeKey = s.ShapeKey, OrientationIndex = s.OrientationIndex
            }));
            return reply;
        });

    public override Task<Pb.ClaimReply> Grant(Pb.GrantRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            var fingerprint = Fingerprint(Encoding.UTF8.GetBytes("housing/grant/v1:").Concat(request.ToByteArray()).ToArray());
            var replay = store.ReplayClaim(request.OperationId, fingerprint);
            if (replay is not null) return ToPb(replay);
            const ClaimRight all = ClaimRight.Build | ClaimRight.OpenDoors | ClaimRight.UseContainers;
            if (string.IsNullOrWhiteSpace(request.OperationId) || request.OperationId.Length > 128 ||
                string.IsNullOrWhiteSpace(request.ClaimId) || string.IsNullOrWhiteSpace(request.ActorId) ||
                request.ExpectedPermissionRevision < 0 || request.Rights < 0 ||
                (request.Rights & ~(int)all) != 0)
                throw new ArgumentException("Invalid grant request.");
            var claim = store.Claim(request.ClaimId) ?? throw new HousingRejectedException("Claim missing.");
            if (claim.OwnerId != request.ActorId)
                throw new HousingRejectedException("Only the claim owner may grant.", HousingRejectionCode.PermissionDenied);
            if (string.IsNullOrWhiteSpace(request.TargetId) || request.TargetId == request.ActorId)
                throw new ArgumentException("Invalid grant target.");
            if (claim.PermissionRevision != request.ExpectedPermissionRevision)
                throw new HousingRejectedException("Stale permission revision.", HousingRejectionCode.FailedPrecondition);
            if (claim.State is "Archived" or "Reclaimed")
                throw new HousingRejectedException("Claim is not active.");
            var grants = new Dictionary<string, int>(claim.Grants, StringComparer.Ordinal);
            if (request.Rights == 0) grants.Remove(request.TargetId);
            else grants[request.TargetId] = request.Rights;
            var saved = store.SaveClaim(request.OperationId, fingerprint,
                claim with { Revision = checked(claim.Revision + 1), PermissionRevision = checked(claim.PermissionRevision + 1), Grants = grants },
                claim.Revision);
            return ToPb(saved);
        });

    public override Task<Pb.PlacementReply> Place(Pb.PlaceRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            var fingerprint = Fingerprint(request.ToByteArray());
            var replay = store.Placement(request.OperationId);
            if (replay is not null)
            {
                if (replay.Fingerprint != fingerprint)
                    throw new HousingRejectedException("Operation ID conflict.", HousingRejectionCode.AlreadyExists);
                if (replay.State == "Committed") return PlacementReply(replay);
            }
            if (!request.StructureId.StartsWith("str_", StringComparison.Ordinal) ||
                request.MaterialQuantity <= 0 || string.IsNullOrWhiteSpace(request.OperationId))
                throw new ArgumentException("Invalid placement identity or quantity.");
            var shapeKey = request.HasShapeKey ? request.ShapeKey : PostgresHousingStore.DefaultShapeKey;
            if (!PostgresHousingStore.ValidGeometry(shapeKey, request.OrientationIndex))
                throw new ArgumentException("Invalid construction geometry identity.");
            var kind = request.ContentId.StartsWith("piece/", StringComparison.Ordinal)
                ? request.ContentId[6..] : "";
            if (kind is not ("foundation" or "wall" or "door" or "roof"))
                throw new ArgumentException("Unknown piece kind.");
            var cost = recipes.Get($"recipe/{kind}_material_bundle").Outputs.Single();
            if (request.MaterialId != cost.DefinitionId || request.MaterialQuantity != cost.Quantity)
                throw new ArgumentException("Placement cost differs from shipped recipe.");
            var container = ledger.Read(request.ContainerId);
            if (container.Spec.OwnerKind != OwnerKind.Character || container.Spec.OwnerId != request.ActorId)
                throw new HousingRejectedException("Container is not owned by actor.", HousingRejectionCode.PermissionDenied);
            var pending = store.Begin(new HousingPlaceRequest(request.OperationId,
                fingerprint, request.ClaimId, request.ActorId,
                request.ExpectedPermissionRevision, request.ExpectedStructureRevision,
                request.StructureId, new HousingCell(request.CellX, request.CellY, request.CellZ),
                request.ContentId, request.MaterialId, request.MaterialQuantity,
                request.ContainerId, request.ExpectedContainerRevision, shapeKey, request.OrientationIndex), DateTimeOffset.UtcNow);
            if (pending.State == "Pending")
            {
                if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_PLACE_INTENT") == request.OperationId)
                    Environment.FailFast("Injected post-placement-intent host fault.");
                if (pending.Decision is null)
                {
                    // DEC-02 prepare: inventory reserves the materials into the builder's hold.
                    try
                    {
                        var hold = ledger.CreateContainer(PostgresHousingStore.HoldSpec(request.ActorId));
                        ledger.Transfer(request.OperationId + "/reserve", request.ContainerId, request.ExpectedContainerRevision,
                            hold.Spec.Id, hold.Revision, request.MaterialId, request.MaterialQuantity);
                    }
                    catch (LedgerRejectedException)
                    {
                        // The decision trusts only the durable ledger operation; a missing reservation aborts.
                    }
                    pending = store.Decide(request.OperationId, ledgerStore, DateTimeOffset.UtcNow);
                }
                if (pending.Decision is not null && pending.State == "Pending" &&
                    Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_PLACE_DECISION") == request.OperationId)
                    Environment.FailFast("Injected post-placement-decision host fault.");
                pending = store.Finish(request.OperationId, ledgerStore);
            }
            if (pending.State != "Committed") throw new InvalidOperationException("Placement compensated.");
            return PlacementReply(pending);
        });

    public override Task<Pb.PlacementReply> Remove(Pb.RemoveRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () =>
        {
            var kind = request.RemovalKind switch
            {
                "undo" => RemovalKind.Undo,
                "voluntary" => RemovalKind.Voluntary,
                "destruction" => RemovalKind.Destruction,
                _ => throw new ArgumentException("Unknown removal kind.")
            };
            var settings = new PlacementRefundSettings();
            settings.Validate();
            var pending = store.BeginRemove(new HousingRemoveRequest(request.OperationId,
                Fingerprint(request.ToByteArray()), request.ClaimId, request.ActorId,
                request.StructureId, request.ExpectedPermissionRevision, request.ExpectedStructureRevision,
                request.ExpectedContainerRevision, settings.For(kind)), DateTimeOffset.UtcNow);
            if (pending.State == "Pending")
            {
                if (pending.RefundQuantity > 0)
                {
                    try
                    {
                        ledger.Grant(request.OperationId + "/refund", pending.Placement.ContainerId,
                            pending.ExpectedContainerRevision, pending.Placement.MaterialId,
                            pending.RefundQuantity, "construction_material_return", request.OperationId);
                    }
                    catch (LedgerRejectedException) { /* The durable receipt decides reconciliation. */ }
                    if (Environment.GetEnvironmentVariable("SF_GAMEPLAY_FAULT_AFTER_REMOVE_GRANT") == request.OperationId)
                        Environment.FailFast("Injected post-refund host fault.");
                }
                pending = store.CompleteRemove(request.OperationId, ledgerStore);
            }
            return new Pb.PlacementReply
            {
                OperationId = pending.OperationId, State = pending.State == "Committed" ? "Removed" : pending.State,
                StructureId = pending.Placement.StructureId, StructureRevision = pending.Revision,
                ConsumeOperationId = pending.RefundReceipt?.OperationId ?? ""
            };
        });

    private static Pb.PlacementReply PlacementReply(HousingPlacement placement) => new()
    {
        OperationId = placement.OperationId, State = placement.State,
        StructureId = placement.StructureId, StructureRevision = placement.Revision,
        ConsumeOperationId = placement.ConsumeReceipt?.OperationId ?? "",
        ShapeKey = placement.ShapeKey, OrientationIndex = placement.OrientationIndex
    };

    private static Pb.ClaimReply ToPb(HousingClaim c) => new()
    {
        ClaimId = c.Id, OwnerId = c.OwnerId, State = c.State, Revision = c.Revision,
        PermissionRevision = c.PermissionRevision, FundedUntilUnixMs = c.FundedUntil.ToUnixTimeMilliseconds()
    };
    private static string Fingerprint(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
}
