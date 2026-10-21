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
                request.ContainerId, request.ExpectedContainerRevision), DateTimeOffset.UtcNow);
            if (pending.State == "Pending")
            {
                try
                {
                    ledger.Consume(request.OperationId + "/consume", request.ContainerId,
                        request.ExpectedContainerRevision, request.MaterialId, request.MaterialQuantity,
                        "construction_material_consumed");
                }
                catch (LedgerRejectedException)
                {
                    // Reconcile closes a reservation only after checking the durable ledger operation.
                }
                pending = store.Reconcile(request.OperationId, ledgerStore);
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
        ConsumeOperationId = placement.ConsumeReceipt?.OperationId ?? ""
    };

    private static Pb.ClaimReply ToPb(HousingClaim c) => new()
    {
        ClaimId = c.Id, OwnerId = c.OwnerId, State = c.State, Revision = c.Revision,
        PermissionRevision = c.PermissionRevision, FundedUntilUnixMs = c.FundedUntil.ToUnixTimeMilliseconds()
    };
    private static string Fingerprint(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
}
