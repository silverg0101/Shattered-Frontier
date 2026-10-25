namespace IdentityGateway;

public interface IKekProvider
{
    string KeyId { get; }
    byte[] GetKey(string keyId);
}

// systemd-creds writes the decrypted credential into CREDENTIALS_DIRECTORY.
public sealed class FileKekProvider(string keyId, string? developmentPath = null) : IKekProvider
{
    public string KeyId => keyId;
    public byte[] GetKey(string requestedId)
    {
        if (requestedId.Length is < 1 or > 64 || requestedId.Any(c => !char.IsAsciiLetterOrDigit(c) && c is not '-' and not '_'))
            throw new InvalidOperationException("Invalid KEK identifier.");
        var path = developmentPath is null
            ? Path.Combine(Environment.GetEnvironmentVariable("CREDENTIALS_DIRECTORY")
                ?? throw new InvalidOperationException("CREDENTIALS_DIRECTORY is required."), "account-data-kek-" + requestedId)
            : requestedId == keyId ? developmentPath : throw new InvalidOperationException("Old KEK unavailable in development path.");
        var key = File.ReadAllBytes(path);
        if (key.Length != 32) throw new InvalidOperationException("Account KEK must contain exactly 32 raw bytes.");
        return key;
    }
}
