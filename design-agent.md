# Design agent: continuation instructions

Last synchronized: 2026-10-18. This file preserves the user's documentation-orchestration instructions and session handoff. The live queue is [docs/design-work-plan.md](docs/design-work-plan.md); update it during every work cycle. Keep this continuation file synchronized when standing rules or the handoff materially change, without duplicating the full work log.

## User mandate and authorization

Act as the project's agent orchestrator and documentation specialist. Understand existing design evidence, identify the next priorities, and generate concrete work assignments and goals for other subagents until the project design is sufficiently defined for implementation. Maintain a running document tracking what comes next. Use an independent agent to look for design gaps after completed work. Selecting the correct model and reasoning effort is important for token efficiency.

The user has explicitly requested subagents and repeatedly authorized continuing the next assignments. On 2026-10-18 the user authorized E1 implementation, then selected `steam-server` instead of Unraid and explicitly authorized Unreal source-build/Linux SDK setup to get the game server running, including allocation/mounting of any drive there. Continue that bounded setup/build/deployment and routine checks; preserve the existing game container. Do not turn a request to continue into only a plan or ask again for authorized work. Resolve genuine missing scope/owner decisions explicitly; do not invent user acceptance. This mandate does not authorize unrelated implementation, public exposure, production release approval, or messages to other user-owned chats or external people.

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
| DOC-04 | Consolidated DEC-01…09 and MVP recovery register in [work plan](docs/design-work-plan.md#consolidated-decision-register--doc-04) | All owner decisions remain open; register is documentation evidence |
| DOC-05 | [Bounded tuning audit](docs/design-tuning-audit.md) | TUN-01/02 release-bound defaults permitted; live exposure requires declarations; REC-03 owner classification pending |
| DOC-06 | [Reviewed bounded concept lookup](docs/design-coverage.md#doc-06-lookup-limits-and-classification) | COV-03 metric definition and COV-04…06 later scope/owner decisions pending; no new E1/H2 requirement |
| DOC-08 | [Recovery profile](docs/decisions/DOC-08-recovery-profile.md) | Owner acceptance and REC evidence pending |
| DOC-09 | [Session applicability proposal](docs/decisions/DEC-04-session-applicability.md) | DEC-04 acceptance, ticket caller reconciliation, atomic entry/fencing, connected-server revocation and SESS fixtures pending |
| DOC-10 | [Compatibility proposal](docs/decisions/DEC-08-compatibility.md) | DEC-08 acceptance, binding/install adapter, schema/codec/rebase choices and CMP fixtures pending |
| DOC-11 | [Reviewed DEC-01 post-install evidence](docs/decisions/DEC-01-engine-evidence.md) | Metadata recheck Done: UE5.8.3 CL58210709 payload present; actual compiler/project/target and dedicated-server route evidence pending |
| DOC-12 | [Reviewed E1 implementation packet](docs/e1-implementation-packet.md) | Project/authority/fixture/check preparation Done while installation runs; execution and E1 acceptance remain gated |

All completed packets received independent review and relevant corrections were rechecked. Last DOC-03/DOC-08 integration validated 82 Markdown link targets. No runtime tests were run for these documentation-only changes; no protected-domain files were changed by these assignments.

Preserve these established contracts when continuing; consult the linked proposals and their exact sources rather than treating this summary as a replacement specification:

- DEC-02 adopts existing Construction Integration: building coordinator `cba_`, Inventory sole item writer, World terrain writer, separate local participant transactions even when cohosted. Prepare all participants, persist immutable commit/abort, apply held/unavailable results, obtain all digests, persist completion certificate, then unlock/activate and expose success. A raw commit decision is not terminal success. Abort only undecided work; committed recovery rolls forward. No timeout refund or participant expiry. Generic seven-day dedup does not replace durable action/key history while references remain. Proposed cohosting/PostgreSQL is not accepted topology.
- DEC-03 adopts existing World 0.5 m voxels, 16 m chunks/32 cubed cells, deterministic signed int16 Q15 arithmetic, packed chunk keys, single layer-0 writer, expected heads and owner epochs. Density is authoritative; Unreal mesh/collision is derived. Inherit World protections and Construction budgets: at most 8 chunks and 8,192 touched cells/action; 20,000 outstanding public changed cells/character/realm. Collision failure after commit stays blocked/rebuilds. Executable codec details, safe collision/readiness measurements and Compact provenance mapping remain owner gates; do not invent a seventh World op enum. Undeclared tuning remains release-bound pending full declaration.
- DOC-08 adopts detailed Backend failure-class RPO/RTO and backup/retention policy. PITR 7 days, daily/weekly retained backups 35 days, world Redis hourly RDB 3 days, published outbox 3 days/unpublished retained, event topics 14 days, inbox over 15 days, DLQ 30 days. Terrain ops through snapshot frontier delete 30 days after compaction, distinct from backup retention. Do not expire unresolved decisions/holds or widen privacy-sensitive retention casually. Restore must fence writers, replay erasure evidence, reconcile decisions and cross-store frontiers, then open only after gates pass. The MVP's provisional 30-minute rehearsal target is separate from failure-class recovery targets.
- DOC-09 proposes the existing Backend/Security UE adapter. Backend §4.7 draws character-service issuing a ticket; Security §9.3 and §7.4 name gateway. Proposed gateway orchestration needs joint acceptance, not an inferred new permission. Session concurrency and character presence are separate gates; validation-cache TTLs and reconnect queue grace do not establish admission fencing or connected-UDP revocation bounds. Session loss cannot abort committed DEC-02 work.
- DOC-10 preserves LiveOps release tuple, N−1 rolling/minimum-build policy, Backend expand/migrate/contract and reference-safe content rollback. Compatibility evidence binding and transactional install staging remain proposed adapters, not accepted schemas. Unsafe content references forbid rollback; disabling definitions is the alternative, not proof rollback is now safe. Terrain base/codec/action formats and cross-store/erasure recovery require their own evidence.

## Open findings to carry forward

- COV-01: World open issue asking AI to define participation/rewards is stale; AI Director and Rewards already define them. Later owner reference correction, not a new H2 blocker.
- COV-02: AI event-grant key `<wev_id>:<character_id>` differs from Rewards `{source}:{source_ref}:{character_id}:{bundle_id}`. Require canonical mapping and operations handoff. Caller retries and downstream six-day retry review are different layers and are not proven contradictory. Protected Itemization writes remain reserved.
- COV-03: DOC-06 inspected Analytics Warehouse §§4.6/6.6 and Economy targets. `m_market_velocity` measures weekly trade value divided by money supply, not player-trade share. Economy/Analytics must define the concept measure's denominator/population/NPC treatment/window/aggregation or explicitly defer/retire it. No invented metric setting; Analytics writes remain reserved.
- COV-04…06: later scope/owner decisions recorded in coverage. AI guards already have attack-triggered offender memory (600 s), faction hostility and safe-zone NPC reactions; broader theft/retaliation/criminal lifecycle is not established by that or by anti-cheat sanctions. Territory siege is specified, but it does not establish every named PvP mode. Dungeon defines five types but does not map the expedition label in inspected sections. Product owners must choose mapping/scope/deferral and leads before dependent authoring; no global absence or new H2 blocker claimed.
- REC-01: DLQ 30-day lifetime exceeds inbox retention; restored inboxes may predate already-visible effects. Require domain durable dedup/reconciliation or quarantine evidence, not an assumed blanket TTL extension.
- REC-02: a retained 35-day snapshot may lack sufficient WAL/event/dedup history for automatic recovery. Owner runbooks must establish eligibility; no blind replay or guessed refunds.
- REC-03: DOC-05 completed bounded Backend/Registry/LiveOps lookup; no complete key-specific live declaration found. Policy keeps the seven-day default release-bound, which is not itself a compliance defect. Backend owner must confirm release classification or deliberately declare live exposure; no invented bounds or extension of the 35-day erasure ceiling.

## Resume with these assignments

1. Read the map and live queue. Check current working changes and ownership before assigning writers. DOC-00…06 and DOC-08…10 are Done as bounded documentation, not accepted/runtime-ready domains. DOC-06 used read-only Luna/medium extraction, coordinator source synthesis and a separate inherited stronger source reviewer with no findings. Do not rerun completed inventories.
2. User subsequently reported installation complete. DOC-11 post-install recheck independently verified UE5.8.3 CL58210709/compatible55116800, editor, Build.bat, RunUAT, UBT DLL and InstalledBuild marker at `C:\Program Files\Epic Games\UE_5.8`. The earlier `.egstore`-only snapshot is retained as history, no longer a blocker. SDK10.0.26100.0 is within engine declarations. Available MSVC14.51.36231 is above minimum and outside banned/preferred ranges: non-preferred, not a demonstrated failure. Actual selection/compile is still required. BaseEngine.ini has no Server target rows; Linux Game entries do not establish a Linux dedicated package. No engine/toolchain/project changes or launches were performed.
3. COV-03…06 owner requests are recorded in the work plan's pending concept-owner table. Author only selected behavior with available write ownership after those decisions; no automatic crime/PvP/expedition rewrite. COV-01/02 stay later event-owner requests; Analytics/Itemization and other reserved writes require ownership release.
4. User authorized E1 implementation with Unraid selected as the Linux dedicated-server target. The initial project, editor build, map and connection smoke are now recorded in `docs/evidence/e1-foundation.md`; use its remaining cards for next work. Do not repeat metadata inventory unless relevant paths/builds change or downgrade the compiler automatically. Linux source-engine/SDK preparation and packaging remain pending; no deployment has run. Independently review implementation and keep focused executable checks separate from documentation checks. DOC-07 stays open and does not certify runtime readiness.

Decision register topics already exist: DEC-01 engine/toolchain; DEC-02 topology/durable applicability; DEC-03 terrain; DEC-04 session/auth; DEC-05 hardware/budgets; DEC-06 market/escrow scope; DEC-07 assets/artifacts; DEC-08 compatibility/update/rollback; DEC-09 hosting/cost/data. Do not relabel them as newly discovered missing protocols.

## Git and environment handoff

Workspace: `C:\Projects\Shattered-Frontier`; PowerShell. Set command workdir explicitly. Repository: [silverg0101/Shattered-Frontier](https://github.com/silverg0101/Shattered-Frontier), remote `origin`, branch `main`, tracking `origin/main`.

The folder originally had no Git metadata and the remote was empty. On 2026-10-17 the user authorized connecting it and publishing the baseline. Initial commit `cc4fb2ace4607f7c2b29f55fa26270069259039c` contains 116 files; local and remote matched and the tree was clean immediately afterward. Historical work-log statements that Git verification was unavailable describe the earlier assessment, not the current environment. Recheck current status and remote before any later commit.

Repository-local identity was set from the supplied GitHub account profile: `Greg silver`, `3045043+silverg0101@users.noreply.github.com`. Git HTTPS push succeeded through existing credentials even though GitHub CLI was not logged in. Do not expose credentials or replace authentication unnecessarily. `.gitignore` excludes `data/*.sqlite*` and `node_modules/`. Initial whitespace review found existing trailing blank lines; no broad normalization was performed. Never claim clean whitespace checks or runtime checks that were not achieved.

Current continuation: user selected steam-server and authorized storage/source/SDK setup plus deployment. New200GiB LV mounted at /srv/shattered-frontier; non-root sfserver created, repeat provisioning passed, existing game container healthy. Windows source checkout C:\UnrealSource\UE_5.8.3 completed at pinned396c9f059903aed5fec78ecd3d437a40c6415368; cross-SDK installed C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8 with verified Epic signature. Dependency sync remains active; logs under ignored Artifacts/Unreal/Setup. Next finish dependencies, run source UAT Linux Server+host UnrealPak with matching external Launcher cook editor, inspect package, build Docker runtime and prove remote client connections. New source/cooker build helper static validation passed; ops/steam-server Docker/profile files are prepared but no game image or service is running yet. UDP7777 is occupied by existing game; use17777, verified free. See docs/e1-linux-server-build.md and docs/evidence/e1-foundation.md; update actual results before publishing. Preserve unrelated workloads, second SSD, protected design files and browser runtime.
