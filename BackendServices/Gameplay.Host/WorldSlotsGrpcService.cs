using Grpc.Core;
using ShatteredFrontier.WorldContent;
using Pb = ShatteredFrontier.World.V1;

// Common boar slots are session state. The host owns WorldContent transitions; Unreal owns actors.
internal sealed class WorldSlotsGrpcService(ZoneContent zone) : Pb.WorldSlots.WorldSlotsBase
{
    private readonly object gate = new();
    private readonly SpawnSlotService slots = new(zone, new WorldTuning(), TimeProvider.System, new SeededSpawnRandom(2077));
    private bool hostileSpawning = true;

    public override Task<Pb.AdvanceSlotsReply> Advance(Pb.AdvanceSlotsRequest request, ServerCallContext context) =>
        GrpcRun.Call(context, () => AdvanceCore(request));

    internal Pb.AdvanceSlotsReply AdvanceCore(Pb.AdvanceSlotsRequest request)
    {
        if (request.Command is not ("tick" or "defeat") || request.PlayersInArea is < 0 or > 1000 ||
            (request.Command == "defeat" && (request.GroupId is not ("boar_farm_edge" or "boar_forest_edge") ||
                request.SlotIndex is < 0 or > 4)))
            throw new GameplayRejectedException("Invalid WorldContent slot request.", StatusCode.InvalidArgument);
        lock (gate)
        {
            if (hostileSpawning != request.HostileSpawning)
            {
                slots.SetHostileSpawning(request.HostileSpawning);
                hostileSpawning = request.HostileSpawning;
            }
            if (request.Command == "defeat")
            {
                // A retried request cannot advance an already defeated slot twice.
                var current = slots.Snapshot(request.GroupId).Single(s => s.SlotIndex == request.SlotIndex);
                if (current.State == SlotState.Active)
                    slots.DefeatNpc(request.GroupId, request.SlotIndex, "common", request.PlayersInArea);
            }
            slots.Tick(DayPhase.Day);
            var reply = new Pb.AdvanceSlotsReply();
            foreach (var group in zone.SpawnGroups.Where(g => g.GroupId is "boar_farm_edge" or "boar_forest_edge"))
            foreach (var slot in slots.Snapshot(group.GroupId))
            {
                var point = group.Points.FirstOrDefault(p => p.PointId == slot.PointId);
                reply.Slots.Add(new Pb.BoarSlot
                {
                    GroupId = group.GroupId, SlotIndex = slot.SlotIndex, Cycle = slot.Cycle,
                    Active = slot.State == SlotState.Active, PointId = point?.PointId ?? "",
                    XMetres = point?.Position[0] ?? 0, YMetres = point?.Position[1] ?? 0,
                    FacingDegrees = point?.Facing ?? 0
                });
            }
            return reply;
        }
    }
}
