# E1 Linux dedicated-server build route

`steam-server` (Ubuntu22.04, Docker) is the selected Linux x86_64 test destination, superseding Unraid on user instruction. The user authorized source-engine/SDK setup, game-server deployment, and storage allocation/mounting. This route builds on Windows; `Build-LinuxServer.ps1` itself only validates/packages and never deploys. A runnable package and measured runtime acceptance remain pending until evidence below records success.

## Setup in progress — 2026-10-18

The source checkout at `C:\UnrealSource\UE_5.8.3` completed at the pinned commit, outside the game repository. Dependency sync is in progress with unrelated platform exclusions. The v26 cross-toolchain installed successfully under `C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8`; version file matches and clang reports20.1.8. The downloaded installer's Epic Games Authenticode signature was valid; SHA256 is `02dee9f48cc65c5abbc9cc429271115f1cce68ebcef632db42ba4b5c9dbe2088`. The installer registered `LINUX_MULTIARCH_ROOT`; existing processes use an explicit process value until restarted.

`tools/Provision-SteamServer.sh` passed independent destructive-safety review and remote Bash syntax validation, then created a fresh200GiB `ubuntu-vg/sf-game` LV and ext4 filesystem at `/srv/shattered-frontier`. It verified exact UUID/type/mount root, backed up `/etc/fstab`, registered the mount, and created non-root `sfserver` with root-owned releases and user-owned shared/home directories. A second invocation passed without formatting or duplicate entries. About174GiB remains unallocated in the VG; root and the second SSD were untouched. Existing game container remained healthy. No new game service/container has started yet.

A C smoke program cross-compiled with this SDK to Linux x86_64 ELF, then ran successfully as UID998 on the host and in the pinned Ubuntu22.04 base container with read-only filesystem/no capabilities/network disabled. Both printed `SF_LINUX_SDK_SMOKE_OK`. This verifies basic cross-compiler/glibc runtime operation, not Unreal package compatibility. Six focused deployment-profile tests passed; remote `sh -n`, validator and clean-environment `docker compose config --quiet` passed for the candidate runtime. No image application was attempted.

## Source/cooker route and prerequisites

The installed Launcher engine is UE 5.8.3 CL58210709. Its `BaseEngine.ini` has Linux Game entries but no Server entries, and its Linux Game receipt is absent. `UEBuildTarget.cs:1395–1412` checks installed-platform target support and rejects unsupported target types. A project Server target cannot add distribution support. Keep this engine for local editor work; do not remove installed-build markers or alter support metadata.

Epic's `5.8.3-release` source is pinned to commit `396c9f059903aed5fec78ecd3d437a40c6415368`, rather than the moving `5.8` branch. Source compilation, cook and runtime acceptance must be recorded after execution.

The engine's `Linux_SDK.json` requires the installed `v26_clang-20.1.8-rockylinux8`. `LinuxPlatformSDK.cs` resolves `LINUX_MULTIARCH_ROOT`, otherwise `Engine/Extras/ThirdPartyNotUE/SDKs/HostWin64/Linux_x64/<version>`; the entry point supports those explicit routes. AutoSDK-only or legacy `LINUX_ROOT` setups require review before use.

Source UAT also supports an explicit matching external cook editor: `-skipbuildeditor -unrealexe=<absolute path>`. Independent source review verified these switches, absolute path handling, and the remaining source Win64 UnrealPak build agenda. This avoids compiling a second full editor; Linux server/UHT/UnrealPak still compile from source. The Launcher-built project editor module must remain loadable. Both releases declare compatible changelist55116800, but actual cook and network/runtime checks must establish compatibility. Stop on a mismatch; do not override version checks.

## Preparation order

1. Obtain Epic's Unreal source in a separate local checkout and pin `5.8.3-release` to the commit above. Do not replace the Launcher engine or put engine source in this repository.
2. Run that checkout's dependency setup and project generation according to its own README. These steps download dependencies and consume substantial storage; they are separate from this script.
3. Prepare the matching Windows-to-Linux x86_64 SDK. Supply `LINUX_MULTIARCH_ROOT` in the invoking process, or use the exact in-tree SDK path above. Verify `ToolchainVersion.txt` and `x86_64-unknown-linux-gnu/bin/clang++.exe`. No persistent environment changes are made by the script.
4. Either build the source Windows editor, or supply `-CookEditorRoot` for the verified matching external editor/project module. Static validation checks the cooker version and file presence; actual loadability remains a cook-time check.
5. Validate the project Server target and configured E1 startup map, then invoke the packaging entry point. UAT owns target discovery, cooking, server compilation and staging. Preserve its actual logs and failure evidence.
6. Inspect the resulting Linux archive, then validate its runtime dependencies, non-root container execution, UDP networking, logs and shutdown on `steam-server`. Keep the E1 server on the trusted LAN; public exposure and production persistence are separate tasks.

## Entry point

From the repository root, with a prepared source checkout at the example path:

```powershell
.\tools\Build-LinuxServer.ps1 -SourceEngineRoot C:\UnrealSource\UE_5.8.3 -ValidateOnly
.\tools\Build-LinuxServer.ps1 -SourceEngineRoot C:\UnrealSource\UE_5.8.3
```

Selected development route with the already-built Launcher editor module:

```powershell
$env:LINUX_MULTIARCH_ROOT = 'C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8'
.\tools\Build-LinuxServer.ps1 -SourceEngineRoot C:\UnrealSource\UE_5.8.3 -CookEditorRoot 'C:\Program Files\Epic Games\UE_5.8' -ValidateOnly
.\tools\Build-LinuxServer.ps1 -SourceEngineRoot C:\UnrealSource\UE_5.8.3 -CookEditorRoot 'C:\Program Files\Epic Games\UE_5.8'
```

Static validation of this route passed; build/cook execution is still pending. `MaxParallelActions` defaults to4, bounded1–16, to limit initial workstation memory pressure. The existing game container occupies UDP7777 on the target host; the new [runtime profile](../ops/steam-server/README.md) therefore defaults to UDP17777 (verified free during preparation). Its Docker image/Compose deployment remains pending package generation. The Ubuntu22.04 base image was pulled successfully and its digest is pinned in the Dockerfile.

These are candidate commands until recorded execution establishes their result. `-ValidateOnly` checks paths, source identity and required files without creating directories or invoking UAT. It rejects missing prerequisites and installed engine distributions. It does not verify dependency completeness, compiler functionality or runnable packages.

The build command invokes the selected source engine's `RunUAT.bat BuildCookRun` with `-build -cook -stage -pak -archive -server -noclient -serverplatform=Linux -serverarchitecture=x64 -serverconfig=Development`. Arguments are grounded in the installed 5.8.3 owning `AutomationTool/AutomationUtils/ProjectParams.cs` declarations and `BuildCookRun.Automation.cs`; the matching source checkout must still execute them successfully. No run/deploy option is supplied. UAT writes its normal project/engine intermediates and logs; explicit staging/archive destinations are `Artifacts/Unreal/LinuxServer/Stage` and `Artifacts/Unreal/LinuxServer/Archive` under the repository's ignored artifact root. Archive subdirectory naming is UAT-owned.

The entry point does not download, install, clone, edit environment registration, delete outputs, copy to remote hosts or handle credentials. Reparse-point paths and ambiguous local paths are rejected. Successful UAT completion is package evidence only; E1 separate-server/two-client authority checks and Linux runtime verification remain required.
