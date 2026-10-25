using CharacterAppearance;
using IdentityGateway;
using Microsoft.AspNetCore.Server.Kestrel.Https;

var builder = WebApplication.CreateBuilder(args);
builder.WebHost.UseUrls(builder.Configuration["Identity:ListenUrl"] ?? "http://127.0.0.1:5084");
builder.WebHost.ConfigureKestrel(server => server.Limits.MaxRequestBodySize = 64 * 1024);
builder.WebHost.ConfigureKestrel(server => server.ConfigureHttpsDefaults(https => https.ClientCertificateMode = ClientCertificateMode.AllowCertificate));
builder.Services.ConfigureHttpJsonOptions(o =>
{
    o.SerializerOptions.PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.SnakeCaseLower;
    o.SerializerOptions.DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull;
});
var options = builder.Configuration.GetSection("Identity").Get<IdentityOptions>() ?? new IdentityOptions();
options.Validate();
var connection = builder.Configuration["Identity:Connection"] ?? throw new InvalidOperationException("Identity:Connection is required.");
var encodedKey = builder.Configuration["Identity:TicketSigningKey"] ?? throw new InvalidOperationException("Identity:TicketSigningKey is required.");
var signingKey = Convert.FromBase64String(encodedKey);
if (signingKey.Length < 32) throw new InvalidOperationException("Ticket signing key must be at least 32 bytes.");
var verifierKey = builder.Configuration["Identity:WorldVerifierKey"] ?? throw new InvalidOperationException("Identity:WorldVerifierKey is required.");
if (verifierKey.Length < 32) throw new InvalidOperationException("World verifier key must be at least 32 characters.");
var chatVerifierKey = builder.Configuration["Identity:ChatVerifierKey"];
if (chatVerifierKey is not null && chatVerifierKey.Length < 32) throw new InvalidOperationException("Chat verifier key must be at least 32 characters.");
var guildVerifierKey = builder.Configuration["Identity:GuildVerifierKey"];
if (guildVerifierKey is not null && guildVerifierKey.Length < 32) throw new InvalidOperationException("Guild verifier key must be at least 32 characters.");
var dataKeyChatKey = builder.Configuration["Identity:DataKeyChatKey"];
if (dataKeyChatKey is not null && dataKeyChatKey.Length < 32) throw new InvalidOperationException("Data key chat credential must be at least 32 characters.");
var dataKeyChatThumbprint = builder.Configuration["Identity:DataKeyChatClientThumbprint"];
var kek = new FileKekProvider(builder.Configuration["Identity:KekId"] ?? "v1", builder.Configuration["Identity:KekDevPath"]);
var startupKek = kek.GetKey(kek.KeyId);
System.Security.Cryptography.CryptographicOperations.ZeroMemory(startupKek);
var dataKeys = new PostgresAccountDataKeys(connection, kek);
if (args.Length > 0)
{
    if (args.Length == 2 && args[0] == "--rewrap-kek") Console.WriteLine($"rewrapped={dataKeys.Rewrap(args[1])}");
    else if (args.Length == 2 && args[0] == "--rotate-data-key") Console.WriteLine($"marked={dataKeys.MarkCompromised(args[1])}");
    else if (args.Length == 1 && args[0] == "--finalize-key-erasure") Console.WriteLine($"erased={dataKeys.FinalizeErasure(DateTimeOffset.UtcNow)}");
    else if (args.Length == 2 && args[0] == "--replay-key-tombstones") Console.WriteLine($"replayed={dataKeys.ReplayTombstones(File.ReadLines(args[1]), DateTimeOffset.UtcNow)}");
    else throw new ArgumentException("Unknown key maintenance command.");
    return;
}
var repoAllowlist = Path.Combine(builder.Environment.ContentRootPath, "content/appearance/human.json");
var allowlistPath = builder.Configuration["Identity:AppearanceAllowlist"] ??
    (File.Exists(repoAllowlist) ? repoAllowlist : Path.GetFullPath(Path.Combine(builder.Environment.ContentRootPath, "../../content/appearance/human.json")));
