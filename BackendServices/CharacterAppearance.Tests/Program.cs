using CharacterAppearance;

string path = args.Length == 1 ? args[0] : "content/appearance/human.json";
var allowlist = AppearanceAllowlist.Load(path);
void Check(bool condition, string name)
{
    if (!condition) throw new Exception(name);
    Console.WriteLine($"PASS {name}");
}
Check(allowlist.Bodies == 2 && allowlist.Parts.Counts.SequenceEqual([8, 10, 6, 4]) &&
    allowlist.Palettes.Counts.SequenceEqual([16, 16, 16, 8]) &&
    allowlist.MorphSlots.Length <= 16 && allowlist.Heritages.SequenceEqual([0, 1, 2, 3]) &&
    allowlist.Faces.Select(f => f.Id).SequenceEqual(["Orlando", "Godric", "Edwin", "Wulfric", "Osric", "Hilda", "Aelfwyn", "Eadgyth", "Ebba"]), "human allowlist");
var validator = new AppearanceValidator(allowlist);
var known = new HashSet<ushort> { 42 };
var owned = new HashSet<ushort> { 42 };
Appearance Base() => new(2, 0, 0, 1, 128, 128,
    new byte[allowlist.MorphSlots.Length], [0, 0, 0, 0], [0, 0, 0, 0], [], 0);
void Reject(Appearance candidate, string name, IReadOnlySet<ushort>? inventory = null)
{
    var result = validator.ValidatePacked(AppearanceCodec.Pack(candidate), known, inventory ?? owned);
    Check(!result.Valid && result.Code == "APPEARANCE_INVALID", name);
}
var good = Base();
Check(validator.ValidatePacked(AppearanceCodec.Pack(good), known, owned).Valid, "valid round trip");
Check(validator.ValidatePacked(AppearanceCodec.Pack(good with { FaceIndex = 4 }), known, owned).Valid, "male face");
Check(validator.ValidatePacked(AppearanceCodec.Pack(good with { Body = 1, FaceIndex = 5 }), known, owned).Valid, "female face");
Reject(good with { FaceIndex = 5 }, "wrong-body female face");
Reject(good with { Body = 1, FaceIndex = 0 }, "wrong-body male face");
Reject(good with { FaceIndex = 9 }, "face out of range");
Check(AppearanceCodec.TryUnpack(AppearanceCodec.Pack(good with { Version = 1, FaceIndex = null }), out var legacy) &&
    legacy is { Version: 1, FaceIndex: null } && validator.Validate(legacy, known, owned).Valid, "v1 decode");
Reject(good with { Parts = [8, 0, 0, 0] }, "unknown part");
Reject(good with { Morphs = new byte[allowlist.MorphSlots.Length - 1] }, "wrong morph count");
Reject(good with { Colors = [16, 0, 0, 0] }, "palette out of range");
Reject(good with { Cosmetics = [new CosmeticOverride(0, 42)] }, "unowned cosmetic", new HashSet<ushort>());
Reject(good with { Body = 2 }, "unknown body");
Reject(good with { Heritage = 4 }, "unknown heritage");
Reject(good with { Cosmetics = [new CosmeticOverride(0, 42), new CosmeticOverride(0, 42)] }, "duplicate cosmetic slot");
Check(!validator.ValidatePacked([1, 0], known, owned).Valid, "truncated packet");
var maximum = good with { Morphs = new byte[16], Cosmetics = Enumerable.Range(0, 8).Select(i => new CosmeticOverride((byte)i, 42)).ToArray() };
Check(AppearanceCodec.Pack(maximum).Length <= AppearanceCodec.MaxBytes, "maximum packed size");
