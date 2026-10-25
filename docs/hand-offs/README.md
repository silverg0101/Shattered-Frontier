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

- **Read only the task you are working on.** Open it by its file name. Do not list, scan or read other files in `queue/` or `completed/`.
- **Read `completed/` only for a reason**, such as a task that names an earlier one. Then open only the file or files it names.
- **One task per file**, named `<area>-<short-name>.md` in lower-case kebab case, e.g. `art-wolf-model.md`. Area is one of `art`, `gameplay`, `backend`, `ui`, `world`, `tools`, `owner`.
- **Keep it short.** Facts only: goal, state, evidence, next step. Link to commits, docs and folders instead of copying their content.
- **Never put secrets** (passwords, keys, tokens) in a hand-off.

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
