# Working in this repository

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
