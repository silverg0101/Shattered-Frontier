# Matching LAN game packages

- **Area/owner:** tools/backend integration / agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal/state
Produce fresh matching LinuxServer + Win64 Development Client archives/content for the approved LAN release; package only, no remote apply/deployment. Production game source/profile/proto baseline is `76d9fa3e258294fe939ba323f87b4a40a86c8c3b`; full cook waits for root final freeze including reviewed rim content and explicit Unreal slot grant. Provisional Linux source compilation passed; no cook/stage/archive has run.

## Read-only evidence
`Build-LinuxServer.ps1 -ValidateOnly` PASS with source engine `C:\UnrealSource\UE_5.8.3` pinned `396c9f059903aed5fec78ecd3d437a40c6415368`, v26 SDK `C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8`, matching Launcher cooker. Source Build.version CL0; Launcher CL58210709/compatibleCL55116800. Source Editor absent; external cooker/module prerequisite passes. All120 Linux and121 Win64 gRPC archives match manifest hashes (commit3252a89f10d8e92997862167ca7d095ecda85973). Disk free186GiB;20 physical cores, select MaxParallelActions8 (below half10). No installation/repair needed; old MinimalInventoryRuntime cook blocker is superseded.

## Phase1 compiled evidence
Actual revision `cb6a53a7934d8ce070de5b5781ece90c2096547c` (docs checkpoint atop ada40e17). Source `Build.bat ShatteredFrontierServer Linux Development <lane-project> -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=8` PASS:1006 actions/1066.96s, existing deprecation warnings. Receipt confirms Linux/x64 Development Server and sourceCL0; actual network/runtime acceptance remains pending. ELF64/little-endian/x86-64 machine62,297843368bytes, SHA256 `19bfd1c89ef737d21078478d8b4ce4f2167d1c5812d8740f32f0675e9ebaf7a6`. Own evidence: `Artifacts/PackagedServer/Matching-cb6a53a7-phase1-20261025-170707` (build log, source-inputs.json, phase1-linux-result.json and raw executable/receipt copy, not a package). All2453 tracked runtime source/config/content/proto hashes unchanged after build; manifestSHA256 `1f9bf48498b537446e4ce9a29be4a6beb8beee949538cbeb8af1c14edabb6449`.174.9GiB free afterward.
Root rescheduled WinClient until the approved highlight gate completes; it has NOT started. UE slot explicitly released after Linux result, no owned build process. Final highlight CPP/assets require new freeze manifest + incremental Linux rebuild; provisional A output is not final B.

## Route/next
After final root freeze/grant: capture full revision + tracked source/content/proto/asset hash manifest, build matching editor module for cooker, then source-engine UAT BuildCookRun for `ShatteredFrontierServer Linux Development` and `ShatteredFrontierClient Win64 Development`. Both runtime targets must use source NetCL0; no network-version override. Launcher is cooker only. Preserve/reuse existing source/toolchain caches without copying old binaries into the package.
The Linux helper hardcodes `Artifacts/Unreal/LinuxServer`; use its validated UAT route with explicit new stage/archive destinations below this lane's ignored `Artifacts/PackagedServer/Matching-<finalrevision>-<timestamp>/{LinuxServer,Win64Client}`. Never overwrite older releases/shared main outputs. Keep exact current JSON, item WorldMesh, rim and class-redirect assets; no geometry/material changes. Validate full content/asset closure, hashes, ELF header/architecture and actual matching network versions; report archives only after these pass. Coordinate immutable artifact identity with runtime_release's same-revision services snapshot and guarded Silverbrook profile. Operator apply remains root-owned.

## Blocked on
New explicit WinClient grant after highlight gate; root final freeze and cook/archive grant. Loot worker may use the released slot for the approved highlight check. Runtime_release bootstrap/profile review is separate. No protected SSH/apply or deployment authorization inferred here.

## Outcome
Pending: Windows source compile and final frozen rebuild/cook/stage/archive/ELF/content/network checks. Only provisional Linux compilation/link succeeded; no package success claimed.
