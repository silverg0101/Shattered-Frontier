using Grpc.Core;
using Grpc.Net.Client;
using Npgsql;
using Pb = ShatteredFrontier.Chat.V1;

static void Check(bool condition, string message)
{
    if (!condition) throw new Exception(message);
}
static void Code(Action call, StatusCode expected)
{
    try { call(); throw new Exception($"Expected {expected}."); }
    catch (RpcException ex) when (ex.StatusCode == expected) { }
}
var mode = args.Single();
var port = Environment.GetEnvironmentVariable("CHAT_HOST_PORT") ?? throw new Exception("CHAT_HOST_PORT required");
var token = Environment.GetEnvironmentVariable("SF_CHAT_TOKEN") ?? throw new Exception("SF_CHAT_TOKEN required");
var connection = Environment.GetEnvironmentVariable("CHAT_CONNECTION") ?? throw new Exception("CHAT_CONNECTION required");
using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{port}");
var client = new Pb.ContactsService.ContactsServiceClient(channel);
var headers = new Metadata { { "authorization", "Bearer " + token } };
var bad = new Metadata { { "authorization", "Bearer wrong" } };
DateTime Deadline() => DateTime.UtcNow.AddSeconds(5);
var a = "chr_dev_alpha"; var b = "chr_dev_beta";
Pb.ContactMutation Request(string key, string kind = "friend", string actor = "chr_dev_alpha", string other = "chr_dev_beta") =>
    new() { IdempotencyKey = key, CharacterId = actor, OtherCharacterId = other, Kind = kind };
if (mode == "first")
{
    Code(() => client.ListContacts(new Pb.CharacterRef { CharacterId = a }, bad, Deadline()), StatusCode.Unauthenticated);
    Check(client.AddContact(Request("contacts/add-friend"), headers, Deadline()).Applied, "Friend add failed.");
    Check(client.AddContact(Request("contacts/add-friend"), headers, Deadline()).Applied, "Replay changed response.");
    Code(() => client.AddContact(Request("contacts/add-friend", "block"), headers, Deadline()), StatusCode.InvalidArgument);
    Check(!client.AddContact(Request("contacts/duplicate"), headers, Deadline()).Applied, "Duplicate friend applied.");
    Code(() => client.AddContact(Request("contacts/self", other: a), headers, Deadline()), StatusCode.InvalidArgument);
    Code(() => client.AddContact(Request("contacts/bad-kind", "ignore"), headers, Deadline()), StatusCode.InvalidArgument);
    var list = client.ListContacts(new Pb.CharacterRef { CharacterId = a }, headers, Deadline());
    Check(list.Contacts.Count == 1 && list.Contacts[0].OtherCharacterId == b && list.Contacts[0].Kind == "friend" &&
        DateTime.TryParse(list.Contacts[0].CreatedAtUtc, out _), "Friend list failed.");
    Check(client.ListContacts(new Pb.CharacterRef { CharacterId = b }, headers, Deadline()).Contacts.Count == 0, "Friend edge became reciprocal.");
    Check(!client.IsBlocked(new Pb.BlockQuery { CharacterId = a, OtherCharacterId = b }, headers, Deadline()).Blocked, "Friend counted as block.");
    Check(client.AddContact(Request("contacts/reverse-block", "block", b, a), headers, Deadline()).Applied, "Reverse block failed.");
    Check(client.IsBlocked(new Pb.BlockQuery { CharacterId = a, OtherCharacterId = b }, headers, Deadline()).Blocked, "Bilateral block missed.");
    using var db = new NpgsqlConnection(connection); db.Open();
    try { using var ddl = new NpgsqlCommand("CREATE TABLE chat.forbidden_runtime_ddl(id int)", db); ddl.ExecuteNonQuery(); throw new Exception("Runtime role has DDL privilege."); }
    catch (PostgresException ex) when (ex.SqlState == "42501") { }
    Console.WriteLine("PASS contacts auth/add/replay/conflict/duplicate/validation/list/direction/block/DML-only (13)");
}
else if (mode == "restart")
{
    Check(client.IsBlocked(new Pb.BlockQuery { CharacterId = b, OtherCharacterId = a }, headers, Deadline()).Blocked, "Block lost on restart.");
    Check(client.RemoveContact(Request("contacts/remove-block", "block", b, a), headers, Deadline()).Applied, "Remove block failed.");
    Check(!client.IsBlocked(new Pb.BlockQuery { CharacterId = a, OtherCharacterId = b }, headers, Deadline()).Blocked, "Block remained.");
    Check(client.RemoveContact(Request("contacts/remove-block", "block", b, a), headers, Deadline()).Applied, "Remove replay changed response.");
    Check(client.AddContact(Request("contacts/forward-block", "block"), headers, Deadline()).Applied, "Forward block failed.");
    Check(client.IsBlocked(new Pb.BlockQuery { CharacterId = b, OtherCharacterId = a }, headers, Deadline()).Blocked, "Forward direction missed.");
    Check(client.RemoveContact(Request("contacts/remove-friend"), headers, Deadline()).Applied, "Friend remove failed.");
    Check(client.ListContacts(new Pb.CharacterRef { CharacterId = a }, headers, Deadline()).Contacts.Single().Kind == "block", "Remove touched other kind.");
    Console.WriteLine("PASS contacts restart/remove/replay/both-directions/kind-isolation (8)");
}
else throw new Exception("Expected first or restart mode.");
