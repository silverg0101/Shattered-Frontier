using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using Grpc.Core;
using Grpc.Net.Client;
using ShatteredFrontier.Chat.Postgres;
using Group = ShatteredFrontier.Group.V1;

internal sealed class GroupMembershipClient : IDisposable
{
    private readonly GrpcChannel channel;
    private readonly Group.GroupService.GroupServiceClient client;
    private readonly Metadata headers;

    public GroupMembershipClient(string address, string token)
    {
        channel = GrpcChannel.ForAddress(address);
        client = new(channel);
        headers = new() { { "authorization", "Bearer " + token } };
    }

    public async Task<PartyMembers?> ForCharacter(string character, CancellationToken cancellationToken)
    {
        var party = await client.GetPartyForCharacterAsync(new Group.CharacterRef { CharacterId = character },
            headers, deadline: DateTime.UtcNow.AddSeconds(3), cancellationToken: cancellationToken);
        return party.State == "active" && party.PartyId.Length > 0
            ? new PartyMembers(party.PartyId, [.. party.MemberCharacterIds]) : null;
    }

    public void Dispose() => channel.Dispose();
}

public sealed record PartyMembers(string PartyId, IReadOnlyList<string> Members);
public sealed record ChatMessage(string Channel, string From, string Body);
public readonly record struct ChatRateLimit(int Capacity, double RefillPeriodSeconds)
{
    // Release-controlled security limit; no runtime override until approved bounds exist.
    public static ChatRateLimit Launch => new(5, 2);
}

// This hub never trusts a client-provided sender or party ID. The backing delegates
// are the group-service lookup and the durable bilateral contact policy.
public sealed class ChatDeliveryHub(
    Func<string, CancellationToken, Task<PartyMembers?>> partyFor,
    Func<string, string, bool> canWhisper,
    TimeProvider? clock = null)
{
    private sealed record Receiver(Guid Id, Func<ChatMessage, Task> Send);
    private sealed class Bucket { public double Tokens = ChatRateLimit.Launch.Capacity; public DateTimeOffset Updated; public readonly object Gate = new(); }
    private readonly ConcurrentDictionary<string, Receiver> online = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, Bucket> buckets = new(StringComparer.Ordinal);
    private readonly TimeProvider time = clock ?? TimeProvider.System;

    public Guid Connect(string actor, Func<ChatMessage, Task> send)
    {
        var id = Guid.NewGuid();
        online[actor] = new Receiver(id, send);
        return id;
    }

    public void Disconnect(string actor, Guid id)
    {
        if (online.TryGetValue(actor, out var receiver) && receiver.Id == id)
            ((ICollection<KeyValuePair<string, Receiver>>)online).Remove(new(actor, receiver));
    }

    private bool TakeToken(string actor)
    {
        var now = time.GetUtcNow();
        var bucket = buckets.GetOrAdd(actor, _ => new Bucket { Updated = now });
        lock (bucket.Gate)
        {
            bucket.Tokens = Math.Min(ChatRateLimit.Launch.Capacity,
                bucket.Tokens + Math.Max(0, (now - bucket.Updated).TotalSeconds) / ChatRateLimit.Launch.RefillPeriodSeconds);
            bucket.Updated = now;
            if (bucket.Tokens < 1) return false;
            bucket.Tokens -= 1;
            return true;
        }
    }

    private async Task Deliver(string actor, Receiver receiver, ChatMessage message)
    {
        try { await receiver.Send(message); }
        catch (Exception ex) when (ex is OperationCanceledException or WebSocketException)
        { Disconnect(actor, receiver.Id); }
    }

    public async Task<string> Publish(string actor, string channel, string? target, string body, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(body) || channel is not ("party" or "whisper")) return "invalid";
        if (!TakeToken(actor)) return "rate_limited";
        if (channel == "whisper")
        {
            if (string.IsNullOrWhiteSpace(target) || target == actor) return "invalid";
            // Recheck immediately before every attempted delivery, including after reconnect.
            if (!canWhisper(actor, target)) return "not_delivered";
            if (online.TryGetValue(target, out var receiver))
                await Deliver(target, receiver, new ChatMessage(channel, actor, body));
            return "sent";
        }
        var party = await partyFor(actor, cancellationToken);
        if (party is null || !party.Members.Contains(actor, StringComparer.Ordinal)) return "not_in_party";
        foreach (var member in party.Members.Distinct(StringComparer.Ordinal))
        {
            var current = await partyFor(member, cancellationToken);
            if (current?.PartyId == party.PartyId && current.Members.Contains(member, StringComparer.Ordinal)
                && online.TryGetValue(member, out var receiver))
                await Deliver(member, receiver, new ChatMessage(channel, actor, body));
        }
        return "sent";
    }
}

