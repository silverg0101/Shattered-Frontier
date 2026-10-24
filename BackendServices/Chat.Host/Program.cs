using System.Net;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using ShatteredFrontier.Chat.Postgres;

static string Required(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } value
    ? value : throw new InvalidOperationException($"{name} is required.");

if (!int.TryParse(Required("CHAT_HOST_PORT"), out var port) || port is < 1 or > 65535)
    throw new InvalidOperationException("CHAT_HOST_PORT must be 1-65535.");
var builder = WebApplication.CreateBuilder(args);
builder.WebHost.ConfigureKestrel(options => options.Listen(IPAddress.Loopback, port, listen => listen.Protocols = HttpProtocols.Http2));
builder.Services.AddGrpc();
builder.Services.AddSingleton(new PostgresContactsStore(Required("CHAT_CONNECTION")));
builder.Services.AddSingleton(new ChatServiceToken(Required("SF_CHAT_TOKEN")));
builder.Services.AddSingleton(new ChatPrivacyToken(Required("SF_CHAT_PRIVACY_TOKEN")));
builder.Services.AddSingleton(new ChatActorVerifier(new HttpClient { BaseAddress = new Uri(Required("CHAT_IDENTITY_URL").TrimEnd('/') + "/"), Timeout = TimeSpan.FromSeconds(3) },
    Required("CHAT_IDENTITY_SERVER_ID"), Required("SF_IDENTITY_CHAT_VERIFIER_KEY")));
var app = builder.Build();
app.MapGrpcService<ContactsGrpcService>();
app.Lifetime.ApplicationStarted.Register(() => Console.WriteLine($"CHAT_HOST_READY port={port}"));
app.Run();
