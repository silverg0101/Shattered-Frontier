# DOC-08 — MVP retention and recovery profile

Status: proposed MVP adoption, 2026-10-17. Backend already defines the operating baseline below. This packet reconciles its applicability to durable construction and names missing implementation evidence; it does not certify deployed backups, accept DEC-02 or amend privacy policy.

## Authority and proposed adoption

Adopt the existing Backend baseline for the PostgreSQL MVP proposed in [DEC-02](DEC-02-durable-actions.md), preserving domain-local transactions and all-participant construction completion. Architecture/persistence and operations must record deployment selection and prove the profile before dependent long-running tests. Any smaller experimental deployment needs an explicit exception with disabled dependent gates, rather than claiming these recovery targets.

Read sources:

- [Backend communication/persistence](../Game-Design/Backend/Legends_Shattered_Frontier_Backend_Service_Communication_Persistence_and_Content_v1.0.md), §§3, 4, 6.1, 6.8: idempotency, outbox/inbox/topic lifetimes, tier-0 topology and object storage.
- [Backend operations](../Game-Design/Backend/Legends_Shattered_Frontier_Backend_Privacy_Transfers_and_Operations_v1.0.md), §§2.7, 5.1–5.4: erasure, recovery targets, schedules, restore ordering and incident authority.
- [Construction Integration](../Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md), §§2–5 and 8: durable decisions, held outputs, receipt history, completion and safe reopening.
- [World terrain](../Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Terrain_Resources_and_Population_v1.0.md), Durable world state retention and Commit and persistence: compacted chunk snapshots, operation tails and rollback history.
- [Database](../Game-Design/Legends_Shattered_Frontier_Database_Architecture_v1.0.md), Backup Strategy and Recovery Targets; [MVP](../Game-Design/Legends_Shattered_Frontier_First_MVP_Milestone_v1.0.md), Persistence schema and Decisions required before implementation; [tuning policy](../Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md), §§2–3.

## Lifetimes and cleanup eligibility

Values below are inherited requirements, not newly measured capabilities. A generic TTL never proves that an asset-changing operation is safe to repeat.

| Record class | Existing baseline | MVP interpretation / cleanup gate |
| --- | --- | --- |
| Generic durable RPC deduplication | At least seven days; long-lived flows extend through flow end + seven days (Backend §3) | Keep key and business effect atomic locally. Construction additionally resolves the durable key-to-action association after cache expiry. Transient errors do not become invented terminal results. |
| Pending construction decision, prepared allocations/intents, fences | No unilateral expiry (Construction §4) | Never remove because of elapsed time. Coordinator may durably abort expired undecided work with a newer fence; a persisted commit must roll forward. Missing decision evidence blocks affected work. |
| Construction action/terminal tombstone and key association | Survive generic cache expiry while geometry, ledger or archive references them; unknown historical action IDs cannot create mutations (§8) | Preserve identity/hash, decision and sufficient receipt references. Compaction requires resolved participants, no unresolved delivery/recovery dependency and proof that remaining references and supported replays cannot lose deduplication. No finite universal action TTL is proposed. |
| Debit/return entitlement and held output records | Unique origin/allocation, cumulative return conservation (§§3, 6) | Follow Inventory ledger/erasure ownership. Completion certificate unlocks output once; repair/compaction cannot turn a held credit into spendable stock or erase return lineage. |
| Construction participant decision/application delivery | Replayable until acknowledged (§3) | Keep pending work until its matching decision/application/completion acknowledgment. A generic published-event expiry cannot erase unacknowledged protocol work. |
| Terrain operation history | World deletes ops at or below `snapshot_seq` 30 days after compaction | Validate snapshot plus surviving tail before cleanup. Compaction of geometry cannot discard required action/yield lineage or unresolved participant evidence; coordinate eligibility with the World runbook and [DEC-03](DEC-03-terrain.md). The 30-day rollback history is distinct from the 35-day backup schedule. |
| Domain event outbox | Unpublished persists; published rows deleted after three days (Backend §4.1) | A published notification is not the authoritative action history. Keep stable event IDs; rebuilding history from receipts must not manufacture new credit commands. |
| Domain topic / consumer inbox | 14-day topic; inbox rows older than 15 days purged (§§4.2–4.3) | Normal replay must stay within verified dedup coverage. A restored inbox may itself be older than current downstream effects; compare restore points and consumer state before replay. |
| Dead-letter queue | 30 days; manual audited replay (§4) | The 30-day queue outlives the 15-day inbox. Age alone does not authorize redrive: require durable domain deduplication or explicit owner reconciliation before any asset-changing redrive outside safe coverage. Otherwise quarantine. |
| Backups | Daily full snapshots and weekly DR immutable archives retained 35 days (Operations §5.2) | Treat 35 days as the documented retention and erasure ceiling, not a minimum that this proposal may extend. Legal/enforcement holds use owning policies; they do not silently extend ordinary backup retention. |

