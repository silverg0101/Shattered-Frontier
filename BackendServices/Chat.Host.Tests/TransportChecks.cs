using System.Net;
using System.Text;
using Grpc.Core;

internal static class TransportChecks
{
    public static async Task Run()
    {
        var passed = 0;
        async Task Check(string name, Func<Task> test)
        {
            await test();
            Console.WriteLine($"PASS {name}");
            passed++;
        }

        await Check("auth reject", async () =>
        {
            var handler = new StubHandler();
            var verifier = new ChatActorVerifier(new HttpClient(handler) { BaseAddress = new Uri("http://localhost/") }, "chat", "key");
            try { await verifier.VerifyTicket("invalid", CancellationToken.None); throw new Exception("Invalid ticket accepted."); }
            catch (RpcException ex) when (ex.StatusCode == StatusCode.Unauthenticated) { }
            if (handler.Requests != 1) throw new Exception("IdentityGateway was not queried.");
        });

        await Check("ticket connect and two-client party round trip", async () =>
        {
            var verifier = new ChatActorVerifier(new HttpClient(new TicketHandler()) { BaseAddress = new Uri("http://localhost/") }, "chat", "key");
            var actorA = await verifier.VerifyTicket("ticket_a", CancellationToken.None);
            var actorB = await verifier.VerifyTicket("ticket_b", CancellationToken.None);
            var members = new[] { actorA, actorB };
            Task<PartyMembers?> Lookup(string actor, CancellationToken _) => Task.FromResult<PartyMembers?>(
                members.Contains(actor) ? new PartyMembers("pty_one", members) : null);
            var hub = new ChatDeliveryHub(Lookup, (_, _) => true);
            var a = new List<ChatMessage>();
            var b = new List<ChatMessage>();
            hub.Connect(actorA, message => { a.Add(message); return Task.CompletedTask; });
            hub.Connect(actorB, message => { b.Add(message); return Task.CompletedTask; });
            if (await hub.Publish(actorA, "party", null, "hello B", CancellationToken.None) != "sent"
                || await hub.Publish(actorB, "party", null, "hello A", CancellationToken.None) != "sent"
                || a.Count != 2 || b.Count != 2 || a[1] != new ChatMessage("party", actorB, "hello A")
                || b[0] != new ChatMessage("party", actorA, "hello B"))
                throw new Exception("Authenticated party round trip failed.");
        });

        await Check("whisper to blocker reports generic non-delivery", async () =>
        {
            var delivered = new List<ChatMessage>();
            var hub = new ChatDeliveryHub((_, _) => Task.FromResult<PartyMembers?>(null), (_, _) => false);
            hub.Connect("chr_b", message => { delivered.Add(message); return Task.CompletedTask; });
            var status = await hub.Publish("chr_a", "whisper", "chr_b", "private", CancellationToken.None);
            if (status != "not_delivered" || delivered.Count != 0) throw new Exception("Blocked whisper status or delivery was wrong.");
        });

        await Check("non-member party send rejected", async () =>
        {
            var delivered = new List<ChatMessage>();
            var hub = new ChatDeliveryHub((_, _) => Task.FromResult<PartyMembers?>(null), (_, _) => true);
            hub.Connect("chr_b", message => { delivered.Add(message); return Task.CompletedTask; });
            if (await hub.Publish("chr_a", "party", null, "private", CancellationToken.None) != "not_in_party"
                || delivered.Count != 0) throw new Exception("Non-member party send status or delivery was wrong.");
        });
        await Check("guild send rechecks membership before delivery", async () =>
        {
            var members = new HashSet<string> { "chr_a", "chr_b" };
            Task<GuildMembers?> Lookup(string actor, CancellationToken _) => Task.FromResult<GuildMembers?>(
                members.Contains(actor) ? new GuildMembers("gld_one", [.. members]) : null);
            var hub = new ChatDeliveryHub((_, _) => Task.FromResult<PartyMembers?>(null), (_, _) => true, Lookup);
            var a = 0; var b = 0;
            hub.Connect("chr_a", _ => { a++; return Task.CompletedTask; });
            hub.Connect("chr_b", _ => { b++; return Task.CompletedTask; });
            if (await hub.Publish("chr_a", "guild", null, "first", CancellationToken.None) != "sent" || a != 1 || b != 1)
                throw new Exception("Guild member delivery failed.");
            members.Remove("chr_b");
            if (await hub.Publish("chr_a", "guild", null, "second", CancellationToken.None) != "sent" || a != 2 || b != 1)
                throw new Exception("Removed guild member received a message.");
        });
        await Check("non-member guild send rejected", async () =>
        {
            var count = 0;
            var hub = new ChatDeliveryHub((_, _) => Task.FromResult<PartyMembers?>(null), (_, _) => true,
                (_, _) => Task.FromResult<GuildMembers?>(null));
            hub.Connect("chr_b", _ => { count++; return Task.CompletedTask; });
            if (await hub.Publish("chr_a", "guild", null, "private", CancellationToken.None) != "not_in_guild" || count != 0)
                throw new Exception("Outsider guild send delivered.");
        });

        await Check("party fan-out to members only", async () =>
        {
            var delivered = new Dictionary<string, int>();
            var members = new[] { "chr_a", "chr_b" };
            Task<PartyMembers?> Lookup(string actor, CancellationToken _) => Task.FromResult<PartyMembers?>(
                members.Contains(actor) ? new PartyMembers("pty_one", members) : null);
            var hub = new ChatDeliveryHub(Lookup, (_, _) => true);
            foreach (var actor in new[] { "chr_a", "chr_b", "chr_outsider" })
            {
                delivered[actor] = 0;
                hub.Connect(actor, _ => { delivered[actor]++; return Task.CompletedTask; });
            }
            await hub.Publish("chr_a", "party", null, "hello", CancellationToken.None);
            if (delivered["chr_a"] != 1 || delivered["chr_b"] != 1 || delivered["chr_outsider"] != 0)
                throw new Exception("Party audience was wrong.");
        });

        await Check("reconnect", async () =>
        {
            var oldCount = 0;
            var newCount = 0;
            var hub = new ChatDeliveryHub((_, _) => Task.FromResult<PartyMembers?>(null), (_, _) => true);
            var old = hub.Connect("chr_b", _ => { oldCount++; return Task.CompletedTask; });
            hub.Connect("chr_b", _ => { newCount++; return Task.CompletedTask; });
            hub.Disconnect("chr_b", old);
            await hub.Publish("chr_a", "whisper", "chr_b", "again", CancellationToken.None);
            if (oldCount != 0 || newCount != 1) throw new Exception("Reconnected receiver lost or duplicated.");
        });
        Console.WriteLine($"CHAT_TRANSPORT_PASS {passed}/8");
    }

    private sealed class StubHandler : HttpMessageHandler
    {
        public int Requests;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
        {
            Requests++;
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.Unauthorized)
            { Content = new StringContent("{}", Encoding.UTF8, "application/json") });
        }
    }

    private sealed class TicketHandler : HttpMessageHandler
    {
        protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
        {
            var body = await request.Content!.ReadAsStringAsync(cancellationToken);
            var actor = body.Contains("ticket_a", StringComparison.Ordinal) ? "chr_a"
                : body.Contains("ticket_b", StringComparison.Ordinal) ? "chr_b" : null;
            if (actor is null || request.Headers.GetValues("X-Chat-Verifier-Key").Single() != "key")
                return new HttpResponseMessage(HttpStatusCode.Unauthorized);
            return new HttpResponseMessage(HttpStatusCode.OK)
            { Content = new StringContent($"{{\"character_id\":\"{actor}\"}}", Encoding.UTF8, "application/json") };
        }
    }
}
