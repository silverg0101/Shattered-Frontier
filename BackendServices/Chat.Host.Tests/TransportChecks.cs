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

        await Check("whisper to blocker dropped", async () =>
        {
            var delivered = new List<ChatMessage>();
            var hub = new ChatDeliveryHub((_, _) => Task.FromResult<PartyMembers?>(null), (_, _) => false);
            hub.Connect("chr_b", message => { delivered.Add(message); return Task.CompletedTask; });
            var status = await hub.Publish("chr_a", "whisper", "chr_b", "private", CancellationToken.None);
            if (status != "dropped" || delivered.Count != 0) throw new Exception("Blocked whisper delivered.");
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
        Console.WriteLine($"CHAT_TRANSPORT_PASS {passed}/4");
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
}