builder.Services.AddSingleton(options);
builder.Services.AddSingleton<TimeProvider>(TimeProvider.System);
builder.Services.AddSingleton(dataKeys);
builder.Services.AddSingleton<IIdentityStore>(new PostgresIdentityStore(connection, dataKeys));
builder.Services.AddSingleton(new AppearanceValidator(AppearanceAllowlist.Load(allowlistPath)));
builder.Services.AddSingleton(s => new TicketVerifier(signingKey, s.GetRequiredService<IIdentityStore>(), s.GetRequiredService<TimeProvider>()));
builder.Services.AddSingleton<IdentityService>();
var app = builder.Build();

app.Use(async (context, next) =>
{
    try { await next(); }
    catch (ApiError e)
    {
        context.Response.StatusCode = e.Status;
        if (e.RetryAfterSeconds is int seconds) context.Response.Headers.RetryAfter = seconds.ToString();
        await context.Response.WriteAsJsonAsync(new ErrorBody(e.Code, e.Reason, e.RetryAfterSeconds));
    }
    catch (System.Security.Cryptography.CryptographicException)
    {
        context.Response.StatusCode = 410;
        await context.Response.WriteAsJsonAsync(new ErrorBody("NOT_FOUND", "DATA_KEY_UNAVAILABLE"));
    }
});

app.MapPost("/v1/accounts", (AccountCreateRequest request, HttpContext http, IdentityService service) =>
{
    RequireKey(http);
    return Results.Json(new { account_id = service.CreateAccount(request.AccountName, request.Password) }, statusCode: 201);
});
app.MapPost("/v1/login/begin", (LoginBeginRequest request, HttpContext http, IdentityService service) =>
{
    RequireKey(http);
    return Results.Ok(new { login_id = service.BeginLogin(request.AccountName, request.Password, http.Connection.RemoteIpAddress?.ToString() ?? "unknown") });
});
app.MapPost("/v1/login/complete", (LoginCompleteRequest request, HttpContext http, IdentityService service) =>
{
    RequireKey(http);
    var (token, session) = service.CompleteLogin(request.LoginId);
    return Results.Ok(new { session_id = session.SessionId, session_token = token, expires_at = session.ExpiresAt });
});
app.MapPost("/v1/sessions/logout", (HttpContext http, IdentityService service) =>
{
    RequireKey(http);
    service.Logout(http.Request.Headers.Authorization);
    return Results.NoContent();
});
app.MapGet("/v1/characters", (string? realm_id, HttpContext http, IdentityService service) =>
    Results.Ok(new { characters = service.ListCharacters(service.RequireSession(http.Request.Headers.Authorization), realm_id).Select(ToResponse) }));
