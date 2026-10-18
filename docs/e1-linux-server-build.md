# E1 Linux dedicated-server build route

`steam-server` (Ubuntu22.04, Docker) is the selected Linux x86_64 test destination, superseding Unraid on user instruction. The user authorized source-engine/SDK setup, game-server deployment, and storage allocation/mounting. This route builds on Windows; `Build-LinuxServer.ps1` itself only validates/packages and never deploys. A runnable package and measured runtime acceptance remain pending until evidence below records success.

## Setup results and packaging blocker — 2026-10-18

The source checkout at `C:\UnrealSource\UE_5.8.3` completed at the pinned commit, outside the game repository. Dependency sync completed successfully with unrelated platform exclusions; source project generation passed and produced the ignored local solution/configuration files. Source UAT completed native compilation and cooking, then failed during staging as described below. The v26 cross-toolchain installed successfully under `C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8`; version file matches and clang reports20.1.8. The downloaded installer's Epic Games Authenticode signature was valid; SHA256 is `02dee9f48cc65c5abbc9cc429271115f1cce68ebcef632db42ba4b5c9dbe2088`. The installer registered `LINUX_MULTIARCH_ROOT`; existing processes use an explicit process value until restarted.

`tools/Provision-SteamServer.sh` passed independent destructive-safety review and remote Bash syntax validation, then created a fresh200GiB `ubuntu-vg/sf-game` LV and ext4 filesystem at `/srv/shattered-frontier`. It verified exact UUID/type/mount root, backed up `/etc/fstab`, registered the mount, and created non-root `sfserver` with root-owned releases and user-owned shared/home directories. A second invocation passed without formatting or duplicate entries. About174GiB remains unallocated in the VG; root and the second SSD were untouched. Existing game container remained healthy. No new game service/container has started yet.

A C smoke program cross-compiled with this SDK to Linux x86_64 ELF, then ran successfully as UID998 on the host and in the pinned Ubuntu22.04 base container with read-only filesystem/no capabilities/network disabled. Both printed `SF_LINUX_SDK_SMOKE_OK`. This verifies basic cross-compiler/glibc runtime operation, not Unreal package compatibility. Six focused deployment-profile tests passed; remote `sh -n`, validator and clean-environment `docker compose config --quiet` passed for the candidate runtime. No image application was attempted.

First UAT attempt failed while building AutomationTool's IOS resources (`GreenCheck.png`/Apple dependency), before server C++ compilation. Even a Linux build compiles those AutomationTool projects. Restoring the IOS dependency set completed successfully and the build was restarted. Correct dependency exclusions retain IOS: `--exclude=Mac --exclude=Android --exclude=TVOS --exclude=VisionOS --exclude=LinuxArm64`. Do not repeat the initial IOS exclusion or disable AutomationTool validation to bypass it. Logs remain in ignored `Artifacts/Unreal/Setup`; the retry is `LinuxServer-Build-Retry.log`.

During native compilation, the matching Launcher commandlet independently cooked `/Game/Frontier/Maps/Greenlands_E1` for `LinuxServer` with `-run=Cook -TargetPlatform=LinuxServer -Map=/Game/Frontier/Maps/Greenlands_E1 -unattended -nop4 -NullRHI`. Exit0, zero errors/warnings; LinuxServer platform loaded successfully. Evidence: ignored `GameClient/Saved/Logs/E1-LinuxCookPreflight.log`. This verifies the external cooker's platform/module/content path; native linking subsequently passed; UAT staging/archive and deployed runtime remain separate checks.

## Current blocker and resume point

Native source compilation succeeded: 1303 actions, 2930.06 seconds, including the Linux `ShatteredFrontierServer` ELF and Windows UnrealPak. The ELF is272482560 bytes; SHA256 `95a19dbd4ec67d2caa39d1f199b6be3d4063d330dc0fd3facd75040db2eb9b21`. The native success log was preserved under ignored `Artifacts/Unreal/Setup/NativeBuild-Succeeded.txt`. Overall UAT exited1 after51m42s: staging could not launch source `Engine/Binaries/Win64/UnrealPak.exe` and exhausted three attempts. Windows Defender event1121 identifies managed ASR rule `01443614-CD74-433A-B99E-2ECDC07BFC25` as the execution block. Raw machine/account-bearing security logs are not versioned.

