using System.Net.Http.Json;
using System.Text.Json.Serialization;
using Grpc.Core;

public sealed class ChatActorVerifier(HttpClient http, string serverId, string verifierKey)
{
    public async Task<string> RequireActor(ServerCallContext context)
    {
        var ticket = context.RequestHeaders.GetValue("x-contact-ticket");
        return await VerifyTicket(ticket, context.CancellationToken);
    }

    public async Task<string> VerifyTicket(string? ticket, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(ticket))
            throw new RpcException(new Status(StatusCode.Unauthenticated, "Contact ticket required."));
        using var request = new HttpRequestMessage(HttpMethod.Post, "v1/chat/verify-ticket")
        {
            Content = JsonContent.Create(new { ticket, server_id = serverId })
        };
        request.Headers.Add("X-Chat-Verifier-Key", verifierKey);
        HttpResponseMessage response;
        try { response = await http.SendAsync(request, cancellationToken); }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException)
        { throw new RpcException(new Status(StatusCode.Unavailable, "Identity gateway unavailable.")); }
        using (response)
        {
            if ((int)response.StatusCode >= 500)
                throw new RpcException(new Status(StatusCode.Unavailable, "Identity gateway unavailable."));
            if (!response.IsSuccessStatusCode)
                throw new RpcException(new Status(StatusCode.Unauthenticated, "Contact ticket invalid."));
            ActorClaims? claims;
            try { claims = await response.Content.ReadFromJsonAsync<ActorClaims>(cancellationToken: cancellationToken); }
            catch (System.Text.Json.JsonException) { throw new RpcException(new Status(StatusCode.Unauthenticated, "Contact ticket invalid.")); }
            if (string.IsNullOrWhiteSpace(claims?.CharacterId))
                throw new RpcException(new Status(StatusCode.Unauthenticated, "Contact ticket invalid."));
            return claims.CharacterId;
        }
    }

    private sealed record ActorClaims([property: JsonPropertyName("character_id")] string CharacterId);
}
