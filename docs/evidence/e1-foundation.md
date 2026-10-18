# E1 foundation evidence — 2026-10-18

User authorized starting E1, initially targeting Unraid and subsequently selecting `steam-server`. Implemented `GameClient/ShatteredFrontier.uproject`, one shared runtime C++ module, Game/Editor/Client/Server targets, authoritative GameMode spawn/possession, replicated CharacterMovement, WASD/mouse/jump bindings, bounded release-only settings and a Greenlands graybox map. This starts E1; it does not complete the milestone. [Current source/Linux setup](../e1-linux-server-build.md) tracks the newly authorized build/deployment work.

## Executed checks

| Check | Observed result | Limits |
| --- | --- | --- |
| Editor Win64 Development build | Passed using UE 5.8.3 CL58210709, MSVC14.51.36260, Windows SDK10.0.26100.0 | Non-preferred compiler warning and engine-header deprecation warnings remain. Game/Client packages not built; later Linux Server compilation is recorded below. |
| Editor Python map creation | Exit0; `E1_MAP_CREATED /Game/Frontier/Maps/Greenlands_E1 Starts=2 Blocks=6`; commandlet reports 0 errors/warnings | NullRHI: no rendering/camera or manual traversal acceptance. Saved map is versioned fixture content. |
| Separate editor server plus two independent clients | `tools/Test-E1Connections.ps1` passed on loopback UDP17777; server NetMode1, two distinct PlayerControllers and `SFE1Character_0`/`SFE1Character_1` with Authority1; both client logs welcomed into Greenlands | Headless connection/spawn evidence only. No movement observation, invalid-input tests, lifecycle cleanup/rejoin acceptance, authentication or capacity claim. Test force-stops its own processes afterward. |
| Linux source build/cook | Source validation passed; Linux Server and host UnrealPak native compilation succeeded (1303 actions); LinuxServer map cook exited0 with zero errors/warnings | UAT staging failed: managed Defender ASR blocks source UnrealPak. No runnable archive, game image or deployment. See build route for logs and resume steps. |