The signed Launcher UnrealPak runs, but replacing tools is not a validated packaging route. Read-only source review found that Launcher UAT expands receipt engine paths against its own engine tree; three required Linux runtime dependencies are absent there (two MsQuic links and ONNX Runtime). No tool swapping, receipt/support-metadata changes or security-policy changes were performed. Packaging needs normal administrator/security approval for the source-built tool, or a separately validated complete build environment. There is no runnable archive, Docker game image or game service yet.

The Windows route resumes only after normal execution approval. The independent native Linux route below is now being prepared under the user's overnight authorization. Neither route has produced a runnable archive yet. `MapsToCook` was corrected to its owning `DefaultGame.ini` (`ProjectPackagingSettings` declares `config=Game`). Inspect any archive before deployment, then run `Test-E1Remote.ps1` and verify distinct authoritative possessions. SDK smoke alone proves only basic ABI operation.

## Native Linux packaging environment — in progress

This uses a complete matching source environment and native Linux tools; it does not run/replace the denied Windows executable, change Defender, rewrite receipts or modify installed-engine support metadata. Source review verified the ordinary Unix UAT route and a portable loose cook. Windows AutomationTool binaries cannot be reused: their runtime configuration requires WindowsDesktop; compile the managed programs normally with native Linux .NET10.

- `UnrealPak Linux Development` cross-build passed254 actions/457.54s with v26. Its original receipt has269 build products and7 runtime dependencies; transfer the complete closure, including modular libraries/manifests, not just the executable. The updated game Server build also passed, most recently108.39s after the capture/scenario changes.
- Matching Launcher cook with `-SkipZenStore` and a new `-OutputDir` passed0errors/0warnings. The fresh `Artifacts/Unreal/NativeLinuxCook/LinuxServer` contains Greenlands `.umap`/`.uexp` and no Zen-store marker. Existing Windows-local Zen cook data is not portable. The owning flag is `SkipZenStore`, not `NoZenStore`.
- Exact engine Git archive at commit396c9f059903aed5fec78ecd3d437a40c6415368:608689270 compressed bytes,2940345078 expanded bytes, SHA256 `80be0941b90b2d801b91499d13142d5e670be5233af07a21849444b31fd4dc63`. Archive paths/links were checked; remote hash matched before extraction. No `.git` directory or credentials were copied.
- Reviewed `Prepare-NativeLinuxBuild.sh` passed Bash syntax and repeat provisioning. It uses only the existing dedicated LV, validates its exact mount/UUID, and creates separate `sfbuild` UID/GID997. Build root: `/srv/shattered-frontier/native-build-5.8.3`; game identity remains998. Existing directories/accounts fail closed on mismatches; children are prepared without root privileges.
- `NativeBuild.Dockerfile` built the pinned Ubuntu22.04 builder `shattered-frontier-native-builder:ue583` (image `sha256:d8ae7b81b28794fe161dc581016d444d40fbc81a5fdb0d8060d6552ac2210709`). `run-native-job.sh` serializes fixed setup/UAT-help/offline-stage jobs: non-root,2CPU,8GiB RAM/no extra swap,512PIDs, read-only root, bounded tmpfs/logs, no ports/socket or existing-game mounts. Setup requires a conservative95GiB remaining-work budget plus25GiB headroom; this is a preflight check, not a filesystem quota.

Native setup container `sf-native-setup-20261018-101457-1505988` completed exit0 (`Setup successful`), using Epic `Setup.sh --force --threads=2 --no-cache` and the exclusions above. About112GiB remained on the game volume afterward. Native UBT/AutomationTool/script-module compilation and `BuildCookRun -help` completed exit0 in `sf-native-uat-help-20261018-102926-1507829`; native AutomationTool executed for1m32s after bootstrap compilation. Windrose remained healthy. Bound carriage-return progress with `docker logs --tail 5 <name> 2>&1 | tail -c 1000`.

Independently reviewed `tools/Export-NativeLinuxInputs.py` passed synthetic checks and exported2270 unchanged files/2964313533 expanded bytes into `Artifacts/Unreal/NativeLinuxInputs-reviewed.tar.gz` (1636578944 bytes, SHA256 `b24ba9f6283787d5f94f736a0634c17ecaad10cd60df327eb609ea5ec59dbf62`). The sibling manifest records every file/hash/mode and both original receipt hashes: UnrealPak269 products+7 runtime dependencies; Server3+975. This is complete input transfer evidence, not package validation. Actual symlink creation was unavailable on Windows; link/reparse/special metadata rejection was synthetically checked.