app.MapPost("/v1/characters", (CharacterCreateRequest request, HttpContext http, IdentityService service) =>
{
    RequireKey(http);
    byte[] packet;
    try { packet = Convert.FromBase64String(request.AppearanceV2Base64 ?? ""); }
    catch (FormatException) { throw new ApiError(400, "INVALID_ARGUMENT", "APPEARANCE_INVALID"); }
    var character = service.CreateCharacter(service.RequireSession(http.Request.Headers.Authorization), request.RealmId, request.Name, packet);
    return Results.Json(ToResponse(character), statusCode: 201);
});
app.MapGet("/v1/characters/{characterId}", (string characterId, HttpContext http, IdentityService service) =>
{
    var session = service.RequireSession(http.Request.Headers.Authorization);
    var character = service.ListCharacters(session, null).FirstOrDefault(c => c.CharacterId == characterId);
    return character is null ? Results.NotFound(new ErrorBody("NOT_FOUND", "CHARACTER_NOT_FOUND")) : Results.Ok(ToResponse(character));
});
app.MapDelete("/v1/characters/{characterId}", (string characterId, HttpContext http, IdentityService service) =>
{
    RequireKey(http);
    service.DeleteCharacter(service.RequireSession(http.Request.Headers.Authorization), characterId);
    return Results.NoContent();
});
app.MapPost("/v1/characters/{characterId}/enter-world", (string characterId, HttpContext http, IdentityService service) =>
{
    RequireKey(http);
    var (address, ticket, expires) = service.EnterWorld(service.RequireSession(http.Request.Headers.Authorization), characterId);
    return Results.Ok(new { address, ticket, expires_at_unix_s = expires });
});
app.MapPost("/v1/world/verify-ticket", (VerifyTicketRequest request, HttpContext http, TicketVerifier verifier) =>
{
    var supplied = System.Text.Encoding.UTF8.GetBytes(http.Request.Headers["X-World-Verifier-Key"].ToString());
    var expected = System.Text.Encoding.UTF8.GetBytes(verifierKey);
    if (supplied.Length != expected.Length || !System.Security.Cryptography.CryptographicOperations.FixedTimeEquals(supplied, expected))
        return Results.Json(new ErrorBody("UNAUTHENTICATED", "WORLD_VERIFIER_UNAUTHORIZED"), statusCode: 401);
    if (string.IsNullOrWhiteSpace(request.Ticket)) return Results.BadRequest(new ErrorBody("INVALID_ARGUMENT", "TICKET_REQUIRED"));
    if (request.ServerId != options.WorldServerId) return Results.Json(new ErrorBody("UNAUTHENTICATED", "SERVER_MISMATCH"), statusCode: 401);
    var claims = verifier.VerifyAndConsume(request.Ticket, options.WorldServerId);
    return claims is null ? Results.Json(new ErrorBody("UNAUTHENTICATED", "TICKET_INVALID"), statusCode: 401) : Results.Ok(new { character_id = claims.CharacterId, account_id = claims.AccountId, realm_id = claims.RealmId });
});
if (chatVerifierKey is not null)
{
    app.MapPost("/v1/characters/{characterId}/contacts-ticket", (string characterId, HttpContext http, IdentityService service) =>
    {
        var (ticket, expires) = service.IssueContactTicket(service.RequireSession(http.Request.Headers.Authorization), characterId);
        return Results.Ok(new { ticket, expires_at_unix_s = expires });
    });
    app.MapPost("/v1/chat/verify-ticket", (VerifyTicketRequest request, HttpContext http, TicketVerifier verifier) =>
    {
        var supplied = System.Text.Encoding.UTF8.GetBytes(http.Request.Headers["X-Chat-Verifier-Key"].ToString());
        var expected = System.Text.Encoding.UTF8.GetBytes(chatVerifierKey);
        if (supplied.Length != expected.Length || !System.Security.Cryptography.CryptographicOperations.FixedTimeEquals(supplied, expected))
            return Results.Json(new ErrorBody("UNAUTHENTICATED", "CHAT_VERIFIER_UNAUTHORIZED"), statusCode: 401);
        if (string.IsNullOrWhiteSpace(request.Ticket) || request.ServerId != options.ChatServerId)
            return Results.Json(new ErrorBody("UNAUTHENTICATED", "TICKET_INVALID"), statusCode: 401);
        var claims = verifier.VerifyAndConsume(request.Ticket, options.ChatServerId);
        return claims is null ? Results.Json(new ErrorBody("UNAUTHENTICATED", "TICKET_INVALID"), statusCode: 401)
            : Results.Ok(new { character_id = claims.CharacterId, account_id = claims.AccountId, realm_id = claims.RealmId });
    });
}
if (guildVerifierKey is not null)
{
    app.MapPost("/v1/characters/{characterId}/guild-ticket", (string characterId, HttpContext http, IdentityService service) =>
    {
        var (ticket, expires) = service.IssueGuildTicket(service.RequireSession(http.Request.Headers.Authorization), characterId);
        return Results.Ok(new { ticket, expires_at_unix_s = expires });
    });
    app.MapPost("/v1/guild/verify-ticket", (VerifyTicketRequest request, HttpContext http, TicketVerifier verifier) =>
    {
        var supplied = System.Text.Encoding.UTF8.GetBytes(http.Request.Headers["X-Guild-Verifier-Key"].ToString());
        var expected = System.Text.Encoding.UTF8.GetBytes(guildVerifierKey);
        if (supplied.Length != expected.Length || !System.Security.Cryptography.CryptographicOperations.FixedTimeEquals(supplied, expected))
            return Results.Json(new ErrorBody("UNAUTHENTICATED", "GUILD_VERIFIER_UNAUTHORIZED"), statusCode: 401);
        if (string.IsNullOrWhiteSpace(request.Ticket) || request.ServerId != options.GuildServerId)
            return Results.Json(new ErrorBody("UNAUTHENTICATED", "TICKET_INVALID"), statusCode: 401);
        var claims = verifier.VerifyAndConsume(request.Ticket, options.GuildServerId);
        return claims is null ? Results.Json(new ErrorBody("UNAUTHENTICATED", "TICKET_INVALID"), statusCode: 401)
            : Results.Ok(new { character_id = claims.CharacterId, account_id = claims.AccountId, realm_id = claims.RealmId });
    });
}
if (dataKeyChatKey is not null)
{
    app.MapPost("/v1/internal/data-keys/{accountId}", (string accountId, DataKeyRequest request, HttpContext http, PostgresAccountDataKeys keys) =>
    {
        RequireDataKeyClient(http, dataKeyChatKey, dataKeyChatThumbprint);
        if (request.Purpose != "mail" || request.Service != "chat") throw new ApiError(403, "PERMISSION_DENIED", "PURPOSE_DENIED");
        var key = request.ForWrite ? keys.GetForWrite(accountId, "chat", "mail") :
            request.KeyVersion is ushort version ? keys.GetForRead(accountId, version, "chat", "mail") : keys.GetCurrent(accountId, "chat", "mail");
        try { return Results.Ok(new { key_version = key.Version, dek_base64 = Convert.ToBase64String(key.Key) }); }
        finally { System.Security.Cryptography.CryptographicOperations.ZeroMemory(key.Key); }
    });
    app.MapGet("/v1/internal/characters/{characterId}/account", (string characterId, HttpContext http, IIdentityStore store) =>
    {
        RequireDataKeyClient(http, dataKeyChatKey, dataKeyChatThumbprint);
        var character = store.FindCharacter(characterId);
        return character is { State: "active" }
            ? Results.Ok(new { account_id = character.AccountId, realm_id = character.RealmId })
            : Results.NotFound();
    });
    app.MapGet("/v1/internal/accounts/{accountId}/erased", (string accountId, HttpContext http, PostgresAccountDataKeys keys) =>
    {
        RequireDataKeyClient(http, dataKeyChatKey, dataKeyChatThumbprint);
        return Results.Ok(new { erased = keys.IsErased(accountId) });
    });
}
app.Run();

