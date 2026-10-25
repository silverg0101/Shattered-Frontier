using System.Security.Cryptography;
using System.Text;
using CharacterAppearance;
using IdentityGateway;
using ShatteredFrontier.AccountData;

var store = new MemoryStore();
var clock = new FakeClock();
var options = new IdentityOptions { SessionLifetimeSeconds = 60, TicketLifetimeSeconds = 5, LoginAttemptsPerWindow = 2 };
options.Validate();
var ticketVerifier = new TicketVerifier(RandomNumberGenerator.GetBytes(32), store, clock);
var service = new IdentityService(store, options, new AppearanceValidator(AppearanceAllowlist.Load(args.Length == 1 ? args[0] : "content/appearance/human.json")), ticketVerifier, clock);
int passes = 0;
void Check(bool condition, string name)
{
    if (!condition) throw new Exception("FAIL " + name);
    Console.WriteLine("PASS " + name);
    passes++;
}
void Reject(Action action, string reason, string name)
{
    try { action(); }
    catch (ApiError e) when (e.Reason == reason) { Check(true, name); return; }
    Check(false, name);
}

var account = service.CreateAccount("alice", "long-secret-password");
Check(account.StartsWith("acct_") && store.FindAccount("alice")!.PasswordHash != "long-secret-password", "account stores PBKDF2 hash");
Reject(() => service.BeginLogin("alice", "bad-password", "127.0.0.1"), "BAD_CREDENTIALS", "login failure");
Reject(() => service.BeginLogin("missing", "unused-dummy-password", "127.0.0.2"), "BAD_CREDENTIALS", "unknown account uses generic failure");
var loginId = service.BeginLogin("alice", "long-secret-password", "127.0.0.1");
var (token, session) = service.CompleteLogin(loginId);
Check(service.RequireSession("Bearer " + token).AccountId == account, "login success");
Reject(() => service.CompleteLogin(loginId), "LOGIN_CHALLENGE_INVALID", "login challenge one time");
Reject(() => service.BeginLogin("alice", "long-secret-password", "127.0.0.1"), "RATE_LIMITED", "login rate limit");
clock.Advance(TimeSpan.FromSeconds(61));
Reject(() => service.RequireSession("Bearer " + token), "SESSION_INVALID", "session expiry");
var freshId = service.BeginLogin("alice", "long-secret-password", "127.0.0.1");
var (freshToken, freshSession) = service.CompleteLogin(freshId);
var packet = AppearanceCodec.Pack(new Appearance(2, 0, 0, 1, 128, 128, new byte[8], [0, 0, 0, 0], [0, 0, 0, 0], [], 0));
Reject(() => service.CreateCharacter(freshSession, "greenlands", "Al", packet), "NAME_INVALID", "name length rejection");
Reject(() => service.CreateCharacter(freshSession, "greenlands", "Αrwen", packet), "NAME_INVALID", "mixed script rejection");
Reject(() => service.CreateCharacter(freshSession, "greenlands", "AdminGuy", packet), "NAME_INVALID", "reserved prefix rejection");
Reject(() => service.CreateCharacter(freshSession, "greenlands", "Arwen", [1, 2, 3]), "APPEARANCE_INVALID", "appearance v2 rejection");
var character = service.CreateCharacter(freshSession, "greenlands", "Arwen", packet);
Check(service.ListCharacters(freshSession, "greenlands").Single().CharacterId == character.CharacterId, "character create and list");
Reject(() => service.CreateCharacter(freshSession, "greenlands", "Unknown", packet, "custom"),
    "ARCHETYPE_INVALID", "unknown archetype rejected before character insertion");
Check(service.CreateCharacter(freshSession, "greenlands", "Chosen", packet, "archer").Archetype == "archer",
    "chosen archetype recorded by identity");
