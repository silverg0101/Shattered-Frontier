# DEC-08 — Compatible updates and rollback

Status: independently reviewed proposal, 2026-10-17. Delivery and persistence owners must accept this MVP adapter and supply schemas, codec and C4 evidence. Documentation review does not establish compatible artifacts, installed migrations, working updates or a recoverable deployment.

## Decision boundary and source authority

[Roadmap, “Decision register”][roadmap] assigns DEC-08 to Delivery + persistence at C4. [Backlog, delivery rows][backlog] requires DEL-01 packaged artifacts, DEL-02 signed/recoverable updates and DEL-03 compatible code/data recovery, dependent on PER-04. E1/H2 may develop fixed candidate contracts; this proposal does not waive their authority, terrain or durability gates.

Binding design sources:

- [Client Delivery][delivery], “Master Manifest Definition,” “File Manifest Definition,” “Patch Selection Logic,” “Validation Architecture,” “Repair System,” “Release Channels” and “Security Requirements”: signed manifests, channel/build identity, `minimumLauncherVersion`, file sizes/SHA-256, package validation, full download when no valid patch path exists, and repair followed by verification.
- [Database][database], “Backup Strategy” and “Recovery Targets”: high-level storage/backup context. It does not define migration compatibility. [Backend persistence §6.5][backend] owns expand/migrate/contract; §7 owns immutable content, validation, activation and rollback reference checks.
- [LiveOps Config and Releases §§3.1–3.8][liveops], read-only: canonical `rel_` release fields, stages/maintenance, activation, content rollback and gateway minimum-client behavior. [Registry][registry] owns names and content identity, not a newly invented version protocol.
- [World terrain][terrain], “Chunks and chunk keys,” “Commit and persistence” and “Rebase on content change”: existing base identity, replay and pre-activation rebase requirements; the latter is a rule within the persistence section, not a complete migration implementation.
- [DEC-02][dec02], “One action, through failure and recovery” and “MVP schema reconciliation”; [DEC-03][dec03], “Proposed adoption,” “Unreal support, collision and visibility,” and “Evidence gates and selectable fixtures”; [DOC-08][recovery], “Restore and reopen sequence”: reviewed proposals, with owner acceptance and implementation evidence still pending.

## Proposed MVP binding

Adopt the existing LiveOps release model and launcher validation. Add a reviewed compatibility record to the candidate's release evidence linking the identities below to its actual artifacts and supported transitions. This is a proposed adapter/delivery requirement, not an accepted JSON schema, public endpoint, new Registry identifier or executable declaration. Delivery, persistence and World owners must select its representation and authenticated distribution before implementation; signing a client file list alone does not prove server/data compatibility.

| Identity / authority | Existing contract | Proposed binding and unresolved evidence |
| --- | --- | --- |
| Release / LiveOps | `rel_`, `version`, `server_build`, `client_build`, `min_client_build`, `content_version`, `migrations[]`, `config_changes[]`, `requires_maintenance`, `kind` | Bind the actual backend/zone-server artifacts to the released client and allowed transition. No concrete build numbers or acceptance window beyond the source contract is selected here. |
| Launcher / Delivery | Master manifest `channel`, `version`, `build`, `minimumLauncherVersion`, `filesManifest`, `signature`; file manifest sizes/hashes | Resolve launcher `build` to release `client_build`, matching semantic `version`. Bind channel and verified package set before launch. Launcher-version and game-build gates serve different purposes; one cannot substitute for the other. |
| Content / catalog | Immutable monotonic `content_version`; client-visible packs follow launcher; `server_only` packs require workload authorization; login reports active realm content | Bind both required client-visible artifacts and authorized server packs to the candidate. Do not expose server-only material to solve a client mismatch. Exact client cache/update negotiation remains an owner deliverable. |
| Persistent state / domain owners | Per-schema versioned SQL and release `migrations[]` classified expand/contract | Record applied migration identities, required reader/writer support and tested source/target states for `building`, `world`, `inventory` and other touched owning schemas. A global schema number or semantic version alone is insufficient. Exact DDL and migration tool remain unselected. |
| Terrain / World | `base_content_version`, `base_hash`, `head_seq`, `snapshot_seq`; deterministic chunk/op encoding; snapshots plus ordered tail | Bind base/palette and executable codec implementation/fixtures to the server/tool/client projection support. No canonical codec-version field is defined by this packet. Byte order, framing, index order, kernel, saturation and integrity checks still need DEC-03 owner definition. Heads identify state, not decoder compatibility. |
| Action/recovery / Building and participants | `cba_`, immutable decisions, digests, completion certificates, fenced ownership and receipts | Prove new and rollback builds can recover every retained pending/completed action format they may encounter, including previously committed work. Record DOC-08 restore points/frontiers separately from software build identity. |

## Compatibility and admission matrix

These are proposed adapter acceptance cases using existing authorities; no table cell asserts a tested combination.

