# SBX-07A: isolated Windows standalone package route

Status: implementation in progress, 2026-10-18; independent packet and runner review completed; launch-isolation and Stage-provenance corrections incorporated. First package compile failure is recorded in [checkpoint evidence](../evidence/construction-sandbox-package-pilot.md); no successful package/runtime acceptance is claimed. This packet defines the bounded route before SBX-07 art and multiplayer acceptance.

## Target and boundary

Build the existing `ShatteredFrontier` **Game** target, Win64 Development, using installed UE 5.8.3. This is the player-facing standalone package. Do not select `ShatteredFrontierClient`: `SFConstructionSandboxState`, Controller and GameMode require local authority and `NM_Standalone`. A Client-only binary cannot supply that authority. A future matching Client/Server pair requires a separately reviewed networking implementation; do not remove these guards to make a network smoke test pass.

Preserve repository DefaultEngine.ini defaults, DefaultGame.ini MapsToCook, normal E1 behavior, the live LAN service, copied previews and all existing saves. Use no deployment, listen-server, remote-server, security-policy changes or external writes. Approved art integration and two-client acceptance remain separate milestone gates.

## Implementation packet

Parent owns `tools/Build-ConstructionSandbox.ps1`, this packet, GameClient README, dependency map, work plan and evidence. No runtime source or original map edits are required. Independent review must inspect the runner before its first build, then inspect the resulting package/provenance and acceptance evidence. Serialize local builds and use one local writer. Local inspection found no Unreal/UAT process and no local automation definitions; remote desktop continuation state is unverified. The user subsequently authorized continued work: proceed with unique local copied-project runs and no remote mutation; coordinate remote writer ownership before any shared remote implementation/build.

The runner must create a new, nonexisting run root under ignored `Artifacts/ConstructionSandbox/Packages/`. Reject traversal, reparse-point ancestors and destinations outside that root. Never delete or reuse earlier runs. Materialize the committed GameClient project inputs (`.uproject`, Source, Config, Content and any required tracked project Plugins) into `Project/GameClient/`, preserving paths and binary bytes. Enumerate tracked inputs explicitly and record their hashes and source revision; refuse dirty tracked project inputs rather than silently package different bytes. Do not sweep in untracked art, Saved, Binaries, Intermediate or another preview. Record engine Build.version, installed-build identity, project/target/plugin inputs and tool invocation. Refuse missing inputs, wrong engine version or an existing destination before launching UAT.

Use only the copied project's configuration to set GameDefaultMap and ServerDefaultMap to `/Game/Frontier/Maps/Greenlands_ConstructionSandbox`, and replace its MapsToCook array with that sandbox map plus `/Game/Frontier/Maps/Greenlands_E1` for the normal-mode negative check. Keep E1 as an explicitly documented test dependency, not a change to the repository cook list. Hash both original inputs and derived copied configuration; record the exact transformation. Required referenced engine assets may also cook; two requested maps is not a claim that only two packages ship.

Candidate UAT argument vector, with absolute validated paths (no shell string concatenation):

```text
RunUAT.bat BuildCookRun
  -project=<run>/Project/GameClient/ShatteredFrontier.uproject
  -target=ShatteredFrontier -platform=Win64 -clientconfig=Development
  -noP4 -unattended -utf8output -build -cook -stage -pak -archive
  -MapsToCook=/Game/Frontier/Maps/Greenlands_ConstructionSandbox+/Game/Frontier/Maps/Greenlands_E1
  -stagingdirectory=<run>/Stage -archivedirectory=<run>/Archive
```

The installed `AutomationUtils/ProjectParams.cs` owns target/map interpretation; validate this argument vector against that installation and the actual UAT log before accepting it. `clientconfig` is UAT's configuration parameter for the Game executable; it does not select TargetType.Client. Do not supply `-client`, `-server`, `-noclient`, `-skipcook`, `-iterate`, `-run` or `-deploy`. Build/cook fresh in the copied project. Retain exit codes/logs and fail on missing target receipts, requested cooked maps or executable. Determine archive layout from actual manifests; never assume a bootstrap executable is the game binary. Record target receipt, engine compatibility identity, executable and staged-file hashes. Launcher packaging support and module availability are build-time gates, not prior evidence of success. If managed execution protection blocks a tool or game, retain the diagnostic and stop that route without weakening policy.

## Acceptance and evidence

Installed `Misc/Paths.cpp` supports an absolute `-UserDir=` override through ProjectUserDir and ProjectSavedDir; SaveStore derives its root from ProjectSavedDir. Every acceptance launch, including the initial rendered and E1 negative launches, must pass a validated fresh run-owned user directory. Reuse only the sandbox directory for its deliberate save/restart pair; give E1 a separate directory. Verify actual log/config/save paths before input dispatch, preserving existing per-user configuration and generations.

Implement a preflight/dry-run mode that validates identities, paths and input selection without building or starting a game. Validate runner syntax and use bounded negative checks for an existing destination, wrong/missing engine and dirty source. No Node suite or unrelated engine tests. After review and writer coordination, perform one new isolated build; rerun only for a concrete corrected failure.

1. Verify source hashes/configuration unchanged after packaging; record both cooked maps, actual Game target/Development receipt, successful cook/stage/archive, module/plugin inclusion and package manifest. A successful editor build is insufficient.
2. Launch the direct packaged executable without an editor/project argument and without a map override. Verify sandbox default map/mode, standalone authority, runtime procedural collision, cursor/palette rendering, held-RMB look, movement and visible development-stock label. Retain log and rendered observations separately.
3. Exercise cube/slab/slope rotation and placement, atomic line ghost/commit/cancel, occupied/unsupported/out-of-range rejection, explicit demolition and support-loss confirmation/grace. Check doorway center interaction, traversal and safe closing, including both-sides-blocked rejection. Record exact observed actions; earlier editor test passes do not establish packaged behavior.
4. Verify the packaged save location first. Using the run-owned sandbox user directory, F5 save an identified structure/open door; close only the owned process, restart the same archive with the same explicit UserDir and F9 compare member content and door state. Record content and runtime revisions separately: import may publish a fresh revision. Verify malformed/incompatible generation rejection through a reviewed bounded fixture, without changing ordinary saves. If user-data isolation cannot be proven, stop this check before writing a save. Installed UEBuildTarget enables WITH_DEV_AUTOMATION_TESTS in Development, but verify actual target definitions, fixture admission and packaged compatibility before reusing any editor-copy save harness or development automation flag.
5. Launch the same archive explicitly on `/Game/Frontier/Maps/Greenlands_E1`. Verify ordinary movement/probe and absence of sandbox controller, palette, stock/mutation and sandbox save dispatch, including when a construction scenario flag is supplied. Stop owned processes and preserve all prior previews/saves.

Evidence must include source/run/engine identities, command vectors, input and output manifests, process ownership/exit codes, targeted logs, observed manual actions and remaining failures. Packaged standalone PASS requires these checks and independent evidence review. Human interaction observations must be collected from an actual rendered session; do not infer them from NullRHI or synthetic input. Missing art does not block a graybox packaging checkpoint but prevents claiming the dressed SBX-07 milestone. Two standalone processes are not multiplayer evidence. SBX-05 network authority, matching isolated Client/Server packages and two-client edits remain open.

## Handoff

Next: complete Game/editor verification of the save-fixture compile boundary, retry in a new copied-project run, then execute the independently reviewed packaged input harness. Coordinate ownership before remote/shared implementation. No gameplay configuration tuning, new runtime dependencies or production closure is part of this packet.
