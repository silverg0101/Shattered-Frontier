# Durable ledger executable

The fixture invokes `InventoryLedger.Postgres.Tests.dll` in five separate phases against its owned PostgreSQL instance: `seed`, `restart`, `concurrent`, `outage`, and `recover`. Use `tools/Test-InventoryLedgerDurable.ps1` to provision and clean up the database; do not pass a production connection string.
