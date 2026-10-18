# Design documentation work plan

Last updated: 2026-10-18. Owner: coordinating documentation agent. This is the running documentation queue; the [gameplay backlog](Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md) owns implementation work and [roadmap](Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md) owns milestone gates. Update this file during each documentation work cycle; no background monitoring is implied. Standing user rules and continuation context are preserved in [design-agent.md](../design-agent.md).

## Goal and current evidence

Goal: make the project design sufficiently explicit for bounded implementation assignments, with traceable scope, domain ownership, failure behavior, configuration policy, and acceptance criteria. Complete documentation does not establish implemented or measured behavior.

- The [dependency map](dependency-map.md) indexes broad domain specifications and eight expanded planning references. Coverage depth across all domains has not yet been audited.
- [Alpha plan](alpha-plan.md) records the browser shared-world loop as implemented. The roadmap distinguishes it from intended Unreal functionality. Those documentation-only assessments are historical; actual E1 results follow below.
- E1 implementation is now authorized and started at `GameClient/ShatteredFrontier.uproject`, with steam-server Linux x86_64 selected for the dedicated server. Editor builds and map generation passed; [foundation evidence](evidence/e1-foundation.md) owns executable results and remaining acceptance cards. Source engine/Linux SDK setup, native Server compilation and cook passed; managed Defender blocks source UnrealPak staging. Packaged deployment remains pending. Earlier documentation-only rows are historical.
- Roadmap DEC-01 through DEC-09 and the MVP's Decisions required before implementation already identify unresolved choices. They are existing decisions to reconcile, not nine newly discovered defects.
- [Construction Integration](Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md) already defines inventory ownership and cross-service durable outcomes. DEC-02 must reconcile this contract with the MVP proposal rather than invent a competing protocol.
- The concept includes later-game and launch ambitions beyond MVP. Unreviewed coverage is an audit task; absence from MVP is not automatically a missing design.

## Overnight continuation — 2026-10-18

User authorized ready implementation work and subagents while away, moving past blockers. Parent coordinates shared docs/GameMode/hosting; gpt-6.1-sol authors/reviews E1 checks and interaction; gpt-6-luna performs bounded inventory/asset QA. Core reviews have no remaining code findings after correcting cooldown feedback, connection-pawn correlation, input auto-upgrade and client-local actor-name assumptions. See [executed evidence](evidence/e1-foundation.md), [Linux packaging](e1-linux-server-build.md) and [optional waystone source](../GameClient/ArtSource/GreenlandsWaystone/README.md).

| Work | Current state | Actual evidence / boundary |
| --- | --- | --- |
| E1-D lifecycle | Passed local check | Six joins/departures, two actual three-session overlaps, all departing state0 and survivor possessions preserved. Account reconnect remains separate. |
| E1-B/C movement | Physics/network checks passed; manual acceptance open | Four focused movement tests plus normal/adverse server/two-client motion/jump/landing. Offscreen frames captured/inspected; keyboard feel and interactive camera remain pending. |
| E1-E non-economic probe | Implemented / scoped checks passed | Three interaction tests plus actual owning-client/server acceptance; range3m, accepted cooldown0.5s startup-only. No economy or adversarial-ownership acceptance. |
| Initial adverse network evidence | Passed | All three NetDrivers verified50ms outgoing lag,±10ms variance,1% configured loss; nominal100ms RTT, not measured RTT/capacity. |
| Optional art prototype | Generated / QA passed | Existing references located in docs/Game-Design/concept-art. Imagegen turnaround plus Blender5.2.2 source/FBX/GLB:1140tris,2.4m,oneUV,fourmaterials; not runtime-imported or production approved. RealityScan installed, not needed for a synthetic concept. |
| Linux package/deploy | Working alternative route | Windows source UnrealPak remains ASR-blocked. Native Linux UnrealPak compiled; fresh portable loose cook passed. Isolated steam-server native source/SDK dependency setup and native UAT-help passed, bounded2CPU/8GiB, separate UID997. Reviewed full input archive transferred/hash verified/extracted; offline native stage is running. Stage/archive/runtime are not proven yet. |

Hourly quiet continuation is registered as `continue-shattered-frontier-overnight`; pause when the user returns or no meaningful authorized ready work remains. Do not rerun passing checks without a change/failure. No new paid tool is required so far. E1-F performance/capacity, manual rendering/input and packaged runtime gates remain open.

