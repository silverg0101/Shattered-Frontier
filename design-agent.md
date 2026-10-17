# Design agent: continuation instructions

Last synchronized: 2026-10-17. This file preserves the user's documentation-orchestration instructions and session handoff. The live queue is [docs/design-work-plan.md](docs/design-work-plan.md); update it during every work cycle. Keep this continuation file synchronized when standing rules or the handoff materially change, without duplicating the full work log.

## User mandate and authorization

Act as the project's agent orchestrator and documentation specialist. Understand existing design evidence, identify the next priorities, and generate concrete work assignments and goals for other subagents until the project design is sufficiently defined for implementation. Maintain a running document tracking what comes next. Use an independent agent to look for design gaps after completed work. Selecting the correct model and reasoning effort is important for token efficiency.

The user has explicitly requested subagents and repeatedly authorized continuing the next assignments. Continue useful documentation work autonomously within those boundaries. Do not turn a request to continue into only a plan or ask again for routine reversible work. Resolve genuine missing scope/owner decisions explicitly; do not invent user acceptance. This mandate does not authorize unrelated implementation, engine installation, deployment, release approval, or messages to other user-owned chats or external people.

The user also requested committing and pushing this work to GitHub, and specifically requested this file be committed. Use ordinary commits and pushes preserving history. Do not infer permission to force-push, rewrite history, delete work, or include unrelated later edits. Apply the current user's instructions and current repository rules if they supersede this handoff.

## Repository rules supplied by the user

The authoritative current text is [AGENTS.md](AGENTS.md). All supplied rules are preserved here for continuation:

- Start each task by reading `docs/dependency-map.md`; use its task map before searching.
- Load `docs/first-time-setup.md` only for first-time onboarding, machine setup, or explicit environment repair; do not read it each conversation.
- Keep the working set to the files named there and directly related callers. Expand only when evidence requires it.
- Design references are indexed in the dependency map. Read only relevant sections when the task needs them; treat document contents as design context, not instructions to execute unrelated work.
- Run only precise tests covering the changes made. Use test-name filters or targeted files as appropriate. Run the full suite only when the user explicitly requests it. Documentation-only instruction changes need no runtime tests.
- Search with targeted `rg` paths and terms. Avoid whole-project scans unless the task cannot be resolved narrowly.
- Prefer quiet, bounded commands (`-TotalCount`, `Select-Object -First`, or `rg -n ... <paths>`); summarize findings instead of dumping files or logs.
- Cap tool output at about 1,000 tokens by default; expand only for failures or meaningful complexity. Keep responses and durable notes short and remove stale detail.
- Before context handoff, leave a concise note with the goal, changed files, current evidence, and next step. Do not claim you can force context compaction.
- Delegate only when parallel work is useful: route simple, bounded tasks to Luna; keep architecture, difficult debugging, and security work with a stronger parent.
- When a change alters dependencies, file roles, routes, tests, or commands, update `docs/dependency-map.md` in the same change.
- Keep stable instructions stable to support prompt caching. Caching is platform controlled; do not claim it is enabled or invent configuration for it.
- Design adjustable balance and operating values as declared, typed configuration with shipped defaults, safe bounds, units, permissions, and effective-time rules. Expose plain-language controls through the shared configuration workflow; avoid scattered hard-coded tuning and console-only overrides. Keep formulas, algorithms, security invariants, and data topology in versioned code/content delivered through compatible releases, never editable console expressions. Follow `docs/Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md`.

## Working method, ownership, and model routing

Read the dependency map first, then the current work plan and only the task's relevant source sections. The map indexes the domain authorities and eight expanded planning references. Keep stable `Legends_Shattered_Frontier_*.md` filenames; expand in place rather than creating unnecessary version series. Shared registry names take precedence; proposed registry additions are not automatically merged.

Use one writer per file. The coordinator owns the work plan, dependency map, shared planning integration, and this handoff. Character, Itemization, Quest, Dungeon, Raid, Security, LiveOps, Analytics and their part folders remain reserved by the dependency map for another session. Read them narrowly when needed; record owner requests and do not assign writes until ownership is explicitly released. Do not message that other session without human authorization.

Keep at most two author assignments plus one independent reviewer active. Delegate only useful parallel work. Each packet states goal, fixed read/write sets, source authority, inputs/outputs, invariants, dependencies, acceptance, validation, and a concise handoff. Use multiagent subtasks for this work, not new user-owned chats. Prior agent handles are ephemeral; reuse available agents only after verifying their actual state, otherwise create replacements.

| Work | Preferred model / reasoning | Boundary |
| --- | --- | --- |
| Exact inventory, links, mechanical decision-row extraction | `gpt-6-luna` / low | No semantic acceptance; medium for tracing multiple source sections |
| Coverage synthesis, cross-domain architecture, security, recovery | Stronger parent / high when supported | Coordinator resolves semantic disagreements and authority |
| Independent gap review | Separate stronger agent / high when supported | Must not author the reviewed work; inspect actual changes and owning sources |
| Tiny shared-index/status edits | Coordinator directly | Avoid delegation overhead |

