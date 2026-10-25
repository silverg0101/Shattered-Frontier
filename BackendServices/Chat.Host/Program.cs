using System.Net;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using ShatteredFrontier.Chat.Postgres;

static string Required(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } value
    ? value : throw new InvalidOperationException($"{name} is required.");

if (!int.TryParse(Required("CHAT_HOST_PORT"), out var port) || port is < 1 or > 65535)
    throw new InvalidOperationException("CHAT_HOST_PORT must be 1-65535.");
var wsPortText = Environment.GetEnvironmentVariable("CHAT_WS_PORT");
int? wsPort = null;
if (!string.IsNullOrWhiteSpace(wsPortText))
{
    if (!int.TryParse(wsPortText, out var value) || value is < 1 or > 65535 || value == port)
        throw new InvalidOperationException("CHAT_WS_PORT must be a different port in 1-65535.");
    wsPort = value;
}
var builder = WebApplication.CreateBuilder(args);
builder.Services.ConfigureHttpJsonOptions(o => o.SerializerOptions.PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.SnakeCaseLower);
builder.WebHost.ConfigureKestrel(o => o.Limits.MaxRequestBodySize = 64 * 1024);
builder.WebHost.ConfigureKestrel(options =>
{
    options.Listen(IPAddress.Loopback, port, listen => listen.Protocols = HttpProtocols.Http2);
    if (wsPort is int value) options.Listen(IPAddress.Loopback, value, listen => listen.Protocols = HttpProtocols.Http1);
});
builder.Services.AddGrpc();
builder.Services.AddSingleton(new PostgresContactsStore(Required("CHAT_CONNECTION")));
builder.Services.AddSingleton(new PostgresMailStore(Required("CHAT_CONNECTION")));
builder.Services.AddSingleton(new ChatServiceToken(Required("SF_CHAT_TOKEN")));
builder.Services.AddSingleton(new ChatPrivacyToken(Required("SF_CHAT_PRIVACY_TOKEN")));
builder.Services.AddSingleton(new ChatActorVerifier(new HttpClient { BaseAddress = new Uri(Required("CHAT_IDENTITY_URL").TrimEnd('/') + "/"), Timeout = TimeSpan.FromSeconds(3) },
    Required("CHAT_IDENTITY_SERVER_ID"), Required("SF_IDENTITY_CHAT_VERIFIER_KEY")));
if (wsPort is not null)
{
    builder.Services.AddSingleton(new MailKeyClient(new HttpClient { BaseAddress = new Uri(Required("CHAT_IDENTITY_URL").TrimEnd('/') + "/"), Timeout = TimeSpan.FromSeconds(3) }, Required("SF_IDENTITY_DATA_KEY_CHAT_KEY")));
    builder.Services.AddSingleton(new GroupMembershipClient(Required("CHAT_GROUP_URL"), Required("SF_GAMEPLAY_TOKEN")));
    builder.Services.AddSingleton(new GuildMembershipClient(new HttpClient {
        BaseAddress = new Uri(Required("CHAT_GUILD_URL").TrimEnd('/') + "/"), Timeout = TimeSpan.FromSeconds(3) }, Required("SF_GUILD_TOKEN")));
    builder.Services.AddSingleton<ChatTransport>();
}
var app = builder.Build();
app.Use(async (context, next) =>
{
    try { await next(); }
    catch (Grpc.Core.RpcException e) when (e.StatusCode == Grpc.Core.StatusCode.Unauthenticated)
    { context.Response.StatusCode = 401; }
    catch (HttpRequestException e) when (e.StatusCode == System.Net.HttpStatusCode.Gone)
    { context.Response.StatusCode = 410; }
    catch (HttpRequestException e) when (e.StatusCode == System.Net.HttpStatusCode.NotFound)
    { context.Response.StatusCode = 404; }
    catch (HttpRequestException)
    { context.Response.StatusCode = 503; }
});
app.UseWebSockets();
if (wsPort is not null) app.Map("/v1/chat", async (HttpContext context, ChatTransport transport) =>
{
    if (wsPort is null || context.Connection.LocalPort != wsPort) { context.Response.StatusCode = 404; return; }
    await transport.Accept(context);
});
if (wsPort is int mailPort) app.MapMail(mailPort);
app.MapGrpcService<ContactsGrpcService>();
app.Lifetime.ApplicationStarted.Register(() => Console.WriteLine($"CHAT_HOST_READY port={port}"));
app.Run();
