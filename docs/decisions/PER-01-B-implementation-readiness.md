# PER-01-B — implementation candidates and owner decision packet

Status: proposed, 2026-10-18. Documentation preparation for [PER-01 topology](PER-01-durable-state-topology.md), not owner acceptance or an executable dependency lock. No SDK installation, package restore, backend code, database, service start or host change was performed. The topology packet's source boundary and protected-domain ownership apply.

## Goal and decisions

Make the next implementation assignment bounded: one Building migration harness and shared contract vectors before any gameplay mutation. Retain separate Building, World and Inventory writers, local transactions, immutable decisions, held results and all-participant completion from [Construction Integration §§3–5](../Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md). Runtime delivery, protected-owner changes and production durability are separate gates.

| Decision / owner | Proposed choice | Acceptance evidence still required |
| --- | --- | --- |
| B-01 / architecture + persistence + Backend/Security role owners | C# `net10.0`; separate domain executables; authored SQL/Npgsql; existing `pg-world` and per-realm `pg-items` boundaries; proposed runtime ownership refinement below | Recorded acceptance of topology, proposed `BackendServices/` paths, migration and role design; no cohosted cross-domain transaction |
| B-02 / build + operations | Exact candidates below; Linux x64 Ubuntu Noble containers; Windows x64 development SDK | Compatible locked restore/build, licenses/advisories/native dependencies, artifact provenance and disposable fixture provisioning |
| B-03 / Backend + Security + World/Character owners | Authenticated server-to-actor binding and action-bound participant authorization; [authority draft](PER-01-authority-fencing-contract.md) proposes durable admission reservations | Owning contract/allowlist changes resolving gateway-only callers, authority-only participation, durable admission and grant lifetime across revocation/handoff; no executable zone adapter until resolved |
| B-04 / persistence + World + Security/operations | Per-action durable generation barriers; separately enrolled restore incarnation | Exact messages, trust/enrollment contract, race vectors and proof that old database/workloads are fenced |
| B-05 / shared-contract + domain owners | Separate RPC and semantic hash profiles from PER-01; one bundled protobuf toolchain | Schema/profile/compiler identity and cross-language byte vectors, including terrain representation through DEC-03 |

All five rows remain proposed. Use two recorded stages: **scope approval** permits only the named isolated evidence-producing assignment; **closure** accepts its exact reviewed artifacts and validation evidence for dependent implementation. Scope approval is not mutation authorization or full decision closure. Each record must name the authority, date, artifact/profile revision, stage, scope and evidence; a review or version lookup does not fill that record. Rejection records the alternative and affected dependent packet instead of silently changing the protocol.

## Dependency candidate manifest

Observed on 2026-10-18 from publisher metadata. Exact versions are candidates for the first restore, not a claim that their combined graph has been restored, vulnerability cleared or licensed for distribution. No floating versions or `latest` tags should enter the accepted build.

