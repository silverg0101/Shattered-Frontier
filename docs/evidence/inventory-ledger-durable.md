# Inventory ledger PostgreSQL fixture

On 2026-10-21, `tools/Test-InventoryLedgerDurable.ps1 -EvidenceRoot Artifacts/Persistence/InventoryLedgerDurable/review-20261021-02` passed on a fresh root. The locked restore and Release build used the pinned .NET 10.0.402 SDK, the existing Npgsql 10.0.3 package pin, and PostgreSQL 18.6 on loopback. All 11 assertions passed: acknowledged grant and convert survive immediate PostgreSQL stops and WAL replay; reply-lost grant retry returns its original receipt; two gated same-revision consumes have one winner; stopped storage rejects a mutation with a retryable error and unchanged state; the same operation succeeds after recovery; SQL class 23 is non-retryable and rolls back. The fixture passes its connection string through the child environment. `harness.json` reports passing status and owned private storage removed. The first fresh root (`review-20261021-01`) caught a fixture compile error, which was corrected before this passing rerun.

The pinned SDK also ran `InventoryLedger.Tests`, `Exchange.Tests`, `CraftingContent.Tests`, and `MvpJourney.Tests` with `--locked-mode`; all four exited 0.

This is synthetic single-database evidence. It does not cover production migration compatibility, backup/restore, a network listener, or cross-domain atomic writes.
