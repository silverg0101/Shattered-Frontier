using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using CharacterAppearance;

namespace IdentityGateway;

public sealed class IdentityOptions
{
    public int SessionLifetimeSeconds { get; set; } = 900; // seconds, effective on issue
    public int LoginChallengeSeconds { get; set; } = 60; // seconds, effective on begin
    public int TicketLifetimeSeconds { get; set; } = 30; // seconds, effective on issue
    public int LoginAttemptsPerWindow { get; set; } = 5; // attempts per account/IP, effective on next attempt
    public int LoginWindowSeconds { get; set; } = 60; // seconds, effective on next attempt
    public int CharacterSlotsPerRealm { get; set; } = 8;
    public int CharacterSlotsPerAccount { get; set; } = 16;
    public int CharacterRestoreDays { get; set; } = 30;
    public int NameReuseQuarantineDays { get; set; } = 90;
    public string WorldServerId { get; set; } = "srvi_dev_local";
    public string ChatServerId { get; set; } = "srvi_chat_local";
    public string GuildServerId { get; set; } = "srvi_guild_local";
    public string WorldAddress { get; set; } = "127.0.0.1:7777";
    public string[] ReservedNames { get; set; } = [];

    public void Validate()
    {
        if (SessionLifetimeSeconds is < 60 or > 1800 || LoginChallengeSeconds is < 10 or > 300 ||
            TicketLifetimeSeconds is < 5 or > 120 || LoginAttemptsPerWindow is < 1 or > 20 ||
            LoginWindowSeconds is < 10 or > 3600 || CharacterSlotsPerRealm is < 1 or > 16 ||
            CharacterSlotsPerAccount is < 1 or > 32 || CharacterRestoreDays is < 1 or > 90 ||
            NameReuseQuarantineDays is < 1 or > 365 || string.IsNullOrWhiteSpace(WorldServerId) ||
            string.IsNullOrWhiteSpace(ChatServerId) || ChatServerId == WorldServerId ||
            string.IsNullOrWhiteSpace(GuildServerId) || GuildServerId == WorldServerId || GuildServerId == ChatServerId ||
            string.IsNullOrWhiteSpace(WorldAddress))
            throw new InvalidOperationException("Identity configuration is outside its safe bounds.");
    }
}

public sealed record Account(string Id, string NameKey, string PasswordHash);
public sealed record CharacterRecord(string CharacterId, string AccountId, string RealmId, string Name,
    string NameKey, byte[] Appearance, string State);
public sealed record SessionRecord(string SessionId, string AccountId, DateTimeOffset ExpiresAt);
public sealed record LoginChallenge(string AccountId, DateTimeOffset ExpiresAt);
public sealed record TicketClaims(string SessionId, string AccountId, string CharacterId, string RealmId,
    string ServerId, long ExpiresAtUnixSeconds, string Nonce);
public enum CreateCharacterOutcome { Created, NameTaken, SlotsFull }

public interface IIdentityStore
{
    bool CreateAccount(Account account);
    Account? FindAccount(string nameKey);
    void SaveSession(byte[] tokenHash, SessionRecord session);
    SessionRecord? FindSession(byte[] tokenHash, DateTimeOffset now);
    bool EndSession(byte[] tokenHash);
    IReadOnlyList<CharacterRecord> ListCharacters(string accountId, string? realmId);
    CharacterRecord? FindCharacter(string characterId);
    CreateCharacterOutcome CreateCharacter(CharacterRecord character, int realmLimit, int accountLimit);
    bool DeleteCharacter(string characterId, string accountId, DateTimeOffset now, DateTimeOffset nameHoldUntil);
    bool ConsumeTicket(byte[] nonceHash, DateTimeOffset expiresAt);
}

public sealed class ApiError(int status, string code, string reason, int? retryAfterSeconds = null) : Exception(reason)
{
    public int Status { get; } = status;
    public string Code { get; } = code;
    public string Reason { get; } = reason;
    public int? RetryAfterSeconds { get; } = retryAfterSeconds;
}

public static class Names
{
    private static readonly Regex Pattern = new(@"^\p{L}[\p{L}'-]{2,15}$", RegexOptions.CultureInvariant);
    private static readonly string[] Denied = ["gm", "admin", "moderator", "support"];

