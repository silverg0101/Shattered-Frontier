using System.Security.Cryptography;
using System.Text;
using Grpc.Core;
using ShatteredFrontier.Chat.Postgres;
using Pb = ShatteredFrontier.Chat.V1;

internal sealed record ChatServiceToken(string Value);

internal sealed class ContactsGrpcService(PostgresContactsStore store, ChatServiceToken token) : Pb.ContactsService.ContactsServiceBase
{
    private void Authorize(ServerCallContext context)
    {
        var supplied = context.RequestHeaders.GetValue("authorization") ?? "";
        var expected = "Bearer " + token.Value;
        if (!CryptographicOperations.FixedTimeEquals(Encoding.UTF8.GetBytes(supplied), Encoding.UTF8.GetBytes(expected)))
            throw new RpcException(new Status(StatusCode.Unauthenticated, "Service token required."));
    }
    private T Run<T>(ServerCallContext context, Func<T> work)
    {
        Authorize(context);
        try { return work(); }
        catch (ContactRejectedException ex) { throw new RpcException(new Status(StatusCode.InvalidArgument, ex.Message)); }
    }
    public override Task<Pb.ContactAck> AddContact(Pb.ContactMutation request, ServerCallContext context) =>
        Task.FromResult(Run(context, () => new Pb.ContactAck { Applied = store.Mutate(request.IdempotencyKey, request.CharacterId, request.OtherCharacterId, request.Kind, true) }));
    public override Task<Pb.ContactAck> RemoveContact(Pb.ContactMutation request, ServerCallContext context) =>
        Task.FromResult(Run(context, () => new Pb.ContactAck { Applied = store.Mutate(request.IdempotencyKey, request.CharacterId, request.OtherCharacterId, request.Kind, false) }));
    public override Task<Pb.ContactList> ListContacts(Pb.CharacterRef request, ServerCallContext context) =>
        Task.FromResult(Run(context, () =>
        {
            var result = new Pb.ContactList();
            result.Contacts.AddRange(store.List(request.CharacterId).Select(value => new Pb.Contact
            {
                OtherCharacterId = value.OtherCharacterId, Kind = value.Kind,
                CreatedAtUtc = value.CreatedAtUtc.ToUniversalTime().ToString("O")
            }));
            return result;
        }));
    public override Task<Pb.BlockStatus> IsBlocked(Pb.BlockQuery request, ServerCallContext context) =>
        Task.FromResult(Run(context, () => new Pb.BlockStatus { Blocked = store.IsBlocked(request.CharacterId, request.OtherCharacterId) }));
}