| Component | Exact candidate | Publisher evidence |
| --- | --- | --- |
| .NET SDK / runtime / ASP.NET Core | `10.0.402` / `10.0.13` / `10.0.13`; release 2026-10-13 | [Microsoft release metadata](https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json) |
| gRPC host / client / build tools | `Grpc.AspNetCore`, `Grpc.Net.Client`, `Grpc.Tools`: all `2.84.0` | [Host](https://www.nuget.org/packages/Grpc.AspNetCore/2.84.0), [client](https://www.nuget.org/packages/Grpc.Net.Client/2.84.0), [tools](https://www.nuget.org/packages/Grpc.Tools/2.84.0) |
| Protobuf runtime | `Google.Protobuf` `3.36.2` | [Package](https://www.nuget.org/packages/Google.Protobuf/3.36.2) |
| PostgreSQL driver | `Npgsql` `10.0.3` | [Package](https://www.nuget.org/packages/Npgsql/10.0.3) |
| Kafka client | `Confluent.Kafka` `2.16.0` | [Package](https://www.nuget.org/packages/Confluent.Kafka/2.16.0) |
| Telemetry hosting / ASP.NET instrumentation / OTLP exporter | `OpenTelemetry.Extensions.Hosting`, `OpenTelemetry.Instrumentation.AspNetCore`, `OpenTelemetry.Exporter.OpenTelemetryProtocol`: all `1.20.0` | [Hosting](https://www.nuget.org/packages/OpenTelemetry.Extensions.Hosting/1.20.0), [instrumentation](https://www.nuget.org/packages/OpenTelemetry.Instrumentation.AspNetCore/1.20.0), [exporter](https://www.nuget.org/packages/OpenTelemetry.Exporter.OpenTelemetryProtocol/1.20.0) |

NuGet registration publication timestamps for these versions precede this observation date. Nuspecs advertise `net10.0` for host/client, Npgsql, Kafka and the three telemetry packages; Protobuf advertises `net8.0`/`netstandard2.0`, and build-only Grpc.Tools advertises `netstandard2.0`/`.NETFramework4.5`. Those declarations alone do not prove generated-code/runtime compatibility. Publisher license expressions are Apache-2.0 except Protobuf BSD-3-Clause and Npgsql PostgreSQL; transitive notices remain unchecked. Metadata was read from NuGet's version-specific registration and flat-container nuspec endpoints.

Proposed immutable Linux/amd64 base-image references, verified by MCR manifest digest headers and image-config OS/architecture (no image pulled or run):

```text
mcr.microsoft.com/dotnet/sdk:10.0.402-noble@sha256:0d99ebf1ccad15037b5c98c9fe8979d6b8dbac23812feae2277a0b277b5c5ffa
mcr.microsoft.com/dotnet/aspnet:10.0.13-noble@sha256:d5633778c754e89a386882eddf8dbee6942447a8a4b8267c19cf1e1158d0b0ac
```

Sources: [SDK manifest](https://mcr.microsoft.com/v2/dotnet/sdk/manifests/10.0.402-noble), [ASP.NET manifest](https://mcr.microsoft.com/v2/dotnet/aspnet/manifests/10.0.13-noble), using Docker schema-2 manifest media-type negotiation. Pins must be revalidated by digest and included in the accepted build record; tags alone are insufficient. Image vulnerability assessment and native Kafka loading remain unperformed.

After B-01 design acceptance and B-02 scope approval, create a backend-scoped `global.json` selecting SDK `10.0.402`, `rollForward: disable`, `allowPrerelease: false`; central exact package versions; a NuGet source policy; and committed per-project `packages.lock.json` files containing resolved transitive versions/content hashes. Generate the graph once through an inspected restore, then require `dotnet restore --locked-mode`. B-LOCK produces the evidence for B-02 closure; that closure is not required to begin B-LOCK. Keep the SDK selector inside `BackendServices/` so it does not change Unreal tooling. [Microsoft SDK selection](https://learn.microsoft.com/en-us/dotnet/core/tools/global-json) and [NuGet lock semantics](https://learn.microsoft.com/en-us/nuget/consume-packages/package-references-in-project-files#locking-dependencies) describe the mechanism.

Use Grpc.Tools as a private build dependency, not an independent PATH `protoc` install. Record the restored tools package content hash, bundled `protoc --version`, C# plugin/tool executable hashes and generated schema descriptors before B-05 can close. A package version is not yet that compiler/vector evidence. Record exact PostgreSQL server/pooler and disposable test-framework dependencies in the same lock/provisioning pass; no server version, image digest, provider or fixture command is selected here without operations compatibility evidence. Kafka/Redis/certificate infrastructure is not needed for a migration-only harness and cannot be replaced with silent no-op production participants.

## Local evidence and provisioning boundary

Read-only PowerShell checks from the repository root: `Get-Command dotnet,docker,protoc -ErrorAction SilentlyContinue`, `dotnet --list-sdks`, `dotnet --list-runtimes`, and `Test-Path` for the proposed backend directory and root package files.

- `dotnet` resolves to `C:\Program Files\dotnet\dotnet.exe`. Only SDK `8.0.420` was listed. Runtimes: ASP.NET Core `8.0.26`; .NETCore and WindowsDesktop `8.0.26`, `9.0.15`, `10.0.7`. Runtime 10 does not supply SDK 10 or ASP.NET Core 10.
- `docker` and `protoc` were not found on PATH; this does not establish absence from disk. No daemon or service was contacted.
- `BackendServices/` and root `global.json`, `NuGet.Config`, `Directory.Packages.props`, `packages.lock.json` were absent. These specific checks do not claim an exhaustive tool/dependency inventory.

Before the harness: record a verified SDK source/hash/install location, exact isolated PostgreSQL artifact/version, connection route and separate migration/runtime/read-only roles, fixture creation/cleanup commands and evidence directory. Target only a new disposable local fixture; never the browser SQLite database, live E1 server or Windrose. No production-like availability claim follows from a one-node fixture. Missing local provisioning blocks execution, not further contract documentation.

## Authority and recovery contract proposal

These are logical fields and validation rules for owner review, not registered RPCs, tokens or a new authentication issuer. Reuse existing mTLS/SPIFFE and [Backend §§2–3](../Game-Design/Backend/Legends_Shattered_Frontier_Backend_Service_Communication_Persistence_and_Content_v1.0.md); do not select a signing format or grant a zone gateway identity to bypass the unresolved caller conflict.

| Boundary | Required binding and failure behavior |
| --- | --- |
| Zone → Building intent | Authenticate the registered `srvi_` workload; bind realm, actor/account/session/character, active presence/allocation and zone epoch, operation and exact intent hash. Verify ownership/presence through owning authority, not body IDs or gateway-only headers. A stale or unprovable binding rejects a new action before reservations. Owners must define the revocation/validation linearization and how the resulting permission fence survives prepare. |
| Building → participant prepare | Bind action ID/hash/profile, fixed participant ID/set, realm/actor, claim/operation, source/beneficiary proposal, permission and geometry revisions, zone evidence, restore incarnation and delivery generation. Participant verifies the server-authenticated action-bound authorization and its own source/revision rules. Authentication cannot substitute for automatic-supply or claim capability. |
| Decision / completion | Reference immutable Building evidence: action/hash, original decision identity/generation and prepared digest set; completion additionally binds every applied digest. Authenticated lookup must match the participant's own durable receipt and current delivery authority. Preserve historical decision generation when a later worker delivers it. Missing authority yields pending, never a caller-selected decision. |
| Restore enrollment | A new opaque incarnation is enrolled through an authenticated operations/World/Security contract after old primaries/workloads are fenced. A random caller-provided incarnation or numerically larger generation cannot enroll itself. Each participant persists the admitted incarnation; mismatches reject until explicit reconciliation/admission. |

Proposed generation representation: positive signed 64-bit integer (PostgreSQL `bigint`), initially 1, incremented with the action-row transaction; overflow fails closed and requires owner intervention, never wraps. It is comparable only within the admitted incarnation/action. Worker identity is bound to generation. These are release-bound schema/invariants, not Feature Control knobs.

Proposed barrier behavior (requires an owner-approved wire extension):

1. Building atomically advances the action's generation and records the replacement identity. Every subsequent local progress/decision/completion write checks both incarnation and generation under that row lock. Original decision/certificate bytes remain immutable.
2. The replacement delivers an authenticated barrier to each fixed participant, including Building's local geometry participant. The participant serializes barrier and phase writes on one durable action record. For an absent record, barrier creates an identity-bound empty record/tombstone so a delayed older prepare cannot recreate authority. Existing action/hash mismatch rejects.
3. Barrier acknowledgement is durable and returns the highest admitted generation and immutable prepared/applied/completed receipts. Same-generation retry must be associated with the same worker; lower generations reject. A higher generation requires authenticated current Building authority, not integer comparison alone. Network authority lookup occurs outside the local lock; the local stored generation serializes effects, so a later barrier fences a delayed verified call.
4. After takeover, Building permits no new decision, conflicting reservation or completion until all barriers/receipts reconcile. A missing participant leaves recovery pending. If an old call commits before its barrier, its durable receipt must be returned and incorporated; after that barrier it rejects. Ordinary first-generation execution still requires all normal prepare/fence checks.
5. Reconcile the already durable decision first. Commit rolls forward; only an undecided action can abort after the DB-clock deadline. A late old worker cannot publish a new decision because its conditional Building write fails. A previously durable commit can still be delivered under current authority; it never becomes a fresh charge or abort. Activation always requires the original matching completion certificate.

Barrier acknowledgement is not application success and cannot release a prepared hold. Read-side projection/occupancy gates remain in place through partial activation. Required race vectors: absent participant + delayed prepare; old apply immediately before/after barrier; delayed old completion; takeover after durable decision with lost reply; unavailable participant; generation overflow; forged higher generation; restored lower generation with old workloads still alive. For each, assert stable action/receipts, conserved material and no mixed visible projection. B-04 remains open until the exact enrollment, lookup/phase authorization and wire/storage contracts have source-backed owner acceptance and executable vectors.

## Migration runner refinement

PER-01's transaction-scoped mutex protects ordinary atomic migrations. For a complete job including nontransactional concurrent-index steps, additionally hold a per-schema session advisory lock on a dedicated direct PostgreSQL connection for the entire run. Do not use transaction-pooling PgBouncer for this connection. All supported runner invocations must share the same stable registered lock identity; jobs cannot silently choose a new ID. Connection loss stops the job; a retry takes the same lock and inspects durable history/index validity before proceeding. No network call to another domain occurs under a gameplay row lock.

Each normal migration and checksum/history update remains one transaction; nontransactional steps have explicit resumable states and verification before completion. Use a separate migration credential and the inherited DDL timeouts. Test two simultaneous jobs at the nontransactional boundary, invalid concurrent index cleanup/retry, lost connection, changed checksum, incompatible schema, and runtime credential DDL denial. This is an explicit refinement of the runner proposal, not a change to Construction's gameplay transaction protocol.

Role conflict requiring B-01 acceptance: Backend §6.4 currently makes runtime `svc_<schema>` the schema owner. Ownership permits DDL; a separate `mig_` credential alone cannot enforce runtime DDL denial. Propose a non-login domain owner role, migration-only ability to assume that role, and explicit necessary DML/sequence grants to runtime with no owner membership or schema-create privilege. Migration objects/default privileges must consistently use that owner. This changes the owning role contract and needs Backend/Security agreement before implementation; do not claim denial under the existing baseline. C-MIG's DDL-denial assertion is conditional on that approved refinement; if rejected, record the accepted ownership risk/alternative and revise its acceptance checks. Cross-domain isolation remains mandatory under either choice.

## Next assignments and completion gates

| Packet | Fixed write set / goal | Entry and precise evidence |
| --- | --- | --- |
| B-ACCEPT | This packet and topology proposal only; record B-01…05 staged decisions or alternatives | Accepting owners and referenced revisions; distinguish scope approval from evidence-backed closure, without protected-domain edits or inferred approval |
| B-LOCK | New backend build/lock files and isolated fixture instructions after B-01 design acceptance and B-02 scope approval | SDK and base digests, full NuGet closure/compiler identities, advisory/license/native checks, PostgreSQL pin and exact create/cleanup commands; restore/build evidence closes B-02, no gameplay mutations |
| [C-MIG](PER-01-migration-fixture-contract.md) | Building migration runner plus disposable fixture only | B-01 design/role acceptance and B-02 closure from B-LOCK; empty/repeat/checksum/concurrent/interrupted migration, role isolation and schema refusal. Other authority decisions need not block this isolated non-gameplay harness. Inventory schema excluded. |
| INT-02-PROFILE | Shared `.proto`/profile/byte fixtures; no gameplay adapter | Owners approve profile scope/toolchain and relevant B-03 binding proposal before authoring; this assignment produces the schema/compiler/vector evidence that closes B-05, so B-05 closure is not its entry gate. Positive/negative C#/UE-language vectors; unknown/duplicate fields, optional/default presence, scoped keys, replay/mismatch and generation-independent semantic hash. Terrain subset awaits DEC-03. |
| [B-AUTH/FENCE](PER-01-authority-fencing-contract.md) | Separately assigned owning contracts and isolated failure vectors | Proposed logical contract prepared; B-03/B-04 scope/owner decisions and exact wire/storage definitions remain open. No protected writes until released. Define enrollment and actor-binding contracts before enabling mutation handlers. |

Independent review and Markdown/link checks complete this bounded preparation only. B-LOCK, accepted authority/profile contracts and required protected-owner releases remain implementation gates. No runtime test was run for this documentation change.