static void RequireDataKeyClient(HttpContext context, string expected, string? allowedThumbprint)
{
    // The token is the client allowlist entry. Non-loopback deployments also require client TLS identity.
    if (context.Connection.RemoteIpAddress is not { } address ||
        (!System.Net.IPAddress.IsLoopback(address) && (context.Connection.ClientCertificate is null ||
            allowedThumbprint is null || !string.Equals(context.Connection.ClientCertificate.Thumbprint,
                allowedThumbprint, StringComparison.OrdinalIgnoreCase))))
        throw new ApiError(401, "UNAUTHENTICATED", "MTLS_REQUIRED");
    var supplied = System.Text.Encoding.UTF8.GetBytes(context.Request.Headers["X-Data-Key-Chat-Key"].ToString());
    var target = System.Text.Encoding.UTF8.GetBytes(expected);
    if (supplied.Length != target.Length || !System.Security.Cryptography.CryptographicOperations.FixedTimeEquals(supplied, target))
        throw new ApiError(401, "UNAUTHENTICATED", "DATA_KEY_CLIENT_DENIED");
}

static void RequireKey(HttpContext context)
{
    var key = context.Request.Headers["Idempotency-Key"].ToString();
    if (key.Length is < 1 or > 128) throw new ApiError(400, "INVALID_ARGUMENT", "IDEMPOTENCY_KEY_REQUIRED");
}

static object ToResponse(CharacterRecord c) => new
{
    character_id = c.CharacterId, realm_id = c.RealmId, name = c.Name, state = c.State,
    appearance_v2_base64 = Convert.ToBase64String(c.Appearance)
};

public sealed record AccountCreateRequest(string? AccountName, string? Password);
public sealed record LoginBeginRequest(string? AccountName, string? Password);
public sealed record LoginCompleteRequest(string? LoginId);
public sealed record CharacterCreateRequest(string? RealmId, string? Name, string? AppearanceV2Base64);
public sealed record VerifyTicketRequest(string? Ticket, string? ServerId);
public sealed record ErrorBody(string Code, string Reason, int? RetryAfterS = null);
public sealed record DataKeyRequest(string Service, string Purpose, bool ForWrite, ushort? KeyVersion);