public sealed class ChatTransport
{
    // A framing safety ceiling, not an owner-defined chat body size control.
    private const int MaxFrameBytes = 65536;
    private readonly Func<string?, CancellationToken, Task<string>> verifyTicket;
    private readonly ChatDeliveryHub hub;
    private static readonly JsonSerializerOptions Json = new(JsonSerializerDefaults.Web);

    internal ChatTransport(ChatActorVerifier actors, GroupMembershipClient groups, PostgresContactsStore contacts)
        : this(actors.VerifyTicket, new ChatDeliveryHub(groups.ForCharacter, contacts.CanDeliverWhisper)) { }

    // The transport fixture supplies actor verification and membership without product credentials.
    public ChatTransport(Func<string?, CancellationToken, Task<string>> verifyTicket, ChatDeliveryHub hub)
    { this.verifyTicket = verifyTicket; this.hub = hub; }

    private sealed record ClientPacket(string? Type, string? Ticket, string? Channel, string? Target, string? Body);

    public async Task Accept(HttpContext context)
    {
        if (!context.WebSockets.IsWebSocketRequest) { context.Response.StatusCode = 400; return; }
        using var socket = await context.WebSockets.AcceptWebSocketAsync();
        string? actor = null;
        Guid connection = default;
        var sendGate = new SemaphoreSlim(1, 1);
        try
        {
            var first = await Read(socket, context.RequestAborted);
            if (first?.Type != "auth") { await Close(socket, WebSocketCloseStatus.PolicyViolation, "auth_required"); return; }
            try { actor = await verifyTicket(first.Ticket, context.RequestAborted); }
            catch (RpcException) { await Close(socket, WebSocketCloseStatus.PolicyViolation, "auth_rejected"); return; }
            connection = hub.Connect(actor, async message =>
            {
                var payload = JsonSerializer.SerializeToUtf8Bytes(new { type = "message", channel = message.Channel, from = message.From, body = message.Body }, Json);
                await sendGate.WaitAsync(context.RequestAborted);
                try { if (socket.State == WebSocketState.Open) await socket.SendAsync(payload, WebSocketMessageType.Text, true, context.RequestAborted); }
                finally { sendGate.Release(); }
            });
            await SendStatus("authenticated");
            while (socket.State == WebSocketState.Open && !context.RequestAborted.IsCancellationRequested)
            {
                var packet = await Read(socket, context.RequestAborted);
                if (packet is null) break;
                if (packet.Type != "send") { await SendStatus("invalid"); continue; }
                string status;
                try { status = await hub.Publish(actor, packet.Channel ?? "", packet.Target, packet.Body ?? "", context.RequestAborted); }
                catch (Exception ex) when (ex is RpcException or Npgsql.NpgsqlException or ContactRejectedException)
                { status = "unavailable"; }
                await SendStatus(status);
            }
        }
        catch (Exception ex) when (ex is WebSocketException or OperationCanceledException or JsonException or InvalidDataException) { }
        finally { if (actor is not null) hub.Disconnect(actor, connection); }

        async Task SendStatus(string status)
        {
            var payload = JsonSerializer.SerializeToUtf8Bytes(new { type = "status", status }, Json);
            await sendGate.WaitAsync(context.RequestAborted);
            try { if (socket.State == WebSocketState.Open) await socket.SendAsync(payload, WebSocketMessageType.Text, true, context.RequestAborted); }
            finally { sendGate.Release(); }
        }
    }

    private static async Task<ClientPacket?> Read(WebSocket socket, CancellationToken cancellationToken)
    {
        using var stream = new MemoryStream();
        var buffer = new byte[4096];
        WebSocketReceiveResult result;
        do
        {
            result = await socket.ReceiveAsync(buffer, cancellationToken);
            if (result.MessageType == WebSocketMessageType.Close) return null;
            if (result.MessageType != WebSocketMessageType.Text || stream.Length + result.Count > MaxFrameBytes)
                throw new InvalidDataException("Invalid chat frame.");
            stream.Write(buffer, 0, result.Count);
        } while (!result.EndOfMessage);
        return JsonSerializer.Deserialize<ClientPacket>(stream.ToArray(), Json);
    }

    private static Task Close(WebSocket socket, WebSocketCloseStatus status, string reason) =>
        socket.CloseAsync(status, reason, CancellationToken.None);
}
