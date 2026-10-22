namespace ShatteredFrontier.InventoryLedger;

// Alpha Starter Kit (owner decision DEC-02, 2026-10-22): a development/alpha grant so every new character can test
// construction. Not a shipped player feature. Contents and quantities are release-declared here; a realm enables it
// at host start. One grant per character: the operation ID is the kit ID plus the character ID.
public sealed record AlphaStarterKitSettings
{
    public string KitId { get; init; } = "alpha-starter-kit/v1";
    public IReadOnlyList<ItemQuantity> Items { get; init; } =
    [
        new("item/wall_material_bundle", 30), new("item/foundation_material_bundle", 6),
        new("item/roof_material_bundle", 6), new("item/door_material_bundle", 2),
        new("item/rough_timber", 12), new("item/iron_nails", 8),
    ];

    // MKT-01 owner answer: a purse of about 5 silver, in copper, credited once through the Exchange wallet (Gameplay.Host).
    public long PurseCopper { get; init; } = 500;

    // Bounds: 1-16 definitions, 1-100 units each, at most 100 units in all (fits the 160-unit backpack); purse 0-10000 copper.
    // No terrain fill: altering land costs and yields nothing (TER-02 owner answer).
    public void Validate()
    {
        if (!KitId.StartsWith("alpha-starter-kit/", StringComparison.Ordinal) || Items.Count is < 1 or > 16 ||
            Items.Any(i => i.Quantity is < 1 or > 100 || !i.DefinitionId.StartsWith("item/", StringComparison.Ordinal)) ||
            Items.Select(i => i.DefinitionId).Distinct(StringComparer.Ordinal).Count() != Items.Count ||
            Items.Sum(i => i.Quantity) > 100 || PurseCopper is < 0 or > 10000)
            throw new InvalidOperationException("Alpha Starter Kit outside declared bounds.");
    }

    public string OperationId(string characterId) => KitId + "/" + characterId;
    public string PurseOperationId(string characterId) => OperationId(characterId) + "/purse";

    // Grants the kit into a character backpack once; returns the backpack view after the grant (or unchanged).
    public ContainerView GrantOnce(InventoryLedger ledger, ContainerView backpack)
    {
        if (backpack.Spec.OwnerKind != OwnerKind.Character || backpack.Spec.Kind != ContainerKind.Backpack) return backpack;
        var operation = OperationId(backpack.Spec.OwnerId);
        if (ledger.Receipt(operation) is not null) return backpack;
        try { ledger.GrantBundle(operation, backpack.Spec.Id, backpack.Revision, Items, "alpha_starter_kit", operation); }
        // A concurrent ensure won the grant; anything else is a real refusal.
        catch (LedgerRejectedException) when (ledger.Receipt(operation) is not null) { }
        return ledger.Read(backpack.Spec.Id);
    }
}
