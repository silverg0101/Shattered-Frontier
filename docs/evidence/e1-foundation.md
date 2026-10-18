# E1 foundation evidence — 2026-10-18

User authorized starting E1, initially targeting Unraid and subsequently selecting `steam-server`. Implemented `GameClient/ShatteredFrontier.uproject`, one shared runtime C++ module, Game/Editor/Client/Server targets, authoritative GameMode spawn/possession, replicated CharacterMovement, WASD/mouse/jump bindings, bounded release-only settings and a Greenlands graybox map. This starts E1; it does not complete the milestone. [Current source/Linux setup](../e1-linux-server-build.md) tracks the newly authorized build/deployment work.

## Executed checks

| Check | Observed result | Limits |
| --- | --- | --- |
| Editor Win64 Development build | Passed using UE 5.8.3 CL58210709, MSVC14.51.36260, Windows SDK10.0.26100.0 | Non-preferred compiler warning and engine-header deprecation warnings remain. Game/Client packages not built; later Linux Server compilation is recorded below. |
| Editor Python map creation | Exit0; `E1_MAP_CREATED /Game/Frontier/Maps/Greenlands_E1 Starts=2 Blocks=6`; commandlet reports 0 errors/warnings | NullRHI: no rendering/camera or manual traversal acceptance. Saved map is versioned fixture content. |
| Separate editor server plus two independent clients | `tools/Test-E1Connections.ps1` passed on loopback UDP17777; server NetMode1, two distinct PlayerControllers and `SFE1Character_0`/`SFE1Character_1` with Authority1; both client logs welcomed into Greenlands | Headless connection/spawn evidence only. No movement observation, invalid-input tests, lifecycle cleanup/rejoin acceptance, authentication or capacity claim. Test force-stops its own processes afterward. |
| Linux source build/cook | Source validation passed; Linux Server and host UnrealPak native compilation succeeded (1303 actions); LinuxServer map cook exited0 with zero errors/warnings | UAT staging failed: managed Defender ASR blocks source UnrealPak. That Windows attempt produced no archive; native Linux packaging/deployment subsequently passed. See the current build route. |

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