| Candidate combination | Required behavior / gate |
| --- | --- |
| Accepted client, supported server, verified content and readable/writable schemas/base/codec | Admit only after ordinary auth/realm readiness and recovery gates. Artifact compatibility is necessary, not sufficient authority. |
| Client older than release `min_client_build` | Existing LiveOps §3.8 gateway login result is `upgrade_required`; launcher updates through the signed manifest path. Do not invent a different wire error or allow writes before upgrade. |
| N−1 client during nonbreaking rolling release | LiveOps §3.8 requires support for at least the rolling window unless protocol changes. Verify both builds against the actual API/content combination; this is not indefinite support for every older build. |
| Breaking protocol or insufficient reader/writer compatibility | Use the maintenance/minimum-build release path; drain/fence affected writes. No mixed writable cohort merely because version strings are adjacent. |
| Launcher below `minimumLauncherVersion`, wrong channel, invalid signature/hash, missing file or incomplete package | Proposed launch adapter blocks game launch until the supported launcher/package set verifies. Exact launcher self-update UX is pending; repair cannot relax signature/TLS checks. |
| Client content differs from realm active `content_version` | Proposed adapter resolves only a source-supported compatible client pack set before entry. Owners must define the check/refresh behavior; no new error enum or universal exact-equality rule is invented here. |
| Unknown schema, unsupported codec/palette, base/hash mismatch or incomplete rebase | Keep affected state unavailable to mutation; quarantine/repair through its owning recovery path. Never decode optimistically or replace saved terrain with an unrelated authored base. |
| New process loads prepared/committed actions from previous build | Recover original identity and decision; committed actions roll forward to matching digests/certificate and activation. No migration-induced refund, new action ID or inferred abort. |

Existing connected-client behavior during a breaking transition must be defined in the drain/session implementation, with DOC-09 and World owners. A login minimum alone does not prove already connected writers are fenced.

## Deployment, migration and rollback boundaries

1. Cut one candidate with pinned artifacts and the proposed binding. Validate client files/packs and server-only visibility, content references, schema transitions and deterministic terrain fixtures. LiveOps §3.8 requires published, verified manifests before switching server requirements; predownload is not activation. No candidate becomes production-ready from this document.
2. Apply Backend §6.5: expand → migrate/backfill and dual-write → switch reads → contract in a later release, across at least two releases; migrations must support N−1 still running. Use owning service migrations, never cross-domain table writes. Destructive steps require `release_manager` approval; LiveOps marks contract/restart/incompatible protocol releases maintenance-required. The exact dual-write/backfill logic and reconciliation evidence belong to domain owners.
3. Before changing a terrain base, complete World release-validation rebase of committed ops, record protected intersections as `rebase_dropped` and validate the result before activation. Preserve DEC-02 pending work and fences across this boundary. World/Building owners must specify pending-action versus rebase ordering, interrupted rebase recovery and Compact provenance mapping; no format or seventh edit kind is introduced here.
4. Drain/fence through existing maintenance/World workflows when required. Migrations precede traffic to the new build; load and verify content, schema and terrain readiness before mutation admission. Non-hot-reloadable geometry/navmesh/encounter changes require server replacement under Backend §7, not a config publication. DEC-03 matching collision/support readiness and safe occupancy remain necessary after durable commit.
5. Roll back code only when the previous build demonstrably reads/writes current schemas/content/codec and recovers all action states now present. An additive migration does not by itself prove semantic compatibility. Leave compatible expanded data in place; do not blindly execute down-migrations or delete durable decisions. After contract or irreversible data change, keep affected writes gated and choose an owner-reviewed forward repair or compatible restore/reconciliation, not an assumed binary rollback.
6. Content rollback follows `CheckRollbackSafety(from_version, to_version)` and persisted-definition reference counts (LiveOps §3.7; Backend §7). If references make rollback unsafe, keep the current pack and disable the new definitions through existing authority instead; disabling them does not make retained references safe for rollback. A passing reference check alone does not prove old terrain-base/codec compatibility; validate the bound base and retained edits separately. Retained pack availability is not proof of safe writable rollback.
7. Backup recovery follows DOC-08: fence old writers; collect schema/build/content, WAL, action/receipt/event/dedup frontiers; replay erasure evidence before traffic; reconcile distributed decisions/held outputs; then rebuild/readmit. An immutable commit never becomes abort. A retained 35-day backup may be ineligible for automatic restore when WAL/events/dedup coverage is insufficient (REC-01/02). Missing erasure evidence or unresolved cross-store history keeps affected state closed; guessed refunds or blind replay are forbidden.

Configuration rollback only changes future effective values through existing workflow. It cannot roll back data migrations, terrain algorithms, consumed resources or completed receipts. This packet adds no tuning values or live-edit exception.

## Interrupted update and repair adapter

Client Delivery specifies detection/repair/reverification, but its inspected sections do not define transactional install staging or crash recovery. Proposed DEL-02 adapter: download and validate a complete candidate in staging, retain a previously verified installation until activation succeeds, and switch the launch selection only to a complete verified candidate. Delivery owners must choose the actual file layout, activation/journaling and supported OS behavior; this is not a claimed implementation.

