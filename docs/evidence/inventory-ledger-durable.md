# Inventory ledger PostgreSQL fixture

On 2026-10-21, `tools/Test-InventoryLedgerDurable.ps1` passed with fresh evidence at `Artifacts/Persistence/InventoryLedgerDurable/run-20261021024523`. The locked restore and Release build used the pinned .NET 10.0.402 SDK, the existing Npgsql 10.0.3 package pin, and PostgreSQL 18.6 on loopback. All nine assertions passed: grant and convert survive process/database restart; reply-lost grant retry returns its original receipt; concurrent same-revision consumes have one winner; stopped storage rejects a mutation with a retryable error and unchanged state; the same operation succeeds after recovery. `harness.json` reports passing status and owned private storage removed.

This is synthetic single-database evidence. It does not cover production migration compatibility, backup/restore, a network listener, or cross-domain atomic writes.
