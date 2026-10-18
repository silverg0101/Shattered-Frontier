# Construction sandbox: laptop handoff

Checkpoint: 2026-10-18. User played the updated cursor/palette prototype and reported **"works much better."** Record this as positive initial controls feedback; final UI design and exhaustive manual door/edge-case acceptance remain open.

Read `docs/dependency-map.md`, `design-agent.md`, then the top of `docs/design-work-plan.md`. Runtime implementation is in commit `bc1595c`; this handoff's commit adds feedback and the already-verified 21-step line runner. Automated checks and independent review are recorded in [the evidence](evidence/construction-sandbox-cursor-door-pilot.md).

## Resume on the laptop

Use the laptop's checkout path. Inspect `git status` before pulling and preserve any local edits; then `git pull origin main` when the checkout can be updated safely. If this is a new machine or the engine/toolchain is missing, follow `docs/first-time-setup.md`. The tested editor version is UE 5.8.3. Build the editor using [GameClient instructions](../GameClient/README.md); generated DLLs are not transferred by Git.

From the repository root after building (adjust the engine path if needed):

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' "$PWD\GameClient\ShatteredFrontier.uproject" /Game/Frontier/Maps/Greenlands_ConstructionSandbox -game -windowed -ResX=1440 -ResY=900
```

This launches the opt-in standalone graybox, not the LAN server. Free cursor aims; hold RMB for camera. LMB places in Build mode or removes in explicit Demolition mode; X/Delete or palette switches mode. E interacts with the doorway. F5/F9 save/load local development generations. Other bindings are in the GameClient README.

## Next work and machine-local state

Laptop continuation: editor build and the 16-step structural-input/E1-negative check passed. [SBX-07A packaging route](decisions/SBX-07A-isolated-package-route.md) and [checkpoint evidence](evidence/construction-sandbox-package-pilot.md) track the reviewed copied-project Game build and packaged finite-input harness. The first package exposed an editor-only save-fixture compile dependency; its WITH_EDITOR boundary is corrected and undergoing Game/editor verification before a fresh package. Remote desktop continuation state remains unverified; all current builds/artifacts are local. Resume from the top of the work plan for current status.

Current assignment: finish the reviewed isolated package build and acceptance route without changing E1 defaults or the normal cook list. Approved scenery/materials and two-client construction acceptance remain open. Continue collecting specific UI feedback; this playtest does not lock final bindings or approve all presentation details.

The desktop preview at `Artifacts/ConstructionSandbox/cursor-preview-20261018-191656` was an eight-file isolated copy, PID31112 at launch. Its Saved directory, compiled binaries and ignored runtime logs are machine-local; pulling Git does not transfer that world save or preview. Preserve them on the desktop. Tracked evidence includes hashes and summaries, not all raw logs. Concurrent art/model work has separate uncommitted/untracked files and is not included in this construction handoff.

The desktop continuation heartbeat was previously active every 15 minutes. Before concurrent implementation from the laptop, inspect its current state and coordinate a single writer/build owner; do not assume pulling the repository stops a desktop process or automation. This status-only checkpoint starts no new implementation/build work.
