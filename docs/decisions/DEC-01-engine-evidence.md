# DEC-01 — E1 engine and toolchain evidence

Status: independently reviewed, blocked evidence packet, 2026-10-18. This is a read-only snapshot of this Windows host, not acceptance of an engine version, project location or server-build route. Installation, project creation, builds and runtime tests were not performed.

## Decision and inspected authority

[Roadmap] E1 Entry requires a known usable engine/build path, compiler/SDK, selected project directory and recorded source/server-target feasibility. [Backlog] ENV-01 records exact toolchain identity; ENV-02 requires shared client/editor/server gameplay source and a reproducible editor build. ENV-03…05 then establish the graybox, separate-server/two-client launch instructions and precise checks. The final DEL-01 packaged gate remains separate.

[UE layout], Foundation decisions, proposes `GameClient/ShatteredFrontier.uproject`; Build targets requires verification against the actual engine. [Repository structure], Target tree, likewise proposes `GameClient`, not a second copied server gameplay project. Neither document proves an engine/project exists or authorizes installation. The E1 roadmap permits an explicitly scoped editor-server demonstration with final packaged readiness still open.

## Observed evidence

| Item / read-only source | Observation on 2026-10-18 | What it establishes / limit |
| --- | --- | --- |
| Epic engine parent directory | `C:\Program Files\Epic Games\UE_5.8` exists; its only immediate entry, including hidden entries, is `.egstore` | A directory name is not an installed engine or a verified 5.8 build. No installation/download progress inferred. |
| Exact engine files under that root | `Engine/Build/Build.version`, `Engine/Binaries/Win64/UnrealEditor.exe`, `Engine/Build/BatchFiles/Build.bat` and `RunUAT.bat` are absent | This root cannot currently supply engine identity, an editor launch or a build command. Engine source/server-target feasibility remains unverified. |
| Epic launcher manifest | `C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat` exists with zero `InstallationList` entries | No installed engine resolved from this manifest; that alone would not establish absence elsewhere. |
| Engine registrations | Current-user `Software\Epic Games\Unreal Engine\Builds` absent. HKLM `SOFTWARE\EpicGames\Unreal Engine\4.0` records `C:\Program Files\Epic Games\4.0\` | A legacy registration is not evidence of the requested engine or an alternative usable root. Other accounts/custom drives/source checkouts were not searched. |
| Visual Studio Installer `vswhere.exe` | Community 2026 at `C:\Program Files\Microsoft Visual Studio\18\Community`; installation version `18.11.12306.390`, reported complete/launchable; VC x86/x64 component query returns that installation | Installer/component discovery, not an Unreal compile or compatibility result. |
| MSVC files | Toolset directory `VC/Tools/MSVC/14.51.36231`; `bin/Hostx64/x64/cl.exe` file version `19.51.36260.0`, product version `14.51.36260.0` | Both directory and binary versions recorded without assuming they are identical. Compiler was not invoked. |
| Windows SDK files | Under `C:\Program Files (x86)\Windows Kits\10`, SDK `10.0.26100.0` has `Include/.../um/Windows.h`, `Lib/.../um/x64/kernel32.lib`, `bin/.../x64/rc.exe` | Representative header/library/resource-compiler presence, not completeness or compatibility. The registered `C:\Program Files\Windows Kits\10\` root alone did not identify these files. |
| Proposed repository project | `C:\Projects\Shattered-Frontier\GameClient` and its proposed `ShatteredFrontier.uproject` are absent | No Unreal project/targets at the proposed location. Another project outside this repository was not searched for or selected. |

Collection used only targeted directory/file existence and metadata reads, Epic/SDK registry queries, launcher-manifest parsing and Visual Studio's inventory query. No exhaustive machine scan, credential access, editor/compiler execution or build was used. Engine build/revision, source entitlement/checkout, Unreal tooling access, compiler/SDK compatibility and packaged-server support remain unknown.

## Blocker and next owner input

**DEC-01/E1 is blocked on a usable engine root.** The inspected `UE_5.8` folder has no engine payload. The current documentation mandate excludes installing/repairing the engine or creating/compiling the game project; no installation was started to close this gate.

Engine/project owner can unblock the next evidence pass by completing the intended installation through an authorized setup task, or supplying another usable installed/source engine root and its intended build. A custom/source location was not ruled out. Preserve the proposed repository path until the owner selects it or implementation is authorized; no directory was created by this packet.

| Option for the owner | Consequence / remaining evidence |
| --- | --- |
| Complete the intended engine installation | Reinspect actual `Build.version`, editor/tool scripts and supported toolchain. Folder name or installer completion alone still cannot prove targets. |
| Supply an existing installed/source engine elsewhere | Record exact root/build/revision and check against the project baseline before adopting it; do not silently substitute another engine version. |
| Later choose a source/server build route if required by verified target constraints | Requires source access, compatible tools and reproducible target evidence. No source-build requirement is inferred solely from today's missing payload. |

Revisit after the engine payload becomes available, the chosen root/version changes, or target/toolchain evidence contradicts the proposed route. This operational prerequisite does not retire the browser PoC or resolve DEC-02…09.

## Next precise evidence pass

1. Record engine `Build.version` and installed/source provenance, actual compiler/SDK compatibility and available Unreal tooling. Distinguish files present from supported combinations.
2. Confirm project location and shared gameplay/editor/client/server target approach before authorized ENV-02 work. Record actual successful commands, build identity and log locations; no speculative command is a verified setup instruction.
3. Under a subsequent authorized implementation task, demonstrate the graybox on separate authority with two independent clients, possession/movement/target validation, ports and per-process logs. E1's editor-server result must be labelled separately from packaged-server capability.
4. Keep final DEL-01 packaged client/server and all performance claims open until their actual evidence exists. No runtime tests are appropriate for this documentation-only snapshot.

[Roadmap]: ../Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md#phase-1a--e1--engine-and-movement
[Backlog]: ../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md#epic-env--reproducible-engine-foundation
[UE layout]: ../Game-Design/Legends_Shattered_Frontier_UE58_Project_Layout_v1.0.md
[Repository structure]: ../Game-Design/Legends_Shattered_Frontier_Repository_Structure_v1.0.md