    public static (string Name, string Key) Validate(string? input, IEnumerable<string> reserved)
    {
        var name = input?.Trim().Normalize(NormalizationForm.FormKC) ?? "";
        if (!Pattern.IsMatch(name) || name.Count(c => c is '\'' or '-') > 1)
            throw new ApiError(400, "INVALID_ARGUMENT", "NAME_INVALID");
        var scripts = new HashSet<int>();
        foreach (var rune in name.EnumerateRunes())
        {
            if (rune.Value is 39 or 45) continue;
            int script = rune.Value switch
            {
                <= 0x024F => 1, // Latin
                >= 0x0370 and <= 0x03FF => 2, // Greek
                >= 0x0400 and <= 0x052F => 3, // Cyrillic
                >= 0x3040 and <= 0x30FF => 4, // Kana
                >= 0x3400 and <= 0x9FFF => 5, // Han
                >= 0xAC00 and <= 0xD7AF => 6, // Hangul
                _ => 0
            };
            if (script == 0) throw new ApiError(400, "INVALID_ARGUMENT", "NAME_INVALID");
            scripts.Add(script);
        }
        if (scripts.Count != 1) throw new ApiError(400, "INVALID_ARGUMENT", "NAME_INVALID");
        var rawKey = name.ToLowerInvariant();
        var key = ConfusableSkeleton.For(name);
        if (Denied.Any(prefix => rawKey.StartsWith(prefix, StringComparison.Ordinal) || key.StartsWith(ConfusableSkeleton.For(prefix), StringComparison.Ordinal)) ||
            reserved.Any(r => string.Equals(key, ConfusableSkeleton.For(r), StringComparison.Ordinal)))
            throw new ApiError(400, "INVALID_ARGUMENT", "NAME_INVALID");
        return (name, key);
    }
}

public static class ConfusableSkeleton
{
    // Unicode UTS #39 18.0.0 mapping, pinned as an embedded resource.
    private static readonly Lazy<IReadOnlyDictionary<int, string>> Map = new(Load);

    public static string For(string value)
    {
        var folded = value.Normalize(NormalizationForm.FormKC).ToLowerInvariant()
            .Replace("ß", "ss").Replace('ς', 'σ').Normalize(NormalizationForm.FormD);
        var output = new StringBuilder();
        foreach (var rune in folded.EnumerateRunes())
            output.Append(Map.Value.TryGetValue(rune.Value, out var target) ? target : rune.ToString());
        return output.ToString().Normalize(NormalizationForm.FormD).ToLowerInvariant();
    }

    private static IReadOnlyDictionary<int, string> Load()
    {
        var result = new Dictionary<int, string>();
        using var stream = typeof(ConfusableSkeleton).Assembly.GetManifestResourceStream("IdentityGateway.confusables.txt")
            ?? throw new InvalidDataException("Missing pinned Unicode confusables data.");
        using var reader = new StreamReader(stream);
        while (reader.ReadLine() is { } line)
        {
            var data = line.Split('#', 2)[0].Split(';');
            if (data.Length < 2) continue;
            var source = data[0].Trim();
            if (!int.TryParse(source, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var codePoint)) continue;
            var target = new StringBuilder();
            foreach (var hex in data[1].Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                target.Append(new Rune(int.Parse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture)).ToString());
            result[codePoint] = target.ToString();
        }
        if (result.Count < 1000) throw new InvalidDataException("Incomplete Unicode confusables data.");
        return result;
    }
}

public static class Passwords
{
    private const int Iterations = 210_000;
    public static string Hash(string password)
    {
        var salt = RandomNumberGenerator.GetBytes(16);
        var key = Rfc2898DeriveBytes.Pbkdf2(password, salt, Iterations, HashAlgorithmName.SHA256, 32);
        return $"pbkdf2-sha256${Iterations}${Convert.ToBase64String(salt)}${Convert.ToBase64String(key)}";
    }
    public static bool Verify(string password, string stored)
    {
        var parts = stored.Split('$');
        if (parts.Length != 4 || parts[0] != "pbkdf2-sha256" || !int.TryParse(parts[1], out var iterations) || iterations is < 100_000 or > 1_000_000)
            return false;
        try
        {
            var expected = Convert.FromBase64String(parts[3]);
            var actual = Rfc2898DeriveBytes.Pbkdf2(password, Convert.FromBase64String(parts[2]), iterations, HashAlgorithmName.SHA256, expected.Length);
            return CryptographicOperations.FixedTimeEquals(expected, actual);
        }
        catch (FormatException) { return false; }
    }
}

public sealed class TicketVerifier(byte[] signingKey, IIdentityStore store, TimeProvider clock)
{
    public string Issue(TicketClaims claims)
    {
        var payload = Convert.ToBase64String(JsonSerializer.SerializeToUtf8Bytes(claims)).TrimEnd('=').Replace('+', '-').Replace('/', '_');
        var signature = Convert.ToBase64String(HMACSHA256.HashData(signingKey, Encoding.ASCII.GetBytes(payload))).TrimEnd('=').Replace('+', '-').Replace('/', '_');
        return payload + "." + signature;
    }

