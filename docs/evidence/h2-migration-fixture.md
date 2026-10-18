# H2 dependency locks and isolated migration evidence

Date: 2026-10-18. Scope: user-selected C#/.NET 10, separate domain services and restricted runtime roles, exercised only through a synthetic Building migration fixture. [Workflow/commands](../../BackendServices/README.md), [decision](../decisions/PER-01-B-implementation-readiness.md), [fixture contract](../decisions/PER-01-migration-fixture-contract.md), [machine-readable summary](h2-migration-fixture-summary.json). Independent final review passed after the desktop crash; results below were inspected from saved evidence, not inferred from file presence.

## Dependency evidence

- Backend SDK `10.0.402`, runtime/ASP.NET `10.0.13`; portable PostgreSQL `18.6` Windows x64. Exact sources, archive hash provenance and entrypoint/compiler hashes are in [toolchain lock](../../BackendServices/toolchain-lock.json). No global SDK/PATH change or registered database service.
- `Verify-H2Dependencies.ps1` completed locked restores and clean Release builds of all three projects. All 20 resolved packages in the union of their lock graphs passed NuGet signature verification and NuGet content-hash comparison. Signed package content hashes exclude signing data; they are not raw ZIP SHA512. The verifier uses NuGet's own verification output for that scope.
- Three NuGet advisory queries reported zero vulnerable packages on this run. This is a dated feed observation, not a full native-component or unknown-vulnerability audit. License metadata covers MIT, Apache-2.0, BSD-3-Clause and PostgreSQL expressions; `librdkafka.redist` retains its bundled multi-license notices. No distributable image or legal approval is produced.
- Protobuf build tool reports `libprotoc 35.1`, with compiler/plugin hashes pinned. The dependency probe loads the selected assemblies, roundtrips a standard protobuf value and loads Windows native Kafka `2.16.0`; it opens no service connection. No gameplay schema generation/cross-language canonicalization or Linux native test is implied.
- An isolated dependency-pin change while retaining the original lock was rejected with `NU1004`; tracked package files were not changed by this negative check.

Raw local dependency evidence: `Artifacts/Persistence/DependencyLock/verified-04/` (summary, restore/build logs, three audits, per-package signature records, namespace-correct license inventory); negative check under `negative-locked-restore/`. Toolchains/packages and raw logs are ignored and not shipped through Git.

## Database evidence

Final coordinator-run evidence: `Artifacts/Persistence/C-MIG/fixture-06/`; fixture ID `87329348-5146-401d-9634-2b78c1d82dc6`, loopback port61544, new database `h2mig_873293485146401d96342b78c1d82dc6`. Cleanup completed at 15:51:33 UTC (11:51:33 America/New_York). Harness exit0; **46 assertions passed, zero failed, zero explicitly unproven branches**. This final rerun uses the LF manifest bytes committed to Git. This count is the implemented synthetic catalogue, not certification of every production migration case.

| Contract group | Observed bounded outcome |
| --- | --- |
| C-MIG-01/02 | Empty/repeat converged once; raw SQL-byte/checksum drift, reordered/gapped/unknown history, unsupported source and completed PK/index drift were refused. |
| C-MIG-03/04 | Backend termination before/after transaction commit recovered the matching durable prefix; competitors returned busy while session lock spanned normal and concurrent-index boundaries. |
| C-MIG-05 | Loopback relay closed the runner's connection while retaining its upstream socket; PostgreSQL lock count remained positive and direct competitor returned busy. Closing upstream released the session; retry succeeded. No firewall change. |
| C-MIG-06/07 | Real conflicting transactions caused DDL lock timeout `55P03` and cancellation `57014`; cancellation left an actual invalid concurrent index. Crashes at cleanup-pending and dropped-index boundaries recovered to one valid index/completed history while preserving error attempts. |
| C-MIG-08 | Synthetic old/new table projections read/write the expanded fixture; unsupported schema readiness was refused. No old/new gameplay binaries are delivered. |
| C-MIG-09 | Runtime DML/read-only SELECT worked; runtime DDL/history mutation/owner assumption, read-only write/sequence advance and cross-domain reads/writes/DDL were denied. Newly created object grants and unchanged World sentinel were checked. |
| C-MIG-10 | Real marker mismatch, forged matching marker with wrong actual cluster identity, wrong database and nonloopback endpoint were refused. Final run confirmed owned cluster stopped/private storage removed, retaining evidence. |

The tested runner DLL SHA256 was `e2f030f8b7727c44f05827ba900d6c5ad613e5daffd428c7f35fc8129392c8bb`; tested migration manifest SHA256 `f08b3f2da350c6324f440e1311a5d8398d0ca2556c3034e1154a9d55b7339f65`. The runner is a fixed `cmig-v1` synthetic catalogue, not an arbitrary production SQL executor. Credentials remained in private temporary files/child environments, outside committed source and evidence.

## Review, cleanup and remaining gates

Two stronger authors implemented runner and harness; parent implemented locks/tool preparation/probe/partition injector/dependency verifier. Independent source review corrected incomplete-start cleanup, actual-server/marker verification and PK/identity postconditions. After the desktop crash a fresh separate reviewer checked final runtime evidence and identified a probe-only dependency-verification gap; the verifier now covers all project graphs. Recheck independently matched all 20 content hashes/signature records, three audit reports and the tested runner/manifest hashes. No blockers remain in this bounded scope.

Early failed run01's PostgreSQL instance was stopped and verified by `pg_ctl status`. Its ignored `.private-aee8fde1f4b2413bae4178339e473972` directory remains because automatic approval review rejected a separate literal-path recursive deletion as “blocked by policy,” without more detail. No workaround was attempted; later fresh runs' normal owned cleanup passed. Do not describe all historical artifacts as removed.

B-01 fixture direction is selected and B-02/isolated C-MIG have bounded Windows evidence. Full production B-02, B-03/B-04 authority/enrollment, B-05 schema/vector acceptance, protected-owner extensions, real Inventory/World participants, DR/load and production deployment remain open. This evidence does not close gameplay PER-01/DEC-02. Next: resolve the already documented authority/profile decisions, then implement separately scoped shared byte vectors and compatible domain adapters. Reuse this fixture and exact commands; do not repeat toolchain inventory or touch the live E1 host.
