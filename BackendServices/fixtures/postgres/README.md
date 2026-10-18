# Disposable PostgreSQL migration fixture

This fixture implements only synthetic Building migrations and an inaccessible World sentinel. It does not create gameplay services or touch browser SQLite, Unreal hosts, other PostgreSQL instances, production credentials, Inventory or player data. See the [C-MIG contract](../../../docs/decisions/PER-01-migration-fixture-contract.md) and [backend dependency locks](../../README.md).

From the repository root, supply the verified portable binaries and built runner explicitly:

```powershell
& tools/Test-H2MigrationFixture.ps1 `
  -PostgresBin "$PWD\Artifacts\Persistence\Toolchains\postgresql-18.6\pgsql\bin" `
  -Dotnet "$PWD\Artifacts\Persistence\Toolchains\dotnet-10.0.402\dotnet.exe" `
  -RunnerDll "$PWD\BackendServices\building-service\bin\Release\net10.0\building-service.dll" `
  -Manifest "$PWD\BackendServices\building-service\Migrations\manifest.json" `
  -PartitionProxyDll "$PWD\BackendServices\fixtures\PartitionProxy\bin\Release\net10.0\PartitionProxy.dll" `
  -EvidenceRoot "$PWD\Artifacts\Persistence\C-MIG\review-<new-run-name>"
```

No installation or registered Windows service is created. Each invocation refuses an existing evidence path and creates a new ACL-private `.private-<UUID>` directory under ignored `Artifacts/Persistence/C-MIG`, a fresh SCRAM-authenticated cluster, a random loopback-only port and `h2mig_<UUID>` database. Limits are 12 connections, 16MB shared buffers and 4MB per-sort work memory. The SQL marker and provisioner-only `pg_control_system()` wrapper bind each runner to the actual database, fixture UUID and PostgreSQL system identifier. Database naming or loopback alone is insufficient.

`bootstrap.sql` creates a NOLOGIN Building owner; the migration identity alone can assume it. Runtime has synthetic DML and sequence use; read-only has SELECT. Neither can modify migration history, become owner, perform DDL or access the World sentinel. Credentials are generated in memory and passed through child environments/private temporary SQL, never command arguments, source control or evidence. The provisioner credential is never supplied to the runner.

The harness emits expected/observed assertions, runner exit/output, catalog/history snapshots, hashes, local identity and teardown receipt. Hooks use explicit `.arm`, `.ready` and `.release` files with finite waits. Provisioner-only history corruption and schema reset are test injections into this newly owned cluster; production recovery must never use those operations. SQL blocker sessions acknowledge a held transaction before injection. Actual cancelled concurrent index creation supplies the invalid-index recovery branch. The optional PartitionProxy helper retains the upstream server socket after severing the runner connection, proving competing runners stay busy until that backend releases its lock; omit it only when recording C-MIG-05 as unproven.

Teardown verifies the absolute run-owned directory marker, postmaster data directory/executable and stopped state before removing private storage. It preserves evidence. If identity or stop proof fails, resources remain for diagnosis and the receipt names the directory; never delete it based on a name alone. A stop/start failure is evidence, not a passing fixture run.

Exit 0 means all implemented assertions passed with no unproven branch, exit 1 means failure, and exit 2 means implemented assertions passed but contract branches remain explicitly unproven. Read `results.json` before interpreting the result. This fixture cannot establish cross-AZ durability, production recovery, load capacity or gameplay N−1 compatibility; synthetic projections are labeled as such.
