using Npgsql;

// Release-declared operating limits. Topology and credentials come from the connection
// strings; pool limits are owned by this host and take effect on process restart.
internal sealed record DatabasePoolSettings
{
    internal sealed record PoolLimit(string Service, int Default, int Minimum, int Maximum);
    public static IReadOnlyList<PoolLimit> Declarations { get; } =
    [
        new("InventoryLedger", 24, 2, 32),
        new("CharacterState", 12, 2, 24),
        new("Housing", 24, 2, 24),
        new("Exchange", 16, 2, 24)
    ];

    public int InventoryLedger { get; init; } = 24;
    public int CharacterState { get; init; } = 12;
    public int Housing { get; init; } = 24;
    public int Exchange { get; init; } = 16;

    public string For(string service, string connectionString)
    {
        var limit = Declarations.Single(d => d.Service == service);
        var value = service switch
        {
            "InventoryLedger" => InventoryLedger,
            "CharacterState" => CharacterState,
            "Housing" => Housing,
            "Exchange" => Exchange,
            _ => throw new InvalidOperationException($"Unknown pool service {service}.")
        };
        if (value < limit.Minimum || value > limit.Maximum)
            throw new InvalidOperationException($"Invalid {service} pool declaration.");
        var builder = new NpgsqlConnectionStringBuilder(connectionString)
        {
            Pooling = true,
            MinPoolSize = 0,
            MaxPoolSize = value,
            ApplicationName = $"sf.gameplay.{service.ToLowerInvariant()}"
        };
        return builder.ConnectionString;
    }
}
