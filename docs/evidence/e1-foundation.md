# E1 foundation evidence — 2026-10-18

User authorized starting E1, initially targeting Unraid and subsequently selecting `steam-server`. Implemented `GameClient/ShatteredFrontier.uproject`, one shared runtime C++ module, Game/Editor/Client/Server targets, authoritative GameMode spawn/possession, replicated CharacterMovement, WASD/mouse/jump bindings, bounded release-only settings and a Greenlands graybox map. This starts E1; it does not complete the milestone. [Current source/Linux setup](../e1-linux-server-build.md) tracks the newly authorized build/deployment work.

## Executed checks

| Check | Observed result | Limits |
| --- | --- | --- |
| Editor Win64 Development build | Passed using UE 5.8.3 CL58210709, MSVC14.51.36260, Windows SDK10.0.26100.0 | Non-preferred compiler warning and engine-header deprecation warnings remain. Game/Client/Server packaged targets not built. |
| Editor Python map creation | Exit0; `E1_MAP_CREATED /Game/Frontier/Maps/Greenlands_E1 Starts=2 Blocks=6`; commandlet reports 0 errors/warnings | NullRHI: no rendering/camera or manual traversal acceptance. Saved map is versioned fixture content. |
| Separate editor server plus two independent clients | `tools/Test-E1Connections.ps1` passed on loopback UDP17777; server NetMode1, two distinct PlayerControllers and `SFE1Character_0`/`SFE1Character_1` with Authority1; both client logs welcomed into Greenlands | Headless connection/spawn evidence only. No movement observation, invalid-input tests, lifecycle cleanup/rejoin acceptance, authentication or capacity claim. Test force-stops its own processes afterward. |
| Linux build entry point | PowerShell parser clean; `-ValidateOnly` against installed engine failed as expected with source-checkout/SDK prerequisites | No UAT build/cook, package, container or deployment ran. |

Exact editor build command is in [GameClient README](../../GameClient/README.md). Map command:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\GameClient\ShatteredFrontier.uproject" -run=pythonscript "-script=$PWD\tools\Create-E1Map.py" -unattended -nop4 -NullRHI "-abslog=$PWD\GameClient\Saved\Logs\E1-Map.log"
.\tools\Test-E1Connections.ps1
.\tools\Build-LinuxServer.ps1 -SourceEngineRoot 'C:\Program Files\Epic Games\UE_5.8' -ValidateOnly
```

The last command intentionally demonstrates rejection, not a successful build route. Map generator refuses overwrite. Saved map SHA256: `98d29084a300c89cc78066696ad5ab1f88117ec6c2854b12b1f7606766e12c55`. Local full logs remain ignored under `GameClient/Saved/Logs`; build logs are in `%LOCALAPPDATA%/UnrealBuildTool/Log.txt`. Initial smoke: `E1-Smoke-20261018-032438` (server PID23844, clients14976/35120). Final rebuild and smoke with typed gravity/step settings and unique controller/pawn assertions passed: `E1-Smoke-20261018-032724` (server PID25732, clients41292/13620). All test processes were stopped. No raw account/host-bearing logs committed.

Independent review corrected the initial jump mismatch to canonical height/gravity, removed editor-generated Android file-server settings and disabled that unrelated plugin. One intermediate rebuild failed because UHT rejected acceleration unit metadata; the field now declares its units in a tooltip and the settings table. These are implementation corrections, not accepted deviations from design.

## Hosting route and next work

Unraid is the selected Linux x86_64 Docker test host. Read-only SSH inspection found an i7-10700 (8 cores/16 threads), nominal32GB RAM, about11GiB available at inspection, Docker27.5.1 and NVMe storage. Shared applications were active; no performance reservation or player-capacity proof exists. The storage pool has no disk redundancy; persistent-world deployment requires independently recoverable backups. No remote workload or network settings were changed.

The Launcher distribution has no installed Server-target support. Epic source is pinned to `396c9f059903aed5fec78ecd3d437a40c6415368`. Source checkout and `v26_clang-20.1.8-rockylinux8` installation have now completed; dependency sync/build/cook and runtime acceptance remain in progress under [the source/Linux packaging route](../e1-linux-server-build.md). No full source editor build is claimed; the selected route uses the matching existing editor as cooker.

### Selected runtime host: steam-server

User supplied a second SSH destination on 2026-10-18; read-only key-authenticated inspection succeeded. Ubuntu22.04.5/kernel5.15, glibc2.35, Docker29.2.1, i7-6700 (4 cores/8 threads, AVX2), nominal32GB RAM with about28GiB available, and 1Gbps LAN. One existing game container used about2.46GiB RAM; a short sample showed91–92% CPU idle. These are snapshots, not load/soak results. [Epic's Linux requirements](https://dev.epicgames.com/documentation/unreal-engine/linux-development-requirements-for-unreal-engine) list Ubuntu22.04 and the relevant Linux/glibc baseline; actual package/container compatibility remains untested.

Initial inspection found about35GiB free on root,374GiB unallocated in LVM and an unmounted457GiB ext4 partition on the second NVMe. The user then selected this host and authorized setup/deployment and any storage allocation/mounting. Provisioning created a fresh200GiB LVM filesystem at `/srv/shattered-frontier` and non-root `sfserver`; repeat validation passed. About174GiB remains unallocated, and the second SSD/root LV were preserved. This host offers more observed memory headroom and less observed contention than Unraid, with an older/fewer-core CPU; capacity remains unmeasured. Existing game owns UDP7777, so the new runtime defaults to17777. Source/cooker and Docker/profile preparation are tracked in the build route; no new game service has started yet.

Next local work is rendered movement/camera/collision verification (E1-B), observed replication and ownership checks (remaining E1-C), join/leave/rejoin cleanup (E1-D), server-validated non-economic interaction (E1-E), then baseline measurements (E1-F). Full vertical movement semantics, modifiers, anti-cheat, remote identity and durable actions remain pending. DEC-01 packaging evidence and DOC-07 readiness stay open.
