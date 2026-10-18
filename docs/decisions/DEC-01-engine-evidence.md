# DEC-01 — E1 engine and toolchain evidence

Status: independently reviewed post-install evidence recheck, 2026-10-18. The missing engine-payload prerequisite is cleared; DEC-01 remains open for project/target-route decisions and executable compatibility evidence. This is read-only host evidence, not a successful editor launch or build. Installation, project creation, builds and runtime tests were not performed by this documentation task.

## Decision and inspected authority

[Roadmap] E1 Entry requires a known usable engine/build path, compiler/SDK, selected project directory and recorded source/server-target feasibility. [Backlog] ENV-01 records exact toolchain identity; ENV-02 requires shared client/editor/server gameplay source and a reproducible editor build. ENV-03…05 then establish the graybox, separate-server/two-client launch instructions and precise checks. The final DEL-01 packaged gate remains separate.

[UE layout], Foundation decisions, proposes `GameClient/ShatteredFrontier.uproject`; Build targets requires verification against the actual engine. [Repository structure], Target tree, likewise proposes `GameClient`, not a second copied server gameplay project. Neither document proves an engine/project exists or authorizes installation. The E1 roadmap permits an explicitly scoped editor-server demonstration with final packaged readiness still open.

## Pre-install snapshot (historical)

The following observations were collected before installation completed. They are retained as history and superseded for current engine-file presence by the recheck below.

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

Collection used only targeted directory/file existence and metadata reads, Epic/SDK registry queries, launcher-manifest parsing and Visual Studio's inventory query. No exhaustive machine scan, credential access, editor/compiler execution or build was used.

## Post-install recheck

After the user reported installation complete, targeted reads at `C:\Program Files\Epic Games\UE_5.8` established:

| Source | Current observation | Interpretation / limit |
| --- | --- | --- |
| `Engine/Build/Build.version` | 5.8.3; changelist `58210709`, compatible changelist `55116800`, branch `++UE5+Release-5.8`, promoted build | Exact installed build metadata recorded; no inference of successful project compatibility. |
| Editor and build tooling | `UnrealEditor.exe`, `Build.bat`, `RunUAT.bat`, UnrealBuildTool DLL and `InstalledBuild.txt` present; editor file/product version names CL `58210709` | The prior missing-payload prerequisite is cleared. Tools were not executed. Installed-build marker is present; no source checkout/entitlement assumed. |
| `Engine/Config/Windows/Windows_SDK.json` | SDK minimum `10.0.19041.0`, maximum `10.9.99999.0`, main version `10.0.22621.0` | SDK `10.0.26100.0` is within the declared range. Its Windows.h/kernel32.lib files remain present; this is not a compilation test. |
| Same file: MSVC declarations | Minimum `14.38.33130`; preferred families `14.50.35717–14.50.99999`, `14.44.35207–14.44.99999`; banned ranges include early 14.50/14.44 and 14.39…14.43 | Available toolset directory `14.51.36231` is above minimum, outside listed banned ranges and outside preferred ranges. It is non-preferred, not a demonstrated failure. No compiler change was made. |
| Compiler inventory | VC component query still identifies VS Community 2026; available family `14.51.36231`, cl.exe file `19.51.36260.0` / product `14.51.36260.0` | Preserve both family/binary identities. Actual UnrealBuildTool selection and compile result remain pending. |
| `Engine/Config/BaseEngine.ini`, InstalledPlatforms | 28 installed-platform rows; six Win64 Game, four Win64 Editor, three Linux Game; zero `PlatformType="Server"` rows. `Engine/Source/UE5Server.Target.cs` absent | Installed metadata does not advertise Server targets. Linux Game entries do not prove a Linux dedicated-server package or cross-toolchain. No target was built; source/server route remains a decision/evidence gate. |
| Proposed `GameClient/ShatteredFrontier.uproject` | Still absent | No project or project targets were created/compiled as part of verification. |

The engine's local declaration is the source for the static compiler/SDK comparison; it does not establish that every compiler above its minimum works. No editor, build, SDK installer, server, network test or project-generation command was run.

## Current gate and next input

**The engine-payload blocker is cleared.** E1 can now proceed to authorized ENV-02 project/target work using the prepared [E1 packet]. DEC-01 itself is not accepted/closed: project location/association, actual selected toolchain, editor compile and dedicated-server route still need evidence. Do not replace those with metadata-only success.

| Route / owner decision | Consequence / remaining evidence |
| --- | --- |
| E1 editor-server demonstration | Use a separate authority process and two independent clients after the first successful project build. Keep final packaged readiness open. |
| Packaged dedicated-server route | Select and validate an engine distribution/source route with Server-target capability; current metadata is not package evidence. Source access, compatible toolchains and Linux runtime/package requirements remain unverified. |
| Non-preferred MSVC family | Record actual selection/compile diagnostics first; use an owner-approved supported toolset change only if required. No automatic installation/downgrade or compatibility failure inferred. |

Revisit on root/build changes, failed compile/toolchain selection, or target evidence contradicting the selected route. This inventory does not retire the browser PoC or resolve DEC-02…09. Installation/project implementation remains separate from this documentation task.

## Next precise evidence pass

1. Record engine `Build.version` and installed/source provenance, actual compiler/SDK compatibility and available Unreal tooling. Distinguish files present from supported combinations.
2. Confirm project location and shared gameplay/editor/client/server target approach before authorized ENV-02 work. Record actual successful commands, build identity and log locations; no speculative command is a verified setup instruction.
3. Under a subsequent authorized implementation task, demonstrate the graybox on separate authority with two independent clients, possession/movement/target validation, ports and per-process logs. E1's editor-server result must be labelled separately from packaged-server capability.
4. Keep final DEL-01 packaged client/server and all performance claims open until their actual evidence exists. No runtime tests are appropriate for this documentation-only snapshot.

[Roadmap]: ../Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md#phase-1a--e1--engine-and-movement
[Backlog]: ../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md#epic-env--reproducible-engine-foundation
[UE layout]: ../Game-Design/Legends_Shattered_Frontier_UE58_Project_Layout_v1.0.md
[Repository structure]: ../Game-Design/Legends_Shattered_Frontier_Repository_Structure_v1.0.md
[E1 packet]: ../e1-implementation-packet.md
