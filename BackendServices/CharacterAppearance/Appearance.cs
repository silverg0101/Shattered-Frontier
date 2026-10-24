using System.Buffers.Binary;
using System.Text.Json;

namespace CharacterAppearance;

public readonly record struct CosmeticOverride(byte Slot, ushort Id);

// The wire order is fixed by version. No client-supplied catalogue or ownership claim is trusted.
public sealed record Appearance(
    byte Version, byte Race, byte Body, byte Heritage, byte Height, byte Build,
    byte[] Morphs, ushort[] Parts, byte[] Colors, CosmeticOverride[] Cosmetics, byte? FaceIndex = null);

public sealed record FaceDefinition(string Id, byte Body);

public sealed class AppearanceAllowlist
{
    public required byte Version { get; init; }
    public required byte Race { get; init; }
    public required int Bodies { get; init; }
    public required int[] Heritages { get; init; }
    public required FaceDefinition[] Faces { get; init; }
    public required PartCounts Parts { get; init; }
    public required PaletteCounts Palettes { get; init; }
    public required string[] MorphSlots { get; init; }
    public required int[] CosmeticSlots { get; init; }
    public required int MaxCosmeticOverrides { get; init; }

    public static AppearanceAllowlist Load(string path)
    {
        var definition = JsonSerializer.Deserialize<AppearanceAllowlist>(File.ReadAllText(path),
            new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower })
            ?? throw new InvalidDataException("Empty appearance allowlist");
        if (definition.Version != 2 || definition.Bodies is < 1 or > 256 ||
            definition.Faces is null || definition.Faces.Length is < 1 or > 256 ||
            definition.Faces.Any(f => f is null || string.IsNullOrWhiteSpace(f.Id) || f.Body >= definition.Bodies) ||
            definition.Faces.Select(f => f.Id).Distinct(StringComparer.Ordinal).Count() != definition.Faces.Length ||
            definition.MorphSlots is null || definition.MorphSlots.Length > 16 ||
            definition.Parts is null || definition.Palettes is null ||
            definition.Heritages is null || definition.CosmeticSlots is null ||
            definition.MaxCosmeticOverrides is < 0 or > 8 ||
            definition.Parts.Counts.Any(n => n is < 1 or > 65536) ||
            definition.Palettes.Counts.Any(n => n is < 1 or > 256) ||
            definition.CosmeticSlots.Any(n => n is < 0 or > 255) ||
            definition.Heritages.Any(n => n is < 0 or > 255) ||
            definition.CosmeticSlots.Distinct().Count() != definition.CosmeticSlots.Length ||
            definition.Heritages.Distinct().Count() != definition.Heritages.Length)
            throw new InvalidDataException("Invalid appearance allowlist");
        return definition;
    }
}

public sealed record PartCounts(int Head, int Hair, int Beard, int Brows)
{
    public int[] Counts => [Head, Hair, Beard, Brows];
}
public sealed record PaletteCounts(int Skin, int Hair, int Beard, int Eyes)
{
    public int[] Counts => [Skin, Hair, Beard, Eyes];
}

public readonly record struct AppearanceValidationResult(bool Valid, string? Code, string? Reason)
{
    public static AppearanceValidationResult Accept => new(true, null, null);
    public static AppearanceValidationResult Reject(string reason) => new(false, "APPEARANCE_INVALID", reason);
}

public static class AppearanceCodec
{
    public const int MaxBytes = 64;
    public const int MaxCosmetics = 8;
    private const int FixedBytes = 7 + 8 + 4 + 1;

