using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Security.Cryptography;
using System.Text.Json;
using IdentityGateway;
using ShatteredFrontier.AccountData;
using Npgsql;

public static class MailChecks
{
    public static async Task Run()
    {
        using var identity = new HttpClient { BaseAddress = new Uri(Environment.GetEnvironmentVariable("CHAT_IDENTITY_URL") + "/") };
        using var mail = new HttpClient { BaseAddress = new Uri("http://127.0.0.1:" + Environment.GetEnvironmentVariable("CHAT_WS_PORT") + "/") };
        int checks = 0;
        void Check(bool okay, string name) { if (!okay) throw new Exception("FAIL " + name); checks++; }
        async Task<JsonElement> PostIdentity(string path, object body, string? token = null)
        {
            using var req = new HttpRequestMessage(HttpMethod.Post, path) { Content = JsonContent.Create(body) };
            req.Headers.Add("Idempotency-Key", Guid.NewGuid().ToString("N"));
            if (token is not null) req.Headers.Authorization = new AuthenticationHeaderValue("Bearer", token);
            using var reply = await identity.SendAsync(req);
            reply.EnsureSuccessStatusCode();
            using var json = JsonDocument.Parse(await reply.Content.ReadAsStringAsync());
            return json.RootElement.Clone();
        }
        async Task<(string Token, string Account, string Character)> Account(string name, string character)
        {
            var acct = (await PostIdentity("v1/accounts", new { account_name = name, password = "mail-test-password" })).GetProperty("account_id").GetString()!;
            var login = (await PostIdentity("v1/login/begin", new { account_name = name, password = "mail-test-password" })).GetProperty("login_id").GetString()!;
            var token = (await PostIdentity("v1/login/complete", new { login_id = login })).GetProperty("session_token").GetString()!;
            var packet = new byte[29]; packet[0] = 2; packet[3] = 1; packet[4] = 128; packet[5] = 128; packet[6] = 8;
            var chr = (await PostIdentity("v1/characters", new { realm_id = "greenlands", name = character, appearance_v2_base64 = Convert.ToBase64String(packet) }, token)).GetProperty("character_id").GetString()!;
            return (token, acct, chr);
        }
        async Task<string> Ticket((string Token, string Account, string Character) actor) =>
            (await PostIdentity($"v1/characters/{actor.Character}/contacts-ticket", new { }, actor.Token)).GetProperty("ticket").GetString()!;
        async Task<HttpResponseMessage> Call(HttpMethod method, string path, (string Token, string Account, string Character) actor, object? body = null, string? idempotencyKey = null)
        {
            var req = new HttpRequestMessage(method, path);
            req.Headers.Add("X-Contact-Ticket", await Ticket(actor));
            req.Headers.Add("Idempotency-Key", idempotencyKey ?? Guid.NewGuid().ToString("N"));
            if (body is not null) req.Content = JsonContent.Create(body);
            return await mail.SendAsync(req);
        }
        var alice = await Account("mailalice", "MailAlice");
        var bob = await Account("mailbob", "MailBob");
        var mailKey = Guid.NewGuid().ToString("N");
        using var send = await Call(HttpMethod.Post, "v1/mail", alice, new { recipient_character_id = bob.Character, subject = "Hello", body = "Encrypted text" }, mailKey);
        Check((int)send.StatusCode == 201, "mail delivered");
        var id = (await send.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("mail_id").GetString()!;
        using (var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("CHAT_CONNECTION")))
        {
            db.Open(); using var cmd = new NpgsqlCommand("SELECT body_ct FROM chat.mail_messages WHERE mail_id=$1", db);
            cmd.Parameters.AddWithValue(id);
            var stored = (byte[])cmd.ExecuteScalar()!;
            Check(!System.Text.Encoding.UTF8.GetString(stored).Contains("Encrypted text", StringComparison.Ordinal), "mail body stored as ciphertext");
        }
        using var replay = await Call(HttpMethod.Post, "v1/mail", alice, new { recipient_character_id = bob.Character, subject = "Hello", body = "Encrypted text" }, mailKey);
        Check((await replay.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("mail_id").GetString() == id, "mail send replay returns original");
        using var conflict = await Call(HttpMethod.Post, "v1/mail", alice, new { recipient_character_id = bob.Character, subject = "Changed", body = "Encrypted text" }, mailKey);
        Check((int)conflict.StatusCode == 400, "mail idempotency input conflict");
        using var noTicket = await mail.GetAsync("v1/mail");
        Check((int)noTicket.StatusCode == 401, "mail requires bound actor ticket");
        using var inbox = await Call(HttpMethod.Get, "v1/mail", bob);
        Check(inbox.IsSuccessStatusCode && (await inbox.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("messages")[0].GetProperty("subject").GetString() == "Hello", "inbox decrypts subject");
        using var wrong = await Call(HttpMethod.Get, "v1/mail/" + id, alice);
        Check((int)wrong.StatusCode == 404, "other actor cannot read");
        using var read = await Call(HttpMethod.Get, "v1/mail/" + id, bob);
        Check(read.IsSuccessStatusCode && (await read.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("body").GetString() == "Encrypted text", "recipient reads body");
        var provider = new FileKekProvider("v1", Environment.GetEnvironmentVariable("Identity__KekDevPath"));
        var keys = new PostgresAccountDataKeys(Environment.GetEnvironmentVariable("Identity__Connection")!, provider, 1);
        var before = keys.GetForWrite(bob.Account, "chat", "mail");
        var after = keys.GetForWrite(bob.Account, "chat", "mail");
        Check(after.Version > before.Version, "counter rotates at injected threshold");
        using var reread = await Call(HttpMethod.Get, "v1/mail/" + id, bob);
        Check(reread.IsSuccessStatusCode && (await reread.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("body").GetString() == "Encrypted text", "old version decrypts and refreshes");
        using (var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("CHAT_CONNECTION")))
        {
            db.Open(); using var cmd = new NpgsqlCommand("SELECT body_ct FROM chat.mail_messages WHERE mail_id=$1", db);
            cmd.Parameters.AddWithValue(id);
            Check(AccountDataCrypto.Version((byte[])cmd.ExecuteScalar()!) == after.Version, "read reencrypts with latest DEK");
        }
        keys.ScheduleErasure(alice.Account, false, DateTimeOffset.UtcNow.AddDays(-8));
        Check(ThrowsCrypto(() => keys.GetForRead(alice.Account, 1, "chat", "mail")), "retired key rejects unwrap");
        using var erased = await Call(HttpMethod.Get, "v1/mail/" + id, bob);
        Check((await erased.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("sender").GetString() == "erased", "erased sender label");
        Check(keys.FinalizeErasure(DateTimeOffset.UtcNow) == 1, "seven-day key erasure finalizes");
        Check(ThrowsCrypto(() => keys.GetForRead(alice.Account, 1, "chat", "mail")), "erased key unavailable");
        using (var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("Identity__Connection")))
        {
            db.Open(); using var cmd = new NpgsqlCommand("SELECT a.data_key_id IS NULL AND NOT EXISTS(SELECT 1 FROM auth.account_data_keys k WHERE k.account_id=a.account_id) AND EXISTS(SELECT 1 FROM auth.erasure_tombstones t WHERE t.account_id=a.account_id) FROM identity.accounts a WHERE a.account_id=$1", db);
            cmd.Parameters.AddWithValue(alice.Account);
            Check((bool)cmd.ExecuteScalar()!, "erasure clears pointer and persists tombstone");
        }
        var oldKek = File.ReadAllBytes(Environment.GetEnvironmentVariable("Identity__KekDevPath")!);
        var newKek = RandomNumberGenerator.GetBytes(32);
        var rotating = new PostgresAccountDataKeys(Environment.GetEnvironmentVariable("Identity__Connection")!,
            new TestKekProvider("v2", new Dictionary<string, byte[]> { ["v1"] = oldKek, ["v2"] = newKek }));
        Check(rotating.Rewrap("v1") >= 2, "KEK rewrap changes stored wrappers");
        var newOnly = new PostgresAccountDataKeys(Environment.GetEnvironmentVariable("Identity__Connection")!,
            new TestKekProvider("v2", new Dictionary<string, byte[]> { ["v2"] = newKek }));
        Check(newOnly.GetForRead(bob.Account, after.Version, "chat", "mail").Key.Length == 32, "old KEK can be removed after rewrap");
        Check(newOnly.MarkCompromised(bob.Account), "compromise marks current DEK for rotation");
        Check(newOnly.GetForWrite(bob.Account, "chat", "mail").Version > after.Version, "compromised DEK rotates on next write");
        using var deleted = await Call(HttpMethod.Delete, "v1/mail/" + id, bob);
        Check((int)deleted.StatusCode == 204, "recipient deletes mail");
        Console.WriteLine($"PASS mail: {checks} checks, 0 failed");
    }

    private sealed class TestKekProvider(string id, IReadOnlyDictionary<string, byte[]> keys) : IKekProvider
    {
        public string KeyId => id;
        public byte[] GetKey(string keyId) => keys[keyId].ToArray();
    }

    private static bool ThrowsCrypto(Action action)
    {
        try { action(); return false; }
        catch (CryptographicException) { return true; }
    }
}
