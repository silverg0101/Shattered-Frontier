using System.Net;
using System.Net.Sockets;
using System.Text.Json;

// One-connection, loopback-only fault injector. No SQL, credential or payload logging.
if (args.Length != 2 || !int.TryParse(args[0], out var targetPort) || targetPort is < 1024 or > 65535)
    throw new ArgumentException("Expected explicit local target port and owned fixture control directory.");
var control = Path.GetFullPath(args[1]);
var repository = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../../../../"));
var allowed = Path.Combine(repository, "Artifacts", "Persistence", "C-MIG") + Path.DirectorySeparatorChar;
if (!control.StartsWith(allowed, StringComparison.OrdinalIgnoreCase) || !Directory.Exists(control))
    throw new ArgumentException("Control directory must already exist inside Artifacts/Persistence/C-MIG.");
using var lifetime = new CancellationTokenSource(TimeSpan.FromSeconds(120));
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
    await upstream.ConnectAsync(IPAddress.Loopback, targetPort, token);
    await File.WriteAllTextAsync(Path.Combine(control, "connected"), "connected", token);
    using var forwarding = CancellationTokenSource.CreateLinkedTokenSource(token);
    var clientToServer = Copy(downstream.GetStream(), upstream.GetStream(), forwarding.Token);
    var serverToClient = Copy(upstream.GetStream(), downstream.GetStream(), forwarding.Token);
    var copies = Task.WhenAll(clientToServer, serverToClient);
    var drop = WaitFlag("drop", token);
    var finished = await Task.WhenAny(clientToServer, serverToClient, drop);
    if (finished == drop)
    {
        await drop;
        forwarding.Cancel();
        downstream.Close();
        try { await copies; } catch (Exception ex) when (ex is OperationCanceledException or IOException or ObjectDisposedException) { }
        // Keep upstream open: PostgreSQL still sees a live backend session after the runner has lost its connection.
        await File.WriteAllTextAsync(Path.Combine(control, "partitioned"), "upstream-retained", token);
        await WaitFlag("stop", token);
    }
    else
    {
        forwarding.Cancel();
        downstream.Close();
        upstream.Close();
        try { await copies; } catch (Exception ex) when (ex is OperationCanceledException or IOException) { }
    }
}
finally { listener.Stop(); }

async Task WaitFlag(string name, CancellationToken cancellation)
{
    while (!File.Exists(Path.Combine(control, name))) await Task.Delay(20, cancellation);
}

static async Task Copy(NetworkStream source, NetworkStream destination, CancellationToken cancellation)
{
    byte[] buffer = new byte[16 * 1024];
    while (true)
    {
        var count = await source.ReadAsync(buffer, cancellation);
        if (count == 0) return;
        await destination.WriteAsync(buffer.AsMemory(0, count), cancellation);
    }
}
