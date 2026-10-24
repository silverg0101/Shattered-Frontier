using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;
using Grpc.Core;
using Grpc.Net.Client;
using Npgsql;
using Pb = ShatteredFrontier.Chat.V1;

var mode = args.Single();
var port = Environment.GetEnvironmentVariable("CHAT_HOST_PORT") ?? throw new Exception("CHAT_HOST_PORT required");
var token = Environment.GetEnvironmentVariable("SF_CHAT_TOKEN") ?? throw new Exception("SF_CHAT_TOKEN required");
var privacy = Environment.GetEnvironmentVariable("SF_CHAT_PRIVACY_TOKEN") ?? throw new Exception("SF_CHAT_PRIVACY_TOKEN required");
var connection = Environment.GetEnvironmentVariable("CHAT_CONNECTION") ?? throw new Exception("CHAT_CONNECTION required");
var statePath = Environment.GetEnvironmentVariable("CHAT_TEST_STATE") ?? throw new Exception("CHAT_TEST_STATE required");
using var identity = new HttpClient { BaseAddress = new Uri((Environment.GetEnvironmentVariable("CHAT_IDENTITY_URL") ?? throw new Exception("CHAT_IDENTITY_URL required")) + "/") };
using var channel = GrpcChannel.ForAddress($"http://127.0.0.1:{port}");
var client = new Pb.ContactsService.ContactsServiceClient(channel);
int checks = 0;
void Check(bool condition, string name) { if (!condition) throw new Exception("FAIL " + name); checks++; }
void Code(Action call, StatusCode expected, string name)
{
    try { call(); throw new Exception($"FAIL {name}: expected {expected}"); }
    catch (RpcException ex) when (ex.StatusCode == expected) { checks++; }
}
DateTime Deadline() => DateTime.UtcNow.AddSeconds(8);
JsonElement Post(string path, object body, string? bearer = null)
{
    using var request = new HttpRequestMessage(HttpMethod.Post, path) { Content = JsonContent.Create(body) };
    request.Headers.Add("Idempotency-Key", "contacts-" + Guid.NewGuid().ToString("N"));
    if (bearer is not null) request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", bearer);
    using var response = identity.Send(request);
    if (!response.IsSuccessStatusCode) throw new Exception($"Identity {path}: {(int)response.StatusCode} {response.Content.ReadAsStringAsync().Result}");
    using var document = JsonDocument.Parse(response.Content.ReadAsStringAsync().Result);
    return document.RootElement.Clone();
}
string CreateAccount(string name)
{
    Post("v1/accounts", new { account_name = name, password = "contacts-test-password" });
    var login = Post("v1/login/begin", new { account_name = name, password = "contacts-test-password" });
    return Post("v1/login/complete", new { login_id = login.GetProperty("login_id").GetString() }).GetProperty("session_token").GetString()!;
}
string CreateCharacter(string bearer, string name)
{
    var packet = new byte[29];
    packet[0] = 2; packet[3] = 1; packet[4] = 128; packet[5] = 128; packet[6] = 8;
    return Post("v1/characters", new { realm_id = "greenlands", name, appearance_v2_base64 = Convert.ToBase64String(packet) }, bearer)
        .GetProperty("character_id").GetString()!;
}
Metadata Headers(string? actor = null, string? bearer = null)
{
    var result = new Metadata { { "authorization", "Bearer " + token } };
    if (actor is not null)
    {
        var ticket = Post($"v1/characters/{actor}/contacts-ticket", new { }, bearer).GetProperty("ticket").GetString()!;
        result.Add("x-contact-ticket", ticket);
    }
    return result;
}
Pb.ContactMutation Mutation(string key, string actor, string other, string kind = "friend") =>
    new() { IdempotencyKey = key, CharacterId = actor, OtherCharacterId = other, Kind = kind };
Pb.BlockQuery Pair(string actor, string other) => new() { CharacterId = actor, OtherCharacterId = other };

