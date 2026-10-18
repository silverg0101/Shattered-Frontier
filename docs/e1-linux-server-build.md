# E1 Linux dedicated-server build route

Unraid's Docker host is the selected Linux x86_64 test destination. A runnable game-server package, container image, deployment and player-capacity measurements remain pending. This route builds on Windows; it neither changes Unraid nor starts containers.

## Current evidence and prerequisites

The installed Launcher engine is UE 5.8.3 CL58210709. Its `BaseEngine.ini` has Linux Game entries but no Server entries, and its Linux Game receipt is absent. `UEBuildTarget.cs:1395–1412` checks installed-platform target support and rejects unsupported target types. A project Server target cannot add distribution support. Keep this engine for local editor work; do not remove installed-build markers or alter support metadata.

Read-only authenticated Git queries resolved Epic's `5.8.3-release` tag to commit `396c9f059903aed5fec78ecd3d437a40c6415368`. Source access is available; checkout, dependency setup and source compilation have not been performed. The source route is pinned to this release rather than the moving `5.8` branch.

The installed engine's `Linux_SDK.json` requires `v26_clang-20.1.8-rockylinux8`. No Linux SDK environment registration or in-tree SDK was found. `LinuxPlatformSDK.cs` resolves `LINUX_MULTIARCH_ROOT`, otherwise `Engine/Extras/ThirdPartyNotUE/SDKs/HostWin64/Linux_x64/<version>`; this entry point intentionally supports those explicit routes. AutoSDK-only or legacy `LINUX_ROOT` setups require review before use.

## Preparation order

1. Obtain Epic's Unreal source in a separate local checkout and pin `5.8.3-release` to the commit above. Do not replace the Launcher engine or put engine source in this repository.
2. Run that checkout's dependency setup and project generation according to its own README. These steps download dependencies and consume substantial storage; they are separate from this script.
3. Prepare the matching Windows-to-Linux x86_64 SDK. Supply `LINUX_MULTIARCH_ROOT` in the invoking process, or use the exact in-tree SDK path above. Verify `ToolchainVersion.txt` and `x86_64-unknown-linux-gnu/bin/clang++.exe`. No persistent environment changes are made by the script.
4. Build and verify the source engine's Windows editor/cook commandlet. The script requires `Engine/Binaries/Win64/UnrealEditor-Cmd.exe`; file presence remains a prerequisite, not a compile proof. Verify compiler selection and the project against this engine before packaging.
5. Validate the project Server target and configured E1 startup map, then invoke the packaging entry point. UAT owns target discovery, cooking, server compilation and staging. Preserve its actual logs and failure evidence.
6. Inspect the resulting Linux archive, then separately validate its runtime dependencies, non-root container execution, UDP networking, logs and shutdown on the Unraid host. Public exposure and production persistence are separate tasks.

## Entry point

From the repository root, with a prepared source checkout at the example path:

```powershell
.\tools\Build-LinuxServer.ps1 -SourceEngineRoot C:\UnrealSource\UE_5.8.3 -ValidateOnly
.\tools\Build-LinuxServer.ps1 -SourceEngineRoot C:\UnrealSource\UE_5.8.3
```

These are candidate commands until recorded execution establishes their result. `-ValidateOnly` checks paths, source identity and required files without creating directories or invoking UAT. It rejects missing prerequisites and installed engine distributions. It does not verify dependency completeness, compiler functionality or runnable packages.

The build command invokes the selected source engine's `RunUAT.bat BuildCookRun` with `-build -cook -stage -pak -archive -server -noclient -serverplatform=Linux -serverarchitecture=x64 -serverconfig=Development`. Arguments are grounded in the installed 5.8.3 owning `AutomationTool/AutomationUtils/ProjectParams.cs` declarations and `BuildCookRun.Automation.cs`; the matching source checkout must still execute them successfully. No run/deploy option is supplied. UAT writes its normal project/engine intermediates and logs; explicit staging/archive destinations are `Artifacts/Unreal/LinuxServer/Stage` and `Artifacts/Unreal/LinuxServer/Archive` under the repository's ignored artifact root. Archive subdirectory naming is UAT-owned.

The entry point does not download, install, clone, edit environment registration, delete outputs, copy to Unraid or handle credentials. Reparse-point paths and ambiguous local paths are rejected. Successful UAT completion is package evidence only; E1 separate-server/two-client authority checks and Linux runtime verification remain required.
