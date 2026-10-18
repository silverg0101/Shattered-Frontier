# E1 Unreal foundation

## Isolated construction geometry pilot

The next playable milestone is the [construction sandbox](../docs/construction-sandbox-milestone.md). Its current [SBX-03/04 checkpoint](../docs/evidence/construction-sandbox-adapter-pilot.md) is a standalone three-shape geometry/convex-collision fixture, with no building controls yet. The installed ProceduralMeshComponent runtime plugin is enabled; generated canonical tables are checked with `py -3 tools/Generate-ConstructionGeometryHeader.py --check` from the repository root.

The dedicated map is `/Game/Frontier/Maps/Greenlands_ConstructionSandbox`; launch it explicitly with the matching editor and project, `-game -windowed`. It shows full/half/slope previews and requires its sandbox GameMode. It does not replace E1 defaults, join the LAN service, grant items or certify packaged/network/rendered acceptance. The new generator `tools/Create-ConstructionSandboxMap.py` refuses an existing map. Focused editor automation prefix: `ShatteredFrontier.Construction.Geometry.`; see checkpoint evidence for the exact command, successful cases and limits.

Open `ShatteredFrontier.uproject` with UE 5.8.3. The shared C++ module supplies Game, Editor, Client and Server targets. The initial Greenlands map is a graybox movement fixture, not a finished biome. WASD moves, mouse looks, Space jumps, E probes the nearest test target. The cube avatar is a placeholder.

