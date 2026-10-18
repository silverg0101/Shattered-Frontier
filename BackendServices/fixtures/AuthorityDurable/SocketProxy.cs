using System.Net;
using System.Net.Sockets;
using System.Text.Json;

namespace AuthorityDurable;

// Finite test-only TCP fault: close the client while retaining the upstream PostgreSQL session.
internal static class SocketProxy
{
    internal static async Task<int> Run(int port, string controlRoot)
    {
        if (port is < 1024 or > 65535) throw new ArgumentException("Proxy target must be a fixture port.");
        var fixtureId = Guid.Parse(Environment.GetEnvironmentVariable("AD_FIXTURE_ID")!);
        var control = Path.GetFullPath(controlRoot);
        var privateRoot = Directory.GetParent(control)?.Parent?.FullName ?? "";
        var repository = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../../../../"));
        var allowed = Path.Combine(repository, "Artifacts", "Persistence", "AuthorityDurable");
        var expectedPrivate = Path.Combine(allowed, ".private-" + fixtureId.ToString("N"));
        if (privateRoot != expectedPrivate || Directory.GetParent(control)?.Name != "control" ||
            !Directory.Exists(control) || (File.GetAttributes(control) & FileAttributes.ReparsePoint) != 0)
            throw new ArgumentException("Proxy control is not an owned fixture subdirectory.");
        using var marker = JsonDocument.Parse(await File.ReadAllTextAsync(Path.Combine(privateRoot, "owner.json")));
        var dataDirectory = Path.Combine(privateRoot, "data");
        if (marker.RootElement.GetProperty("fixtureId").GetString() != fixtureId.ToString() ||
            marker.RootElement.GetProperty("port").GetInt32() != port ||
            marker.RootElement.GetProperty("dataDirectory").GetString() != dataDirectory)
            throw new ArgumentException("Proxy fixture marker mismatch.");
        var postmaster = await File.ReadAllLinesAsync(Path.Combine(dataDirectory, "postmaster.pid"));
        if (postmaster.Length < 4 || Path.GetFullPath(postmaster[1]) != dataDirectory ||
            !int.TryParse(postmaster[3], out var ownedPort) || ownedPort != port)
            throw new ArgumentException("Proxy target is not the recorded owned PostgreSQL endpoint.");
        using var lifetime = new CancellationTokenSource(TimeSpan.FromSeconds(90));
        var token = lifetime.Token;
        var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start(1);
        try
        {
            await File.WriteAllTextAsync(Path.Combine(control, "ready.json"),
                JsonSerializer.Serialize(new { port = ((IPEndPoint)listener.LocalEndpoint).Port }), token);
            using var downstream = await listener.AcceptTcpClientAsync(token);
            listener.Stop();
            using var upstream = new TcpClient();
            await upstream.ConnectAsync(IPAddress.Loopback, port, token);
            await File.WriteAllTextAsync(Path.Combine(control, "connected"), "connected", token);
            using var forwarding = CancellationTokenSource.CreateLinkedTokenSource(token);
            var towardServer = Copy(downstream.GetStream(), upstream.GetStream(), forwarding.Token);
            var towardClient = Copy(upstream.GetStream(), downstream.GetStream(), forwarding.Token);
            var dropped = WaitFlag(control, "drop", token);
            var first = await Task.WhenAny(towardServer, towardClient, dropped);
            if (first != dropped)
            {
                // Closing a client naturally is not the retained-session fault being tested.
                forwarding.Cancel();
                return 3;
            }
            await dropped;
            forwarding.Cancel();
            downstream.Dispose();
            await File.WriteAllTextAsync(Path.Combine(control, "clientDropped"), "upstream still owned and open", token);
            await WaitFlag(control, "stop", token);
            upstream.Dispose();
            await File.WriteAllTextAsync(Path.Combine(control, "upstreamClosed"), "closed", token);
            return 0;
        }
        finally { listener.Stop(); }
    }

    private static async Task WaitFlag(string root, string name, CancellationToken token)
    {
        while (!File.Exists(Path.Combine(root, name))) await Task.Delay(25, token);
    }

    private static async Task Copy(NetworkStream source, NetworkStream destination, CancellationToken token)
    {
        try { await source.CopyToAsync(destination, token); }
        catch (Exception e) when (e is OperationCanceledException or IOException or SocketException or ObjectDisposedException) { }
    }
}