REC-01: Backend's nominal inbox, DLQ and backup windows do not by themselves establish safe old replay. This is a reconciliation requirement, not a claim that a duplicate has occurred. Operations and each state-changing consumer must document the actual safe replay frontier and reference durable business keys; if proof is unavailable, keep the affected consumer/realm closed. A blanket extension of every dedup TTL is not selected here.

## Backup schedule and recovery targets

Use Backend Operations §5.2 as written:

- Continuous WAL archiving, segments shipped at least every 60 seconds; seven-day PITR window.
- Daily full snapshots and weekly immutable DR archives, each retained 35 days. Weekly archives use the same ceiling.
- Hourly RDB for `redis-game`/`redis-world`, kept three days. No backup for `redis-cache`/`redis-session`; authoritative actions cannot depend on those caches surviving.
- Domain event bus is not backed up by that schedule; the runbook uses a DR-site mirror and available topic history. Verify the mirror/offset gap before relying on replay. Object-store versioning/replication follows owning bucket policies.
- Monthly isolated restore for every cluster, comparing row counts and ledger checksums. This documentation adds targeted construction reconciliation to the H2 rehearsal; it does not replace the recurring baseline.

Construction, inventory and World durable storage are tier 0 under existing ownership. Backend §5.1 targets RPO 0 / RTO ≤1 minute for pod/node loss; RPO 0 / RTO ≤5 minutes for availability-zone loss; region-site loss RPO ≤5 minutes / RTO ≤60 minutes. Logical corruption uses a point before the fault and RTO ≤4 hours per schema. These are different failure classes. Daily snapshots alone do not establish the seven-day PITR window or the region-loss target; storage layout, WAL availability and DR replication must be proved.

The MVP Performance budgets section separately proposes a backup restore rehearsal ≤30 minutes for a defined dataset, including human/runbook time. Retain that provisional rehearsal target and record its dataset and measured outcome; it does not replace Backend's failure-class targets or prove regional recovery.

## Restore and reopen sequence