    public TicketClaims? VerifyAndConsume(string ticket, string expectedServerId)
    {
        var parts = ticket.Split('.');
        if (parts.Length != 2 || parts[0].Length > 2048 || parts[1].Length is < 43 or > 44) return null;
        try
        {
            var expected = HMACSHA256.HashData(signingKey, Encoding.ASCII.GetBytes(parts[0]));
            var supplied = Decode(parts[1]);
            if (!CryptographicOperations.FixedTimeEquals(expected, supplied)) return null;
            var claims = JsonSerializer.Deserialize<TicketClaims>(Decode(parts[0]));
            if (claims is null || claims.ServerId != expectedServerId || claims.ExpiresAtUnixSeconds <= clock.GetUtcNow().ToUnixTimeSeconds() ||
                string.IsNullOrWhiteSpace(claims.CharacterId) || string.IsNullOrWhiteSpace(claims.SessionId) || string.IsNullOrWhiteSpace(claims.Nonce)) return null;
            return store.ConsumeTicket(SHA256.HashData(Encoding.UTF8.GetBytes(claims.Nonce)), DateTimeOffset.FromUnixTimeSeconds(claims.ExpiresAtUnixSeconds)) ? claims : null;
        }
        catch (Exception e) when (e is FormatException or JsonException or ArgumentException) { return null; }
    }

    private static byte[] Decode(string input) => Convert.FromBase64String(input.Replace('-', '+').Replace('_', '/') + new string('=', (4 - input.Length % 4) % 4));
}

