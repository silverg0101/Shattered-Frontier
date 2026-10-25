using System.Net;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using ShatteredFrontier.Chat.Postgres;

static string Required(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } value
    ? value : throw new InvalidOperationException($"{name} is required.");
static string Credential(string name, string file)
{
    var directory = Environment.GetEnvironmentVariable("CREDENTIALS_DIRECTORY");
    return directory is null ? Required(name) : File.ReadAllText(Path.Combine(directory, file)).TrimEnd('\r', '\n');
}

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
var bindAddress = IPAddress.Parse(Environment.GetEnvironmentVariable("CHAT_BIND_ADDRESS") ?? "127.0.0.1");
builder.Services.ConfigureHttpJsonOptions(o => o.SerializerOptions.PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.SnakeCaseLower);
builder.WebHost.ConfigureKestrel(o => o.Limits.MaxRequestBodySize = 64 * 1024);
builder.WebHost.ConfigureKestrel(options =>
{
    options.Listen(bindAddress, port, listen => listen.Protocols = HttpProtocols.Http2);
    if (wsPort is int value) options.Listen(bindAddress, value, listen => listen.Protocols = HttpProtocols.Http1);
});
builder.Services.AddGrpc();
builder.Services.AddSingleton(new PostgresContactsStore(Credential("CHAT_CONNECTION", "chat-connection")));
builder.Services.AddSingleton(new PostgresMailStore(Credential("CHAT_CONNECTION", "chat-connection")));
builder.Services.AddSingleton(new ChatServiceToken(Credential("SF_CHAT_TOKEN", "chat-token")));
builder.Services.AddSingleton(new ChatPrivacyToken(Credential("SF_CHAT_PRIVACY_TOKEN", "chat-privacy-token")));
builder.Services.AddSingleton(new ChatActorVerifier(new HttpClient { BaseAddress = new Uri(Required("CHAT_IDENTITY_URL").TrimEnd('/') + "/"), Timeout = TimeSpan.FromSeconds(3) },
    Required("CHAT_IDENTITY_SERVER_ID"), Credential("SF_IDENTITY_CHAT_VERIFIER_KEY", "chat-verifier-key")));
if (wsPort is not null)
{
    builder.Services.AddHostedService<MailExpiryJob>();
    var internalUrl = new Uri(Required("CHAT_IDENTITY_INTERNAL_URL").TrimEnd('/') + "/");
    if (internalUrl.Scheme != Uri.UriSchemeHttps) throw new InvalidOperationException("CHAT_IDENTITY_INTERNAL_URL must use HTTPS.");
    var serverPin = Convert.FromHexString(Required("CHAT_IDENTITY_SERVER_CERT_SHA256"));
    if (serverPin.Length != 32) throw new InvalidOperationException("Invalid identity server certificate SHA256.");
    var handler = new HttpClientHandler();
    handler.ClientCertificates.Add(X509CertificateLoader.LoadPkcs12FromFile(Required("CHAT_IDENTITY_CLIENT_CERT_PATH"),
        Environment.GetEnvironmentVariable("CHAT_IDENTITY_CLIENT_CERT_PASSWORD")));
    handler.ServerCertificateCustomValidationCallback = (_, certificate, _, _) => certificate is not null &&
        CryptographicOperations.FixedTimeEquals(SHA256.HashData(certificate.RawData), serverPin);
    builder.Services.AddSingleton(new MailKeyClient(new HttpClient(handler) { BaseAddress = internalUrl, Timeout = TimeSpan.FromSeconds(3) }, Credential("SF_IDENTITY_DATA_KEY_CHAT_KEY", "data-key-chat-key")));
    builder.Services.AddSingleton(new GroupMembershipClient(Required("CHAT_GROUP_URL"), Credential("SF_GAMEPLAY_TOKEN", "gameplay-token")));
    builder.Services.AddSingleton(new GuildMembershipClient(new HttpClient {
        BaseAddress = new Uri(Required("CHAT_GUILD_URL").TrimEnd('/') + "/"), Timeout = TimeSpan.FromSeconds(3) }, Credential("SF_GUILD_TOKEN", "guild-token")));
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

public sealed class MailExpiryJob(PostgresMailStore store, ILogger<MailExpiryJob> logger) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var timer = new PeriodicTimer(TimeSpan.FromHours(1));
        do
        {
            try { store.PurgeExpired(DateTimeOffset.UtcNow); }
            catch (Exception e) { logger.LogError(e, "Expired mail ciphertext purge failed"); }
        } while (await timer.WaitForNextTickAsync(stoppingToken));
    }
}
