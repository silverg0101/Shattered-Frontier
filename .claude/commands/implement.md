---
description: Implement a feature spec end to end and record evidence
argument-hint: <spec path or ID>
---
Implement the spec `$ARGUMENTS` (a path under `docs/`, or an ID such as FEAT-012 to find under `docs/specs/`).

1. Read `docs/dependency-map.md`, then the spec. If the spec lacks testable acceptance (named tests or log markers), stop and ask.
2. Keep to the files the spec and map name. Editor work that must last goes into committed scripts or assets; use the `unreal-editor` MCP server only for inspection and checks.
3. Update `docs/dependency-map.md` in the same change when routes, files, tests or commands change.
4. Build with `/build-editor`, then run only the spec's named tests (`/test`, `/netcheck`).
5. Add or update the spec's Evidence section: commit, tests run with results, `Artifacts/` path, and anything not verified.

Do not commit or push unless I ask.
