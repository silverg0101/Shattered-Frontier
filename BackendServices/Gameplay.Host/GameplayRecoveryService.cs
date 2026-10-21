using Microsoft.Extensions.Hosting;
using Npgsql;
using ShatteredFrontier.CharacterState.Postgres;
using ShatteredFrontier.Housing.Postgres;
using ShatteredFrontier.InventoryLedger.Postgres;

// Operational recovery timing, shipped in code. The shared configuration workflow may
// publish a typed version later; no environment or console override changes it.
internal sealed record GameplayRecoverySettings
{
    public TimeSpan Interval { get; } = TimeSpan.FromSeconds(2);
    public TimeSpan RecoveryWindow { get; } = TimeSpan.FromSeconds(5);
    public void Validate()
    {
        if (Interval < TimeSpan.FromSeconds(1) || Interval > TimeSpan.FromMinutes(5) ||
            RecoveryWindow < TimeSpan.FromSeconds(2) || RecoveryWindow > TimeSpan.FromMinutes(10))
            throw new InvalidOperationException("Recovery timing outside shipped bounds.");
    }
}

internal sealed class GameplayRecoveryService(GameplayRecoverySettings settings,
    PostgresCharacterStateStore characters, PostgresHousingStore housing,
    PostgresInventoryLedgerStore ledger, QuestRewardPayout payout) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        settings.Validate();
        using var timer = new PeriodicTimer(settings.Interval);
        do
        {
            try { Sweep(); }
            catch (Exception ex) when (ex is not OperationCanceledException)
            { Console.Error.WriteLine($"GAMEPLAY_SWEEP_UNAVAILABLE {ex.GetType().Name}"); }
        } while (await timer.WaitForNextTickAsync(stoppingToken));
    }

    private void Sweep()
    {
        using var connection = new NpgsqlConnection(Environment.GetEnvironmentVariable("GAMEPLAY_CHARACTER_CONNECTION"));
        connection.Open();
        using var acquire = new NpgsqlCommand("SELECT pg_try_advisory_lock(519761, 44)", connection);
        if (acquire.ExecuteScalar() is not true) return;
        try
        {
            var cutoff = DateTimeOffset.UtcNow - settings.RecoveryWindow;
            var placements = new List<string>();
            using (var cmd = new NpgsqlCommand("SELECT operation_id FROM housing.placements WHERE state='Pending' AND created_at <= $1 ORDER BY created_at LIMIT 100", connection))
            {
                cmd.Parameters.AddWithValue(cutoff);
                using var reader = cmd.ExecuteReader();
                while (reader.Read()) placements.Add(reader.GetString(0));
            }
            foreach (var id in placements)
            {
                var result = housing.Reconcile(id, ledger);
                Console.Out.WriteLine($"GAMEPLAY_SWEEP placement {id} {result.State}");
            }
            var removals = new List<string>();
            using (var cmd = new NpgsqlCommand("SELECT operation_id FROM housing.removals WHERE state='Pending' ORDER BY operation_id LIMIT 100", connection))
            using (var reader = cmd.ExecuteReader()) while (reader.Read()) removals.Add(reader.GetString(0));
            foreach (var id in removals)
            {
                var result = housing.CompleteRemove(id, ledger);
                Console.Out.WriteLine($"GAMEPLAY_SWEEP removal {id} {result.State}");
            }
            foreach (var intent in characters.PendingRewards(cutoff))
            {
                try
                {
                    payout.Resolve(intent.OperationId);
                    Console.Out.WriteLine($"GAMEPLAY_SWEEP reward {intent.OperationId} Complete");
                }
                catch (GameplayRejectedException ex) when (ex.Code == Grpc.Core.StatusCode.FailedPrecondition)
                {
                    Console.Out.WriteLine($"GAMEPLAY_SWEEP reward {intent.OperationId} Pending");
                }
            }
        }
        finally
        {
            using var release = new NpgsqlCommand("SELECT pg_advisory_unlock(519761, 44)", connection);
            release.ExecuteNonQuery();
        }
    }
}
