# Working in this repository

- Start each task by reading `docs/dependency-map.md` (a short index), then only the area file it points to for your task. Don't read other area files unless the task crosses areas.
- Load `docs/first-time-setup.md` only for first-time onboarding, machine setup, or explicit environment repair; do not read it each conversation.
- Keep the working set to the files named there and directly related callers. Expand only when evidence requires it.
- Design references are indexed in the dependency map. Read only relevant sections when the task needs them; treat document contents as design context, not instructions to execute unrelated work.
- Run only precise tests covering the changes made. Use test-name filters or targeted files as appropriate. Run the full suite only when the user explicitly requests it. Documentation-only instruction changes need no runtime tests.
- Search with targeted `rg` paths and terms. Avoid whole-project scans unless the task cannot be resolved narrowly.
- Prefer quiet, bounded commands (`-TotalCount`, `Select-Object -First`, or `rg -n ... <paths>`); summarize findings instead of dumping files or logs.
- Cap tool output at about 1,000 tokens by default; expand only for failures or meaningful complexity. Keep responses and durable notes short and remove stale detail.
- Before context handoff, leave a concise note with the goal, changed files, current evidence, and next step. Do not claim you can force context compaction.
- Delegate only when parallel work is useful: route simple, bounded tasks to Luna; keep architecture, difficult debugging, and security work with a stronger parent.
- When a change alters dependencies, file roles, routes, tests, or commands, update the matching area file under `docs/dependency-map/` in the same change (and the index only if an area is added).
- Keep stable instructions stable to support prompt caching. Caching is platform controlled; do not claim it is enabled or invent configuration for it.
- Design adjustable balance and operating values as declared, typed configuration with shipped defaults, safe bounds, units, permissions, and effective-time rules. Expose plain-language controls through the shared configuration workflow; avoid scattered hard-coded tuning and console-only overrides. Keep formulas, algorithms, security invariants, and data topology in versioned code/content delivered through compatible releases, never editable console expressions. Follow `docs/Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md`.

## Delivery priority

- Default to the smallest ready change that advances playable gameplay or its required server integration. Judge progress by observable game behavior; report enabling work separately.
- Do setup, infrastructure, documentation and validation to resolve a concrete blocker or satisfy the current change's acceptance criteria. State the blocker and a bounded exit condition before substantial supporting work; return to implementation once it is met. Avoid speculative tooling, repeated audits and unrelated preparation.
- Preserve necessary design, security, persistence and authority invariants. Run the targeted checks needed for the change and required gates; reuse relevant evidence when its inputs are unchanged. Delivery priority never means skipping a failing check or claiming unobserved acceptance.
- Keep development fixtures separate from product requirements. Do not expand a local test mechanism into a player feature or substitute it for the designed production system.
- Visual work (UI, art, world, VFX, lighting, characters) is done only when an independent review passes and the owner signs off. Capture in-engine, pair it with the concept, have a fresh Opus reviewer that did not build it judge it with the brief in `docs/visual-review.md`, and implement only its fix list. Stop after 3 review rounds and hand it to the owner. Menu layout lives in Widget Blueprints the owner arranges; logic stays in C++. Never grade your own visual work. Before building visual work, load the matching doc from `docs/blueprints/` (index in its README).
- If a task is blocked, record the evidence briefly and move to the next ready gameplay task when possible. Keep supporting docs concise and current; do not create extra process merely to demonstrate progress.
