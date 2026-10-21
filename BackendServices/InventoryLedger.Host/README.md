# Inventory ledger gRPC host

Development host for the INT-03 gathering slice: serves `proto/sf/inventory/v1/ledger.proto` (`EnsureContainer`, `GetContainer`, `Grant`) over cleartext HTTP/2 on `127.0.0.1` only, backed by `PostgresInventoryLedgerStore`. It adds no package pins beyond the central `Grpc.AspNetCore`. This is a development fixture, not the DEC-04 session protocol; there is no TLS and no production authentication.

Environment (read at start; a bad value refuses to start with exit code 2):

- `INVENTORY_LEDGER_PORT`: required, 1-65535.
- `INVENTORY_LEDGER_CONNECTION`: required Npgsql connection string (runtime role, DML only). Never logged.
- `SF_INVENTORY_LEDGER_TOKEN`: required. Every call must send `authorization: Bearer <token>` or gets `UNAUTHENTICATED`.
- `INVENTORY_LEDGER_FAULT`: empty, or `drop_first_reply`. The first `Grant` commits, then the reply is `UNAVAILABLE`; a retry with the same operation ID returns the original receipt.

Status mapping follows the proto header: stale revision `ABORTED`; changed spec or reused operation ID `ALREADY_EXISTS`; unknown container `NOT_FOUND`; other rejections `FAILED_PRECONDITION`; store or schema trouble `UNAVAILABLE`; unspecified enums `INVALID_ARGUMENT`. An expired deadline returns `DEADLINE_EXCEEDED` before the store is touched.

Stdout markers: `INVENTORY_LEDGER_HOST_READY port=<port>` once listening, `INVENTORY_LEDGER_GRANT op=<id> container=<id> status=<code>` per Grant, and `INVENTORY_LEDGER_FAULT_INJECTED op=<id>`.

Probe: `dotnet InventoryLedger.Host.dll get <containerId>` (needs `INVENTORY_LEDGER_PORT` and `SF_INVENTORY_LEDGER_TOKEN`) prints `INVENTORY_LEDGER_VIEW container=<id> revision=<n> <definition>=<qty> ...` and exits 0; an RPC failure prints `INVENTORY_LEDGER_VIEW_FAILED code=<StatusCode>` and exits 3.

Build from the repository root with the pinned SDK (`Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe`): `restore --locked-mode` (after the first lock generation), then `build --no-restore --configuration Release`.