Build the editor from the repository root:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' ShatteredFrontierEditor Win64 Development "-Project=$PWD\GameClient\ShatteredFrontier.uproject" -WaitMutex -NoHotReloadFromIDE
```

Map source generation is `tools/Create-E1Map.py`, invoked through the editor's Python commandlet. It refuses to overwrite an existing map. Edit the saved map in the editor thereafter. Python and Editor Scripting Utilities are editor-only dependencies.

From the repository root, `./tools/Test-E1Connections.ps1` runs the bounded headless loopback server/two-client connection check, writes ignored logs and stops its own processes. It does not verify rendered movement or lifecycle cleanup. See [recorded evidence and remaining checks](../docs/evidence/e1-foundation.md).

After the Linux server is running, `./tools/Test-E1Remote.ps1 -ServerAddress steam-server -Port 17777` checks two independent client welcomes for the expected E1 map/mode and stops its own clients. Inspect server logs separately for distinct authoritative possessions; no rendered movement or account-identity acceptance is implied.

`./tools/Test-E1Lifecycle.ps1` checks individual disconnect/rejoin and overlapping anonymous sessions on loopback UDP17779. Deferred server snapshots retain the departing pawn's weak reference and verify cleanup plus surviving possessions. `./tools/Test-E1NetworkScenario.ps1` checks normal owning-client movement, server/observer replication and an accepted probe on UDP17781. Its explicitly enabled development-only scenario runs for30 seconds; cross-process correlation uses replicated PlayerId because actor names differ between clients. This identifier is not account authentication.

The network helper accepts `-NetworkProfile Normal` (default) or `Adverse100` (50ms outgoing lag,±10ms variance and1% configured loss on each process). It reads back actual NetDriver settings; nominal100ms RTT is not a measured RTT. `-CaptureFrames` replaces client NullRHI with Unreal offscreen rendering and requires six real PNGs in the ignored run directory. Inspect those frames separately; they do not certify physical keyboard input, full UI or interactive camera feel. Each run writes `Summary.json` with identities, configured/actual emulation, movement and sampled jump/landing evidence.

`SFE1GameMode` owns spawn/possession through Unreal's standard authoritative GameMode. `SFE1Character` uses CharacterMovement replication and collision. A replicated probe is spawned at fixture coordinates(-300,-750,60)cm. The owning `SFE1PlayerController` sends only target/revision intent; the server derives its current pawn and checks ownership, liveness, world, revision, enabled state, finite positions, range, mandatory eye-to-anchor Visibility trace and server-clock cooldown. The owner's pending/result/cooldown feedback is advisory. Acceptance changes only the controller's transient cooldown. There is no account authentication, persistence, production backend or economic action. Keep the server on a trusted development network.

The fixture deliberately uses legacy input bindings. EnhancedInput is disabled in the project; the editor auto-upgrade is also disabled in shipped editor configuration to preserve those bindings. The optional [waystone art source](ArtSource/GreenlandsWaystone/README.md) is not imported into the runtime map.

## Release-controlled fixture settings

The typed `SFE1Settings` object reads the shipped `Config/DefaultGame.ini` section `/Script/ShatteredFrontier.SFE1Settings`. Release authors may change these values within the declared bounds; they take effect at process startup. Missing entries use shipped C++ defaults; non-finite or out-of-range values fail startup. These are prospective fixture settings, not live GM controls. Any future live controls must use the shared configuration workflow and its permissions/audit rules. Algorithms, replication authority and map topology remain versioned code/content.

| Plain-language setting | Property | Default | Bounds | Units |
| --- | --- | --- | --- | --- |
| Run speed | RunSpeedMetresPerSecond | 7 | 0.5–12 | m/s |
| Jump height | JumpHeightMetres | 1.2 | 0.1–3 | m |
| Gravity magnitude | GravityMetresPerSecondSquared | 9.81 | 1–20 | m/s² |
| Step height | StepHeightMetres | 0.45 | 0–1 | m |
| Character turn rate | TurnRateDegreesPerSecond | 540 | 90–1080 | degrees/s |
| Camera distance | CameraBoomLengthCm | 350 | 100–1000 | cm |
| Mouse look sensitivity | LookSensitivity | 1 | 0.1–4 | multiplier |
| E1 probe range | InteractionRangeMetres | 3 | 0.5–5 | m |
| Time between accepted probes | InteractionAcceptedCooldownSeconds | 0.5 | 0.1–10 | s |

Run speed, jump height, gravity and step height follow the canonical unmodified baseline. Launch speed derives from height/gravity; the character scales world gravity to the declared magnitude on startup. Air control is disabled. Other defaults are E1 fixture choices, not acceptance of the full movement design (including terminal speed and modifiers). Fixture geometry is described in the generator in centimetres.

The two interaction values apply only to the non-economic E1 probe, not harvesting/loot/combat. Range measures pawn origin to fixed target anchor; there is no hidden harvesting tolerance. The server snapshots these rules once per process. Only accepted probes consume gameplay cooldown; local input coalescing is separate and cannot authorize an action.

Focused editor automation names are `ShatteredFrontier.E1.Interaction.*` and `ShatteredFrontier.E1.Movement.*`; their source is excluded from non-editor builds. They exercise isolated authority physics/rules, not rendered acceptance. See the evidence document for the executed command and results.

The dedicated destination is Linux x86_64 Docker on `steam-server`, selected by the user after initial Unraid assessment. See [the Linux build route](../docs/e1-linux-server-build.md). Source engine/SDK setup, native Linux staging and initial non-root Docker deployment passed. The LAN server runs on192.168.50.133:17777. Launcher clients correctly reject its differing source network changelist; the matching-source WindowsClient compiled/staged but its first execution was denied by managed Defender ASR. Native Linux diagnostic Clients now passed the isolated packaged two-client movement/jump/probe fixture. Remote gameplay acceptance and capacity remain open.

For a verified standalone source-client archive, `tools/Test-E1Remote.ps1 -ServerAddress 192.168.50.133 -ClientExecutable <direct-staged-ShatteredFrontierClient.exe>` uses the code-project prefix and checks both clients for explicit network failures before success. Use the nested executable, not a bootstrap launcher. The helper does not prove possession; correlate authoritative server logs separately. See the build route for successful staging and the current execution-approval blocker.

Optional collection check: `tools/Test-E1NetworkScenario.ps1 -CaptureProfile -RenderOffscreen` records unique per-role CSV files after two characters exist, stops after10s and verifies asynchronous flush/header/duration/row coverage. `-NetworkProfile Adverse100` adds the fixed documented packet conditions. `-RenderOffscreen` uses1280x720 rendering without screenshot stalls; combine `-CaptureFrames` only for visual checks. Current captures taken during compilation validate collection; frame intervals include waits, dedicated-server CSV omits total GameThreadTime, and no provisional budget or capacity is accepted.

Remote connection observation accepts `-HoldSeconds` (integer seconds, default3, bounds3-120), checked throughout for owned-process exits and reported network failures. Developer/operator only; takes effect per test invocation. A60s hold can accompany server resource observation; it does not prove tick progress, capacity or authenticated identity. Invalid bounds are rejected before launch.

Network helper options: use `-Resolution 720p` (default1280x720) or `-Resolution 1080p` (1920x1080). Decoded screenshot dimensions are checked when `-CaptureFrames` is used; rendering without captures records launch dimensions only. `-ProbePawnCache` opts into a finite local-reference substitution check with normal retreat/settling and authoritative denial/stationarity checks. It does not certify arbitrary RPC or account-identity security. Normal and Adverse100 checks and compiler-free1080p profiles passed; see foundation evidence for scoped results.

Native Linux diagnostic Clients and the source Server now passed an isolated packaged two-client Normal run (game2a5b634), including matching checksum, distinct possessions, replicated motion/jump and accepted probe. All three temporary containers stopped cleanly; the LAN service remains healthy. This NullRHI run does not establish Windows execution, LAN-client/rendered/manual acceptance, authentication or capacity. See the foundation/build evidence for full provenance and limits.

Windows connection update: the user's folder exclusion resolved source Client execution. Two packaged Windows clients now pass LAN welcome checks against steam-server17777, with distinct authoritative possessions confirmed separately. Use the same staged Client path; no PowerShell/PickerHost process exclusion was needed. Rendered/manual and workload acceptance remain open; see the latest foundation evidence.

The user confirmed basic rendered WASD movement, mouse look, Space jump and E interaction worked against steam-server. Detailed visual edge cases and performance/capacity limits remain in the foundation evidence.