On network loss, disk exhaustion, process termination or patch verification failure, keep launch blocked for the incomplete target; retry a valid patch path or full download and verify again. Reuse of partial downloads must remain hash-verified. If the prior installation is retained, launch it only if its release/channel/content combination remains accepted by the server; local fallback cannot bypass `min_client_build`. Interrupted server deployment/backfill/rebase instead uses the domain migration/recovery record with fenced writes. Local client repair never repairs authoritative persistent state.

## C4 evidence fixtures

| Fixture / backlog gate | Required recorded outcome |
| --- | --- |
| CMP-01 / DEL-01/02 | Actual packaged client/server and release/manifest identities, channel, content/packs, migration state, base/codec fixture identity; launch without editor dependencies. Test matching, below-minimum, wrong-channel and incompatible combinations. |
| CMP-02 / DEL-02 | Tampered signature/hash, missing/truncated file, invalid patch path, corrupt cached download; reject unsafe launch, repair/full download, verify complete candidate. No secrets/server-only pack delivered to clients. |
| CMP-03 / DEL-02 | Interrupt download, installation and activation; restart/resume or repair without mixed-file launch. Retained prior build works only while server eligibility holds. Record disk/OS/build and actual adapter behavior. |
| CMP-04 / DEL-03, PER-04/06 | Rehearse expand/backfill/read-switch with N−1 and N; rollback against new writes and pending DEC-02 actions. Inject crash after prepare/commit/apply/completion. Preserve original receipt, conservation and no usable partial result. |
| CMP-05 / DEL-03, TER-03 | Unknown codec, malformed snapshot, mismatched base/palette and interrupted protected-volume rebase: deterministic repair or isolated refusal; validate snapshot/tail/head and collision readiness. No reset to guessed terrain. |
| CMP-06 / DEL-03 | Content reference blocks rollback; disable through existing controls. Separately prove accepted base/schema/code rollback, and refusal/forward-repair path after incompatible contract change. |
| CMP-07 / PER-04, DEL-03 | Restore compatible code/data with divergent coordinator/participant frontiers, aged replay source and erasure-list outage. Record recovery manifest, quarantine/refusal or conserved reconciliation, measured RPO/RTO and authorized reopen. |

These are targeted future fixtures, not tests run here or an instruction to run the full suite. Record build/hardware/dataset/scenario and evidence for each; DOC-08's provisional rehearsal target and Backend failure-class recovery targets remain separate. C4 requires actual compatible recovery and tester handoff; documentation alone does not close DEL-02/03.

## Alternatives, owner requests and revisit triggers

| Alternative | Consequence / selection boundary |
| --- | --- |
| Existing release model plus explicit compatibility binding (proposed) | Keeps canonical contracts and makes missing state/codec linkage reviewable; owners still choose schema/tooling and demonstrate transitions. |
| Exact client/server build pairing for every change | Simplifies some combinations but removes normal N−1 rolling support; requires explicit LiveOps-owner policy acceptance and planned interruption. Not selected implicitly. |
| Rolling deployment after compatible expansion | Reduces downtime only when N−1/new readers, writers and content are proven compatible; cannot waive maintenance for contract/protocol changes. |
| Restore prior backup for every rollback | Risks lost acknowledged state and unavailable replay/erasure history. Not a default rollback strategy; use only an eligible DOC-08 reconciliation runbook. |

Pending decisions: Delivery + persistence must accept the binding, admission checks, migration tool and rollback eligibility record. World + Building must define executable codec/base transition, pending-action rebase ordering and supported recovery formats; Unreal must establish matching collision/readiness. Reserved LiveOps/Security/Itemization owners must review release/min-build enforcement, signatures/visibility/erasure authority and inventory migration/recovery respectively; these are requests, with no protected-file changes or invented grants. DEC-02/03 and DOC-08 acceptance remain pending independently.

Revisit on protocol, schema, content ID/reference, base/palette/codec or supported launcher/OS change; a failed interrupted-update/rollback/recovery fixture; or lost backup/replay eligibility. Recheck the affected matrix/fixtures rather than treating a higher semantic version as permission to read or mutate old state.

[roadmap]: ../Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md#decision-register
[backlog]: ../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md
[delivery]: ../Game-Design/Legends_Shattered_Frontier_Client_Delivery_Launcher_Patching_Architecture_v1.0.md
[database]: ../Game-Design/Legends_Shattered_Frontier_Database_Architecture_v1.0.md
[backend]: ../Game-Design/Backend/Legends_Shattered_Frontier_Backend_Service_Communication_Persistence_and_Content_v1.0.md#65-migrations
[liveops]: ../Game-Design/LiveOps/Legends_Shattered_Frontier_LiveOps_Config_and_Releases_v1.0.md#3-release-management
[registry]: ../Game-Design/Legends_Shattered_Frontier_Shared_Registry_v1.0.md
[terrain]: ../Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Terrain_Resources_and_Population_v1.0.md
[dec02]: DEC-02-durable-actions.md
[dec03]: DEC-03-terrain.md
[recovery]: DOC-08-recovery-profile.md
