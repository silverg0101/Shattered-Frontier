# PostgreSQL inventory ledger store

`PostgresInventoryLedgerStore` implements the existing `IInventoryLedgerStore` callback. Install `Schema.sql` with a migration identity before constructing it; the runtime connection needs only schema usage and table DML. It opens one transaction per ledger call, takes a transaction-scoped PostgreSQL advisory lock shared by all store instances, loads the state, applies the ledger callback, then commits container revisions, quantities, and any new operation receipt together. Container revision updates use compare-and-set. A retry after commit reads the persisted operation ID and returns its original receipt. Connection and database failures throw `LedgerStoreUnavailableException`; callers should retry the same operation ID after recovery. No state is held or published in memory on failure.

The advisory lock serializes the current whole-state interface across this database. This is an MVP correctness boundary, not a high-throughput partitioned ledger. The adapter reads all containers, entries, and receipts per call; schema installation and production migration/compatibility gates remain separate. This project adds no adjustable balance or operating value. Container capacity remains the existing bounded, trusted `ContainerSpec` value.

Run the owned synthetic loopback fixture from the repository root with a fresh child path:

```powershell
& tools/Test-InventoryLedgerDurable.ps1 -EvidenceRoot "$PWD/Artifacts/Persistence/InventoryLedgerDurable/review-01"
```

The wrapper verifies the pinned SDK and PostgreSQL tools, restores with committed package locks and the existing Npgsql 10.0.3 pin, creates a new local cluster, uses generated credentials, runs the test executable across PostgreSQL stops and restarts, and removes only its owned private directory. It does not connect to a shared or remote database. The fixture is synthetic; no listener, client authentication, backup/restore, or cross-domain transaction coordinator is included.