Exact editor build command is in [GameClient README](../../GameClient/README.md). Map command:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\GameClient\ShatteredFrontier.uproject" -run=pythonscript "-script=$PWD\tools\Create-E1Map.py" -unattended -nop4 -NullRHI "-abslog=$PWD\GameClient\Saved\Logs\E1-Map.log"
.\tools\Test-E1Connections.ps1
.\tools\Build-LinuxServer.ps1 -SourceEngineRoot 'C:\Program Files\Epic Games\UE_5.8' -ValidateOnly
```

The last command intentionally demonstrates rejection, not a successful build route. Map generator refuses overwrite. Saved map SHA256: `98d29084a300c89cc78066696ad5ab1f88117ec6c2854b12b1f7606766e12c55`. Local full logs remain ignored under `GameClient/Saved/Logs`; build logs are in `%LOCALAPPDATA%/UnrealBuildTool/Log.txt`. Initial smoke: `E1-Smoke-20261018-032438` (server PID23844, clients14976/35120). Final rebuild and smoke with typed gravity/step settings and unique controller/pawn assertions passed: `E1-Smoke-20261018-032724` (server PID25732, clients41292/13620). All test processes were stopped. No raw account/host-bearing logs committed.

Independent review corrected the initial jump mismatch to canonical height/gravity, removed editor-generated Android file-server settings and disabled that unrelated plugin. One intermediate rebuild failed because UHT rejected acceleration unit metadata; the field now declares its units in a tooltip and the settings table. These are implementation corrections, not accepted deviations from design.

## Hosting route and next work

### Overnight implementation and checks

The user authorized autonomous ready work while away. Added the owning interaction controller/replicated non-economic target, released startup settings, local feedback, lifecycle audits and focused movement/network checks. Scope remains the E1 fixture. Independent stronger review found a cooldown feedback inconsistency; it was corrected and rechecked without remaining code findings. A separate bounded Luna check verified the optional art prototype's hashes and Blender exchange files.

| Check | Actual result | Boundary |
| --- | --- | --- |
| Final editor build and scoped automation | Build succeeded; `ShatteredFrontier.E1.` found7 tests,7 Success,0 warnings/failures/not-run | Three interaction tests and four movement tests only; no Node suite. JSON report and logs remain ignored. |
| Movement/collision | 583.29cm translation in1s; peak700cm/s; capsule stops0.10cm before wall; jump apex120cm and lands;33.75cm step crossed,65cm blocked with45cm threshold | Isolated actual CharacterMovement/world collision, not network prediction or rendered traversal. Test fixture needed a normally local-designated controller; no production movement rule/tolerance was changed. |
| Camera collision | Spring arm retracts to118cm at wall, recovers350cm after removal | Headless collision query; visual framing/recovery still pending. |
| Lifecycle | `Test-E1Lifecycle.ps1` passed54.5s;6 joins/6 departures; authoritative player/controller/pawn/possession counts1,2,1,2,1,2,3,2,3,2,1,0; departing controller/pawn/possession flags all0 and surviving pairs preserved | Actual three-session overlaps, local anonymous connections only; no durable/account reconnect acceptance. Pawn correlation is saved at RestartPlayer because native cleanup clears it before Logout. |
| Normal owning-client movement/probe | Final `E1-Network-20261018-055225-294`: PlayerId257 moved1387.5cm | Separate real server/two clients, loopback/headless; server acceptance matched owning-client feedback. Correlation uses server-allocated replicated PlayerId, not account identity. |
| Adverse network scenario | `E1-Network-20261018-055313-448`: PlayerId256 moved1378.5/1379.1/1380cm on Driver/server/Observer; jump118/118.4cm on server/Observer. Configured50ms outgoing delay at each endpoint,10ms variance and1% loss; nominal100ms RTT was not separately measured. | Separate real server/two clients, loopback. This is one bounded adverse scenario, not a capacity or broad network-robustness result. |
| Offscreen capture scenario | `E1-Network-20261018-055718-948`: PlayerId256 moved1385.6/1385.6/1385.9cm on Driver/server/Observer; jump117.4/115.2cm on server/Observer; six valid PNG captures. | Offscreen capture passed after interactive inspection was blocked by the Windows lock screen. It provides no physical-input, camera-feel, or rendered-UI acceptance. |
| Final Linux Server build | Native setup completed with exit0; Linux Server source target compiled/linked in108.39s. Log: `Artifacts/Unreal/Setup/Native-Current-LinuxServer-Build.log`. | Native build completion does not establish staged packaging or runtime acceptance; see [source/Linux packaging route](../e1-linux-server-build.md). |

Final automation command:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\GameClient\ShatteredFrontier.uproject" -unattended -NullRHI -nosound -nop4 -NoSplash '-ExecCmds=Automation RunTests ShatteredFrontier.E1.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD\Artifacts\Unreal\Final-E1-Automation" "-abslog=$PWD\GameClient\Saved\Logs\E1-Final-Automation.log"
.\tools\Test-E1Lifecycle.ps1
.\tools\Test-E1NetworkScenario.ps1
```

Evidence: ignored `Artifacts/Unreal/Final-E1-Automation/index.json`, `GameClient/Saved/Logs/E1-Final-Automation.log`, `E1-Lifecycle-20261018-053317-356/Lifecycle-results.json`, `E1-Network-20261018-054501-064/`, the three final network scenario IDs above, and `Artifacts/Unreal/Setup/Native-Current-LinuxServer-Build.log`. All owned test processes exited. Unreal test process exit0 alone is insufficient; the final report's7 successes/zero failures were checked explicitly.

Probe settings are range3m (0.5–5m) and accepted cooldown0.5s (0.1–10s), reviewed release authors/next-startup only. Server tests cover acceptance without target mutation; strict range boundary; a real occluding wall; server-time cooldown; null/wrong/foreign/removed/disabled/stale targets; unpossessed/mismatched-owner pawn; invalid settings and long-cooldown/denial feedback. These are E1 fixture declarations, not new gathering rules or live controls.

