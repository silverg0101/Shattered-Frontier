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
        var charlie = await Account("mailcharlie", "MailCharlie");
        using (var publicInternal = new HttpRequestMessage(HttpMethod.Post, "v1/internal/data-keys/" + bob.Account)
        { Content = JsonContent.Create(new { service = "chat", purpose = "mail", for_write = false, key_version = 1 }) })
        {
            publicInternal.Headers.Add("X-Data-Key-Chat-Key", Environment.GetEnvironmentVariable("Identity__DataKeyChatKey"));
            using var response = await identity.SendAsync(publicInternal);
            Check(response.StatusCode == System.Net.HttpStatusCode.NotFound, "internal route absent on public listener");
        }
        async Task<bool> InternalAccepted(string? certPath)
        {
            using var handler = new HttpClientHandler { ServerCertificateCustomValidationCallback = (_, cert, _, _) =>
                cert is not null && Convert.ToHexString(SHA256.HashData(cert.RawData)) == Environment.GetEnvironmentVariable("CHAT_IDENTITY_SERVER_CERT_SHA256") };
            if (certPath is not null) handler.ClientCertificates.Add(System.Security.Cryptography.X509Certificates.X509CertificateLoader.LoadPkcs12FromFile(certPath, null));
            using var client = new HttpClient(handler) { BaseAddress = new Uri(Environment.GetEnvironmentVariable("CHAT_IDENTITY_INTERNAL_URL") + "/") };
            using var request = new HttpRequestMessage(HttpMethod.Post, "v1/internal/data-keys/" + bob.Account)
            { Content = JsonContent.Create(new { service = "chat", purpose = "mail", for_write = false, key_version = 1 }) };
            request.Headers.Add("X-Data-Key-Chat-Key", Environment.GetEnvironmentVariable("Identity__DataKeyChatKey"));
            try { using var reply = await client.SendAsync(request); return reply.IsSuccessStatusCode; }
            catch (HttpRequestException) { return false; }
        }
        Check(!await InternalAccepted(null), "internal listener rejects no certificate");
        Check(!await InternalAccepted(Environment.GetEnvironmentVariable("CHAT_IDENTITY_WRONG_CERT_PATH")), "internal listener rejects wrong certificate");
        Check(await InternalAccepted(Environment.GetEnvironmentVariable("CHAT_IDENTITY_CLIENT_CERT_PATH")), "internal listener accepts pinned certificate");
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
        using var secondSend = await Call(HttpMethod.Post, "v1/mail", charlie,
            new { recipient_character_id = bob.Character, subject = "Second", body = "Another encrypted text" });
        Check(secondSend.StatusCode == System.Net.HttpStatusCode.Created, "second sender mail delivered");
        var secondId = (await secondSend.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("mail_id").GetString()!;
        long AuditCount()
        {
            using var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("Identity__Connection")); db.Open();
            using var cmd = new NpgsqlCommand("SELECT count(*) FROM auth.data_key_unwraps WHERE account_id=$1", db);
            cmd.Parameters.AddWithValue(bob.Account); return (long)cmd.ExecuteScalar()!;
        }
        var auditBeforeInbox = AuditCount();
        using var inbox = await Call(HttpMethod.Get, "v1/mail", bob);
        var inboxRows = (await inbox.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("messages");
        Check(inbox.IsSuccessStatusCode && inboxRows.GetArrayLength() == 2 && inboxRows.EnumerateArray().Any(m => m.GetProperty("subject").GetString() == "Hello"), "inbox decrypts both subjects");
        Check(AuditCount() - auditBeforeInbox == 1, "inbox fetches one key per version");
        using var wrong = await Call(HttpMethod.Get, "v1/mail/" + id, alice);
        Check((int)wrong.StatusCode == 404, "other actor cannot read");
        using var read = await Call(HttpMethod.Get, "v1/mail/" + id, bob);
        Check(read.IsSuccessStatusCode && (await read.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("body").GetString() == "Encrypted text", "recipient reads body");
        var provider = new FileKekProvider("v1", Environment.GetEnvironmentVariable("Identity__KekDevPath"));
        var keys = new PostgresAccountDataKeys(Environment.GetEnvironmentVariable("Identity__Connection")!, provider, 7);
        long Counter(ushort version)
        {
            using var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("Identity__Connection")); db.Open();
            using var cmd = new NpgsqlCommand("SELECT messages_written FROM auth.account_data_keys WHERE account_id=$1 AND key_version=$2", db);
            cmd.Parameters.AddWithValue(bob.Account); cmd.Parameters.AddWithValue((int)version);
            return (long)cmd.ExecuteScalar()!;
        }
        Check(Counter(1) == 4, "each mail send counts two encryptions");
        var before = keys.GetForWrite(bob.Account, "chat", "mail", 2);
        Check(Counter(before.Version) == 6, "write reservation increments by two");
        var after = keys.GetForWrite(bob.Account, "chat", "mail", 2);
        Check(after.Version > before.Version && Counter(after.Version) == 2, "threshold rotates before two encryptions");
        using var reread = await Call(HttpMethod.Get, "v1/mail/" + id, bob);
        Check(reread.IsSuccessStatusCode && (await reread.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("body").GetString() == "Encrypted text", "old version decrypts and refreshes");
        Check(Counter(after.Version) == 4, "read re-encryption reserves two encryptions");
        using (var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("CHAT_CONNECTION")))
        {
            db.Open(); using var cmd = new NpgsqlCommand("SELECT body_ct FROM chat.mail_messages WHERE mail_id=$1", db);
            cmd.Parameters.AddWithValue(id);
            Check(AccountDataCrypto.Version((byte[])cmd.ExecuteScalar()!) == after.Version, "read reencrypts with latest DEK");
        }
        using (var handler = new HttpClientHandler { ServerCertificateCustomValidationCallback = (_, cert, _, _) =>
            cert is not null && Convert.ToHexString(SHA256.HashData(cert.RawData)) == Environment.GetEnvironmentVariable("CHAT_IDENTITY_SERVER_CERT_SHA256") })
        {
            handler.ClientCertificates.Add(System.Security.Cryptography.X509Certificates.X509CertificateLoader.LoadPkcs12FromFile(
                Environment.GetEnvironmentVariable("CHAT_IDENTITY_PRIVACY_CERT_PATH")!, null));
            using var client = new HttpClient(handler) { BaseAddress = new Uri(Environment.GetEnvironmentVariable("CHAT_IDENTITY_INTERNAL_URL") + "/") };
            async Task<HttpResponseMessage> Handoff(string credential)
            {
                using var request = new HttpRequestMessage(HttpMethod.Post, $"v1/internal/privacy/accounts/{alice.Account}/erasure")
                { Content = JsonContent.Create(new { request_id = "prq_test", verified_at = DateTimeOffset.UtcNow.AddDays(-15), legal_hold_checked_at = DateTimeOffset.UtcNow, audit_hold = true }) };
                request.Headers.Add("X-Privacy-Workflow-Key", credential);
                return await client.SendAsync(request);
            }
            using var denied = await Handoff("wrong");
            Check(denied.StatusCode == System.Net.HttpStatusCode.Unauthorized, "erasure handoff requires coordinator credential");
            using var accepted = await Handoff(Environment.GetEnvironmentVariable("Identity__PrivacyWorkflowKey")!);
            Check(accepted.StatusCode == System.Net.HttpStatusCode.Accepted, "verified erasure handoff schedules audit hold");
        }
        Check(!keys.IsErased(alice.Account), "pending audit hold is not erased");
        Check(keys.FinalizeErasure(DateTimeOffset.UtcNow) == 0, "audit hold blocks finalization");
        keys.ScheduleErasure(alice.Account, false, DateTimeOffset.UtcNow.AddDays(-8));
        Check(ThrowsCrypto(() => keys.GetForRead(alice.Account, 1, "chat", "mail")), "retired key rejects unwrap");
        using var erased = await Call(HttpMethod.Get, "v1/mail/" + id, bob);
        Check((await erased.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("sender").GetString() == alice.Character, "pending erasure keeps sender label");
        Check(keys.FinalizeErasure(DateTimeOffset.UtcNow) == 1, "seven-day key erasure finalizes");
        using var finalized = await Call(HttpMethod.Get, "v1/mail/" + id, bob);
        Check((await finalized.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("sender").GetString() == "erased", "finalized sender label");
        using var labeledInbox = await Call(HttpMethod.Get, "v1/mail", bob);
        var labels = (await labeledInbox.Content.ReadFromJsonAsync<JsonElement>()).GetProperty("messages").EnumerateArray().ToArray();
        Check(labels.Any(m => m.GetProperty("sender").GetString() == "erased") && labels.Any(m => m.GetProperty("sender").GetString() == charlie.Character),
            "batch sender check distinguishes erased and active accounts");
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
        Check(newOnly.GetForWrite(bob.Account, "chat", "mail", 2).Version > after.Version, "compromised DEK rotates on next write");
        using var deleted = await Call(HttpMethod.Delete, "v1/mail/" + id, bob);
        Check((int)deleted.StatusCode == 204, "recipient deletes mail");
        using (var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("Identity__Connection")))
        {
            db.Open();
            using var permissions = new NpgsqlCommand("SELECT has_table_privilege(current_user,'auth.data_key_unwraps','DELETE') OR has_table_privilege(current_user,'auth.erasure_tombstones','UPDATE') OR has_table_privilege(current_user,'auth.erasure_tombstones','DELETE')", db);
            Check(!(bool)permissions.ExecuteScalar()!, "runtime cannot mutate audit or tombstones");
            using var old = new NpgsqlCommand("INSERT INTO auth.data_key_unwraps(account_id,service,purpose,at) VALUES ($1,'test','retention',$2)", db);
            old.Parameters.AddWithValue(bob.Account); old.Parameters.AddWithValue(DateTimeOffset.UtcNow.AddDays(-91)); old.ExecuteNonQuery();
        }
        var maintenance = new PostgresAccountDataKeys(Environment.GetEnvironmentVariable("CHAT_MAINTENANCE_CONNECTION")!, provider);
        Check(maintenance.PurgeUnwrapAudit(DateTimeOffset.UtcNow) == 1, "owner maintenance purges 90-day audit rows");
        using (var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("CHAT_CONNECTION")))
        {
            db.Open();
            using var expire = new NpgsqlCommand("UPDATE chat.mail_messages SET expires_at=$2 WHERE mail_id=$1", db);
            expire.Parameters.AddWithValue(secondId); expire.Parameters.AddWithValue(DateTimeOffset.UtcNow.AddMinutes(-1)); expire.ExecuteNonQuery();
        }
        Check(new ShatteredFrontier.Chat.Postgres.PostgresMailStore(Environment.GetEnvironmentVariable("CHAT_CONNECTION")!).PurgeExpired(DateTimeOffset.UtcNow) == 1,
            "expired mail ciphertext purged");
        using (var db = new NpgsqlConnection(Environment.GetEnvironmentVariable("CHAT_CONNECTION")))
        {
            db.Open(); using var cmd = new NpgsqlCommand("SELECT octet_length(subject_ct)=0 AND octet_length(body_ct)=0 FROM chat.mail_messages WHERE mail_id=$1", db);
            cmd.Parameters.AddWithValue(secondId);
            Check((bool)cmd.ExecuteScalar()!, "expired ciphertext is empty");
        }
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
