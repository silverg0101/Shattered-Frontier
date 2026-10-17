# DEC-02 — MVP durable construction actions

Status: proposed, 2026-10-17. Documentation reconciliation complete only after independent review; architecture and persistence owners must accept deployment and implementation evidence separately. This is not an installed schema or runtime guarantee.

## Decision boundary and sources

DEC-02 in the [roadmap](../Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md#decision-register) gates H2. It maps to PER-01, INT-02, PER-02/03/04, BLD-03 and TER-01 in the [backlog](../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md). E1 movement can proceed only within its own readiness gates; this proposal does not authorize durable asset mutations.

Binding design sources:

- [Construction Integration](../Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md), §§1, 3–5, 7–8: sole writers, staged participants, immutable decision, completion certificate and recovery.
- [Shared Registry](../Game-Design/Legends_Shattered_Frontier_Shared_Registry_v1.0.md), §§3–4 and 10: `cba_`, `building-service`, domain ownership and event names.
- [Backend communication/persistence](../Game-Design/Backend/Legends_Shattered_Frontier_Backend_Service_Communication_Persistence_and_Content_v1.0.md), §§3–4 and 6: RPC deduplication, transactional outboxes and durable storage/backup requirements.
- [Itemization](../Game-Design/Legends_Shattered_Frontier_Itemization_Architecture_v1.0.md), §1 and its Item Model part: inventory is the sole writer; append-only ledger and cross-owner escrow remain authoritative.
- [First MVP](../Game-Design/Legends_Shattered_Frontier_First_MVP_Milestone_v1.0.md), Operational architecture, persistent placement and persistence schema sections: conditional single-store examples, not a superseding construction protocol.

## Proposed choice

Use one MVP simulation region, cohost logical backend services where practical, and preserve the existing Construction Integration protocol even when participants share a deployment or PostgreSQL cluster. Each domain uses its own local transaction and schema. Cohosting reduces deployment count; it does not grant Building permission to write inventory or merge domain commits.

Building owns `cba_` actions, authorization/geometry fences, immutable commit/abort decisions, completion certificates and result receipts. Inventory owns reservations, debits, held returns, release and ledger writes in `inventory`; World owns durable terrain intents and revisions; Building owns structural intents and revisions in `building`. The current authenticated zone owner validates live geometry, collision, support, reach and actor state. A World-only durable mutation must use its owning contract; not every gameplay command becomes a construction action.

PostgreSQL is the proposed H2 durable backend, matching existing domain design. The browser SQLite harness remains separate evidence. Database hosting, service language, process packaging, World schema migrations and credential isolation require owner selection before implementation. Redis caches cannot replace decisions, fences or receipts. Outbox delivery follows Backend's transport contract; MVP must not silently replace the event bus with an in-memory queue.

| Alternative | Consequence / revisit trigger |
| --- | --- |
| Cohosted participants, separate local commits (proposed) | Fewer deployables, same recovery semantics. Revisit when fault isolation, deployment cadence or measured load requires separation. |
| Independently deployed participants | Same protocol over authenticated domain interfaces; higher operational cost. Select if cohosting cannot meet isolation or recovery needs. |
| One transaction writing costs and geometry | MVP example permits consideration, but incompatible with the current construction ownership/commit contract as written. Requires an explicit reviewed domain-contract change; not selected here. |

## One action, through failure and recovery

The states below describe the existing protocol, not new public enums or RPCs.

1. Zone obtains a non-reserving preview. It includes exact costs, affected geometry, source contributions/beneficiaries, revisions, config/content versions and authorization. Acceptance creates or retrieves one durable Building action with immutable hash and identity-scoped retry-key association. No usable geometry or spendable credit exists yet.
2. Building revalidates and fences the claim/authorization and geometry. Inventory `PrepareConstructionMaterials` reserves all selected lots atomically in its local transaction; World stages terrain with `CommitTerrainEdits(prepare)`; Building stages structures. Prepared votes, deltas and digests survive crashes. No network call waits inside a database row lock.
3. All required participants prepared: Building durably records commit with matching action hash, fence and participant digest. Otherwise it records abort. The existing five-second undecided-work deadline is resolved by the coordinator, never by unilateral participant expiry. Commit is immutable; a timeout cannot turn it into a refund.
4. Participants apply the authenticated decision once. Inventory `ApplyConstructionDecision` consumes held inputs and creates any outputs as unavailable held allocations; World applies staged heads while retaining inactive projection; Building applies structures behind fences. Abort releases prepared holds and staged intents without charges. An undecided expired action can be aborted only through durable coordinator recovery with a newer fence.
5. After every required applied digest agrees, Building durably completes the action and issues the replayable completion certificate. Inventory `CompleteConstructionMaterials` unlocks held outputs; World activates terrain; Building releases geometry through that certificate. Only then may the stable completed result become player-visible success. Delivery lag may leave a region recovery-pending; it cannot expose incomplete harvest yield or usable geometry.
6. Completed domain facts are published from their owning outboxes with stable event IDs. Lost replies resolve by the original key/action. Publication retry repairs notifications/projections without rerunning item credits or geometry mutations.

| Case | Required result / acceptance fixture |
| --- | --- |
| Same key, same hash, concurrent or reconnect retry | Same action/status/receipt; no second reservation, debit or placement. Query `GetConstructionAction` before resubmitting an unknown outcome. |
| Same key/action, changed semantic payload | Reject mismatch without changing original action. A refreshed preview is a new intentional request only after the old outcome is resolved. |
| Stale geometry, source or permission revision | Refresh/reject before prepare; overlapping prepared actions serialize through fences. Losing command has no partial debit. |
| Claim capture/revocation/cache destruction races | Competitor completes first and invalidates preview, or waits behind prepared authorization; no authority change crosses the decision. |
| Crash before decision | Recover durable votes/intents. Expired undecided action gets one fenced abort; unavailable decision store keeps prepared holds blocked. |
| Crash after commit, before one participant applies | Resend immutable commit and roll forward. No timeout compensation; no usable partial result. |
| Crash after all apply, before completion/release | Reconstruct agreed digests, persist/replay completion certificate, release once. Held yields cannot fund another action early. |
| Crash after completion or reply loss | Return stable receipt and repair pending activation/publication. No fresh action for unresolved original intent. |
| Event bus/publication failure | Keep durable outbox work and reconcile delivery; stable event ID/revision deduplication. World/quest consumers cannot create another reward from a replay. |
| Backup restore or decision store unavailable | Isolate affected claims; restore decision log and reconcile all participant receipts/fences before reopening. Never infer abort from missing cache data. |

These fixtures must inject failures at local transaction boundaries and assert material conservation, held-output inaccessibility, revisions and visible geometry. They are required future evidence, not tests run for this document.

## MVP schema reconciliation

The MVP `operation_receipts` table is a boundary receipt proposal, not the complete coordinator schema. For construction it must reference the canonical `cba_` action instead of providing another authoritative decision.

| MVP proposal | Construction mapping / required constraint |
| --- | --- |
| Principal, operation family, key, request digest | Unique authenticated realm/principal/family/key association → one immutable action/hash; persist atomically with action creation. Exact wire normalization is an INT-02/PER-01 deliverable. |
| `accepted` | Pending action only; not evidence of paid cost or durable success. Required votes, fences, deadlines, staged data and participant digests live in owning records. |
| `committed` | Must not expose success at decision persistence. Boundary terminal success maps to completed action/certificate; internally commit decision and completion remain distinct. |
| `rejected` | Definitive pre-mutation rejection or durable abort receipt; retain original deterministic outcome. An unknown decision cannot be labeled rejected. |
| `recovery_required` | Diagnostic projection of unresolved progress, never permission to change commit to abort or start a new debit. |
| Result and expected revision | Reference canonical receipt and before/after domain revisions; protect private source details by authorization. |

Backend's generic RPC key cache (at least seven days) is distinct from action history. Construction §8 retains terminal tombstones while geometry, ledger or archive can reference them, rejects unknown historical action IDs, and preserves key-to-action identity beyond generic cache expiry. Unresolved decisions, holds and unacknowledged delivery records are not cleanup candidates. [DOC-08 in the work plan](../design-work-plan.md#doc-08--doc-09--doc-10--operating-and-compatibility-decisions) must reconcile replay horizons, tombstone/key-association cleanup, backups (existing 35-day retention), restore points and erasure/holds; no arbitrary new lifetime is selected here.

Transaction/fencing algorithms, hash normalization and topology are release-bound code/contracts. Existing construction timing defaults remain governed by their owning declarations; this proposal adds no live console override or new tuning value.

## Remaining acceptance gates and owner requests

| Owner / follow-up | Required output before dependent work is ready |
| --- | --- |
| Architecture + persistence, PER-01 / DEC-02 | Accept or amend proposed topology; record deployables, language, durable-store access boundaries, migrations, authenticated participant transport, coordinator takeover/fence allocation and boundary key normalization. |
| Inventory owner (reserved, read-only here) | Confirm implementation of Construction §5 participant extensions, reservation/ledger/held-output schema and atomic receipt association. This is an implementation dependency, not evidence that the current specification lacks the contract. |
| World + Building owners, BLD-03 / TER-01 | Implement staged delta/version/fence records, completion activation and zone ownership checks; terrain encoding/integration is DEC-03. |
| Persistence/operations, DOC-08 / PER-04 | Select retention/backup schedule and restore/reconciliation procedure with measured evidence; respect current Backend requirements unless explicitly amended. |
| Integration, INT-02 | Exact bounded wire envelope, deterministic semantic normalization, changed-payload rejection and authorized receipt lookup. Security-owned changes require a separate owner packet. |
| Independent reviewer then implementation owners | Challenge the sequence and mapping against source contracts. Later prove every listed crash/conflict fixture and recovery diagnostics; no documentation review accepts runtime readiness. |

After review, link this proposal from the MVP's placement/schema sections and roadmap decision register. Keep DEC-02 proposed until owner acceptance is recorded; retain H2 durable-mutation gates meanwhile. Revisit on deployment separation, changed domain contract, incompatible schema, failed recovery rehearsal or evidence that the proposed topology cannot meet requirements.