## Status and ownership rules

Queue states: Ready → Working → Reviewable → Done; Blocked names the exact missing input. Findings are classified as design gap, contradiction, product backlog, experiment, or operational risk using [debt standards](Game-Design/Legends_Shattered_Frontier_Technical_Debt_Standards_v1.0.md). Confirm gaps with owning-section evidence before calling them blockers.

Keep one write owner per file. The coordinator owns this queue and dependency-map edits. Character, Itemization, Quest, Dungeon, Raid, Security, LiveOps, Analytics and their part folders are reserved by the dependency map for another session. Read relevant contracts, record requested changes here, and do not assign writes there until ownership is explicitly released. No other-chat messages are authorized by this plan.

## Prioritized queue

Priorities below are documentation order, not changes to the gameplay backlog's P0/P1 labels.

| ID | Priority / state | Outcome | Dependencies / next action |
| --- | --- | --- | --- |
| DOC-00 | First / Done | Initial evidence assessment, work queue, agent routing and review loop | Independent review found no blockers; ownership clarification integrated |
| DOC-01 | Complete / Done | [Reviewed concept coverage matrix](design-coverage.md) across MVP, slice and later scope | DOC-06 updates bounded coverage; COV-01/02 event requests and COV-03…06 owner decisions remain open |
| DOC-02 | Complete / Done | [Reviewed durable-action proposal](decisions/DEC-02-durable-actions.md) and MVP receipt mapping | No review blockers; DEC-02 acceptance and implementation evidence remain pending with architecture + persistence |
| DOC-03 | Complete / Done | [Reviewed terrain adoption proposal](decisions/DEC-03-terrain.md) | Acceptance pending; codec, collision readiness and Compact/provenance mapping remain World/Construction/Unreal gates |
| DOC-04 | Complete / Done | Consolidated decision register linked to gates and owners | DEC-01…09 plus MVP recovery row reconciled; independent source review found no blockers; decisions remain open |
| DOC-05 | Complete / Done | [Reviewed bounded tuning audit](design-tuning-audit.md) | E1/H2 sample and REC-03 lookup complete; release-bound defaults remain permitted; conditional live declarations and owner classifications remain pending |
| DOC-06 | Complete / Done | [Reviewed lookup and owner requests](design-coverage.md#doc-06-lookup-limits-and-classification) | Analytics and guard/validation/territory/dungeon evidence checked; independent source review found no findings; COV-03…06 owner decisions remain open, not new E1/H2 requirements |
| DOC-07 | Later / Blocked | Implementation-ready documentation gate for the selected milestone | E1 editor compile/map/connection evidence now exists; full E1 behavior and Linux package/runtime checks remain pending. No milestone readiness certified. |
| DOC-08 | Complete / Done | [Reviewed recovery-profile proposal](decisions/DOC-08-recovery-profile.md) | Existing schedule adopted as proposal; REC-01/02 replay/reconciliation evidence and REC-03 owner classification remain pending |
| DOC-09 | Complete / Done | [Reviewed session applicability proposal](decisions/DEC-04-session-applicability.md) | Existing Backend/Security adapter mapped; ticket caller, atomic admission/fencing, revocation and SESS-01…07 evidence remain owner gates; DEC-04 open |
| DOC-10 | Complete / Done | [Reviewed compatibility proposal](decisions/DEC-08-compatibility.md) | Release/migration/rollback and CMP-01…07 mapped; binding/codec/schema choices, owner acceptance and C4 evidence remain pending; DEC-08 open |
| DOC-11 | Complete / Done | [Reviewed DEC-01 metadata recheck](decisions/DEC-01-engine-evidence.md) | Historical metadata packet cleared missing payload. Current E1 project/editor compile evidence is recorded separately; source/Linux packaged-server evidence remains pending and DEC-01 stays open. |
| DOC-12 | Complete / Done | [Reviewed E1 implementation assignment](e1-implementation-packet.md) | Proposed paths/authority, Greenlands fixture, two-client checklist and scoped cards prepared; module path corrected and independently rechecked. No engine execution or implementation readiness inferred. |

DOC-02/03/08/09/10 are reviewed proposals; no owner acceptance or runtime readiness is inferred. DOC-04 consolidates decision status below; DOC-05 records only its bounded sample. DOC-06 separates existing behavior from later scope/metric questions. DOC-11/12 prepared the implementation now started under user authorization. Use [current E1 foundation evidence](evidence/e1-foundation.md) for completed checks and next local movement/lifecycle/interaction work; use [the Linux route](e1-linux-server-build.md) for source/toolchain preparation. Keep at most two author tasks plus one reviewer active. DOC-07 and full E1 acceptance remain open.

## Consolidated decision register — DOC-04

This register reconciles the [roadmap decision IDs](Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md#decision-register) with the [MVP decision rows](Game-Design/Legends_Shattered_Frontier_First_MVP_Milestone_v1.0.md#decisions-required-before-implementation). Owners are roles awaiting named assignment where needed. All decisions below remain open: reviewed documentation is not owner acceptance, selected infrastructure, or measured runtime evidence. Backlog IDs refer to the [gameplay backlog](Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md).

| ID / duplicate MVP question | Owner; dependent work | Current status and evidence needed |
| --- | --- | --- |
| DEC-01 / installed engine and packaged-server toolchain | Engine; E1 ENV-01/02, final DEL-01 | [E1 foundation](evidence/e1-foundation.md): project created; editor compile passed with MSVC14.51/SDK26100. Launcher editor supports local smoke; pinned 5.8.3 source-engine/Linux SDK route selected for steam-server. Source checkout and cross-SDK installation completed; dependency sync/build/cook/deployment remain in progress. |
| DEC-02 / atomic world/inventory topology and recovery | Architecture + persistence; H2 PER-01, INT-02, BLD-03; downstream gathering/trade | [Reviewed proposal](decisions/DEC-02-durable-actions.md), acceptance pending. Select deployables/language/store access and migrations, participant transport, takeover fences and key normalization. Prove conservation and crash/conflict recovery before dependent durability claims. Construction protocol does not automatically cover every gameplay command. |
| DEC-03 / terrain, limits, support and collision | World + engine; H2 TER-01/02, later TER-03 | [Reviewed adoption proposal](decisions/DEC-03-terrain.md), acceptance pending. Existing World baseline is specified; executable codec, derived collision/readiness choice, Compact/provenance mapping and representative measurements remain required. DEC-01 capability and DEC-02 integration remain dependencies. |
| DEC-04 / account/session and active-character lease | Backend + Security; G3 reconnect/remote testers; MOV-03 lifecycle applicability | [Reviewed adapter proposal](decisions/DEC-04-session-applicability.md), acceptance pending. Backend §4.7 ticket issuer differs from Security §9.3/§7.4 caller; joint reconciliation, atomic entry/fencing, connected-server revocation and SESS fixtures remain required. Local E1 evidence does not accept remote security; protected-owner requests stay pending. |
| DEC-05 / reference hardware and target settings | Performance + project owner; C4 CAP-02/03 and ratified capacity budgets | Unresolved. Select client/server machines/settings, density and workload; ratify provisional budgets and record staged load/soak results. Initial E1 measurements can precede final C4 budget acceptance. |
| DEC-06 / restricted market subset and escrow | Economy + Itemization; C4 MKT-01/02/03 | Unresolved selection/applicability. Confirm the allowed fixed-price item class and owning settlement contract, then evidence concurrent buy/cancel, retries and ledger reconciliation. Reserved Itemization changes need owner assignment. |
| DEC-07 / no duplicate MVP decision row | Repository + operations; content growth, DEL-01/02 artifact delivery | Unresolved binary-asset storage and artifact hosting. Record chosen storage, provenance/access, artifact identity and reproducible delivery evidence before dependent automation/content growth. |
| DEC-08 / no separate MVP decision row; relates to delivery and PER-04 | Delivery + persistence; C4 DEL-02/03, PER-04 compatibility rehearsal | [Reviewed compatibility proposal](decisions/DEC-08-compatibility.md), acceptance pending. Existing release/N−1/min-build/content-reference checks preserved; evidence binding/install adapter, exact schemas/codec, rebase ordering and actual CMP fixtures remain gates. |
| DEC-09 / no duplicate MVP decision row | Project owner + operations; later closed-alpha exposure | User selected steam-server Docker/Linux x86_64, superseding Unraid, for the dedicated test server; read-only hardware inspection supports further small-group validation, not capacity certification. No deployment yet. Public hosting/cost/data/support approval remains unresolved before exposed alpha. |
| MVP receipt-retention/backup row / DOC-08 (no new DEC ID) | Persistence + operations, privacy/domain owners; H2 PER-04 and long-running duplicate protection | [Reviewed profile proposal](decisions/DOC-08-recovery-profile.md), acceptance pending. REC-01 aged replay, REC-02 old-backup eligibility and REC-03 owner classification remain tracked. Require restore manifests, erasure evidence and participant/frontier reconciliation; unresolved decisions/holds cannot expire by generic TTL. |

Alternatives below are evaluation paths, not approved scope cuts or substitutions for owning contracts. Where source tables name no options, the next owner packet must supply them with consequences before acceptance.

| ID | Alternatives / consequences and revisit trigger |
| --- | --- |
| DEC-01 | Installed engine can support the next authorized E1 project/compile investigation; separate editor-server evidence from packaged readiness. Installed metadata advertises no Server target rows; select and verify a dedicated-server-capable route before packaging. Non-preferred compiler alone does not justify automatic changes. Revisit on build/root or actual target/toolchain evidence changes. |
| DEC-02 | Proposed cohosting retains separate domain commits; separate deployment retains the same protocol with more operating cost. A shared cross-domain transaction requires a reviewed contract change. Revisit on isolation/load needs, schema/contract changes or failed recovery. |
| DEC-03 | If measured capability fails, compare reduced content scope, another derived collision adapter preserving storage, then explicit representation migration. Revisit on incompatible codec/engine or failed support/collision fixtures; no live algorithm override. |
| DEC-04 | DOC-09 proposes the existing-contract UE adapter and gateway ticket issuance pending caller reconciliation; separate deployment keeps the same obligations. Local E1 join/leave is scoped evidence only. Revisit on single-session/reconnect/revocation failures or owner-contract changes; no second auth protocol. |
| DEC-05 | Reduce workload/content or revise hardware/budgets through owner review if measured targets fail. No machine or capacity selected here. Revisit on hardware, workload, scope or measurement changes. |
| DEC-06 | Restricted fixed-price market is the current C4 scope. Explicit deferral requires a scope decision; direct trade alone does not satisfy it. Revisit on item/settlement scope or conservation failures. |
| DEC-07 | Storage/hosting options and cost consequences still need an owner packet. Revisit before large binary growth, delivery automation or access/provenance requirements change. |
| DEC-08 | DOC-10 proposes existing release semantics plus explicit compatibility evidence. N−1 rolling requires proven compatibility; exact build pairing needs an explicit policy change. Restore is eligible only under reconciliation, not a default rollback. Revisit on schema/content/base/codec change or failed update/rollback rehearsal. |
| DEC-09 | Hosting/exposure/cost options remain unselected and require the project owner. Revisit before exposure, population, spending or data-handling changes. |
| DOC-08 | Reduced local crash rehearsal is useful evidence but cannot close production disaster-recovery gates. Revisit on topology/privacy/runbook changes, aged replay beyond dedup history or missed restore targets. |

DOC-06 completed the COV-03 trade-metric lookup and recorded COV-04…06 scope/owner questions in coverage. They and COV-01/02 event requests remain later-scope follow-ups, not new E1/H2 decision IDs. DOC-03 codec/collision/Compact issues remain under DEC-03. DOC-08 recovery findings remain under its MVP row and DEC-02/08 dependencies. DOC-07 stays blocked on the selected milestone's specific decisions and reviewed contracts; unrelated open decisions do not block all work.

### Pending concept-owner packets after DOC-06

These are concrete inputs for later owner assignments, not permission to edit reserved files or adopt new scope. The current E1 critical path remains DEC-01's actual toolchain/project/server-target evidence; H2 retains DEC-02/03 and recovery gates above.

| Request / owner | Required decision before dependent authoring | Boundary |
| --- | --- | --- |
| COV-03 / Economy + Analytics | Define player-trade share (count/value, exchange population, NPC treatment, time/aggregation, corrections) with examples, or defer/retire the aspiration | Analytics writes reserved; existing velocity is not a substitute. No restricted-market blocker inferred. |
| COV-04 / project/product, then named crime lead and AI/Combat/World/Building collaborators | Choose existing guard-only interpretation, selected additional criminal gameplay, or deferral; assign authoritative behavior/validation ownership | Preserve temporary guard offender state and property rights. Security validation/sanctions do not silently own criminal gameplay. |
| COV-05 / project/product + World/Combat/Territory | Select/defer each non-siege PvP mode, assign lead and define eligibility/lifecycle before detailed contracts | Keep accepted siege protections; no new PvP milestone or generic Dungeon ownership inferred. |
| COV-06 / project/product + Dungeon | Map expedition label to an existing dungeon type, define distinct scope/lead, or defer | Dungeon writes reserved; no new enum/service or content commitment. |

## Executable next assignments

All packets use the [agent task contract](Game-Design/Legends_Shattered_Frontier_AI_Agent_Responsibilities_v1.0.md). Start with dependency-map.md. Expected output is a concise document/diff plus evidence and unresolved decisions. Documentation checks only; no runtime tests, installs, engine changes, deployment or full-suite runs.

### DOC-11 — DEC-01 metadata evidence (reviewed; build gates open)

Read roadmap E1/DEC-01, ENV-01…05 and proposed UE/repository boundaries. Write only `docs/decisions/DEC-01-engine-evidence.md`; coordinator integrates references. Collect targeted read-only engine/build metadata, compiler/SDK and project-path evidence, distinguishing presence/static declarations from actual successful targets. Initial payload absence is historical; post-install metadata now verifies UE5.8.3 and tool files. Independent review repeated named observations. Project creation/compile and any toolchain change remain separate authorized work. Reinspect only after relevant version/path changes; keep E1 editor-server and final packaged gates separate.

### DOC-12 — E1 implementation assignment while installation completes

Read E1/ENV/MOV/INT backlog, UE framework/target/verification sections, repository tree, Greenlands region layout and MVP budget caveats. Write only `docs/e1-implementation-packet.md`; coordinator updates queue/index/handoff. Output: proposed project/write boundaries, minimal fixture, authority map, independent two-client/server launch evidence, scoped acceptance cards and stop conditions. No new feature scope, guessed verified commands/defaults, code creation or engine/install polling. Independent reviewer checks source alignment and the distinction between prepared assignments and actual runnable/accepted E1. DOC-11 remains the executable entry prerequisite.

### DOC-01 — coverage and player experience

- Goal: trace each concept pillar into its owning specification and scoped milestone; distinguish specified, deferred, unresolved and not yet inspected.
- Read set: concept.md, alpha-plan.md, gameplay backlog, vertical-slice journey/content/scenario headings; dependency-map-selected owning sections only when coverage needs confirmation.
- Write set: new `docs/design-coverage.md`; coordinator updates its index after review. Do not rewrite domain specs during this audit.
- Inputs/outputs: concept pillars → rows with scope, owner/section, coverage evidence, open question and affected gate/task. Include player journey, social/permissions, combat/recovery, economy, construction/world and later launch/content ambitions where the concept calls for them.
- Authority/invariants: owning domains and registry retain authority; planned omissions are not defects; no invented scope decisions or runtime claims.
- Acceptance: every concept pillar is traced or explicitly unverified; suspected gaps name the exact missing behavior and searched owning sections; confirmed findings get stable local IDs and next packets. The matrix need not quote or duplicate specifications.
- Validation/handoff: verify cited links/sections and milestone labels; return changed paths, confirmed findings, unknown coverage, and next action. Reviewer samples coverage against the concept independently.

### DOC-02 — durable action decision reconciliation

- Goal: establish what DEC-02 still requires after existing construction integration, and produce one compatible proposed MVP decision.
- Read set: roadmap DEC-02, MVP Operational architecture proposal / persistent placement / schema requirements, Construction Integration durable-action and recovery sections; Backend, Database, World and Itemization owning sections only where these contracts refer to them; shared registry for names.
- Write set: new `docs/decisions/DEC-02-durable-actions.md`. Coordinator integrates agreed references into planning documents only after review; reserved domains remain read-only.
- Inputs/outputs: compare cohosting versus separated participants; name writer/store, commit decision, fences, retries, timeout/restart recovery, publication repair, retention constraints and compatibility. Trace each choice to the existing contract or flag its proposed change.
- Authority/invariants: inventory remains sole item writer; no cross-service atomic transaction may be assumed. Existing construction guarantees and configuration/release boundaries must hold.
- Dependencies: named retention/backup and storage questions may require operational evidence. Record a proposed choice with alternatives and revisit trigger; retain unresolved evidence gates rather than declaring a decision accepted.
- Acceptance: one sequence covers success, retry, changed payload, competing revisions, crash before/after durable decision, and failed publication. Explain how the MVP schema maps to construction receipts and which questions remain. Flag protected-domain change requests separately.
- Validation/handoff: check contract names and all sequence transitions against cited owning sections; reviewer independently challenges conservation and recovery. Return proposal, conflicts, dependent backlog IDs and next decision owner.

### DOC-03 — terrain decision preparation

- Goal: reconcile DEC-03 with the existing World terrain baseline and produce an MVP adoption/Unreal integration packet. World already specifies 0.5 m voxels, 16 m chunks and deterministic Q15 arithmetic; this is not a blank-sheet representation decision.
- Read set: roadmap DEC-03, backlog BLD/TER, World terrain parts, Construction support/adoption/healing and MVP terrain scope; UE layout only for affected ownership.
- Write set: new `docs/decisions/DEC-03-terrain.md`; domain edits require a subsequent assigned write set.
- Output: explicit adoption of the baseline or a clearly proposed compatible scope exception; chunk/cell ownership, persistence/versioning, Unreal support/collision integration, edit bounds, structure protection, cross-boundary conflicts and restart behavior. Compare alternatives only where an evidenced MVP constraint requires it; identify missing measurements.
- Invariants/dependencies: compatible with DOC-02 for durable writes. Representation, topology and collision algorithms are release-bound; adjustable limits require typed declarations under tuning policy. Do not choose an algorithm based on an unrun benchmark.
- Acceptance/validation: alternatives have consequences and revisit triggers; proposed fixtures cover edits near claims/structures, chunk edges, simultaneous edits and recovery. Budgets remain provisional pending evidence. Reviewer checks against World and Construction ownership.

### DOC-04 / DOC-05 / DOC-06 — bounded follow-through

DOC-04 reads roadmap DEC-01…09 and MVP decision headings; returns proposed register rows to the coordinator, who alone edits this file. Map duplicate questions, owner, dependent backlog IDs, status, evidence needed and revisit trigger. Never accept a human scope/cost decision or claim a toolchain check from document text. Luna may extract the rows; the stronger coordinator resolves semantic differences.

DOC-05 reads tuning policy and only the settings on the selected E1/H2 path. Write `docs/design-tuning-audit.md` with canonical key/owner, classification, default, type, units, bounds, permissions, apply time and missing declarations. Invariants, formulas and topology stay release-bound. Each finding links its exact source and proposes a bounded owning-domain change; protected-domain requests remain pending. Stronger review is required where timing, authority or compatibility changes.

DOC-06 is a read-only follow-up to [coverage](design-coverage.md). Use Luna / medium for exact ownership/metric lookup in dependency-map-selected sections, returning source-backed rows to the coordinator. First check Analytics for the trade-share metric, then registry/Combat/Territory and directly named callers for crime, PvP mode and expedition ownership. Do not infer missing architecture from a failed keyword search. Coordinator classifies evidence and assigns a stronger author only for confirmed semantic gaps. COV-01/02 remain later event-owner reconciliation requests; reserved Itemization/Analytics writes require ownership release.

### DOC-08 / DOC-09 / DOC-10 — operating and compatibility decisions

Use the parent model / high reasoning for these cross-domain packets. Writes are isolated proposals under `docs/decisions/`; no protected-domain edits. Each packet returns source links, alternatives, proposed choice, remaining evidence, owner requests and revisit trigger for independent review.

- DOC-08 writes `DOC-08-recovery-profile.md`. Read MVP receipt retention/backup questions, Backend idempotency/backup requirements and Construction recovery. Backend already specifies at least seven-day idempotency and 35-day backup retention; verify exact applicability before proposing an MVP profile. Acceptance: explicit replay/dedup/decision lifetimes, unresolved-operation protection, cleanup eligibility, backup schedule, restore reconciliation and compatibility with these existing requirements. Any exception stays proposed pending the owning decision; documentation cannot establish restore performance.
- DOC-09 writes `DEC-04-session-applicability.md`. Read roadmap DEC-04 and Backend Identity session semantics, then referenced Security-owned sections only as needed. Acceptance: MVP issuance/validation/reconnect and active-character authority map, single-session conflict cases, selected adapter and explicit pending Security-owner changes. Do not invent a second authentication protocol.
- DOC-10 writes `DEC-08-compatibility.md`. Read roadmap DEC-08 and Client Delivery/Database compatibility sections after DOC-02/03. Acceptance: client/server/content/schema compatibility matrix, minimum-build behavior, migration and rollback boundaries, interrupted update/recovery cases and C4 evidence requirements. Runtime proof remains a separate implementation gate.

DOC-01 traced the world-boss lead: AI/Itemization already define participation and grants. See design-coverage.md COV-01 for the stale World open issue, COV-02 for an event-grant key/retry discrepancy, and COV-03 for a trade-share metric requiring Analytics lookup. These are later-scope follow-ups, not new H2 blockers; protected-owner changes remain reserved.

## Model routing and independent review

| Assignment | Model / reasoning | Use and escalation |
| --- | --- | --- |
| Bounded inventory, exact links, prepared row extraction | gpt-6-luna / low | Fixed read set and no semantic decisions; use medium if tracing requires multiple referenced sections |
| Coverage synthesis, DOC-02/03, cross-domain design | Parent model / high | Narrow owning-section reads; escalate uncertainty through explicit decision/evidence needs |
| Independent gap/closure reviewer | Separate agent, parent model / high | Did not author reviewed changes; inspect sources and diff, not only author's summary |
| Tiny index/status edits | Coordinator directly | Delegation would add overhead |

Use Luna with a context-free bounded packet for mechanical work. Stronger agents receive only the context needed for their task where practical. Reuse a completed reviewer for a related review; avoid duplicate whole-project audits. Architecture/security work is never accepted solely from a small-model summary. Availability and active harness settings govern actual model/effort; record deviations rather than implying a configuration was applied.

After each author task:

1. Coordinator inspects actual changes and resolves shared-file ownership.
2. Independent agent reads changed sections and direct contracts; returns severity, source evidence, missing decision, affected task/gate and smallest correction, or an explicit no-findings result with review limits.
3. Author/coordinator fixes findings; reviewer rechecks the affected sections. Blocking findings reopen the task.
4. Coordinator marks Done only when acceptance and review evidence are present; record remaining unknowns and queue the next bounded work. Do not silently expand a completed task.

Documentation gate: selected milestone's scope/journey maps to owning contracts; names/writers and failure/recovery behavior agree; adjustable controls meet policy; acceptance cases are executable; required decisions are resolved or dependent work remains explicitly blocked. Later-game coverage may remain deferred by an explicit scope decision. Page counts and document presence do not establish completeness.

## Review and handoff log

| Date | Work / evidence | Result / next step |
| --- | --- | --- |
| 2026-10-17 | Coordinator read mapped planning/agent/tuning sections; Luna independently inventoried concept and planning coverage | Broad baseline exists; no distinct design queue found in inspected references. DOC-01 and DOC-02 recommended next. |
| 2026-10-17 | Independent reviewer inspected the complete plan, index entry and cited domain sections | No blockers; clarified DOC-04 coordinator-only writes. Review was bounded, not a comprehensive domain audit. |
| 2026-10-17 | Coordinator checked relative Markdown links in both changed documents | All targets exist; documentation-only change, no runtime tests. Git diff unavailable because this workspace has no Git metadata. |
| 2026-10-17 | DOC-01 stronger author and DOC-02 coordinator used inherited parent model/settings; separate reviewer checked source contracts and integration references | Both documentation tasks closed. COV-02 retry-layer clarification corrected and independently rechecked; no remaining review findings. Owner decisions/runtime gates stay open. |
| 2026-10-17 | Coordinator validated 86 inline/reference link targets across six changed documents | All targets exist; bounded source/heading checks complete. Documentation only; no runtime tests or Git verification. |
| 2026-10-17 | DOC-03 inherited stronger author; DOC-08 coordinator; separate source reviewer | Reviewed proposals closed. Added missing Construction terrain budgets after P2 review and independently rechecked. DOC-08 schedule, terrain retention and MVP rehearsal target verified; owner acceptance/runtime evidence pending. |
| 2026-10-17 | Coordinator checked 82 inline/reference link targets across six changed files | All targets exist. No runtime tests; no protected-domain edits. |
| 2026-10-17 | User requested GitHub publication and a durable continuation file | Baseline committed/pushed as `cc4fb2a` on `main`; Git is now available. Added root `design-agent.md` with user rules, reviewed-contract boundaries and next packets; documentation queue unchanged. |
| 2026-10-17 | DOC-04 Luna/low extraction reconciled by coordinator; DOC-05 inherited stronger author; separate inherited stronger reviewer inspected both outputs and owning sources | DOC-04/05 Done as bounded documentation. Reclassified undeclared movement/T9 values as release-bound conformance, not automatic defects; REC-03 lookup complete with owner classification pending. Reviewer rechecked with no remaining findings. No protected-domain edits or runtime tests. |
| 2026-10-17 | Final integration review and coordinator documentation checks | No blockers; corrected stale REC-03 status summaries. All 80 local Markdown targets and nine heading anchors across four changed files resolve. Whitespace check passes with Windows CR-at-EOL recognized; no runtime tests. |
| 2026-10-17 | DOC-09 and DOC-10 separate inherited stronger authors; independent inherited stronger reviewer inspected actual packets and owning sections | Both bounded proposals reviewed with no remaining findings after DOC-09 citation fixes. Ticket issuer conflict stays explicit; serialization/revocation, schema/codec/install adapters and all runtime fixtures remain owner gates. No protected-domain edits or runtime tests. |
| 2026-10-17 | Final DOC-09/10 integration review and coordinator documentation validation | No remaining findings; clarified unsafe-content rollback mitigation and rechecked citations. All 118 local link targets and ten heading anchors across seven changed files resolve. Whitespace check recognizes Windows CR-at-EOL; documentation only, no runtime tests. |
| 2026-10-18 | DOC-06 read-only Luna/medium lookup, coordinator direct-source synthesis and separate inherited stronger source reviewer | Bounded lookup Done with no review findings. COV-03 measurement remains undefined in sampled authorities; COV-04…06 request later scope/ownership decisions while preserving guard/siege/dungeon contracts. No protected-domain writes or runtime tests. |
| 2026-10-18 | Final DOC-06 integration review and documentation checks | No remaining findings. All 105 local link targets and six heading anchors across four changed files resolve; whitespace check passes with Windows CR-at-EOL recognized. Only coverage, queue, index and handoff changed. |
| 2026-10-18 | DOC-11 Luna/low requirement extraction, coordinator read-only host inventory and separate inherited stronger reviewer | No review findings; reviewer repeated named file/metadata checks. Inspected UE_5.8 root has only .egstore; VS/VC and representative SDK files exist. DEC-01/DOC-11 blocked on usable engine payload/root; no setup, builds, runtime tests or protected-domain writes. |
| 2026-10-18 | Final DOC-11 integration review and documentation checks | No findings. All 111 local link targets and eight heading anchors across six changed files resolve; whitespace check passes with Windows CR-at-EOL recognized. Stop at the documented engine-root prerequisite. |
| 2026-10-18 | User reports installation continuing; coordinator prepared DOC-12, separate inherited stronger reviewer checked owning sources | Reviewed assignment proposal Done. Corrected module Build.cs path and independently rechecked with no remaining findings. No engine polling, setup, code creation, protected-domain changes or runtime tests. |
| 2026-10-18 | Final DOC-12 integration review and documentation checks | No findings. All 108 local link targets and nine heading anchors across five changed files resolve; whitespace passes with Windows CR-at-EOL recognized. Executable E1/DEC-01 and DOC-07 remain gated. |
| 2026-10-18 | User reports installation complete; coordinator post-install read-only recheck, independent stronger reviewer repeated metadata checks and reviewed integration | No findings. UE5.8.3 CL58210709 payload confirmed; SDK range/MSVC non-preferred status recorded without compile claims. DOC-11 metadata Done, missing-payload prerequisite cleared; DEC-01/project/compile/server route remains open. All 125 local targets and 12 heading anchors across seven files resolve; whitespace passes with CR-at-EOL recognized. No setup, project, toolchain, remote server or runtime changes. |

Current handoff: overnight E1 implementation/checks passed as summarized above. Current priority is native Linux packaging on steam-server, not retrying blocked Windows UnrealPak. Native tool compiled and portable loose cook succeeded. Matching source archive hash verified on host; native build UID997, game UID998. Setup container sf-native-setup-20261018-101457-1505988 completed exit0; native UAT-help sf-native-uat-help-20261018-102926-1507829 also passed exit0; use bounded docker logs (tail -c to cap carriage-return progress), inspect state/disk, then native Linux UAT compilation, original receipt/tool transfer, stage/archive and remote startup/client proof. See docs/e1-linux-server-build.md and design-agent.md for live paths. Preserve Windrose/7777; new game17777. No game image/service or final E1 acceptance yet.
