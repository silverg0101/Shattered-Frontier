using System.Net;
using Grpc.Core;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Server.Kestrel.Core;

// Opt-in transport fixture for two separate Unreal client processes. It uses the
// production ChatTransport and ChatDeliveryHub with fixed actor/membership policy.
internal static class UnrealRelayFixture
{
    public static async Task Run(int port)
    {
        if (port is < 1024 or > 65535) throw new ArgumentOutOfRangeException(nameof(port));
        var members = new[] { "chr_chat_a", "chr_chat_b" };
        Task<PartyMembers?> Lookup(string actor, CancellationToken _) => Task.FromResult<PartyMembers?>(
            members.Contains(actor, StringComparer.Ordinal) ? new PartyMembers("pty_chat_fixture", members) : null);
        var hub = new ChatDeliveryHub(Lookup, (_, _) => false);
        Task<string> Verify(string? ticket, CancellationToken _) => ticket switch
        {
            "fixture_a" => Task.FromResult(members[0]),
            "fixture_b" => Task.FromResult(members[1]),
            _ => Task.FromException<string>(new RpcException(new Status(StatusCode.Unauthenticated, "invalid fixture ticket")))
        };
        var transport = new ChatTransport(Verify, hub);
        var builder = WebApplication.CreateBuilder();
        builder.WebHost.ConfigureKestrel(options => options.Listen(IPAddress.Loopback, port,
            listen => listen.Protocols = HttpProtocols.Http1));
        var app = builder.Build();
        app.UseWebSockets();
        app.Map("/v1/chat", transport.Accept);
        app.Lifetime.ApplicationStarted.Register(() => Console.WriteLine($"CHAT_UNREAL_RELAY_READY port={port}"));
        await app.RunAsync();
    }
}
