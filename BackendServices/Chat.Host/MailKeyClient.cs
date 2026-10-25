using System.Net.Http.Json;
using System.Text.Json.Serialization;

public sealed class MailKeyClient(HttpClient http, string clientKey)
{
    private async Task<T> Get<T>(string path, CancellationToken cancellationToken)
    {
        using var request = new HttpRequestMessage(HttpMethod.Get, path);
        request.Headers.Add("X-Data-Key-Chat-Key", clientKey);
        using var response = await http.SendAsync(request, cancellationToken);
        response.EnsureSuccessStatusCode();
        return (await response.Content.ReadFromJsonAsync<T>(cancellationToken: cancellationToken))!;
    }

    public Task<CharacterAccount> Resolve(string characterId, CancellationToken cancellationToken) =>
        Get<CharacterAccount>("v1/internal/characters/" + Uri.EscapeDataString(characterId) + "/account", cancellationToken);

    public Task<ErasedState> Erased(string accountId, CancellationToken cancellationToken) =>
        Get<ErasedState>("v1/internal/accounts/" + Uri.EscapeDataString(accountId) + "/erased", cancellationToken);

    public async Task<AccountKey> Key(string accountId, bool forWrite, ushort? version, CancellationToken cancellationToken)
    {
        using var request = new HttpRequestMessage(HttpMethod.Post, "v1/internal/data-keys/" + Uri.EscapeDataString(accountId))
        {
            Content = JsonContent.Create(new { service = "chat", purpose = "mail", for_write = forWrite, key_version = version })
        };
        request.Headers.Add("X-Data-Key-Chat-Key", clientKey);
        using var response = await http.SendAsync(request, cancellationToken);
        response.EnsureSuccessStatusCode();
        var key = (await response.Content.ReadFromJsonAsync<AccountKey>(cancellationToken: cancellationToken))!;
        if (key.DekBase64 is null || Convert.FromBase64String(key.DekBase64).Length != 32) throw new InvalidDataException("Invalid data key response.");
        return key;
    }
}

public sealed record CharacterAccount([property: JsonPropertyName("account_id")] string AccountId,
    [property: JsonPropertyName("realm_id")] string RealmId);
public sealed record ErasedState([property: JsonPropertyName("erased")] bool Erased);
public sealed record AccountKey([property: JsonPropertyName("key_version")] ushort KeyVersion,
    [property: JsonPropertyName("dek_base64")] string DekBase64)
{
    public byte[] Bytes => Convert.FromBase64String(DekBase64);
}