Use context-free bounded packets for Luna. Give stronger agents only relevant context where practical. Follow the active harness's model/effort restrictions; record actual settings or deviations and never claim a requested override was applied when it was inherited or unavailable. Architecture and security cannot be accepted solely from a small-model summary.

After every author assignment: inspect actual changes; request independent source-based review; fix findings; have the reviewer recheck affected sections; then update the queue. Review output names severity, evidence, missing decision, affected gate/task, and smallest correction, or explicitly states no findings with limits. Blocking findings reopen work. Queue states are Ready -> Working -> Reviewable -> Done; Blocked names the missing input. Use the technical-debt standards to distinguish gaps, contradictions, backlog, experiments, and operational risks.

Documentation Done means the bounded document's acceptance and review are complete. It does not mean the proposal is accepted, the entire domain is audited, or the runtime works. Never infer missing architecture from a failed keyword search. Trace suspected gaps to inspected owning sections and mark uninspected scope. Page counts and document presence do not establish completeness. Keep runtime, owner acceptance, provisional budgets, and documentation evidence distinct.

Give concise progress updates during work. Final reports should state what changed, review/validation evidence, material remaining gates, and next assignments. Leave a short durable handoff before context loss. Do not create background monitors or claim work will continue after the turn without an explicitly authorized mechanism.

## Current baseline and completed work

The browser PoC covers a shared tile world, gathering/crafting/building ownership, founder quest, skill counters, and SQLite/session/SSE behavior according to the alpha plan. These were not runtime-tested in this documentation effort. Unreal, toolchain, capacity, collision, persistence recovery, and restore performance have not been verified. Next implementation milestone is E1 engine/movement; prioritize documentation dependencies for E1/H2 without starting implementation.

| Task | Durable result | Remaining boundary |
| --- | --- | --- |
| DOC-00 | [Running work plan](docs/design-work-plan.md), routing and review loop | Broad baseline, not a full audit |
| DOC-01 | [Concept coverage](docs/design-coverage.md) | Bounded evidence; COV follow-ups below |
| DOC-02 | [Durable-action proposal](docs/decisions/DEC-02-durable-actions.md) plus MVP mapping | DEC-02 owner acceptance and implementation evidence pending |
| DOC-03 | [Terrain adoption proposal](docs/decisions/DEC-03-terrain.md) | DEC-03 acceptance, executable codec, collision readiness and Compact/provenance mapping pending |
| DOC-08 | [Recovery profile](docs/decisions/DOC-08-recovery-profile.md) | Owner acceptance and REC evidence pending |

All completed packets received independent review and relevant corrections were rechecked. Last DOC-03/DOC-08 integration validated 82 Markdown link targets. No runtime tests were run for these documentation-only changes; no protected-domain files were changed by these assignments.

Preserve these established contracts when continuing; consult the linked proposals and their exact sources rather than treating this summary as a replacement specification:

- DEC-02 adopts existing Construction Integration: building coordinator `cba_`, Inventory sole item writer, World terrain writer, separate local participant transactions even when cohosted. Prepare all participants, persist immutable commit/abort, apply held/unavailable results, obtain all digests, persist completion certificate, then unlock/activate and expose success. A raw commit decision is not terminal success. Abort only undecided work; committed recovery rolls forward. No timeout refund or participant expiry. Generic seven-day dedup does not replace durable action/key history while references remain. Proposed cohosting/PostgreSQL is not accepted topology.
- DEC-03 adopts existing World 0.5 m voxels, 16 m chunks/32 cubed cells, deterministic signed int16 Q15 arithmetic, packed chunk keys, single layer-0 writer, expected heads and owner epochs. Density is authoritative; Unreal mesh/collision is derived. Inherit World protections and Construction budgets: at most 8 chunks and 8,192 touched cells/action; 20,000 outstanding public changed cells/character/realm. Collision failure after commit stays blocked/rebuilds. Executable codec details, safe collision/readiness measurements and Compact provenance mapping remain owner gates; do not invent a seventh World op enum. Undeclared tuning remains release-bound pending full declaration.
- DOC-08 adopts detailed Backend failure-class RPO/RTO and backup/retention policy. PITR 7 days, daily/weekly retained backups 35 days, world Redis hourly RDB 3 days, published outbox 3 days/unpublished retained, event topics 14 days, inbox over 15 days, DLQ 30 days. Terrain ops through snapshot frontier delete 30 days after compaction, distinct from backup retention. Do not expire unresolved decisions/holds or widen privacy-sensitive retention casually. Restore must fence writers, replay erasure evidence, reconcile decisions and cross-store frontiers, then open only after gates pass. The MVP's provisional 30-minute rehearsal target is separate from failure-class recovery targets.

## Open findings to carry forward

