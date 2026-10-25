# Matching LAN game packages

- **Area/owner:** tools/backend integration / agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal/state
Complete: fresh matching LinuxServer and Win64 Development Client archives/content produced and verified at owner-approved freeze `ff59b35dd17fef0edd1a7b7f5b9161b867370bc8`. Artifact preparation only; no remote apply/deployment. No game source, geometry/material appearance, SDK, network-version or security-policy changes made by this task.

## Outcome
Immutable local root: `C:/Projects/SF-lane-e/Artifacts/PackagedServer/Matching-ff59b35d-20261025-175956`. Manifest paths below are relative to it.

| Artifact | Files / bytes | SHA256 manifest |
| --- | --- | --- |
| `LinuxServer/Archive` | 69 / 3,223,592,580 | `LinuxServer-archive-manifest.json`: `91b1b15ba927429e6ab3630a49661b5334b93e5319b2e2a516f7bf338816974d` |
| `Win64Client/Archive` | 79 / 5,250,026,353 | `Win64Client-archive-manifest.json`: `4b31f83f609342f1e1b6ed2efd6bea7559e0c63f0d26590fddbc807b7279b2d2` |
| `LinuxDockerContext` | 72 / 3,223,595,670 | `LinuxDockerContext-manifest.json`: `763d9106b3b13c7250e03d6e0e73ec62f4b2fb5460db30b907f26da934dfed4c` |

Windows launcher: `Win64Client/Archive/ShatteredFrontierClient.exe`; actual network-probed binary: `Win64Client/Archive/ShatteredFrontier/Binaries/Win64/ShatteredFrontierClient.exe` (350,078,976 bytes, x86-64 PE, SHA256 `2f79f5bb3825fb9315b073a12ef02dd3ddedab21a94d0e92c68139d5988d597c`). Linux executable: `LinuxServer/Archive/ShatteredFrontier/Binaries/Linux/ShatteredFrontierServer` (297,839,336 bytes, ELF64 little-endian x86-64 machine62, SHA256 `efa2ab6f55888ddb09d40e279854cb158ada5e2a1ff418a9fc86de8f5574acd1`).

## Evidence
- Pinned source engine `396c9f059903aed5fec78ecd3d437a40c6415368`, existing v26 clang20.1.8;120 Linux/121 Win64 gRPC archive hashes passed. No prerequisite installation/repair. MaxParallelActions8 on20 physical cores. Final source WinClient compile/link1083actions/1004.94s; Linux incremental8actions/98.01s; matching Launcher cooker project Editor DLL37.57s: all PASS. Launcher was cooker only; both runtime receipts sourceCL0.
- Source UAT Linux build/cook/stage/archive PASS1m26s; WinClient PASS1m36s. Existing deprecation warnings, no build/cook errors. Actual BOTH IoStore listings contain all8 canonical item WorldMesh packages, fallbackCube and owned M_ActionTargetOutline. Client contains4ShaderCodeLibrary/1563ShaderCode chunks. BOTH extracted archived configs retain old SFPilotBoar→SFCombatNpc redirect and r.CustomDepth3 (`cooked-asset-closure.json`, `client-shader-evidence.json`, `archived-config-checks.json`). Presence does not itself prove packaged ServerDrop bounds behavior.
- All28 canonical content files (26JSON+2MD) and3Silverbrook runtimeJSON match source hashes in both archives. Shared BackendServices336/content28/proto10 manifests independently match runtime_release. `frozen-runtime-inputs.json` binds2808 code/config/content/proto/backend/ops files, SHA256 `40a683a00ded20f469e6186e1c5fd1529942c528e745d0f5f6ff706832d0191e`; all2808 reverified unchanged after build/cook/probe. Raw proto SHA manifests are release evidence, not configured runtime NonUFS payload.
- Actual owned local packaged Linux+Windows probe: BOTH NetCL0/EngineNetworkVersion44/GameNetworkVersion0/checksum1737972550; Win client Welcomed Silverbrook and server authoritative pawn possession observed (`network-version-check.json`, both network logs). Deliberately services-disabled probe logged missing development WorldContent bridge/COMBAT_PILOT_DISABLED; no service-backed gameplay acceptance claimed. Owned75s server timeout delivered SIGINT with engine shutdown (wrapper124/engine130); owned client PID stopped. Generated Saved files preserved outside archives in RuntimeProbeSaved; sealed archive payloads/hashes reverified. UE slot RELEASED; no owned processes remain.
- Docker context copies exact frozen Dockerfile, entrypoint.sh and playable-entrypoint.sh, includes playable-runtime=1 label and preserves default E1 entrypoint. Context remains immutable while runtime_release audits/streams it. No game-image or deployment success claimed here. Earlier provisional cb6 Linux compile is separate evidence, not final ff59 output.

## Remaining ownership
Matching-artifact criteria are met. Game/services image, service-backed packaged drop/bounds/network acceptance and operator apply remain in `queue/backend-playable-runtime-release.md`; no protected remote action is authorized by this closure. No redundant cook/runtime checks are needed for this docs-only close. Shared scratch report: `Artifacts/Orchestration/20261025-resume/game-package-report.md` in the main checkout.