User supplied a second SSH destination on 2026-10-18; read-only key-authenticated inspection succeeded. Ubuntu22.04.5/kernel5.15, glibc2.35, Docker29.2.1, i7-6700 (4 cores/8 threads, AVX2), nominal32GB RAM with about28GiB available, and 1Gbps LAN. One existing game container used about2.46GiB RAM; a short sample showed91–92% CPU idle. These are snapshots, not load/soak results. [Epic's Linux requirements](https://dev.epicgames.com/documentation/unreal-engine/linux-development-requirements-for-unreal-engine) list Ubuntu22.04 and the relevant Linux/glibc baseline; initial package/container startup now passed; remote compatible-client gameplay and workload acceptance remain pending.

Initial inspection found about35GiB free on root,374GiB unallocated in LVM and an unmounted457GiB ext4 partition on the second NVMe. The user then selected this host and authorized setup/deployment and any storage allocation/mounting. Provisioning created a fresh200GiB LVM filesystem at `/srv/shattered-frontier` and non-root `sfserver`; repeat validation passed. About174GiB remains unallocated, and the second SSD/root LV were preserved. This host offers more observed memory headroom and less observed contention than Unraid, with an older/fewer-core CPU; capacity remains unmeasured. Existing game owns UDP7777, so the new runtime defaults to17777. Source/cooker and Docker/profile preparation are tracked in the build route; the initial non-root read-only game service is now healthy on LAN17777; compatible remote clients remain pending as below.

Next work is physical-input and camera/feedback verification when the desktop is unlocked, remaining ownership checks and broader baseline measurement (E1-F). Local lifecycle, normal headless replication/probe, bounded adverse-network and offscreen capture evidence are recorded above. Full vertical movement semantics, modifiers, anti-cheat, remote identity and durable actions remain pending. DEC-01 packaging evidence and DOC-07 readiness stay open.

Final harness review corrected two possible false passes: stale markers after an owned-process exit, and header-only PNGs. It now checks process liveness before marker acceptance/final PASS and fully decodes1280x720 PNGs. Focused regressions rejected an exited process and truncated PNG. Combined `-NetworkProfile Adverse100 -CaptureFrames` rerun `E1-Network-20261018-063420-445` passed: PlayerId256 moved1339.4cm on all three processes, jump117.0/117.9cm server/observer, accepted owner feedback, actual three-driver packet settings and six decoded frames. Existing manual/identity/capacity limits still apply.

## Packaged Linux runtime update

Native Linux UAT stage/archive passed and image `shattered-frontier:e1-2a5b634429b75d46519ad2a28560b8953ed99831` is healthy on steam-server192.168.50.133:17777/udp, sfserver998:998, read-only root,4CPU/8GiB limits. `E1_WORLD_READY NetMode=1` confirms the dedicated map; controlled stop/start produced orderly shutdown (SIGTERM exit143, no OOM) and returned healthy. Windrose remains healthy. A short idle89.67MiB/2.25% CPU observation is not E1-F/capacity evidence. Full provenance/logs are in the [Linux build record](../e1-linux-server-build.md).

Launcher remote-client check reached the host but was correctly rejected before possession: server effective NetCL0/checksum1737972550 differs from Launcher55116800/checksum1533752452. Both use protocol44/game0. Independent source review traced this to the normal unstamped source build's effective changelist rule; asset-compatible metadata was insufficient. Matching-source WindowsClient compiled and ordinary loose stage/archive passed in the isolated2a5b634 project; its first launch was blocked by managed Defender ASR before a process spawned. See the build record for exact receipt/hash/event. Native Linux diagnostic Clients are the next independent route. Keep network checks and source identity intact. No successful packaged remote-client gameplay is claimed.

Remote-helper hardening passed AST/synthetic review and an actual negative check: the incompatible Launcher client failed after9.9s instead of the full120s timeout, and its owned process exited. A final scan rejects either client reporting NetworkFailure after welcome; no compatible-client success is inferred from this negative check.

## E1-F collection tooling - validated, baseline pending

Optional `-CaptureProfile` now records10s after each process sees two characters, gates Driver input until its capture begins, and requires asynchronous CSV flush plus complete header/metadata/finite rows/duration coverage. Per-role directories prevent collisions. Independent review corrected malformed-row dropping; synthetic truncated/bad/nonfinite/zero/insufficient-duration cases now fail. The editor build passed25.84s. No gameplay rules or live controls changed.

Two actual collection checks passed while the separate source Client compiler was active (up to3 parallel actions). These are Windows Editor-server/two-client diagnostics, not the Linux package or a quiet performance baseline. Hardware: Core Ultra7 255H,32GiB nominal RAM, NVIDIA RTX PRO1000 Blackwell laptop adapter, D3D12 clients1280x720 offscreen, NullRHI server, Greenlands graybox. Every role had2 characters; world actor snapshots were27-29 (different replication audiences).

| Run | Observed frame intervals, milliseconds | Boundary |
| --- | --- | --- |
| Normal + profiles + six decoded screenshots, `E1-Network-20261018-070306-151` | Server mean33.411/p95 34.208; Driver p95 17.822/max609.225; Observer p95 17.972/max644.194 | Three completed CSVs cover about10s. Compilation and synchronous screenshot writing contaminate frame timing. |
| Adverse100 + profiles + rendering without screenshots, `E1-Network-20261018-070655-726` | Server301 samples, mean33.430/p95 34.169/max34.701; Driver633 samples, mean15.841/p95 19.119/max27.112; Observer638 samples, mean15.720/p95 18.133/max20.966 | About10s each; actual packet settings rechecked. Driver/server displacement1331.1cm; Observer sampled1439.2cm; jump119.7/120.4cm server/observer. This is sampled trajectory evidence, not perfect prediction/smoothing acceptance. |

`FrameTime` is wall-clock interval including pacing/waits. Dedicated servers omit total `GameThreadTime`; `Replication/ServerReplicateActorTimeMS` covers replication only. CSV rate counters are normalized networking-interval snapshots, not bytes to sum per frame. Raw CSVs include machine metadata and remain ignored. These1280x720/compiler-contended samples cannot ratify the MVP1080p client budget, server simulation CPU budget, network envelope, real RTT, Linux load or100-player capacity. Next: quiet repeated rendering/profile runs without PNG writing, then properly scoped workload measurements on a compatible packaged client/server pair.

## Pawn-cache validation and compiler-free1080p diagnostics

The opt-in `-ProbePawnCache` check passed Normal (`E1-Network-20261018-072417-631`) and Adverse100 (`E1-Network-20261018-072621-361`). After normal owning-input retreat and one second stationary, Observer temporarily substitutes its local pawn reference with the peer and requests the existing probe, restoring its reference synchronously before ticking. The server still derives the original Observer pawn: OutOfRange result5 in both authoritative and owning-client logs. Real Observer distance466.361/650.697cm exceeded300cm; peer distance180.899/189.968cm was inside. Both original possession pairs stayed unchanged; four samples before and after each client/server request showed zero horizontal/vertical displacement. Driver acceptance remained successful. This establishes local pawn-cache substitution is ignored; no pawn field exists in the RPC. Arbitrary forged RPCs, non-owned controllers and authenticated identity remain unproven. Focused Editor rebuild passed15.83s; independent code review found no remaining findings.

Two subsequent `-CaptureProfile -RenderOffscreen -Resolution1080p` checks passed after compilation/UAT finished, with no screenshot writing. D3D12 clients used the RTX PRO1000 Blackwell laptop adapter and1920x1080 explicit launch dimensions (no decoded frame); logged quality sections were3. Both clients share the laptop/GPU with the Editor server; other background contention was uncontrolled.

| Run | Server frame interval ms | Driver frame interval ms | Observer frame interval ms |
| --- | --- | --- | --- |
| Normal `E1-Network-20261018-073417-635` | mean33.491/p9534.190/max34.872;300 samples | mean8.236/p9510.446/max14.754;1216 samples | mean8.226/p9510.081/max17.809;1218 samples |
| Adverse100 `E1-Network-20261018-073543-441` | mean33.455/p9534.204/max34.548;300 samples | mean8.411/p9510.299/max12.809;1190 samples | mean8.319/p959.832/max14.170;1204 samples |

Each CSV covered about10s with valid flush/duration coverage, two characters and27-29 world actors. Both movement/jump/probe checks passed; Adverse100 readbacks matched on all three NetDrivers. These are short graybox diagnostics, not budget ratification: frame intervals include pacing, server simulation CPU is not measured, actual RTT is unknown, and native Linux workload/100-player capacity remain open. Raw machine-metadata CSVs remain ignored.
