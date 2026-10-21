# H2 backend foundation

TerrainDelta backup and restore use `tools/Backup-ServiceStore.ps1` and `tools/Restore-ServiceStore.ps1` with the version 1 `terrain_delta` schema. The restore checks row counts and canonical row digests for `schema_version`, `regions`, and `operations`; `tools/Test-RestoreRehearsal.ps1` seeds an edit and verifies five-schema backup-point parity and newer-schema refusal.

The package-free [WorldContent](WorldContent/README.md) project adds zone JSON validation and an in-memory deterministic World spawn-slot model. Its CLI and focused tests have separate package locks; neither is part of the historical three-project dependency verifier.

The user selected C#/.NET 10, separate domain services and restricted runtime roles on 2026-10-18. This slice delivers build locks, a dependency probe and a synthetic Building migration fixture. World/Inventory services, gameplay schemas, command adapters and production deployment are not implemented. See [PER-01-B](../docs/decisions/PER-01-B-implementation-readiness.md) and the [fixture contract](../docs/decisions/PER-01-migration-fixture-contract.md).

## Prepare and verify

Run PowerShell 7 on Windows x64 from the repository root:

```powershell
& tools/Prepare-H2Toolchains.ps1
& tools/Verify-H2Dependencies.ps1 -EvidenceRoot "$PWD\Artifacts\Persistence\DependencyLock\review-01"
```

Choose a new evidence directory for each run. Preparation downloads portable archives only into ignored `Artifacts/Persistence/Toolchains`, verifies the pinned archive and entrypoint hashes, and refuses mismatches. No registered Windows service or global SDK/PATH change is made. Existing extracted dependencies are not a fully rehashed tree. The SDK's archive checksum comes from Microsoft's release metadata; the EDB PostgreSQL archive pin is its observed SHA256 on HTTPS acquisition, with no independently retrieved publisher checksum. `toolchain-lock.json` records that distinction.

`global.json` is backend-scoped and disables SDK roll-forward. Central versions are exact; committed per-project `packages.lock.json` files pin the complete NuGet graph and content hashes. Verification uses locked restores and no-restore Release builds for the Building runner, dependency probe and partition injector, audits each project and verifies signatures/NuGet content hashes for the union of all lock graphs, records license metadata, checks bundled protobuf compiler/plugin identities and runs the Windows native Kafka/protobuf dependency probe. Audits are dated feed observations, not proof against unknown issues or every native component. Keep acquired third-party notices with any future redistribution; no distributable image is produced here.

The probe references the proposed gRPC/Kafka/telemetry stack without opening connections or listeners. Protobuf runtime and compiler loading do not establish game-schema generation, cross-language bytes or B-05 acceptance. Linux native dependencies and the previously proposed container images have not been executed by this Windows fixture.

## Run the isolated fixture

```powershell
& tools/Test-H2MigrationFixture.ps1 `
  -PostgresBin "$PWD\Artifacts\Persistence\Toolchains\postgresql-18.6\pgsql\bin" `
  -Dotnet "$PWD\Artifacts\Persistence\Toolchains\dotnet-10.0.402\dotnet.exe" `
  -RunnerDll "$PWD\BackendServices\building-service\bin\Release\net10.0\building-service.dll" `
  -Manifest "$PWD\BackendServices\building-service\Migrations\manifest.json" `
  -PartitionProxyDll "$PWD\BackendServices\fixtures\PartitionProxy\bin\Release\net10.0\PartitionProxy.dll" `
  -EvidenceRoot "$PWD\Artifacts\Persistence\C-MIG\review-01"
```

The [fixture README](fixtures/postgres/README.md) describes identity, credentials, bounded hooks and cleanup. The finite loopback partition injector drops the runner connection while retaining the PostgreSQL backend socket, allowing the harness to observe the still-held advisory lock without changing firewall policy. Evidence records actual outcomes; missing branches remain unproven, never passing by omission. Only the provisioner manipulates synthetic schema/history for failure injection.

Migration files are UTF-8/LF and hashed as exact bytes. This runner intentionally supports the fixed synthetic `cmig-v1` catalogue, not arbitrary production migrations. A dedicated connection holds the session advisory lock through all steps; normal steps also take a distinct transaction mutex. Schema readiness checks exact fixture tables/PK/identity sequence/index and immutable history. Runtime credentials have synthetic DML/sequence privileges, read-only credentials have SELECT, and only migrations can assume the NOLOGIN Building owner. Separate credentials alone would not restrict a runtime schema owner.

Gameplay authority, production role-contract updates, real World/Inventory participants, backup/DR, live deployment, production load and game N−1 compatibility retain their documented gates. [Work plan](../docs/design-work-plan.md) and [evidence](../docs/evidence/h2-migration-fixture.md) record bounded completion and remaining work.

## Authority design experiment

The separate package-free [AuthorityModel](fixtures/AuthorityModel/README.md) explores proposed B-04 ordering with an idealized lookup/atomic-state oracle. Run only its focused checks with `tools/Test-H2AuthorityModel.ps1 -EvidenceRoot <new Artifacts/Persistence/AuthorityModel child>`. It is a fourth project tested separately from the three-project dependency verifier; no package graph is added. [Authority evidence](../docs/evidence/h2-authority-fencing.md) records results and assumptions. It does not implement the [B-03 actor admission](../docs/decisions/B-03-actor-admission-contract.md) proposal or release any production/security gate.

The fifth project, [AuthorityDurable](fixtures/AuthorityDurable/README.md), is the separately authorized Stage S storage experiment. Its precise command is `tools/Test-H2AuthorityDurable.ps1 -EvidenceRoot <new Artifacts/Persistence/AuthorityDurable child>`. The wrapper verifies existing tool pins, locks/restores/builds only this project's existing Npgsql dependency graph, provisions three new loopback databases with isolated roles, runs synthetic fault cases and tears down only owned resources. The historical three-project dependency verifier is unchanged; this fixture's build/restore/evidence is separate. It uses trusted authorization/proof stubs and installs no private CA or production services. Private-CA direction and policy/fixture scope authorization are recorded [here](../docs/decisions/B-03-B-04-stage-s-approval.md).
