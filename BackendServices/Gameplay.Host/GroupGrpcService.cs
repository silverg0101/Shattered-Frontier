using Grpc.Core;
using ShatteredFrontier.Group.Postgres;
using Pb = ShatteredFrontier.Group.V1;

internal sealed class GroupGrpcService(PostgresGroupStore store) : Pb.GroupService.GroupServiceBase
{
    private static Pb.Party Wire(PartySnapshot? value)
    {
        var party = new Pb.Party();
        if (value is null) return party;
        party.PartyId = value.PartyId; party.LeaderCharacterId = value.Leader;
        party.AssistantCharacterId = value.Assistant; party.Version = value.Version; party.State = value.State;
        party.MemberCharacterIds.AddRange(value.Members);
        return party;
    }
    private static Task<Pb.Party> Run(ServerCallContext context, Func<PartySnapshot?> work) => GrpcRun.Call(context, () =>
    {
        try { return Wire(work()); }
        catch (GroupRejectedException ex)
        {
            throw new RpcException(new Status(ex.Code switch
            {
                "invalid" => StatusCode.InvalidArgument,
                "permission" => StatusCode.PermissionDenied,
                "not_found" => StatusCode.NotFound,
                _ => StatusCode.FailedPrecondition
            }, ex.Message));
        }
    });
    public override Task<Pb.Party> CreateParty(Pb.CreatePartyRequest r, ServerCallContext c) =>
        Run(c, () => store.Create(r.IdempotencyKey, r.LeaderCharacterId, r.AccountId, r.RealmId, r.DcRegion));
    public override Task<Pb.Party> JoinParty(Pb.JoinPartyRequest r, ServerCallContext c) =>
        Run(c, () => store.Join(r.IdempotencyKey, r.PartyId, r.ActorCharacterId, r.CharacterId, r.AccountId, r.RealmId));
    public override Task<Pb.Party> LeaveParty(Pb.MemberActionRequest r, ServerCallContext c) =>
        Run(c, () => store.Leave(r.IdempotencyKey, r.PartyId, r.ActorCharacterId, r.CharacterId, false));
    public override Task<Pb.Party> KickMember(Pb.MemberActionRequest r, ServerCallContext c) =>
        Run(c, () => store.Leave(r.IdempotencyKey, r.PartyId, r.ActorCharacterId, r.CharacterId, true));
    public override Task<Pb.Party> PromoteLeader(Pb.MemberActionRequest r, ServerCallContext c) =>
        Run(c, () => store.SetRole(r.IdempotencyKey, "promote", r.PartyId, r.ActorCharacterId, r.CharacterId));
    public override Task<Pb.Party> AssignAssistant(Pb.MemberActionRequest r, ServerCallContext c) =>
        Run(c, () => store.SetRole(r.IdempotencyKey, "assistant", r.PartyId, r.ActorCharacterId, r.CharacterId));
    public override Task<Pb.Party> GetPartyForCharacter(Pb.CharacterRef r, ServerCallContext c) =>
        Run(c, () => store.GetForCharacter(r.CharacterId));
}
