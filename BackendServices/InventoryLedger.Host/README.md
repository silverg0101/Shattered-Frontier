# Inventory ledger gRPC host

Development host for the INT-03 gathering slice: serves `proto/sf/inventory/v1/ledger.proto` (`EnsureContainer`, `GetContainer`, `Grant`) over cleartext HTTP/2 on `127.0.0.1` only, backed by `PostgresInventoryLedgerStore`. It adds no package pins beyond the central `Grpc.AspNetCore`. This is a development fixture, not the DEC-04 session protocol; there is no TLS and no production authentication.

INT-06: `GrantRequest.follow_up_intent` optionally supplies version 1 of the exact serialized `sf.progression.v1.ApplyReceiptRequest` and its lowercase SHA-256 digest. The host validates source operation and character identity before Grant; the store commits the bytes and digest with the grant receipt. Identical replay returns the original receipt; changed or omitted intent on replay is rejected. Legacy grants without an intent still work. Run `tools/Assert-InventoryOutboxMigration.ps1` before applying `Schema.Outbox.sql` to an existing version-1 database.

Environment (read at start; a bad value refuses to start with exit code 2):

- `INVENTORY_LEDGER_PORT`: required, 1-65535.
- `INVENTORY_LEDGER_CONNECTION`: required Npgsql connection string (runtime role, DML only). Never logged.
- Its release-declared operating pool is 12 client connections per host (safe bounds 2..32, unit connections), zero preopened idle connections, with application name `sf.inventory-ledger`. The host enforces the shipped default over connection-string pool fields at startup. Realm configuration operators may publish a reviewed change through the shared configuration workflow, effective on restart; this development host does not yet consume a published snapshot. PostgreSQL `pg_stat_activity.application_name` shows effective usage. No console-only override is provided.
- `SF_INVENTORY_LEDGER_TOKEN`: required. Every call must send `authorization: Bearer <token>` or gets `UNAUTHENTICATED`.
- `INVENTORY_LEDGER_FAULT`: empty, or `drop_first_reply`. The first `Grant` commits, then the reply is `UNAVAILABLE`; a retry with the same operation ID returns the original receipt.

Status mapping follows the proto header: stale revision `ABORTED`; changed spec or reused operation ID `ALREADY_EXISTS`; unknown container `NOT_FOUND`; other rejections `FAILED_PRECONDITION`; store or schema trouble `UNAVAILABLE`; unspecified enums `INVALID_ARGUMENT`. An expired deadline returns `DEADLINE_EXCEEDED` before the store is touched.

Stdout markers: `INVENTORY_LEDGER_HOST_READY port=<port>` once listening, `INVENTORY_LEDGER_GRANT op=<id> container=<id> status=<code>` per Grant, and `INVENTORY_LEDGER_FAULT_INJECTED op=<id>`.

Probe: `dotnet InventoryLedger.Host.dll get <containerId>` (needs `INVENTORY_LEDGER_PORT` and `SF_INVENTORY_LEDGER_TOKEN`) prints `INVENTORY_LEDGER_VIEW container=<id> revision=<n> <definition>=<qty> ...` and exits 0; an RPC failure prints `INVENTORY_LEDGER_VIEW_FAILED code=<StatusCode>` and exits 3.

Build from the repository root with the pinned SDK (`Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe`): `restore --locked-mode` (after the first lock generation), then `build --no-restore --configuration Release`.
