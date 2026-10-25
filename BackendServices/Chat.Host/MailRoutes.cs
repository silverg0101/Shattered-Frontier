using System.Security.Cryptography;
using System.Text;
using ShatteredFrontier.AccountData;
using ShatteredFrontier.Chat.Postgres;

public sealed record SendTextMail(string RecipientCharacterId, string Subject, string Body);

public static class MailRoutes
{
    public static void MapMail(this WebApplication app, int port)
    {
        app.MapPost("/v1/mail", async (SendTextMail request, HttpContext http, ChatActorVerifier verifier,
            PostgresContactsStore contacts, PostgresMailStore mail, MailKeyClient keys) =>
        {
            if (http.Connection.LocalPort != port) return Results.NotFound();
            var sender = await Actor(http, verifier);
            var key = http.Request.Headers["Idempotency-Key"].ToString();
            if (key.Length is < 1 or > 128 || request.Subject is null || request.Body is null ||
                request.RecipientCharacterId is null || !System.Text.RegularExpressions.Regex.IsMatch(request.RecipientCharacterId, "^chr_[a-z0-9_]{1,76}$") ||
                Encoding.UTF8.GetByteCount(request.Subject) is < 1 or > 256 ||
                Encoding.UTF8.GetByteCount(request.Body) is < 1 or > 4096)
                return Results.BadRequest(new { reason = "MAIL_INVALID" });
            var hash = SHA256.HashData(Encoding.UTF8.GetBytes(string.Join('\0', request.RecipientCharacterId, request.Subject, request.Body)));
            try
            {
                if (mail.Prior(sender, key, hash) is string priorId) return Results.Ok(new { mail_id = priorId });
            }
            catch (InvalidOperationException e) when (e.Message == "MAIL_IDEMPOTENCY_CONFLICT")
            { return Results.BadRequest(new { reason = e.Message }); }
            if (sender == request.RecipientCharacterId || contacts.IsBlocked(sender, request.RecipientCharacterId))
                return Results.BadRequest(new { reason = "MAIL_NOT_DELIVERED" });
            var from = await keys.Resolve(sender, http.RequestAborted);
            CharacterAccount to;
            try { to = await keys.Resolve(request.RecipientCharacterId, http.RequestAborted); }
            catch (HttpRequestException e) when (e.StatusCode == System.Net.HttpStatusCode.NotFound)
            { return Results.BadRequest(new { reason = "MAIL_NOT_DELIVERED" }); }
            if (from.RealmId != to.RealmId) return Results.BadRequest(new { reason = "MAIL_NOT_DELIVERED" });
            var mailId = "mal_" + Guid.NewGuid().ToString("N");
            var dataKey = await keys.Key(to.AccountId, true, null, http.RequestAborted, 2);
            var dek = dataKey.Bytes;
            try
            {
                var subject = AccountDataCrypto.Encrypt(Encoding.UTF8.GetBytes(request.Subject), dek, dataKey.KeyVersion,
                    to.AccountId, "chat.mail_messages", "subject_ct", mailId);
                var body = AccountDataCrypto.Encrypt(Encoding.UTF8.GetBytes(request.Body), dek, dataKey.KeyVersion,
                    to.AccountId, "chat.mail_messages", "body_ct", mailId);
                var id = mail.Send(mailId, from.AccountId, sender, to.AccountId, request.RecipientCharacterId,
                    to.RealmId, subject, body, key, hash, DateTimeOffset.UtcNow);
                return Results.Json(new { mail_id = id }, statusCode: 201);
            }
            catch (InvalidOperationException e) when (e.Message is "MAILBOX_FULL" or "MAIL_RATE_LIMITED" or "MAIL_IDEMPOTENCY_CONFLICT")
            { return Results.BadRequest(new { reason = e.Message }); }
            finally { CryptographicOperations.ZeroMemory(dek); }
        });

        app.MapGet("/v1/mail", async (HttpContext http, ChatActorVerifier verifier, PostgresMailStore mail, MailKeyClient keys) =>
        {
            if (http.Connection.LocalPort != port) return Results.NotFound();
            var actor = await Actor(http, verifier);
            var account = await keys.Resolve(actor, http.RequestAborted);
            var result = new List<object>();
            var rows = mail.Inbox(actor);
            var erased = await keys.ErasedMany(rows.Select(row => row.SenderAccountId), http.RequestAborted);
            using var cache = new MailDecryptCache(account.AccountId, keys, http.RequestAborted);
            foreach (var row in rows)
            {
                var subject = await cache.Decrypt(row.Subject, "subject_ct", row.MailId);
                var sender = erased.Contains(row.SenderAccountId) ? "erased" : row.SenderCharacterId;
                result.Add(new { mail_id = row.MailId, sender, subject, state = row.State, sent_at = row.SentAt });
            }
            return Results.Ok(new { messages = result });
        });

        app.MapGet("/v1/mail/{mailId}", async (string mailId, HttpContext http, ChatActorVerifier verifier,
            PostgresMailStore mail, MailKeyClient keys) =>
        {
            if (http.Connection.LocalPort != port) return Results.NotFound();
            var actor = await Actor(http, verifier);
            var row = mail.Find(actor, mailId);
            if (row is null) return Results.NotFound();
            var account = await keys.Resolve(actor, http.RequestAborted);
            using var cache = new MailDecryptCache(account.AccountId, keys, http.RequestAborted);
            var current = await keys.Key(account.AccountId, false, null, http.RequestAborted);
            var currentDek = current.Bytes;
            cache.Add(current.KeyVersion, currentDek);
            var subject = await cache.Decrypt(row.Subject, "subject_ct", mailId);
            var body = await cache.Decrypt(row.Body, "body_ct", mailId);
            if (AccountDataCrypto.Version(row.Body) != current.KeyVersion || AccountDataCrypto.Version(row.Subject) != current.KeyVersion)
            {
                var write = await keys.Key(account.AccountId, true, null, http.RequestAborted, 2);
                var writeDek = write.Bytes;
                try { mail.Reencrypt(actor, mailId,
                    AccountDataCrypto.Encrypt(Encoding.UTF8.GetBytes(subject), writeDek, write.KeyVersion, account.AccountId, "chat.mail_messages", "subject_ct", mailId),
                    AccountDataCrypto.Encrypt(Encoding.UTF8.GetBytes(body), writeDek, write.KeyVersion, account.AccountId, "chat.mail_messages", "body_ct", mailId)); }
                finally { CryptographicOperations.ZeroMemory(writeDek); }
            }
            mail.MarkRead(actor, mailId);
            var sender = (await keys.Erased(row.SenderAccountId, http.RequestAborted)).Erased ? "erased" : row.SenderCharacterId;
            return Results.Ok(new { mail_id = mailId, sender, subject, body, state = "read", sent_at = row.SentAt });
        });

        app.MapDelete("/v1/mail/{mailId}", async (string mailId, HttpContext http, ChatActorVerifier verifier, PostgresMailStore mail) =>
        {
            if (http.Connection.LocalPort != port) return Results.NotFound();
            if (http.Request.Headers["Idempotency-Key"].ToString().Length is < 1 or > 128)
                return Results.BadRequest(new { reason = "IDEMPOTENCY_KEY_REQUIRED" });
            var actor = await Actor(http, verifier);
            return mail.Delete(actor, mailId) ? Results.NoContent() : Results.NotFound();
        });
    }

    private static Task<string> Actor(HttpContext http, ChatActorVerifier verifier) =>
        verifier.VerifyTicket(http.Request.Headers["X-Contact-Ticket"].ToString(), http.RequestAborted);

    private sealed class MailDecryptCache(string accountId, MailKeyClient keys, CancellationToken cancellationToken) : IDisposable
    {
        private readonly Dictionary<ushort, byte[]> cache = [];
        public void Add(ushort version, byte[] key) => cache.Add(version, key);
        public async Task<string> Decrypt(byte[] envelope, string column, string id)
        {
            var version = AccountDataCrypto.Version(envelope);
            if (!cache.TryGetValue(version, out var dek))
            {
                dek = (await keys.Key(accountId, false, version, cancellationToken)).Bytes;
                cache.Add(version, dek);
            }
            return Encoding.UTF8.GetString(AccountDataCrypto.Decrypt(envelope, dek, version, accountId, "chat.mail_messages", column, id));
        }
        public void Dispose() { foreach (var key in cache.Values) CryptographicOperations.ZeroMemory(key); }
    }
}