Portable cook command, already executed:

```powershell
$env:LINUX_MULTIARCH_ROOT = 'C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8'
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\GameClient\ShatteredFrontier.uproject" -run=Cook -TargetPlatform=LinuxServer -Map=/Game/Frontier/Maps/Greenlands_E1 -SkipZenStore "-OutputDir=$PWD\Artifacts\Unreal\NativeLinuxCook\[Platform]" -unattended -nop4 -NullRHI
```

Remote archive SHA256 matched, extraction as sfbuild succeeded, executable modes were retained, and103GiB remained. Offline native stage is running in `sf-native-stage-20261018-103539-1509891`; native UnrealPak ABI, stage/archive and runtime are not yet proven. Next: inspect actual stage results. Then validate native UAT `BuildCookRun -skipbuild -skipcook -stage -pak -skipiostore -archive -server -noclient -serverplatform=Linux -serverarchitecture=x64 -serverconfig=Development -CookOutputDir=/work/cook/LinuxServer -stagingdirectory=/work/stage -archivedirectory=/work/archive`. The source reader verified these switches and native UnrealPak lookup; this stage command remains unexecuted. No full native LinuxEditor build is needed for the selected route.

Local ignored logs: `Native-Linux-UnrealPak-Build.log`, `Native-Linux-LooseCook.log`, `Native-Current-LinuxServer-Build.log`, `Native-Builder-Image.log`, `Native-Source-Transfer.log`, `Native-Setup-Dispatch.log`, `Native-UAT-Help.log` under `Artifacts/Unreal/Setup`. Packaging, non-root game startup, actual remote clients, shutdown and resource checks remain required.

## Source/cooker route and prerequisites

The installed Launcher engine is UE 5.8.3 CL58210709. Its `BaseEngine.ini` has Linux Game entries but no Server entries, and its Linux Game receipt is absent. `UEBuildTarget.cs:1395–1412` checks installed-platform target support and rejects unsupported target types. A project Server target cannot add distribution support. Keep this engine for local editor work; do not remove installed-build markers or alter support metadata.

Epic's `5.8.3-release` source is pinned to commit `396c9f059903aed5fec78ecd3d437a40c6415368`, rather than the moving `5.8` branch. Source compilation and cook evidence is recorded here; runtime acceptance remains pending.

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

Static validation, native compilation and cooking passed; packaging is blocked during staging. `MaxParallelActions` defaults to4, bounded1–16, to limit initial workstation memory pressure. The existing game container occupies UDP7777 on the target host; the new [runtime profile](../ops/steam-server/README.md) therefore defaults to UDP17777 (verified free during preparation). Its Docker image/Compose deployment remains pending package generation. The Ubuntu22.04 base image was pulled successfully and its digest is pinned in the Dockerfile.

These are candidate commands until recorded execution establishes their result. `-ValidateOnly` checks paths, source identity and required files without creating directories or invoking UAT. It rejects missing prerequisites and installed engine distributions. It does not verify dependency completeness, compiler functionality or runnable packages.

The build command invokes the selected source engine's `RunUAT.bat BuildCookRun` with `-build -cook -stage -pak -archive -server -noclient -serverplatform=Linux -serverarchitecture=x64 -serverconfig=Development`. Arguments are grounded in the installed 5.8.3 owning `AutomationTool/AutomationUtils/ProjectParams.cs` declarations and `BuildCookRun.Automation.cs`; the matching source checkout must still execute them successfully. No run/deploy option is supplied. UAT writes its normal project/engine intermediates and logs; explicit staging/archive destinations are `Artifacts/Unreal/LinuxServer/Stage` and `Artifacts/Unreal/LinuxServer/Archive` under the repository's ignored artifact root. Archive subdirectory naming is UAT-owned.

The entry point does not download, install, clone, edit environment registration, delete outputs, copy to remote hosts or handle credentials. Reparse-point paths and ambiguous local paths are rejected. Successful UAT completion is package evidence only; E1 separate-server/two-client authority checks and Linux runtime verification remain required.
