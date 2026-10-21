# PostgreSQL housing store

`PostgresHousingStore` durably stores claims, character build grants, funded/grace/notice deadlines, claim revision receipts, claim-scoped structure revisions, and pending/committed/compensated placement records. Install `Schema.sql` with a migration identity. The runtime role needs schema usage, table DML, and marker read access. Every call checks the version 1 marker and refuses missing, inaccessible, or newer schemas. Calls use a transaction-scoped advisory lock; claim edits and structure commits use revision compare-and-set. SQL classes 22 and 23 are non-retryable `HousingRejectedException`; connection failure is `HousingUnavailableException` and the same operation ID should be retried. No new adjustable value is introduced.

`SaveClaim` accepts an authoritative claim snapshot, expected revision, operation ID and fingerprint. It persists grants and lifecycle timestamps but does not calculate territory geometry or advance timers; `ClaimTerritory` remains the rules owner. It returns the original snapshot on an exact operation retry. `Begin` checks the stored build grant, permission revision and structure revision before reserving a unique structure ID and cell. The caller must validate content/cost against the release-shipped recipe catalog and authenticate actor, claim and container ownership before `Begin`; this storage boundary does not perform those checks. A retry with the same operation ID returns its stored placement decision.

The placement coordinator must call `Begin` before the ledger debit, use `operationId + "/consume"` as its durable inventory operation ID, then call `Reconcile`. After a worker crash, stop/fence that worker before `Reconcile` so no late debit can race the decision. `Reconcile` reads the ledger's durable operation receipt: a matching debit commits the reserved structure and revision, while a missing debit closes the pending row as compensated without issuing a refund. A ledger outage leaves the row pending. A mismatched receipt or stale revision is refused for manual reconciliation. The reservation prevents a second placement from winning its cell. This MVP protocol assumes a single fenced placement worker and a reachable ledger; a production authority adapter, worker lease/fencing, cross-service atomicity, and automatic recovery scheduling remain to be built. No guessed refund is issued.

Run the owned synthetic loopback fixture from the repository root with a fresh child path:

```powershell
& tools/Test-HousingDurable.ps1 -EvidenceRoot "$PWD/Artifacts/Persistence/HousingDurable/review-01"
```

The fixture uses pinned .NET and PostgreSQL, the existing pinned Npgsql package, generated per-run credentials passed through child environment variables, immediate-stop restart, outage and newer-schema refusal, and removes its owned cluster. It does not use a shared or remote database.
