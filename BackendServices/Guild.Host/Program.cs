using System.Net;
using System.Net.Http.Json;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Serialization;
using ShatteredFrontier.Guild.Postgres;

static string Required(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } value
    ? value : throw new InvalidOperationException($"{name} is required.");
if (!int.TryParse(Required("GUILD_HOST_PORT"), out var port) || port is < 1 or > 65535)
    throw new InvalidOperationException("GUILD_HOST_PORT must be 1-65535.");
var builder = WebApplication.CreateBuilder(args);
builder.WebHost.ConfigureKestrel(options => options.Listen(IPAddress.Loopback, port));
builder.Services.AddSingleton(new PostgresGuildStore(Required("GUILD_CONNECTION"), GuildControls.Shipped));
builder.Services.AddSingleton(new GuildActorVerifier(
    new HttpClient { BaseAddress = new Uri(Required("GUILD_IDENTITY_URL").TrimEnd('/') + "/"), Timeout = TimeSpan.FromSeconds(3) },
    Required("GUILD_IDENTITY_SERVER_ID"), Required("SF_IDENTITY_GUILD_VERIFIER_KEY")));
var serviceToken = Required("SF_GUILD_TOKEN");
var app = builder.Build();
app.Use(async (http, next) =>
{
    try { await next(http); }
    catch (GuildRejectedException ex)
    {
        var status = ex.Code switch { "invalid" => 400, "permission" => 403, "not_found" => 404,
            "conflict" => 409, "capacity" => 429, _ => 412 };
        http.Response.StatusCode = status;
        await http.Response.WriteAsJsonAsync(new { code = ex.Code });
    }
    catch (GuildAuthException ex)
    {
        http.Response.StatusCode = ex.Status;
        await http.Response.WriteAsJsonAsync(new { code = ex.Code });
    }
});
async Task<GuildActor> Actor(HttpContext http, GuildActorVerifier verifier)
{
    var supplied = Encoding.UTF8.GetBytes(http.Request.Headers.Authorization.ToString());
    var expected = Encoding.UTF8.GetBytes("Bearer " + serviceToken);
    if (supplied.Length != expected.Length || !CryptographicOperations.FixedTimeEquals(supplied, expected))
        throw new GuildAuthException(401, "SERVICE_TOKEN_REQUIRED");
    return await verifier.Verify(http.Request.Headers["X-Guild-Ticket"].ToString(), http.RequestAborted);
}
app.MapPost("/v1/guilds", async (CreateGuildRequest request, HttpContext http, GuildActorVerifier verifier, PostgresGuildStore store) =>
{
    var actor = await Actor(http, verifier);
    return Results.Ok(store.Create(request.IdempotencyKey ?? "", actor.CharacterId, actor.RealmId, request.Name ?? "", request.Tag ?? ""));
});
app.MapPost("/v1/guilds/{guildId}/invitations", async (string guildId, InviteGuildRequest request, HttpContext http, GuildActorVerifier verifier, PostgresGuildStore store) =>
{
    var actor = await Actor(http, verifier);
    return Results.Ok(new { invited = store.Invite(request.IdempotencyKey ?? "", guildId, actor.CharacterId, request.RecipientCharacterId ?? "") });
});
app.MapPost("/v1/guilds/{guildId}/accept", async (string guildId, AcceptGuildRequest request, HttpContext http, GuildActorVerifier verifier, PostgresGuildStore store) =>
{
    var actor = await Actor(http, verifier);
    return Results.Ok(store.Accept(request.IdempotencyKey ?? "", guildId, actor.CharacterId, actor.RealmId));
});
app.MapGet("/v1/guilds/{guildId}/roster", async (string guildId, HttpContext http, GuildActorVerifier verifier, PostgresGuildStore store) =>
{
    var actor = await Actor(http, verifier);
    return Results.Ok(store.Roster(guildId, actor.CharacterId));
});
app.Lifetime.ApplicationStarted.Register(() => Console.WriteLine($"GUILD_HOST_READY port={port}"));
app.Run();

public sealed record CreateGuildRequest(string? IdempotencyKey, string? Name, string? Tag);
public sealed record InviteGuildRequest(string? IdempotencyKey, string? RecipientCharacterId);
public sealed record AcceptGuildRequest(string? IdempotencyKey);
public sealed record GuildActor(string CharacterId, string AccountId, string RealmId);
public sealed class GuildAuthException(int status, string code) : Exception(code)
{
    public int Status { get; } = status;
    public string Code { get; } = code;
}
public sealed class GuildActorVerifier(HttpClient http, string serverId, string verifierKey)
{
    public async Task<GuildActor> Verify(string? ticket, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(ticket)) throw new GuildAuthException(401, "TICKET_REQUIRED");
        using var request = new HttpRequestMessage(HttpMethod.Post, "v1/guild/verify-ticket")
        { Content = JsonContent.Create(new { ticket, server_id = serverId }) };
        request.Headers.Add("X-Guild-Verifier-Key", verifierKey);
        HttpResponseMessage response;
        try { response = await http.SendAsync(request, cancellationToken); }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException)
        { throw new GuildAuthException(503, "IDENTITY_UNAVAILABLE"); }
        using (response)
        {
            if ((int)response.StatusCode >= 500) throw new GuildAuthException(503, "IDENTITY_UNAVAILABLE");
            if (!response.IsSuccessStatusCode) throw new GuildAuthException(401, "TICKET_INVALID");
            TicketClaims? claims;
            try { claims = await response.Content.ReadFromJsonAsync<TicketClaims>(cancellationToken: cancellationToken); }
            catch (System.Text.Json.JsonException) { throw new GuildAuthException(401, "TICKET_INVALID"); }
            if (string.IsNullOrWhiteSpace(claims?.CharacterId) || string.IsNullOrWhiteSpace(claims.AccountId) || string.IsNullOrWhiteSpace(claims.RealmId))
                throw new GuildAuthException(401, "TICKET_INVALID");
            return new(claims.CharacterId, claims.AccountId, claims.RealmId);
        }
    }
    private sealed record TicketClaims([property: JsonPropertyName("character_id")] string CharacterId,
        [property: JsonPropertyName("account_id")] string AccountId,
        [property: JsonPropertyName("realm_id")] string RealmId);
}