    public static byte[] Pack(Appearance appearance)
    {
        ArgumentNullException.ThrowIfNull(appearance);
        if (appearance.Morphs is null || appearance.Parts is null || appearance.Colors is null || appearance.Cosmetics is null ||
            appearance.Morphs.Length > 16 || appearance.Parts.Length != 4 || appearance.Colors.Length != 4 ||
            appearance.Cosmetics.Length > MaxCosmetics ||
            appearance.Version is not (1 or 2) || (appearance.Version == 1) != (appearance.FaceIndex is null) ||
            FixedBytes + appearance.Morphs.Length + 3 * appearance.Cosmetics.Length + (appearance.Version == 2 ? 1 : 0) > MaxBytes)
            throw new ArgumentException("Appearance cannot be packed", nameof(appearance));
        byte[] bytes = new byte[FixedBytes + appearance.Morphs.Length + 3 * appearance.Cosmetics.Length + (appearance.Version == 2 ? 1 : 0)];
        bytes[0] = appearance.Version; bytes[1] = appearance.Race; bytes[2] = appearance.Body;
        bytes[3] = appearance.Heritage; bytes[4] = appearance.Height; bytes[5] = appearance.Build;
        bytes[6] = checked((byte)appearance.Morphs.Length);
        appearance.Morphs.CopyTo(bytes, 7);
        int offset = 7 + appearance.Morphs.Length;
        foreach (ushort part in appearance.Parts) { BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(offset), part); offset += 2; }
        appearance.Colors.CopyTo(bytes, offset); offset += 4;
        bytes[offset++] = checked((byte)appearance.Cosmetics.Length);
        foreach (var cosmetic in appearance.Cosmetics)
        {
            bytes[offset++] = cosmetic.Slot;
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(offset), cosmetic.Id);
            offset += 2;
        }
        if (appearance.Version == 2) bytes[offset] = appearance.FaceIndex!.Value;
        return bytes;
    }

    public static bool TryUnpack(ReadOnlySpan<byte> bytes, out Appearance? appearance)
    {
        appearance = null;
        if (bytes.Length < FixedBytes || bytes.Length > MaxBytes || bytes[0] is not (1 or 2) || bytes[6] > 16) return false;
        int morphCount = bytes[6];
        int offset = 7 + morphCount;
        if (bytes.Length < offset + 13) return false;
        byte[] morphs = bytes.Slice(7, morphCount).ToArray();
        ushort[] parts = new ushort[4];
        for (int i = 0; i < 4; i++) { parts[i] = BinaryPrimitives.ReadUInt16LittleEndian(bytes.Slice(offset)); offset += 2; }
        byte[] colors = bytes.Slice(offset, 4).ToArray(); offset += 4;
        int cosmeticCount = bytes[offset++];
        if (cosmeticCount > MaxCosmetics || bytes.Length != offset + cosmeticCount * 3 + (bytes[0] == 2 ? 1 : 0)) return false;
        CosmeticOverride[] cosmetics = new CosmeticOverride[cosmeticCount];
        for (int i = 0; i < cosmeticCount; i++)
        {
            byte slot = bytes[offset++];
            ushort id = BinaryPrimitives.ReadUInt16LittleEndian(bytes.Slice(offset)); offset += 2;
            cosmetics[i] = new CosmeticOverride(slot, id);
        }
        appearance = new Appearance(bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], morphs, parts, colors, cosmetics,
            bytes[0] == 2 ? bytes[offset] : null);
        return true;
    }
}

public sealed class AppearanceValidator(AppearanceAllowlist allowlist)
{
    public AppearanceValidationResult ValidatePacked(ReadOnlySpan<byte> bytes,
        IReadOnlySet<ushort> knownCosmetics, IReadOnlySet<ushort> ownedCosmetics) =>
        AppearanceCodec.TryUnpack(bytes, out var appearance)
            ? Validate(appearance!, knownCosmetics, ownedCosmetics)
            : AppearanceValidationResult.Reject("malformed packet");

    public AppearanceValidationResult Validate(Appearance appearance,
        IReadOnlySet<ushort> knownCosmetics, IReadOnlySet<ushort> ownedCosmetics)
    {
        if (appearance is null || knownCosmetics is null || ownedCosmetics is null ||
            appearance.Version is not (1 or 2) || appearance.Race != allowlist.Race ||
            appearance.Body >= allowlist.Bodies || !allowlist.Heritages.Contains(appearance.Heritage))
            return AppearanceValidationResult.Reject("version, race, body or heritage");
        if (appearance.Version == 2 && (appearance.FaceIndex is null || appearance.FaceIndex.Value >= allowlist.Faces.Length ||
            allowlist.Faces[appearance.FaceIndex.Value].Body != appearance.Body) ||
            appearance.Version == 1 && appearance.FaceIndex is not null)
            return AppearanceValidationResult.Reject("face out of range or wrong body");
        if (appearance.Morphs is null || appearance.Morphs.Length != allowlist.MorphSlots.Length ||
            appearance.Parts is null || appearance.Parts.Length != 4 ||
            appearance.Colors is null || appearance.Colors.Length != 4 ||
            appearance.Cosmetics is null || appearance.Cosmetics.Length > allowlist.MaxCosmeticOverrides ||
            appearance.Cosmetics.Length > AppearanceCodec.MaxCosmetics)
            return AppearanceValidationResult.Reject("field count");
        for (int i = 0; i < 4; i++)
        {
            if (appearance.Parts[i] >= allowlist.Parts.Counts[i]) return AppearanceValidationResult.Reject("unknown part");
            if (appearance.Colors[i] >= allowlist.Palettes.Counts[i]) return AppearanceValidationResult.Reject("palette out of range");
        }
        var slots = new HashSet<byte>();
        foreach (var cosmetic in appearance.Cosmetics)
            if (cosmetic.Id == 0 || !allowlist.CosmeticSlots.Contains(cosmetic.Slot) || !slots.Add(cosmetic.Slot) ||
                !knownCosmetics.Contains(cosmetic.Id) || !ownedCosmetics.Contains(cosmetic.Id))
                return AppearanceValidationResult.Reject("unknown or unowned cosmetic");
        return AppearanceValidationResult.Accept;
    }
}