1. Named incident authority gates entry/mutations for affected state and fences the old writer/coordinator. Preserve forensic references and establish build/schema/content compatibility before opening restored stores. Follow existing maintenance and audit authority; no new privileged API is introduced.
2. Record a recovery manifest: each domain's backup identity, WAL/restore point, schema/build/content versions, action decision frontier, participant receipt frontier, event offsets and available dedup coverage. Different clusters can be at different points; never equate wall-clock proximity with a coherent distributed action.
3. Follow Backend §5.3 erasure replay first: union current `auth.erasure_tombstones` where available with ID-only `audit-worm` tombstones, rerun owning erasure before serving traffic. If required erasure evidence is unreachable, do not open. Respect holds and pseudonymization through the existing privacy participants; conservation history does not authorize retaining erased private payloads.
4. Reconcile each construction action with Building's durable decision, Inventory's allocations/ledger/held outputs, World's staged/applied/active terrain and Building's structures. Recover all known decisions, including decisions newer than one participant's restore point, from retained authoritative evidence. Prepared without a trustworthy decision stays fenced. Do not guess abort or re-create a new action ID because a restored row is absent.
5. Apply commit/abort idempotently; for committed work collect matching participant digests, durably recover completion certificates and replay activation/unlock. If one store contains a debit or active geometry with missing counterpart history, isolate it for owner reconciliation rather than mint a refund. Immutable commit cannot become abort during restore.
6. Reconcile event mirror/history, restored inboxes and external consumer effects before replaying. Use stable event IDs and business-action identities. Older snapshots may predate the 14-day event history; neither warehouse history nor a three-day outbox is assumed to be a runnable recovery command log. Require a domain runbook/snapshot rebuild with conserved effects, or refuse that restore point for service reopening.
7. Rebuild projections and volatile caches, verify permissions and current zone ownership, then run isolated restore fixtures and canary entry/read/mutation checks. Open affected claims/consumers only after erasure, compatibility, action reconciliation and dedup gates pass. Record actual RPO/RTO and any lost frontier; do not promise that region-loss recovery has zero data loss.

REC-02: A 35-day snapshot may remain a valid retained backup while being unsuitable for automated restoration of a particular domain because WAL, event or downstream dedup history is unavailable. Owner runbooks must define acceptable restore points and cross-domain repair. No fallback to blind replay or guessed compensation is permitted.

## Configuration, owner work and evidence

This packet adds no adjustable values. Existing `tune.backend.pitr_window_days` (default seven) is named in Backend Operations but its complete type/bounds/permission/effective-time declaration was not found in the inspected recovery sections. REC-03 queues a DOC-05 lookup of the owning LiveOps/registry declaration before calling it compliant. Keep the inherited profile; do not invent a safe range or an operator override here. `BACKUP_RETENTION_DAYS = 35` also cannot be widened through an unreviewed console because it interacts with erasure. Recovery algorithms, writer fencing and topology remain release-bound.

| Owner / dependency | Concrete remaining deliverable |
| --- | --- |
| Architecture + persistence, DEC-02/PER-01 | Accept topology and recovery-store boundaries; record durable key/action identity, coordinator takeover and migration compatibility. DEC-02 is still proposed. |
| Operations + privacy owners, PER-04 / DOC-09 / DOC-10 | Physical backup/mirror destinations, authorized jobs/credentials, compatible restore tooling, erasure evidence availability and reopen authority. Reserved Security/LiveOps/Analytics contracts remain read-only. |
| Building/World/Inventory owners, PER-02/03 / BLD-03 / TER-03 | Domain reconciliation runbooks and retained receipts/digests proving both participant conservation and geometry activation; Inventory changes require reserved-owner assignment. |
| State-changing consumers, REC-01/02 | Safe replay frontier for live/restored inbox and business keys, aged DLQ handling, old-snapshot eligibility and downstream compensation authority. |
| Configuration owners, DOC-05 / REC-03 | Locate or propose full declarations for exposed recovery settings without changing the inherited values. |

Required targeted implementation evidence: same-key retry after seven-day generic-cache expiry; attempted cleanup while a prepare/commit/output-release is unresolved; participant restore behind/ahead of the coordinator; activation/reply loss; delayed publication; DLQ older than 15 days; backup older than event/WAL history; erasure-list outage; incomplete WAL chain; incompatible schema; process, zone and region failure classes. Each fixture records expected refusal or conserved final outcome and the actual restore manifest. No runtime tests were executed for this documentation-only assignment.

Alternative: a reduced local rehearsal can prove crash safety without production DR infrastructure, but cannot close region-loss, monthly-restore or public-alpha operating gates. Revisit this proposal when topology changes, a rehearsal misses targets, a replay source exceeds dedup history, privacy policy changes or a domain reconciliation runbook changes. Close DOC-08 as a reviewed proposal only; acceptance and operational evidence remain with the named owners.