State state;
if (mode == "first")
{
    var suffix = new string(Guid.NewGuid().ToString("N")[..10].Select(c => char.IsDigit(c) ? (char)('a' + c - '0') : c).ToArray());
    var tokenA = CreateAccount("contacta" + suffix);
    var tokenB = CreateAccount("contactb" + suffix);
    state = new State(CreateCharacter(tokenA, "Alina" + suffix), CreateCharacter(tokenB, "Brenna" + suffix), tokenA, tokenB);
    File.WriteAllText(statePath, JsonSerializer.Serialize(state));
}
else if (mode == "restart") state = JsonSerializer.Deserialize<State>(File.ReadAllText(statePath))!;
else throw new Exception("Expected first or restart mode.");
var a = state.A; var b = state.B;
var authA = state.TokenA; var authB = state.TokenB;
if (mode == "first")
{
    Code(() => client.ListContacts(new Pb.CharacterRef { CharacterId = a }, new Metadata { { "authorization", "Bearer wrong" } }, Deadline()), StatusCode.Unauthenticated, "service auth");
    Code(() => client.AddContact(Mutation("contacts/missing", a, b), Headers(), Deadline()), StatusCode.Unauthenticated, "missing ticket");
    Code(() => client.AddContact(Mutation("contacts/spoof", b, a), Headers(a, authA), Deadline()), StatusCode.PermissionDenied, "spoofed actor");
    using (var request = new HttpRequestMessage(HttpMethod.Post, $"v1/characters/{b}/contacts-ticket"))
    {
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", authA);
        using var response = identity.Send(request);
        Check(response.StatusCode == System.Net.HttpStatusCode.NotFound, "ticket issuance owner guard");
    }
    var expired = Headers(a, authA);
    Thread.Sleep(6000);
    Code(() => client.AddContact(Mutation("contacts/expired", a, b), expired, Deadline()), StatusCode.Unauthenticated, "expired ticket");
    var worldTicket = Post($"v1/characters/{a}/enter-world", new { }, authA).GetProperty("ticket").GetString()!;
    Code(() => client.AddContact(Mutation("contacts/world-ticket", a, b), new Metadata { { "authorization", "Bearer " + token }, { "x-contact-ticket", worldTicket } }, Deadline()), StatusCode.Unauthenticated, "world ticket scope");
    var singleUse = Headers(a, authA);
    Check(client.CheckWhisperDelivery(Pair(a, b), singleUse, Deadline()).Allowed, "unfriended whisper allowed");
    Code(() => client.CheckWhisperDelivery(Pair(a, b), singleUse, Deadline()), StatusCode.Unauthenticated, "contact ticket replay");
    Check(client.AddContact(Mutation("contacts/add-friend", a, b), Headers(a, authA), Deadline()).Applied, "friend add");
    Check(client.AddContact(Mutation("contacts/add-friend", a, b), Headers(a, authA), Deadline()).Applied, "friend replay");
    Code(() => client.AddContact(Mutation("contacts/add-friend", a, b, "block"), Headers(a, authA), Deadline()), StatusCode.InvalidArgument, "receipt conflict");
    Check(!client.AddContact(Mutation("contacts/duplicate", a, b), Headers(a, authA), Deadline()).Applied, "duplicate friend");
    Code(() => client.AddContact(Mutation("contacts/self", a, a), Headers(a, authA), Deadline()), StatusCode.InvalidArgument, "self contact");
    Code(() => client.AddContact(Mutation("contacts/bad-kind", a, b, "ignore"), Headers(a, authA), Deadline()), StatusCode.InvalidArgument, "kind validation");
    var list = client.ListContacts(new Pb.CharacterRef { CharacterId = a }, Headers(a, authA), Deadline());
    Check(list.Contacts.Count == 1 && list.Contacts[0].OtherCharacterId == b && list.Contacts[0].Kind == "friend" && DateTime.TryParse(list.Contacts[0].CreatedAtUtc, out _), "friend list");
    Check(client.ListContacts(new Pb.CharacterRef { CharacterId = b }, Headers(b, authB), Deadline()).Contacts.Count == 0, "directed friend");
    Check(client.CheckWhisperDelivery(Pair(b, a), Headers(b, authB), Deadline()).Allowed, "friend needs no reciprocity");
    Check(!client.IsBlocked(Pair(a, b), Headers(), Deadline()).Blocked, "friend is not block");
    Check(client.AddContact(Mutation("contacts/reverse-block", b, a, "block"), Headers(b, authB), Deadline()).Applied, "reverse block");
    Check(client.IsBlocked(Pair(a, b), Headers(), Deadline()).Blocked, "bilateral block lookup");
    Check(!client.CheckWhisperDelivery(Pair(a, b), Headers(a, authA), Deadline()).Allowed, "reverse block denies whisper");
    Check(!client.CheckWhisperDelivery(Pair(b, a), Headers(b, authB), Deadline()).Allowed, "sender block denies whisper");
    using var db = new NpgsqlConnection(connection); db.Open();
    try { using var ddl = new NpgsqlCommand("CREATE TABLE chat.forbidden_runtime_ddl(id int)", db); ddl.ExecuteNonQuery(); throw new Exception("Runtime role has DDL privilege."); }
    catch (PostgresException ex) when (ex.SqlState == "42501") { checks++; }
}
else
{
    Check(client.IsBlocked(Pair(b, a), Headers(), Deadline()).Blocked, "block survives restart");
    Check(client.RemoveContact(Mutation("contacts/remove-block", b, a, "block"), Headers(b, authB), Deadline()).Applied, "remove block");
    Check(client.CheckWhisperDelivery(Pair(a, b), Headers(a, authA), Deadline()).Allowed, "whisper allowed after unblock");
    Check(client.RemoveContact(Mutation("contacts/remove-block", b, a, "block"), Headers(b, authB), Deadline()).Applied, "remove replay");
    Check(client.AddContact(Mutation("contacts/forward-block", a, b, "block"), Headers(a, authA), Deadline()).Applied, "forward block");
    Check(!client.CheckWhisperDelivery(Pair(b, a), Headers(b, authB), Deadline()).Allowed, "forward block denies whisper");
    Check(client.RemoveContact(Mutation("contacts/remove-friend", a, b), Headers(a, authA), Deadline()).Applied, "friend remove");
    Check(client.ListContacts(new Pb.CharacterRef { CharacterId = a }, Headers(a, authA), Deadline()).Contacts.Single().Kind == "block", "kind isolation");
    Check(client.AddContact(Mutation("contacts/incoming-friend", b, a), Headers(b, authB), Deadline()).Applied, "incoming friend");
    var privacyHeaders = new Metadata { { "authorization", "Bearer " + privacy } };
    Code(() => client.EraseContacts(new Pb.EraseContactsRequest { CharacterIds = { a } }, Headers(), Deadline()), StatusCode.Unauthenticated, "erasure auth");
    Check(client.EraseContacts(new Pb.EraseContactsRequest { CharacterIds = { a } }, privacyHeaders, Deadline()).RemovedEdges == 2, "erasure removes incoming and outgoing");
    Check(client.EraseContacts(new Pb.EraseContactsRequest { CharacterIds = { a } }, privacyHeaders, Deadline()).RemovedEdges == 0, "erasure retry");
    Check(client.ListContacts(new Pb.CharacterRef { CharacterId = b }, Headers(b, authB), Deadline()).Contacts.Count == 0, "erased incoming edge absent");
    Check(!client.CheckWhisperDelivery(Pair(b, a), Headers(b, authB), Deadline()).Allowed, "erased recipient denied");
    Code(() => client.AddContact(Mutation("contacts/resurrect", b, a), Headers(b, authB), Deadline()), StatusCode.InvalidArgument, "erased contact cannot return");
}
Console.WriteLine($"PASS contacts {mode}: {checks} checks, 0 failed");

sealed record State(string A, string B, string TokenA, string TokenB);
