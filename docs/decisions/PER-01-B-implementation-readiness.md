# PER-01-B — implementation candidates and owner decision packet

Status: fixture baseline selected by the user, 2026-10-18. The user approved C#/.NET 10, separate backend services and restricted runtime database roles, and explicitly requested dependency locks and an isolated migration fixture. This records B-01 design direction and B-02 evidence scope for that fixture; B-03/B-04/B-05 and production/protected-domain acceptance remain open. Implementation/evidence entry: [backend workflow](../../BackendServices/README.md). The original candidate observations below remain dated; executable pins live in `BackendServices/`.

## Goal and decisions

Make the next implementation assignment bounded: one Building migration harness and shared contract vectors before any gameplay mutation. Retain separate Building, World and Inventory writers, local transactions, immutable decisions, held results and all-participant completion from [Construction Integration §§3–5](../Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md). Runtime delivery, protected-owner changes and production durability are separate gates.

| Decision / owner | Proposed choice | Acceptance evidence still required |
| --- | --- | --- |
| B-01 / architecture + persistence + Backend/Security role owners | C# `net10.0`; separate domain executables; authored SQL/Npgsql; existing `pg-world` and per-realm `pg-items` boundaries; proposed runtime ownership refinement below | Recorded acceptance of topology, proposed `BackendServices/` paths, migration and role design; no cohosted cross-domain transaction |
| B-02 / build + operations | Exact candidates below; Linux x64 Ubuntu Noble containers; Windows x64 development SDK | Compatible locked restore/build, licenses/advisories/native dependencies, artifact provenance and disposable fixture provisioning |
| B-03 / Backend + Security + World/Character owners | [Gateway intent/current-zone endorsement and durable actor grants](B-03-actor-admission-contract.md), with separate pre-action/allocated bindings | Caller/identity amendments, authority-only participation, grant-wins/restriction bridge and delayed-handoff tradeoff remain proposed; no gameplay adapter until resolved |
| B-04 / persistence + World + Security/operations | [Concrete delivery/restore proposal](B-04-delivery-fencing-and-restore.md); local barriers and external-proof enrollment | Logical contract reviewed with bounded in-memory races; real wire/storage, proof authority/frontier eligibility and old-workload fencing evidence remain open |
| B-05 / shared-contract + domain owners | Separate RPC and semantic hash profiles from PER-01; one bundled protobuf toolchain; [restricted profile draft](INT-02-command-hash-profile.md) | Accepted schema/profile dispatch, rolling compatibility, compiler identity and cross-language byte vectors; terrain representation through DEC-03 remains separate |

B-01 fixture direction and B-02 evidence scope were approved directly by the user on 2026-10-18 against this packet and the migration fixture contract, authorizing the Building synthetic runner, locks, local provisioning and focused checks. Runtime-role refinement applies to this fixture; the owning production role contract is not silently amended. Use two recorded stages: **scope approval** permits the named evidence-producing assignment; **closure** accepts its exact reviewed artifacts and evidence for dependent implementation. B-03…05 remain proposed. Each record names authority, date, artifact revision, stage, scope and evidence; review/version lookup alone is not acceptance.

## Baseline alternatives for the user decision

The user requested pros/cons and AAA examples, then selected the recommended fixture baseline on 2026-10-18. Language, process layout and privileges are independently selectable. The comparison below records the rationale, not measured performance or production acceptance.

| Language | Advantages for this project | Costs / adoption consequence |
| --- | --- | --- |
| C#/.NET 10 (recommended) | Typed application code and integrated ASP.NET Core/gRPC fit service rules, transaction handling and recovery workflows; LTS through November 2028 | Adds a toolchain alongside Unreal C++; garbage collection/allocation needs profiling; package graph still unverified |
| Go | Built-in concurrency and a relatively small language suit network services | Different package/toolchain selection; garbage collection remains; same durable-action protocol and correctness obligations |
| Rust | Memory safety without garbage collection suits tightly controlled resource/latency needs | Ownership/async implementation and review complexity; select for maintainer preference or measured need, not assumed capacity |
| TypeScript/Node | Reuses the browser prototype's language and supports rapid application work | Revises Backend's C# / Go / Rust shortlist; CPU work must avoid event-loop blocking; prototype SQLite is not the proposed service storage |

