# PER-01 — H2 durable-state implementation topology

Status: proposed, 2026-10-18. This makes [DEC-02](DEC-02-durable-actions.md) implementation choices reviewable; architecture, domain and operations acceptance remain open. No service, migration, credential, deployment or test is delivered here. The existing E1 server and browser SQLite harness remain unchanged.

## Source boundary

Binding names and behavior come from:

- [Construction Integration](../Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md), §§1, 3–5, 7–8: separate transactions, sole writers, immutable decision, completion and retained identity.
- [Backend communication/persistence](../Game-Design/Backend/Legends_Shattered_Frontier_Backend_Service_Communication_Persistence_and_Content_v1.0.md), §§2–4, 6: authenticated gRPC, key hashing, durable outbox, region/schema isolation, roles and migrations.
- [Backend stack](../Game-Design/Legends_Shattered_Frontier_Backend_Architecture_v1.0.md#14-technology-stack), [Database](../Game-Design/Legends_Shattered_Frontier_Database_Architecture_v1.0.md), and [Registry](../Game-Design/Legends_Shattered_Frontier_Shared_Registry_v1.0.md), §§1, 3–4, 10: allowed implementation families, canonical service/schema/ID/event names.
- [Repository Structure](../Game-Design/Legends_Shattered_Frontier_Repository_Structure_v1.0.md), Persistence and schema placement, and [backlog](../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md), INT-01/02 and PER-01…04: implementation placement, dependency and acceptance boundaries.
- [Recovery profile](DOC-08-recovery-profile.md), [session applicability](DEC-04-session-applicability.md) and [compatibility](DEC-08-compatibility.md): reviewed proposals with unresolved owner decisions; none supersedes its domain source.
- [GM architecture](../Game-Design/Legends_Shattered_Frontier_Game_Master_Architecture_v0.1.md#12-scoped-player-and-instance-recovery), §12: requested durable recovery provenance, pending domain contracts.

New physical record names, envelope layouts and implementation paths below are proposals, not additions to the Shared Registry. Protected Inventory, Character, Quest, Dungeon, Raid, Security, LiveOps and Analytics contracts remain read-only.

## Proposed deployables and implementation stack

Select C# on .NET 10 for the three H2 service implementations. Select ASP.NET Core/Kestrel plus `Grpc.AspNetCore`, `Grpc.Net.Client` and `Google.Protobuf` for gRPC; `Npgsql` for explicit parameterized SQL/local transactions; `Confluent.Kafka` for the Backend outbox relay; OpenTelemetry .NET for existing observability. Use authored SQL rather than ORM-generated migrations. These are release-bound selections, not claims that packages are installed, audited or engine-compatible. Before code begins, owners must pin exact SDK/runtime, package, protobuf compiler and base-image identities and verify license, native dependencies and security compatibility in a lock manifest.

| Deployable proposal | Process and ownership |
| --- | --- |
| `building-service` | One domain executable/container: RPC handling, durable action coordination, fenced recovery worker, structure/claim persistence and own outbox relay. Multiple replicas are permitted only with the takeover rules below. |
| `world-service` | Separate executable/container and credential. Owns terrain prepare/apply/activate, durable revisions and World-owned allocation/authority integration. This packet does not implement the entire World catalogue. |
| `inventory-service` | Separate executable/container and credential; protected owner implements existing participant methods and private ledger/holds. Building never imports its DB model or writes its schema. |
| Per-domain migration job | Same pinned implementation artifact with an explicit migration entry point, separate migration credential, no gameplay traffic. One writer per migration history. |
| `zone-server` | Existing Unreal dedicated server; future server-only asynchronous gRPC adapter validates live intent and submits to Building. No DB credentials or blocking service call on the simulation tick. |

Cohost these containers on selected backend compute if isolation/resource evidence permits; keep network/process identities and credentials distinct. Kafka-compatible `event-bus`, pooler, PostgreSQL, Redis and certificate issuance remain platform dependencies, not substitutes inside the service processes. A one-box developer fixture may emulate the topology but cannot claim production tier-0 availability or authorize deployment changes.

Alternative Go or Rust implementations remain compatible with Backend's language allowance, but adopting several languages for these first participants adds codec/tooling work without current measured benefit. A single executable holding all domain credentials weakens enforceable ownership; a cross-schema single transaction contradicts Construction §1. Neither is selected. Revisit compute separation on measured resource/failure isolation needs; language changes require shared-vector and compatibility evidence.

## Store, record and migration ownership

Adopt Backend §6.1 rather than inventing a single MVP database: regional `pg-world` holds `world` and `building`; `pg-items` holds `inventory` in one database per realm. Tier 0 requires the stated primary, synchronous cross-AZ standby (`remote_apply`) and DR replica. Host/provider, capacity and credentials remain DEC-09/operations decisions. If an MVP cannot meet this baseline, obtain an explicit scoped exception; do not describe a lone database as equivalent durability.

| Owner | Logical records required before mutations |
| --- | --- |
| Building, `building` | Action and atomically associated boundary key; fixed participant set; request hash/version; pinned content/config; authorization/geometry fences; deadline; coordinator generation; staged structures; prepared/applied receipts; immutable decision; completion certificate; stable result; pending delivery; retained tombstones and contribution references. |
| Inventory, `inventory` | Existing owner/container/lot order; all-or-none reservations and allocation provenance; participant request association; immutable preparation/application receipts; ledger and action-held outputs; completion release receipt; refund entitlement conservation. Physical schema remains its owner's deliverable. |
| World, `world` | Terrain intents with exact before/after heads, immutable deltas and digests; coordinator and zone authority references; applied-but-inactive projection; activation receipt; pending delivery and terrain provenance. Preserve existing World codec/revision contracts and DEC-03 gates. |
| Each owner | Own generic RPC key cache, outbox/inbox and migration history. No cross-schema FK/join for integrity; resolve external IDs through authenticated contracts/reconciliation. |

Use `svc_building`, `svc_world`, `svc_inventory` and the corresponding `mig_<schema>`/`ro_<schema>` roles as Backend specifies. Keep runtime credentials out of migration jobs and migration credentials out of running workloads; no cross-domain grants. `inventory` routes through its realm database; `building`/`world` use required realm columns and compound uniqueness wherever identity is realm-scoped. Sensitive columns follow existing encryption/erasure policy; no new staff DB access.

Proposed paths: `BackendServices/{building-service,world-service,inventory-service}/` for owners, `BackendServices/Shared/Contracts/` for canonical `.proto` sources and hash-profile fixtures, and each owner's `Migrations/NNNN_<name>.sql`. Shared contracts contain wire primitives, never inventory or terrain algorithms. Confirm these paths with Repository ownership before creation.

Select a small Npgsql-based SQL migration runner in each domain artifact: checksum-verified ordered files, per-schema history and a transaction-scoped migration mutex. Ordinary DDL applies atomically with its history row. Explicit nontransactional concurrent-index steps use an inspected resumable state and validity check before marking completion; an interrupted index is not accepted by name alone. No automatic down-migration or runtime startup DDL. This runner itself needs targeted tests before the first schema is applied.

Pipeline: verify target/environment and compatible source schema → establish eligible backup/restore frontier → apply additive expansion → verify history/checksums and N−1 compatibility → deploy gated binaries → bounded backfill/dual-write validation → switch reads → later approved contraction. Retain Backend `lock_timeout=2s`, DDL `statement_timeout=60s`, concurrent indexes and ≤5% primary CPU backfills outside the DDL transaction. Destructive changes need the existing release-manager approval. Reject unknown/newer incompatible schema before serving; code rollback requires a compatible schema/data pair, per DEC-08.

## Transport and authority

Use the existing gRPC participant methods, packages and phases in Construction §5, over Backend mTLS with SPIFFE workload identities, per-RPC default-deny `authz.yaml` and the registered zone `srvi_` certificate identity. Preserve metadata trust: gateway identity alone can assert Backend's gateway-only account/session/character headers. A zone's construction request uses its explicit domain actor/authority assertion; it cannot forge those gateway headers. The precise zone-to-actor/session assertion and allowlist need Backend/Security/World owner agreement before an executable adapter is enabled; the DEC-04 conflict is still open.

Building verifies the current zone allocation/ownership epoch and actor authorization, then binds validated geometry/permission revisions to the immutable action and held fences. Inventory verifies its action-bound server-authenticated authorization assertion, sources, beneficiary and fence. Transport identity alone does not confer claim rights. An old zone, realm mismatch, stale permission/revision, client-selected material return or forged certificate rejects without a new mutation. Preserve protected-content minimization on receipt queries.

Participant calls drive prepare/apply/complete; notifications do not drive a second material credit. Each owner commits its application/receipt/pending work in one local DB transaction, then performs network work outside row locks. Persist prepared/apply status before replying. Stable completed domain events enter owning outboxes only when the fact is completed; relay via the specified Kafka-compatible durable log with stable event IDs and existing deduplication. The outbox's Redis publisher lease is delivery coordination only; it is never the action-decision authority or takeover fence.

Backend per-call deadlines/retry policy remain unchanged. The Construction five-second undecided-action deadline is a distinct durable action deadline, not an RPC timeout or a prepared-hold TTL. Unavailable dependencies before commit permit a coordinator abort; after commit recovery rolls forward. Pending work must not occupy a row lock while waiting for a participant, certificate issuer, audit dependency or broker.

## Coordinator takeover and monotonic fencing proposal

The proposed physical implementation extends existing fencing semantics; it does not add a second decision maker. A PostgreSQL primary is the only authority for a Building action's current coordinator generation, decision and completion. Redis, local clocks and process leader status cannot authorize a decision. Each action fixes its complete required participant set before any prepare is sent.

1. Creation atomically records the boundary association, action/hash, original UTC deadline, initial generation and participant set. Generation is a monotonically increasing integer allocated/updated under the Building action row transaction, never reused by restarting a process. The active worker identity is recorded with it.
2. A replacement transaction locks the action row, reads any decision/completion, increments generation and assigns the new worker. Every coordinator progress/decision/completion write conditionally checks that generation in the same transaction. Old workers cannot commit after takeover even if their local leader belief or a delayed heartbeat says otherwise. No network call occurs under this lock.
3. Recovery establishes a durable generation barrier at every named participant and the Building geometry fence, querying existing prepared/applied state. Each participant serializes its barrier and phase calls on its action record: lower delivery generations reject after the barrier; an already committed old-generation local effect is reported and reconciled. Do not declare takeover safe merely because an online epoch lookup succeeded before a later DB write.
4. Until all barriers and prior receipts reconcile, the replacement must not create a conflicting abort, completion or new reservation. An unavailable participant leaves the action recovery-pending. Prepared votes bind immutable action semantics and remain evidence across delivery-generation changes; barrier updates cannot rewrite a recorded prepared digest or historical decision epoch.
5. With no prior decision, a current-generation conditional transaction can commit only after all durable prepared digests/fences match, or abort otherwise. Expiry is evaluated using the authoritative Building DB UTC clock after acquiring the action lock; do not use transaction-start `now()` captured before a long lock wait. Deadline crossing authorizes only an undecided abort, never a commit reversal. Clock anomalies may block/abort undecided work; they cannot release a prepared hold independently.
6. A persisted decision/certificate retains its original decision identity, hash, participant digests and epoch. Recovery transports that immutable evidence under the current delivery generation, rather than rewriting history. Participants validate evidence against the authoritative Building decision/completion through authenticated lookup and their own durable receipt; a bare high integer or caller-provided `commit` flag is insufficient.
7. Completion is a conditional local transaction after every required matching applied digest; only its durable certificate unlocks the exact held outputs/geometry. Delayed completion delivery is safe to replay. Loss of the primary/quorum, ambiguous commit acknowledgement or an unreconciled certificate produces pending status and original-key lookup, never timeout refunds or visible success.

An infrastructure failure must fence the old PostgreSQL primary before promoting/reopening a replacement. A restored database may contain lower numeric generations: PER-04 must fence all old workloads and use a new restore/allocation incarnation in authenticated delivery, then reconcile participants before admission. Comparison across incarnations cannot rely on numeric epoch alone. This is an operations/World/Security integration gate, not permission to elect independent writable primaries.

Zone ownership epoch is separate from coordinator generation. A new zone cannot validate with an old allocation certificate; already committed actions retain their original validation evidence and recover through the current owner. Live occupancy/geometry remains blocked until completion and safe activation. Neither coordinator takeover nor zone handoff is permission to revalidate a committed action into a different charge/result.

## INT-02 identity, keys and hashing

Keep two explicitly versioned hash scopes; do not silently substitute a JSON hash for Backend's protobuf RPC hash.

| Scope | Proposed exact boundary |
| --- | --- |
| Boundary action association | Authenticated realm, actor principal, owning service/RPC operation family and original key → one action/hash. Derive actor from trusted context; never from a client body alone. Association and action creation share Building's local transaction. |
| Generic RPC idempotency | Existing `(service,rpc,key)` uniqueness and SHA-256 of deterministic protobuf request with only key cleared. Preserve request-bearing fences/expected revisions. Use deterministic service-flow step keys for participants; a new recovery delivery generation changes its step key, not its `cba_` action. |
| Action semantic digest | SHA-256 of a versioned typed construction intent containing realm/actor, operation/quote identity, exact geometry/source/beneficiary proposal, expected revisions and pinned content/config/permission evidence. Exclude transport deadline, correlation metadata, delivery generation and retry key. Server-issued action ID is not a mutable client intent field. |
| Participant/certificate digests | Versioned typed immutable action ID/hash, participant identity, original preparation identity, exact staged allocation/delta and revision evidence; decision binds the selected prepared digest set; completion binds every matching applied digest. Delivery retries cannot replace that set. |

INT-02 must publish a byte-level profile and vectors before mutation handling: one pinned protobuf schema/compiler/serializer profile, field order, map sorting, repeated-list semantics, scalar/default/optional presence, unknown-field rejection and hash-domain/version tags. Backend's word “deterministic” alone is not a cross-language canonicalization proof. For these construction payloads, use integer quantities/time durations/grid coordinates and exact prescribed terrain representations; no locale-dependent numbers, floats, JSON field ordering or implicit Unicode/case normalization. Registry IDs are validated and retained exactly. Treat source allocation and participant sets as uniquely keyed sorted sets; preserve order only where the owning contract says it is meaningful; reject duplicate entries. Terrain byte serialization remains DEC-03's owner gate.

Proposed boundary keys are canonical uppercase ULIDs for client actions (Backend §3.1); service-flow keys retain their deterministic registered-ID form. Validate length ≤128, nonempty and profile grammar without trimming/lowercasing. Internally namespace the generic Building key by authenticated scope using a fixed-length versioned digest, so two principals cannot collide in Backend's `(rpc,key)` table. Persist original scope/key and internal mapping with the action; encoding and collision handling need vectors. No new key on an unresolved intent. Unknown historical `cba_` rejects rather than creating an action.

Same action/same semantics returns its current state or original receipt; changed semantics reject. A refreshed quote is a new intentional request only after resolving the old outcome. Higher delivery generation permits only replay/reconciliation of the same action, not new source selection, quantities or beneficiaries.

Generic ≥7-day cache cleanup cannot remove long-lived construction identity. Keep original key association, immutable hash, decision/certificate and reference-safe tombstones while any geometry/ledger/archive or eligible replay can reference them. Never purge unresolved decisions, prepared holds or unacknowledged delivery. Terminal compaction requires an owning-domain proof covering DOC-08's event/inbox/DLQ/backup horizons and erasure/holds; no universal new TTL or garbage-collection setting is selected.

## Recovery provenance schema prerequisites

GM §12 is a pending domain-extension request, not authority to invent a recovery table owned by Building. Before affected persistent gameplay schemas are finalized, ask the following owners to declare their authoritative records and compatibility/retention rules:

Read-only evidence: [Itemization Item Model](../Game-Design/Itemization/Legends_Shattered_Frontier_Itemization_Item_Model_and_Lifecycle_v1.0.md), §6.1 excludes individual wear deltas from the ledger, and its `inventory.wear_batches` schema stores only batch/character/server/applied time. Batch-level `ApplyWear` idempotency therefore does not establish exact residual per-death wear. Do not reuse that receipt as if it already proves the new recovery contract.

| Owners | Required evidence and joins by immutable IDs |
| --- | --- |
| Combat/Character/World | Durable death incident and generation, actor/realm, death/resurrection/safe-placement authority and expected versions; current consent/dead-state rules; supported per-step receipts. Best-effort combat telemetry cannot be the incident source of truth. |
| Inventory/Itemization | Incident → sealed/acknowledged original wear batch → equipped item/version and actual durability delta including zero/clamped/broken cases; causal repairs/later wear/upgrades/max changes; outstanding attributable correction; deduplicated compensating receipt linking incident/item/batch. Late wear cannot reapply a corrected death penalty. |
| Dungeon/Raid/Rewards | Supported checkpoint/encounter and recovery lineage, membership/allocation fences, drained/reconciled old outcomes, prior-defeat eligibility across new attempt IDs including no-lockout modes, exported reward references and explicit scoring/timer/death-penalty policy. |
| Security/LiveOps and each writer | Frozen approved plan, reason/support routing, current capability/approval/audit links, expected revisions, durable per-step idempotency/results, declared evidence eligibility and pseudonymization/holds. Protect later trades/ownership and economic history. |

Recovery is a resumable set of owning-domain compensating steps: revival completed before a crash is not repeated while wear correction resumes. Ambiguous causality, ownership/lifecycle conflict or missing eligible evidence blocks that correction. No historical snapshot overwrite, general grant, repair-cost refund or arbitrary timestamp rewind is implied. Physical names/types/retention and protected domain writes require their owners; GM UI/tool implementation is outside PER-01.

## Bounded next assignments and gates

These are proposed assignments to dispatch after this packet's review and relevant owner acceptance, not implementation authorization granted by a completed document.

| Packet / proposed write set | Dependencies and focused acceptance |
| --- | --- |
| PER-01-B owner decision/lock manifest; `docs/decisions/` only | Architecture/persistence accept stack, deployment/store boundaries, migration runner and capacity/exception path; Security/World confirm actor assertion, takeover/incarnation proof. Pin identities after package/tool compatibility checks; no host alteration. |
| PER-01-C skeleton/migration harness; new Building/Shared contracts and isolated fixture files only | B acceptance and exact local provisioning/commands recorded first. Verify empty/repeated/interrupted/concurrent migration, role isolation and incompatible-schema refusal against disposable stores. Protected Inventory implementation remains a separate owner packet. |
| INT-02-A exact envelope/profile; shared contract/fixture files and Building adapter only | Accepted identity/normalization design and actor binding. Cross-language byte vectors, concurrent same-key replay, changed payload, realm/principal separation, malformed key/unknown fields, deterministic rejection and cache-expiry mapping. UE adapter write set needs separate E1 owner assignment. |
| PER-02/BLD-03 participant slice; separately assigned owning-domain files | Accepted schema, protected-owner release and INT-02 vectors. Prepare/decision/apply/complete crash matrix, fence contention, receipt association, held-yield inaccessibility and one-time activation; never enable with a no-op participant. |
| PER-04-A recovery/restart fixture and runbook; isolated fixture/evidence paths | Reconciled DOC-08/DEC-08, eligible backup frontier and all domain adapters. Lost reply, restart, DB partition, takeover races, stale-zone calls, delayed old-generation prepare/apply/complete, restore incarnation, aged replay and erasure evidence. Assert conserved materials, immutable decision, stable receipts, matching revisions and blocked mixed projections. |
| GM-PROV owner requests; scoped design packets only | GM12 owner acceptance before relevant schema commitment. Cover delayed wear, partial/full repair plus later wear, ownership/upgrade conflicts, stale approval, partial recovery resume and new-attempt reward suppression. This does not block unrelated incident-free records. |

PER-01 documentation completion does not close DEC-02, INT-02 or H2 runtime acceptance. Remaining gates: named owner acceptance; exact pinned dependencies/provisioning; authenticated actor and certificate/barrier contracts; physical schemas/hash vectors; DEC-03 codec/support integration; protected-domain implementation; measured crash/recovery/migration evidence; host/DR readiness. No tests were run for this document-only change.