public sealed class IdentityService(IIdentityStore store, IdentityOptions options, AppearanceValidator appearance,
    TicketVerifier tickets, TimeProvider clock)
{
    private readonly Dictionary<string, Queue<DateTimeOffset>> _attempts = [];
    private readonly Dictionary<string, LoginChallenge> _challenges = [];
    private readonly object _sync = new();
    private static readonly IReadOnlySet<ushort> NoCosmetics = new HashSet<ushort>();

    public string CreateAccount(string? name, string? password)
    {
        var key = (name ?? "").Trim().Normalize(NormalizationForm.FormKC).ToLowerInvariant();
        if (!Regex.IsMatch(key, "^[a-z][a-z0-9_]{2,31}$") || password is null || password.Length is < 12 or > 256)
            throw new ApiError(400, "INVALID_ARGUMENT", "ACCOUNT_INVALID");
        var id = "acct_" + Guid.NewGuid().ToString("N");
        if (!store.CreateAccount(new Account(id, key, Passwords.Hash(password))))
            throw new ApiError(409, "ALREADY_EXISTS", "ACCOUNT_EXISTS");
        return id;
    }

    public string BeginLogin(string? name, string? password, string ip)
    {
        var key = (name ?? "").Trim().Normalize(NormalizationForm.FormKC).ToLowerInvariant();
        var now = clock.GetUtcNow();
        lock (_sync)
        {
            foreach (var scope in new[] { "ip:" + ip, "account:" + key })
            {
                if (!_attempts.TryGetValue(scope, out var queue)) _attempts[scope] = queue = new();
                while (queue.TryPeek(out var first) && first <= now.AddSeconds(-options.LoginWindowSeconds)) queue.Dequeue();
                if (queue.Count >= options.LoginAttemptsPerWindow)
                    throw new ApiError(429, "RESOURCE_EXHAUSTED", "RATE_LIMITED", options.LoginWindowSeconds);
            }
            _attempts["ip:" + ip].Enqueue(now);
            _attempts["account:" + key].Enqueue(now);
        }
        var account = store.FindAccount(key);
        // Run the KDF for unknown accounts too, so credential errors use one path.
        var passwordMatches = Passwords.Verify(password ?? "", account?.PasswordHash ?? DummyHash);
        if (account is null || !passwordMatches)
            throw new ApiError(401, "UNAUTHENTICATED", "BAD_CREDENTIALS");
        var loginId = Convert.ToBase64String(RandomNumberGenerator.GetBytes(32));
        lock (_sync) _challenges[loginId] = new(account!.Id, now.AddSeconds(options.LoginChallengeSeconds));
        return loginId;
    }

    private static readonly string DummyHash = Passwords.Hash("unused-dummy-password");

    public (string Token, SessionRecord Session) CompleteLogin(string? loginId)
    {
        LoginChallenge? challenge;
        lock (_sync)
        {
            if (loginId is null || !_challenges.Remove(loginId, out challenge) || challenge.ExpiresAt <= clock.GetUtcNow())
                throw new ApiError(401, "UNAUTHENTICATED", "LOGIN_CHALLENGE_INVALID");
        }
        var token = Convert.ToBase64String(RandomNumberGenerator.GetBytes(32));
        var session = new SessionRecord("ses_" + Guid.NewGuid().ToString("N"), challenge!.AccountId,
            clock.GetUtcNow().AddSeconds(options.SessionLifetimeSeconds));
        store.SaveSession(SHA256.HashData(Encoding.UTF8.GetBytes(token)), session);
        return (token, session);
    }

    public SessionRecord RequireSession(string? authorization)
    {
        if (authorization is null || !authorization.StartsWith("Bearer ", StringComparison.Ordinal))
            throw new ApiError(401, "UNAUTHENTICATED", "SESSION_INVALID");
        return store.FindSession(SHA256.HashData(Encoding.UTF8.GetBytes(authorization[7..])), clock.GetUtcNow())
            ?? throw new ApiError(401, "UNAUTHENTICATED", "SESSION_INVALID");
    }

    public void Logout(string? authorization)
    {
        RequireSession(authorization);
        store.EndSession(SHA256.HashData(Encoding.UTF8.GetBytes(authorization![7..])));
    }

    public IReadOnlyList<CharacterRecord> ListCharacters(SessionRecord session, string? realmId) => store.ListCharacters(session.AccountId, realmId);

    public CharacterRecord CreateCharacter(SessionRecord session, string? realmId, string? name, byte[]? packet)
    {
        if (realmId is null || !Regex.IsMatch(realmId, "^[a-z0-9][a-z0-9_-]{1,31}$"))
            throw new ApiError(400, "INVALID_ARGUMENT", "REALM_INVALID");
        var (display, key) = Names.Validate(name, options.ReservedNames);
        if (packet is null || packet.Length == 0 || packet[0] != 2 || !appearance.ValidatePacked(packet, NoCosmetics, NoCosmetics).Valid)
            throw new ApiError(400, "INVALID_ARGUMENT", "APPEARANCE_INVALID");
        var record = new CharacterRecord("chr_" + Guid.NewGuid().ToString("N"), session.AccountId, realmId,
            display, key, packet, "active");
        var outcome = store.CreateCharacter(record, options.CharacterSlotsPerRealm, options.CharacterSlotsPerAccount);
        if (outcome == CreateCharacterOutcome.NameTaken) throw new ApiError(409, "ALREADY_EXISTS", "NAME_TAKEN");
        if (outcome == CreateCharacterOutcome.SlotsFull) throw new ApiError(429, "RESOURCE_EXHAUSTED", "SLOTS_FULL");
        return record;
    }

    public void DeleteCharacter(SessionRecord session, string id)
    {
        var now = clock.GetUtcNow();
        if (!store.DeleteCharacter(id, session.AccountId, now, now.AddDays(options.CharacterRestoreDays + options.NameReuseQuarantineDays)))
            throw new ApiError(404, "NOT_FOUND", "CHARACTER_NOT_FOUND");
    }

    public (string Address, string Ticket, long ExpiresAtUnixSeconds) EnterWorld(SessionRecord session, string id)
    {
        var (ticket, expires) = IssueCharacterTicket(session, id, options.WorldServerId);
        return (options.WorldAddress, ticket, expires);
    }

    public (string Ticket, long ExpiresAtUnixSeconds) IssueContactTicket(SessionRecord session, string id) =>
        IssueCharacterTicket(session, id, options.ChatServerId);
    public (string Ticket, long ExpiresAtUnixSeconds) IssueGuildTicket(SessionRecord session, string id) =>
        IssueCharacterTicket(session, id, options.GuildServerId);

    private (string Ticket, long ExpiresAtUnixSeconds) IssueCharacterTicket(SessionRecord session, string id, string serverId)
    {
        var character = store.FindCharacter(id);
        if (character is null || character.AccountId != session.AccountId || character.State != "active")
            throw new ApiError(404, "NOT_FOUND", "CHARACTER_NOT_FOUND");
        var expires = clock.GetUtcNow().AddSeconds(options.TicketLifetimeSeconds).ToUnixTimeSeconds();
        var claims = new TicketClaims(session.SessionId, session.AccountId, id, character.RealmId,
            serverId, expires, Convert.ToBase64String(RandomNumberGenerator.GetBytes(24)));
        return (tickets.Issue(claims), expires);
    }
}