The Windows lock screen blocked interactive rendered inspection only. The later offscreen CaptureFrames scenario passed with six valid PNGs, but no physical-input, camera-feel, or rendered-UI acceptance was established. Optional [concept-derived waystone](../../GameClient/ArtSource/GreenlandsWaystone/README.md) has editable Blender/FBX/GLB and an inspected preview, but is not imported/cooked or production approved. Headless tests and art QA do not close E1-B visual traversal, rendered E1-C/E, adversarial ownership, E1-F capacity/performance, DOC-07 or DEL-01 package gates.

Unraid was the initial Linux x86_64 Docker candidate; steam-server supersedes it. Read-only SSH inspection found an i7-10700 (8 cores/16 threads), nominal32GB RAM, about11GiB available at inspection, Docker27.5.1 and NVMe storage. Shared applications were active; no performance reservation or player-capacity proof exists. The storage pool has no disk redundancy; persistent-world deployment requires independently recoverable backups. No remote workload or network settings were changed.

The Launcher distribution has no installed Server-target support. Epic source is pinned to `396c9f059903aed5fec78ecd3d437a40c6415368`. Source checkout and `v26_clang-20.1.8-rockylinux8` installation have completed; dependency sync, native Linux Server compilation and cook passed. The latest native setup/build finished with exit0 and is logged at `Artifacts/Unreal/Setup/Native-Current-LinuxServer-Build.log`; packaging and runtime acceptance remain pending under [the source/Linux packaging route](../e1-linux-server-build.md). No full source editor build is claimed; the selected route uses the matching existing editor as cooker.

### Selected runtime host: steam-server

User supplied a second SSH destination on 2026-10-18; read-only key-authenticated inspection succeeded. Ubuntu22.04.5/kernel5.15, glibc2.35, Docker29.2.1, i7-6700 (4 cores/8 threads, AVX2), nominal32GB RAM with about28GiB available, and 1Gbps LAN. One existing game container used about2.46GiB RAM; a short sample showed91–92% CPU idle. These are snapshots, not load/soak results. [Epic's Linux requirements](https://dev.epicgames.com/documentation/unreal-engine/linux-development-requirements-for-unreal-engine) list Ubuntu22.04 and the relevant Linux/glibc baseline; actual package/container compatibility remains untested.

Initial inspection found about35GiB free on root,374GiB unallocated in LVM and an unmounted457GiB ext4 partition on the second NVMe. The user then selected this host and authorized setup/deployment and any storage allocation/mounting. Provisioning created a fresh200GiB LVM filesystem at `/srv/shattered-frontier` and non-root `sfserver`; repeat validation passed. About174GiB remains unallocated, and the second SSD/root LV were preserved. This host offers more observed memory headroom and less observed contention than Unraid, with an older/fewer-core CPU; capacity remains unmeasured. Existing game owns UDP7777, so the new runtime defaults to17777. Source/cooker and Docker/profile preparation are tracked in the build route; no new game service has started yet.

Next work is physical-input and camera/feedback verification when the desktop is unlocked, remaining ownership checks and broader baseline measurement (E1-F). Local lifecycle, normal headless replication/probe, bounded adverse-network and offscreen capture evidence are recorded above. Full vertical movement semantics, modifiers, anti-cheat, remote identity and durable actions remain pending. DEC-01 packaging evidence and DOC-07 readiness stay open.

Final harness review corrected two possible false passes: stale markers after an owned-process exit, and header-only PNGs. It now checks process liveness before marker acceptance/final PASS and fully decodes1280x720 PNGs. Focused regressions rejected an exited process and truncated PNG. Combined `-NetworkProfile Adverse100 -CaptureFrames` rerun `E1-Network-20261018-063420-445` passed: PlayerId256 moved1339.4cm on all three processes, jump117.0/117.9cm server/observer, accepted owner feedback, actual three-driver packet settings and six decoded frames. Existing manual/identity/capacity limits still apply.
