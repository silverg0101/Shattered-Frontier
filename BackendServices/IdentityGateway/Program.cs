using CharacterAppearance;
using IdentityGateway;

var builder = WebApplication.CreateBuilder(args);
builder.WebHost.UseUrls(builder.Configuration["Identity:ListenUrl"] ?? "http://127.0.0.1:5084");
builder.WebHost.ConfigureKestrel(server => server.Limits.MaxRequestBodySize = 64 * 1024);
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
var repoAllowlist = Path.Combine(builder.Environment.ContentRootPath, "content/appearance/human.json");
var allowlistPath = builder.Configuration["Identity:AppearanceAllowlist"] ??
    (File.Exists(repoAllowlist) ? repoAllowlist : Path.GetFullPath(Path.Combine(builder.Environment.ContentRootPath, "../../content/appearance/human.json")));
builder.Services.AddSingleton(options);
builder.Services.AddSingleton<TimeProvider>(TimeProvider.System);
builder.Services.AddSingleton<IIdentityStore>(new PostgresIdentityStore(connection));
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
    return claims is null ? Results.Json(new ErrorBody("UNAUTHENTICATED", "TICKET_INVALID"), statusCode: 401) : Results.Ok(new { character_id = claims.CharacterId });
});
app.Run();

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
