using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;

namespace ShatteredFrontier.AccountData;

public static class AccountDataCrypto
{
    public static byte[] Wrap(ReadOnlySpan<byte> dek, ReadOnlySpan<byte> kek, string accountId)
    {
        if (dek.Length != 32 || kek.Length != 32) throw new ArgumentException("AES-256 keys required.");
        var nonce = RandomNumberGenerator.GetBytes(12);
        var output = new byte[12 + 32 + 16];
        nonce.CopyTo(output);
        using var aes = new AesGcm(kek, 16);
        aes.Encrypt(nonce, dek, output.AsSpan(12, 32), output.AsSpan(44, 16), Encoding.UTF8.GetBytes(accountId));
        return output;
    }

    public static byte[] Unwrap(ReadOnlySpan<byte> wrapped, ReadOnlySpan<byte> kek, string accountId)
    {
        if (wrapped.Length != 60 || kek.Length != 32) throw new CryptographicException("Invalid wrapped key.");
        var dek = new byte[32];
        using var aes = new AesGcm(kek, 16);
        aes.Decrypt(wrapped[..12], wrapped.Slice(12, 32), wrapped.Slice(44, 16), dek, Encoding.UTF8.GetBytes(accountId));
        return dek;
    }

    public static byte[] Encrypt(ReadOnlySpan<byte> plaintext, ReadOnlySpan<byte> dek, ushort keyVersion,
        string accountId, string table, string column, string recordId)
    {
        if (keyVersion == 0 || dek.Length != 32) throw new ArgumentException("Invalid data key.");
        var output = new byte[1 + 2 + 12 + plaintext.Length + 16];
        output[0] = 1;
        BinaryPrimitives.WriteUInt16BigEndian(output.AsSpan(1, 2), keyVersion);
        RandomNumberGenerator.Fill(output.AsSpan(3, 12));
        using var aes = new AesGcm(dek, 16);
        aes.Encrypt(output.AsSpan(3, 12), plaintext, output.AsSpan(15, plaintext.Length),
            output.AsSpan(15 + plaintext.Length, 16), Aad(accountId, table, column, recordId));
        return output;
    }

    public static ushort Version(ReadOnlySpan<byte> envelope)
    {
        if (envelope.Length < 31 || envelope[0] != 1) throw new CryptographicException("Unsupported envelope.");
        var version = BinaryPrimitives.ReadUInt16BigEndian(envelope.Slice(1, 2));
        if (version == 0) throw new CryptographicException("Invalid key version.");
        return version;
    }

    public static byte[] Decrypt(ReadOnlySpan<byte> envelope, ReadOnlySpan<byte> dek, ushort expectedVersion,
        string accountId, string table, string column, string recordId)
    {
        if (Version(envelope) != expectedVersion || dek.Length != 32) throw new CryptographicException("Wrong key version.");
        var length = envelope.Length - 31;
        var plaintext = new byte[length];
        using var aes = new AesGcm(dek, 16);
        aes.Decrypt(envelope.Slice(3, 12), envelope.Slice(15, length), envelope.Slice(15 + length, 16),
            plaintext, Aad(accountId, table, column, recordId));
        return plaintext;
    }

    private static byte[] Aad(string accountId, string table, string column, string recordId) =>
        Encoding.UTF8.GetBytes($"{accountId}|{table}.{column}|{recordId}");
}

public static class DataKeyRotation
{
    public static bool Due(long messagesWritten, long limit = 1L << 32)
    {
        if (messagesWritten < 0 || limit < 1) throw new ArgumentOutOfRangeException(nameof(limit));
        return messagesWritten >= limit;
    }
}