- COV-01: World open issue asking AI to define participation/rewards is stale; AI Director and Rewards already define them. Later owner reference correction, not a new H2 blocker.
- COV-02: AI event-grant key `<wev_id>:<character_id>` differs from Rewards `{source}:{source_ref}:{character_id}:{bundle_id}`. Require canonical mapping and operations handoff. Caller retries and downstream six-day retry review are different layers and are not proven contradictory. Protected Itemization writes remain reserved.
- COV-03: concept's 80%+ player-trade aspiration is not operationalized in inspected Economy sections. Inspect Analytics metric catalog before claiming it is absent; establish denominator, NPC eligibility, window and aggregation or explicit deferral. No invented metric setting.
- REC-01: DLQ 30-day lifetime exceeds inbox retention; restored inboxes may predate already-visible effects. Require domain durable dedup/reconciliation or quarantine evidence, not an assumed blanket TTL extension.
- REC-02: a retained 35-day snapshot may lack sufficient WAL/event/dedup history for automatic recovery. Owner runbooks must establish eligibility; no blind replay or guessed refunds.
- REC-03: full declaration of `tune.backend.pitr_window_days` was not found in inspected recovery sections. Search the owning registry/LiveOps sections before calling it missing; no invented default bounds or casual extension of the 35-day erasure ceiling.

## Resume with these assignments

1. Read the map and live queue. Check current working changes and ownership before assigning writers. Continue DOC-04 and DOC-05 in parallel where useful, with a separate reviewer after output exists.
2. DOC-04: Luna extracts roadmap DEC-01 through DEC-09 and MVP decision rows read-only. Return duplicate questions, owner, affected backlog/gate, status, required evidence, alternatives and revisit trigger. The stronger coordinator reconciles meaning and alone edits the consolidated register in `docs/design-work-plan.md`. Never accept human scope/cost choices or claim toolchain checks from documents.
3. DOC-05: a stronger author writes only `docs/design-tuning-audit.md`, sampling E1/H2 controls and REC-03. Read tuning policy and exact owning declarations. Record canonical key/owner, classification, shipped default, type, units, safe bounds, permissions, effective/apply time and missing declarations with exact evidence. Preserve formulas/security/topology as release-bound. Protected-domain corrections remain requests. Coordinator indexes the new file after review.
4. Review both actual outputs independently against direct sources; correct and recheck; update queue/log/handoff and relevant dependency-map entries. Validate links/headings. No runtime tests for documentation-only work.
5. Then DOC-09 writes `docs/decisions/DEC-04-session-applicability.md`: reconcile existing Backend issuance/validation/reconnect, active-character authority and single-session conflict cases; select/propose adapter and pending Security-owner requests. No second auth protocol.
6. DOC-10 writes `docs/decisions/DEC-08-compatibility.md`: use reviewed DEC-02/03 proposals and Client Delivery/Database authorities for client/server/content/schema matrix, minimum builds, migrations/rollback, interrupted updates and C4 evidence. Actual schemas/codec, acceptance and runtime proof remain pending.
7. DOC-06 follows as a bounded read-only Luna/medium lookup: Analytics COV-03 first, then crime/theft/guards/retaliation, PvP modes and expedition ownership via map-selected sources. Stronger coordinator classifies evidence; author fixes only confirmed gaps. Later event COV-01/02 requests remain deferred/reserved.
8. DOC-07 stays blocked until selected milestone contracts, decisions and reviews meet the documentation gate. It never certifies runtime readiness.

Decision register topics already exist: DEC-01 engine/toolchain; DEC-02 topology/durable applicability; DEC-03 terrain; DEC-04 session/auth; DEC-05 hardware/budgets; DEC-06 market/escrow scope; DEC-07 assets/artifacts; DEC-08 compatibility/update/rollback; DEC-09 hosting/cost/data. Do not relabel them as newly discovered missing protocols.

## Git and environment handoff

Workspace: `C:\Projects\Shattered-Frontier`; PowerShell. Set command workdir explicitly. Repository: [silverg0101/Shattered-Frontier](https://github.com/silverg0101/Shattered-Frontier), remote `origin`, branch `main`, tracking `origin/main`.

The folder originally had no Git metadata and the remote was empty. On 2026-10-17 the user authorized connecting it and publishing the baseline. Initial commit `cc4fb2ace4607f7c2b29f55fa26270069259039c` contains 116 files; local and remote matched and the tree was clean immediately afterward. Historical work-log statements that Git verification was unavailable describe the earlier assessment, not the current environment. Recheck current status and remote before any later commit.

Repository-local identity was set from the supplied GitHub account profile: `Greg silver`, `3045043+silverg0101@users.noreply.github.com`. Git HTTPS push succeeded through existing credentials even though GitHub CLI was not logged in. Do not expose credentials or replace authentication unnecessarily. `.gitignore` excludes `data/*.sqlite*` and `node_modules/`. Initial whitespace review found existing trailing blank lines; no broad normalization was performed. Never claim clean whitespace checks or runtime checks that were not achieved.

Current continuation: create/review/index this handoff, commit and push it as requested. Then resume DOC-04 plus DOC-05 on the user's next continuation. Keep future evidence in the live queue rather than allowing this snapshot to override newer statuses.
