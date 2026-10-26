# Hand-offs

A hand-off passes one unfinished task to the next session or agent. Each task is one Markdown file: `queue/` for open tasks, `completed/` for finished ones. Copy `TEMPLATE.md` to start one.

## Rules

- **Read only your task.** Open it by name; don't scan `queue/` or `completed/`. Open a completed file only when your task names it.
- **One task per file**, named `<area>-<short-name>.md`. Area is `art`, `gameplay`, `backend`, `ui`, `world`, `tools` or `owner`.
- **Short.** Keep it to about 40 lines: goal, state, the next step. Put hashes, byte counts and logs in evidence files and link to them. Replace stale text; don't append history.
- **No secrets** in a hand-off.

## How we work

- **Land on main as you go.** Once a change's focused checks pass, rebase onto origin/main and push to main. Don't hold finished work on side or integration branches waiting for a bigger acceptance.
- **Committing:** `git fetch`, rebase onto origin/main, stage your files by name (never `git add -A`, `.` or `commit -a`), then `git push origin HEAD:main`. Never force-push, stash or reset others' work. Commit finished work without asking.
- **Worktrees:** each task names its worktree. `pwsh tools/Enter-Lane.ps1 -Name <a..e>` gives `C:\Projects\SF-lane-<name>` at origin/main. The main checkout is shared; don't edit, stage or reset it. Untracked keepers go in its git-ignored `Artifacts\`.
- **Removing a lane:** first unlink its junctions without following them (`[System.IO.Directory]::Delete(path, $false)`), then `git worktree remove`. Following a junction deletes main's files.
- **Checks:** run only the focused tests for your change. Prefer an automated netcheck or test over a manual run. A manual playtest by the owner is optional, not a gate.
- **Deploying to the LAN server:** `pwsh tools/Deploy-Lan.ps1` (backup, build, swap, health, automatic rollback). See `ops/steam-server/playable-release.md`.
- **Visual work:** a fresh reviewer that didn't build it judges it with [visual review](../visual-review.md); implement only its fix list. After 3 rounds it goes to the owner, and owner sign-off ends it. If round 2 still fails on the same kind of defect, change route instead of iterating.
- **Unreal:** write only under `/Game/Frontier/`, run one Unreal-heavy job at a time, and stop only processes you started. Run netchecks from PowerShell.
- **Design values:** never invent them. Note the default you used in `docs/decisions/` for the owner.

## Picking up and finishing

1. Open the named task, read `docs/dependency-map.md` and the one area file it needs.
2. If `Blocked on` still holds, report it and move to your next task.
3. Do the `Next step`; keep `State` current.
4. When done, fill in `Outcome`, `git mv` the file to `completed/`, write a new queue file for any remaining work, commit and push.