Reject(() => service.CreateCharacter(freshSession, "greenlands", "arwen", packet), "NAME_TAKEN", "realm name unique");
service.CreateCharacter(freshSession, "greenlands", "Papa", packet);
Reject(() => service.CreateCharacter(freshSession, "greenlands", "Рара", packet), "NAME_TAKEN", "whole-script confusable collision");
for (var i = 0; i < 5; i++) service.CreateCharacter(freshSession, "greenlands", "Nameaa" + (char)('a' + i), packet);
Reject(() => service.CreateCharacter(freshSession, "greenlands", "Extrafolk", packet), "SLOTS_FULL", "realm slot limit");
var otherAccount = service.CreateAccount("bob", "another-long-password");
var otherSession = new SessionRecord("ses_other", otherAccount, clock.GetUtcNow().AddMinutes(1));
Reject(() => service.DeleteCharacter(otherSession, character.CharacterId), "CHARACTER_NOT_FOUND", "character owner guard");
var (address, ticket, expires) = service.EnterWorld(freshSession, character.CharacterId);
Check(address == options.WorldAddress && ticketVerifier.VerifyAndConsume(ticket, options.WorldServerId)?.CharacterId == character.CharacterId, "ticket verifies once");
Check(ticketVerifier.VerifyAndConsume(ticket, options.WorldServerId) is null, "ticket replay rejected");
var (_, expiringTicket, _) = service.EnterWorld(freshSession, character.CharacterId);
clock.Advance(TimeSpan.FromSeconds(6));
Check(ticketVerifier.VerifyAndConsume(expiringTicket, options.WorldServerId) is null, "ticket expiry");
Check(ticketVerifier.VerifyAndConsume(ticket + "x", options.WorldServerId) is null, "ticket signature tamper");
service.DeleteCharacter(freshSession, character.CharacterId);
Check(service.ListCharacters(freshSession, null).All(c => c.CharacterId != character.CharacterId), "character delete");
Reject(() => service.CreateCharacter(freshSession, "greenlands", "Arwen", packet), "NAME_TAKEN", "deleted name hold");
Reject(() => service.EnterWorld(freshSession, character.CharacterId), "CHARACTER_NOT_FOUND", "deleted character cannot enter");
var dek = RandomNumberGenerator.GetBytes(32);
var kek = RandomNumberGenerator.GetBytes(32);
var wrapped = AccountDataCrypto.Wrap(dek, kek, account);
Check(AccountDataCrypto.Unwrap(wrapped, kek, account).SequenceEqual(dek), "DEK wrap round trip");
Check(ThrowsCrypto(() => AccountDataCrypto.Unwrap(wrapped, kek, otherAccount)), "DEK account AAD binding");
var envelope = AccountDataCrypto.Encrypt(Encoding.UTF8.GetBytes("private mail"), dek, 1, account, "chat.mail_messages", "body_ct", "mal_1");
Check(Encoding.UTF8.GetString(AccountDataCrypto.Decrypt(envelope, dek, 1, account, "chat.mail_messages", "body_ct", "mal_1")) == "private mail", "envelope round trip");
Check(ThrowsCrypto(() => AccountDataCrypto.Decrypt(envelope, dek, 1, account, "chat.mail_messages", "body_ct", "mal_2")), "wrong record AAD rejected");
var tampered = envelope.ToArray(); tampered[^1] ^= 1;
Check(ThrowsCrypto(() => AccountDataCrypto.Decrypt(tampered, dek, 1, account, "chat.mail_messages", "body_ct", "mal_1")), "tampered tag rejected");
Check(ThrowsCrypto(() => AccountDataCrypto.Decrypt(envelope, dek, 2, account, "chat.mail_messages", "body_ct", "mal_1")), "wrong key version rejected");
var nonces = Enumerable.Range(0, 1000).Select(_ => Convert.ToHexString(AccountDataCrypto.Encrypt([1], dek, 1, account, "chat.mail_messages", "body_ct", "mal_1").AsSpan(3, 12))).ToHashSet();
Check(nonces.Count == 1000, "random nonce uniqueness sample");
Check(!DataKeyRotation.Due(2, 3) && DataKeyRotation.Due(3, 3), "injectable counter rotation threshold");
Console.WriteLine($"TOTAL {passes} passed, 0 failed");

static bool ThrowsCrypto(Action action)
{
    try { action(); return false; }
    catch (CryptographicException) { return true; }
}

sealed class FakeClock : TimeProvider
{
    private DateTimeOffset _now = DateTimeOffset.Parse("2026-10-24T00:00:00Z");
    public override DateTimeOffset GetUtcNow() => _now;
    public void Advance(TimeSpan duration) => _now += duration;
}

sealed class MemoryStore : IIdentityStore
{
    private readonly Dictionary<string, Account> _accounts = [];
    private readonly Dictionary<string, (SessionRecord Session, bool Ended)> _sessions = [];
    private readonly Dictionary<string, CharacterRecord> _characters = [];
    private readonly HashSet<string> _heldNames = [];
    private readonly HashSet<string> _tickets = [];
    public bool CreateAccount(Account a) { if (_accounts.ContainsKey(a.NameKey)) return false; _accounts[a.NameKey] = a; return true; }
    public Account? FindAccount(string key) => _accounts.GetValueOrDefault(key);
    public void SaveSession(byte[] hash, SessionRecord s) => _sessions[Convert.ToHexString(hash)] = (s, false);
    public SessionRecord? FindSession(byte[] hash, DateTimeOffset now) => _sessions.TryGetValue(Convert.ToHexString(hash), out var entry) && !entry.Ended && entry.Session.ExpiresAt > now ? entry.Session : null;
    public bool EndSession(byte[] hash) { var key = Convert.ToHexString(hash); if (!_sessions.TryGetValue(key, out var entry)) return false; _sessions[key] = (entry.Session, true); return true; }
    public IReadOnlyList<CharacterRecord> ListCharacters(string accountId, string? realmId) => _characters.Values.Where(c => c.AccountId == accountId && c.State == "active" && (realmId is null || c.RealmId == realmId)).ToArray();
    public CharacterRecord? FindCharacter(string id) => _characters.GetValueOrDefault(id);
    public CreateCharacterOutcome CreateCharacter(CharacterRecord c, int realmLimit, int accountLimit)
    {
        if (_heldNames.Contains(c.RealmId + ":" + c.NameKey)) return CreateCharacterOutcome.NameTaken;
        if (ListCharacters(c.AccountId, c.RealmId).Count >= realmLimit || ListCharacters(c.AccountId, null).Count >= accountLimit) return CreateCharacterOutcome.SlotsFull;
        _characters.Add(c.CharacterId, c); _heldNames.Add(c.RealmId + ":" + c.NameKey); return CreateCharacterOutcome.Created;
    }
    public bool DeleteCharacter(string id, string accountId, DateTimeOffset now, DateTimeOffset nameHoldUntil)
    {
        if (!_characters.TryGetValue(id, out var c) || c.AccountId != accountId || c.State != "active") return false;
        _characters[id] = c with { State = "deleted" }; return true;
    }
    public bool ConsumeTicket(byte[] nonceHash, DateTimeOffset expiresAt) => _tickets.Add(Convert.ToHexString(nonceHash));
}
