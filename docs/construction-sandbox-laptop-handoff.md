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

New gameplay checkpoint: Upper-floor line placement is complete: P/PageUp/B creates3supported upper slabs in one revision16, both clients agree on8members and exact collision/content. Focused layer/control test1/1 clean; editor build27.06s; integration171.52s including lengthy editor startup; cleanup complete. [Evidence](evidence/construction-network-layer-pilot.md). User priority: development progress; batch substantive work and keep checks/docs proportional. Next: [free Fab candidate register](Game-Design/Legends_Shattered_Frontier_Gameplay_Reuse_Candidate_Register_v0.1.md) records inventory/interaction/gathering options and later dialogue/animation/UI uses. Evaluate source and Linux/authority fit before the INV/GAT integration; preserve governing mechanics unless a documented player benefit justifies a design change. Gameplay worktree remains isolated from the environment checkout.

Laptop continuation: editor/Game builds, 16-step structural-input/E1-negative and retained editor save regression passed. Local branch `codex/construction-package-laptop`, commit `be15127`, adds the reviewed [SBX-07A route](decisions/SBX-07A-isolated-package-route.md). Fresh package build/container verification passed, but managed Defender ASR blocks Game execution; no packaged acceptance is claimed. [Evidence](evidence/construction-sandbox-package-pilot.md) records the blocker. Continue gameplay through the supported editor; no policy workaround or more package reruns. Remote desktop state is unverified; current artifacts are local. See the work plan for next gameplay work.

Current assignment: advance ready gameplay/server integration; packaged execution awaits policy-owner resolution. E1 defaults and the normal cook list remain unchanged. Approved scenery/materials and two-client construction acceptance remain open. Continue collecting specific UI feedback; this playtest does not lock final bindings or approve all presentation details.

The desktop preview at `Artifacts/ConstructionSandbox/cursor-preview-20261018-191656` was an eight-file isolated copy, PID31112 at launch. Its Saved directory, compiled binaries and ignored runtime logs are machine-local; pulling Git does not transfer that world save or preview. Preserve them on the desktop. Tracked evidence includes hashes and summaries, not all raw logs. Concurrent art/model work has separate uncommitted/untracked files and is not included in this construction handoff.

The desktop continuation heartbeat was previously active every 15 minutes. Before concurrent implementation from the laptop, inspect its current state and coordinate a single writer/build owner; do not assume pulling the repository stops a desktop process or automation. This status-only checkpoint starts no new implementation/build work.
