# Hand-offs

A hand-off passes one unfinished task to the next session or agent. Each task is one Markdown file.

```
docs/hand-offs/
  README.md        these instructions
  TEMPLATE.md      copy this for a new task
  queue/           open tasks, one file each
  completed/       finished tasks, moved here unchanged except for the Outcome section
```

## Rules

- **Workers read only the task they are working on.** Open it by its file name; do not list, scan or read other files in `queue/` or `completed/`. A coordinating session may list queue filenames when the owner authorizes queue selection; that permission does not extend to workers or unrelated completed tasks.
- **Read `completed/` only for a reason**, such as a task that names an earlier one. Then open only the file or files it names.
- **One task per file**, named `<area>-<short-name>.md` in lower-case kebab case, e.g. `art-wolf-model.md`. Area is one of `art`, `gameplay`, `backend`, `ui`, `world`, `tools`, `owner`.
- **Keep it short.** Facts only: goal, state, evidence, next step. Link to commits, docs and folders instead of copying their content.
- **Never put secrets** (passwords, keys, tokens) in a hand-off.

## How we work

- **One task, one worktree.** Each task names its worktree. Lanes are reusable worktrees: `pwsh tools/Enter-Lane.ps1 -Name <a..e>` creates `C:\Projects\SF-lane-<name>` from origin/main, or moves an existing clean lane up to origin/main. Run one Unreal-heavy job at a time across all lanes.
- **The main checkout (`C:\Projects\Shattered-Frontier`) is shared.** Other sessions keep staged changes there; don't edit, stage, stash or reset it. Untracked art and reviews that must outlive a lane go in its git-ignored `Artifacts\` folder.
- **Committing:** `git fetch`, rebase onto origin/main, stage your files by name (never `git add -A`, `.` or `commit -a`), check `git diff --cached --stat`, then `git push origin HEAD:main`. Never force-push, stash, autostash or reset others' work. Commit and push finished work without asking. End commit messages with the co-author trailer of whoever did the work (Codex for this workflow).
- **Removing a lane:** first unlink its junctions with `cmd /c rmdir` (`Artifacts\ThirdParty` and the others `tools/Link-WorktreeDeps.ps1` made), then `git worktree remove`. A remove that follows a junction deletes the main checkout's files.
- **Workers:** delegate only useful parallel work, with at most three subagents active at once. GPT-6.1 Sol handles implementation, design, difficult debugging and independent reviews; use high effort for judgement. GPT-6 Luna may handle simple, bounded mechanical tasks at medium effort. Give each a narrow brief and take back a short result. Run one Unreal-heavy job at a time across all lanes.
- **Visual work:** the builder verifies returned assets and captures; a fresh reviewer that did not build the work judges them against the concept using [the central routing](../blueprints/README.md#routing) and [visual review](../visual-review.md). Implement only its fix list. Stop after 3 rounds and hand it to the owner; completion requires owner sign-off.
- **3D art generation** runs on local models via the Asset Forge (`http://192.168.50.133:8090`). Fix generated models at the input-image level; never patch them in Blender. Raster generation follows [central routing](../blueprints/README.md#routing).
- **Unreal:** write only under `/Game/Frontier/`; stop only processes you started; with the editor open, use `python tools/Run-InEditor.py <script.py>` instead of commandlets. Run netchecks from PowerShell (Git Bash rewrites `/Game/` paths).
- **Design values:** never invent them. Add the question, with the default you used, to `docs/decisions/` for the owner.

## Writing a hand-off

1. Copy `TEMPLATE.md` to `queue/<area>-<short-name>.md`.
2. Fill every section. Write "none" when a section has nothing.
3. Commit it with your work. Stage it by name.

## Picking up a hand-off

1. Open the task file you were given, by name.
2. Read `docs/dependency-map.md` and only the area file the task names.
3. Check `Blocked on` first. If it is still blocked, stop and report, or move to another task you were given.
4. Do the `Next step`. Update `State` and `Evidence` in the file as you go.

## Finishing a hand-off

1. Fill in the `Outcome` section: commit hashes, checks run, what is left.
2. Move the file with `git mv docs/hand-offs/queue/<file> docs/hand-offs/completed/<file>`.
3. If work remains, write a new queue file for it rather than leaving the finished one in `queue/`.
4. Commit and push.