Capability references: [ASP.NET gRPC](https://learn.microsoft.com/en-us/aspnet/core/grpc/aspnetcore?view=aspnetcore-10.0), [.NET support](https://dotnet.microsoft.com/en-us/platform/support/policy), [Go FAQ](https://go.dev/doc/faq), [Rust](https://rust-lang.org/), [Node event-loop guidance](https://nodejs.org/en/learn/asynchronous-work/dont-block-the-event-loop). No language benchmark or relative hosting-cost claim has been established here. Unreal gameplay remains in its existing C++ project under any backend selection.

| Layout | Advantage | Cost / recommendation |
| --- | --- | --- |
| Separate domain processes, initially on one host | Separate credentials and process boundaries; domain deployment/scale can evolve independently | More calls and failure paths; recommended starting layout subject to isolation/resource evidence. One host is a common failure domain, not tier-0 availability |
| One modular backend process | Fewer deployments and simpler local debugging | Weaker process isolation; revise credential boundaries and revisit the separate-deployable proposal before adoption; no automatic cross-domain transaction permission |
| Separate hosts immediately | Stronger host failure/resource isolation | More infrastructure and operating cost; capacity/failure evidence should justify it. Does not by itself meet the database's AZ/DR requirements |

For privileges, recommend a non-login object owner, migration-only ownership access and restricted runtime DML as the fixture contract specifies. Runtime schema ownership is simpler initially but permits altering/dropping owned objects; separating credentials alone does not remove that power ([PostgreSQL ownership/privileges](https://www.postgresql.org/docs/17/sql-grant.html)). The fixture role design does not amend the owning production Backend/Security contract without its separate acceptance.

The fixture-baseline selection authorizes B-01's isolated direction and B-02 evidence scope. It does not close B-02 before locked build/provisioning evidence, approve B-03/B-04, release protected specs or prove production readiness.

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
| Gateway intent + current-zone endorsement → Building | B-03 recommends gateway-validated actor intent plus Auth-owned opaque intent binding; Building separately obtains current-zone facts bound to `srvi_`, realm/allocation/owner epoch and exact intent. Zone facts cannot assert gateway identity or replace durable actor admission. Stale/unprovable bindings reject before reservations. Caller amendments and grant/revocation/handoff linearization remain owner decisions. |
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
| [INT-02-PROFILE](INT-02-command-hash-profile.md) | Shared `.proto`/profile/byte fixtures; no gameplay adapter | Documentation draft prepared; owners approve profile scope/toolchain and relevant B-03 binding proposal before executable authoring. This assignment produces schema/compiler/vector evidence for B-05 closure, not its own entry gate. Include wire rejection, rolling N/N−1/profile dispatch, scoped keys/replay and generation-independent semantic hashes. Terrain subset awaits DEC-03. |
| [B-AUTH/FENCE](PER-01-authority-fencing-contract.md) | Separately assigned owning contracts and isolated failure vectors | Proposed logical contract prepared; B-03/B-04 scope/owner decisions and exact wire/storage definitions remain open. No protected writes until released. Define enrollment and actor-binding contracts before enabling mutation handlers. |
| [B03-OWNER-PACKET](B-03-owner-decision-packet.md) | Response form only; seven exact owner decisions and sourced alternatives | Record authority/date/revisions and scope approval separately from closure; protected amendments need assigned ownership. No responses are implied by completing the form's design. |
| [B04-DURABLE-PACKET](B-04-durable-fixture-assignment.md) | Assignment design only; staged storage, test-authenticated transport and operational restore evidence | Exact stage scope/contracts precede executable authoring; synthetic authority/proof adapters remain declared assumptions. Independent document review does not accept production interfaces or external fence proof. |

Initial preparation was documentation-only. User-selected fixture implementation, locked dependency checks and focused database evidence are now tracked in [H2 fixture evidence](../evidence/h2-migration-fixture.md). That bounded result does not accept authority/profile contracts, production topology or protected-owner changes.
