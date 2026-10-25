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
            var dataKey = await keys.Key(to.AccountId, true, null, http.RequestAborted);
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
            foreach (var row in mail.Inbox(actor))
            {
                var subject = await Decrypt(row.Subject, account.AccountId, "subject_ct", row.MailId, keys, http.RequestAborted);
                var sender = (await keys.Erased(row.SenderAccountId, http.RequestAborted)).Erased ? "erased" : row.SenderCharacterId;
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
            var subject = await Decrypt(row.Subject, account.AccountId, "subject_ct", mailId, keys, http.RequestAborted);
            var body = await Decrypt(row.Body, account.AccountId, "body_ct", mailId, keys, http.RequestAborted);
            var current = await keys.Key(account.AccountId, false, null, http.RequestAborted);
            var currentDek = current.Bytes;
            try
            {
                if (AccountDataCrypto.Version(row.Body) != current.KeyVersion)
                    mail.Reencrypt(actor, mailId,
                        AccountDataCrypto.Encrypt(Encoding.UTF8.GetBytes(subject), currentDek, current.KeyVersion, account.AccountId, "chat.mail_messages", "subject_ct", mailId),
                        AccountDataCrypto.Encrypt(Encoding.UTF8.GetBytes(body), currentDek, current.KeyVersion, account.AccountId, "chat.mail_messages", "body_ct", mailId));
            }
            finally { CryptographicOperations.ZeroMemory(currentDek); }
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

    private static async Task<string> Decrypt(byte[] envelope, string accountId, string column, string id,
        MailKeyClient keys, CancellationToken cancellationToken)
    {
        var version = AccountDataCrypto.Version(envelope);
        var key = await keys.Key(accountId, false, version, cancellationToken);
        var dek = key.Bytes;
        try { return Encoding.UTF8.GetString(AccountDataCrypto.Decrypt(envelope, dek, version, accountId, "chat.mail_messages", column, id)); }
        finally { CryptographicOperations.ZeroMemory(dek); }
    }
}
