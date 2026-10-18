# E1 Linux dedicated-server build route

`steam-server` (Ubuntu22.04, Docker) is the selected Linux x86_64 test destination, superseding Unraid on user instruction. The user authorized source-engine/SDK setup, game-server deployment, and storage allocation/mounting. Native compilation uses Windows cross-tools; native Linux staging and the initial dedicated-server deployment now pass. `Build-LinuxServer.ps1` remains a Windows-only packaging entry point and never deploys. The running LAN server still lacks compatible remote-client acceptance, as recorded below.

## Setup results and packaging blocker — 2026-10-18

The source checkout at `C:\UnrealSource\UE_5.8.3` completed at the pinned commit, outside the game repository. Dependency sync completed successfully with unrelated platform exclusions; source project generation passed and produced the ignored local solution/configuration files. Source UAT completed native compilation and cooking, then failed during staging as described below. The v26 cross-toolchain installed successfully under `C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8`; version file matches and clang reports20.1.8. The downloaded installer's Epic Games Authenticode signature was valid; SHA256 is `02dee9f48cc65c5abbc9cc429271115f1cce68ebcef632db42ba4b5c9dbe2088`. The installer registered `LINUX_MULTIARCH_ROOT`; existing processes use an explicit process value until restarted.

`tools/Provision-SteamServer.sh` passed independent destructive-safety review and remote Bash syntax validation, then created a fresh200GiB `ubuntu-vg/sf-game` LV and ext4 filesystem at `/srv/shattered-frontier`. It verified exact UUID/type/mount root, backed up `/etc/fstab`, registered the mount, and created non-root `sfserver` with root-owned releases and user-owned shared/home directories. A second invocation passed without formatting or duplicate entries. About174GiB remains unallocated in the VG; root and the second SSD were untouched. Existing game container remained healthy. At this provisioning step no game service existed; the later native deployment is recorded below.

A C smoke program cross-compiled with this SDK to Linux x86_64 ELF, then ran successfully as UID998 on the host and in the pinned Ubuntu22.04 base container with read-only filesystem/no capabilities/network disabled. Both printed `SF_LINUX_SDK_SMOKE_OK`. This verifies basic cross-compiler/glibc runtime operation, not Unreal package compatibility. Six focused deployment-profile tests passed; remote `sh -n`, validator and clean-environment `docker compose config --quiet` passed for the candidate runtime. That initial profile check did not apply an image; later deployment is recorded below.

First UAT attempt failed while building AutomationTool's IOS resources (`GreenCheck.png`/Apple dependency), before server C++ compilation. Even a Linux build compiles those AutomationTool projects. Restoring the IOS dependency set completed successfully and the build was restarted. Correct dependency exclusions retain IOS: `--exclude=Mac --exclude=Android --exclude=TVOS --exclude=VisionOS --exclude=LinuxArm64`. Do not repeat the initial IOS exclusion or disable AutomationTool validation to bypass it. Logs remain in ignored `Artifacts/Unreal/Setup`; the retry is `LinuxServer-Build-Retry.log`.

During native compilation, the matching Launcher commandlet independently cooked `/Game/Frontier/Maps/Greenlands_E1` for `LinuxServer` with `-run=Cook -TargetPlatform=LinuxServer -Map=/Game/Frontier/Maps/Greenlands_E1 -unattended -nop4 -NullRHI`. Exit0, zero errors/warnings; LinuxServer platform loaded successfully. Evidence: ignored `GameClient/Saved/Logs/E1-LinuxCookPreflight.log`. This verifies the external cooker's platform/module/content path; native linking subsequently passed; UAT staging/archive and deployed runtime remain separate checks.

## Earlier Windows staging blocker

Native source compilation succeeded: 1303 actions, 2930.06 seconds, including the Linux `ShatteredFrontierServer` ELF and Windows UnrealPak. The ELF is272482560 bytes; SHA256 `95a19dbd4ec67d2caa39d1f199b6be3d4063d330dc0fd3facd75040db2eb9b21`. The native success log was preserved under ignored `Artifacts/Unreal/Setup/NativeBuild-Succeeded.txt`. Overall UAT exited1 after51m42s: staging could not launch source `Engine/Binaries/Win64/UnrealPak.exe` and exhausted three attempts. Windows Defender event1121 identifies managed ASR rule `01443614-CD74-433A-B99E-2ECDC07BFC25` as the execution block. Raw machine/account-bearing security logs are not versioned.

The signed Launcher UnrealPak runs, but replacing tools is not a validated packaging route. Read-only source review found that Launcher UAT expands receipt engine paths against its own engine tree; three required Linux runtime dependencies are absent there (two MsQuic links and ONNX Runtime). No tool swapping, receipt/support-metadata changes or security-policy changes were performed. Packaging needs normal administrator/security approval for the source-built tool, or a separately validated complete build environment. That Windows attempt produced no archive; the independent native Linux route below subsequently produced and deployed one.

The Windows route resumes only after normal execution approval. The independent native Linux route below completed under the user's overnight authorization. `MapsToCook` was corrected to its owning `DefaultGame.ini` (`ProjectPackagingSettings` declares `config=Game`). Inspect any archive before deployment, then run `Test-E1Remote.ps1` and verify distinct authoritative possessions. SDK smoke alone proves only basic ABI operation.

## Native Linux packaging and initial deployment - passed

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

Remote input archive SHA256 matched, extraction as sfbuild succeeded, executable modes were retained, and103GiB remained. Offline `run-native-job.sh stage` completed exit0 in `sf-native-stage-20261018-103539-1509891`: native UnrealPak exit0, stage7.57s, archive2.23s, BuildCookRun11.37s. It used the reviewed skipbuild/skipcook/legacy-Pak flags, original receipts and portable cook; no full LinuxEditor build or tool swapping was needed.

The2.2GiB `archive/LinuxServer` tree was inspected (no symlinks), copied into root-owned `/srv/shattered-frontier/releases/e1-2a5b634429b75d46519ad2a28560b8953ed99831/package`, and hashed into adjacent `package-manifest.sha256`. The game ELF's `ldd` had no missing dependencies. Runtime image `shattered-frontier:e1-2a5b634429b75d46519ad2a28560b8953ed99831` built as `sha256:42b382b6e3d581388be3bc8341a10ba7ff08876e9b0788457f71cb8e53d6f0ce`, labelled with that full game revision. The validated profile applied successfully: sfserver998:998, read-only root,4CPU/8GiB limits, dropped capabilities, no-new-privileges, writable Saved/home mounts plus bounded /tmp. `shattered-frontier-game-1` became healthy with zero restarts, `E1_WORLD_READY Map=Greenlands_E1 NetMode=1`, and only192.168.50.133:17777/udp published. Initial engine startup0.64s and short idle snapshot89.67MiB/2.25% CPU are observations, not capacity/tick budgets. Windrose remained healthy; no public firewall/router changes. About97GiB remained on the new volume after the image build.

### Current blocker: matching remote client

`Test-E1Remote.ps1 -ServerAddress 192.168.50.133` reached the server but correctly failed the handshake: source server NetCL0/checksum1737972550, Launcher editor client NetCL55116800/checksum1533752452; both EngineNetworkVersion44/GameNetworkVersion0. Evidence: `GameClient/Saved/Logs/E1-Remote-20261018-063833/Client1.log` and host `shared/Logs/Server.log`. No remote pawn was admitted. Independent source review reproduced both checksums from this single difference: UBT `BuildVersion.EffectiveCompatibleChangelist` returns0 when source Changelist=0 despite raw CompatibleChangelist55116800. Asset-compatible release numbers alone did not prove network-compatible binaries. Preserve checks and original release identity.

The matching-source `ShatteredFrontierClient Win64 Development` build passed1027 actions in2744.59s, isolated under `Artifacts/Unreal/SourceClientProject/GameClient` exported from game commit2a5b634. The Launcher editor/module is preserved. Fresh `WindowsClient` loose cook passed15.81s with zero errors/warnings. Independent source review identified ordinary source Windows UAT loose-file staging below: omit `-pak`, disable IoStore and bootstrap generation; no UnrealPak execution is needed. Native Linux Win64 staging is not required for this initial client route. The source-produced client also needs ordinary Windows execution-policy acceptance; compilation is not execution proof. Do not edit changelists/receipts, override network checks or change Defender policy to make it connect.

Executed client staging command (passed; stage6.74s, archive4.34s, UAT28s):

```powershell
& 'C:/UnrealSource/UE_5.8.3/Engine/Build/BatchFiles/RunUAT.bat' BuildCookRun '-project=C:/Projects/Shattered-Frontier/Artifacts/Unreal/SourceClientProject/GameClient/ShatteredFrontier.uproject' -client -target=ShatteredFrontierClient -platform=Win64 -clientconfig=Development -skipbuild -skipcook -stage -archive -skipiostore -nobootstrapexe -nocodesign -unattended '-CookOutputDir=C:/Projects/Shattered-Frontier/Artifacts/Unreal/SourceClientCook/WindowsClient' '-stagingdirectory=C:/Projects/Shattered-Frontier/Artifacts/Unreal/SourceClientLooseStage' '-archivedirectory=C:/Projects/Shattered-Frontier/Artifacts/Unreal/SourceClientLooseArchive'
```

Source `UsePak=false` copies loose UFS files; omit `-pak`, `-skippak`, `-makebinaryconfig`, `-crashreporter` and `-prereqs`. Project/default configuration does not enable binary-config generation. Target=Client requires WindowsClient cook, not Windows. The actual nested `WindowsClient/ShatteredFrontier/Binaries/Win64/ShatteredFrontierClient.exe` is324199424 bytes, SHA256 `49a62f1a302aa6810beb6760532aad1ecff7f2fbe43066d678d967411d8484fd`, receipt Client/Win64/Development/x64. Its first ordinary `Test-E1Remote.ps1 -ClientExecutable ... -HoldSeconds60` launch was denied before any PID spawned: managed Defender ASR event1121 at07:31:02 local, rule01443614-CD74-433A-B99E-2ECDC07BFC25. Normal administrator/security execution approval is required. Do not retry this binary via another path, replace tools, change policy or falsify network identity. Bootstrap descendants are not supported by that helper.

Controlled no-client stop/restart was also exercised: Docker stop produced exit143, `OOM=false`, and ordinary engine/object/log shutdown; the validated profile restarted the same image and returned healthy with zero crash restarts. This checks shutdown/startup mechanics, not reconnect behavior. Current game ELF SHA256 is `7b4f81abb6e8a68e9e7236cbdc79cb2a2bf0caa6f53bc7f9ddf849a845156174`.


Local ignored logs: `Native-Linux-UnrealPak-Build.log`, `Native-Linux-LooseCook.log`, `Native-Current-LinuxServer-Build.log`, `Native-Builder-Image.log`, `Native-Source-Transfer.log`, `Native-Setup-Dispatch.log`, `Native-UAT-Help.log` under `Artifacts/Unreal/Setup`. Native package/startup are now proven above. Compatible remote clients and meaningful workload measurements remain required; controlled stop/restart passed above.

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

The earlier Windows route passed static validation/native compilation/cook but remains blocked during staging; the native Linux route above subsequently packaged/deployed successfully. `MaxParallelActions` defaults to4, bounded1–16, to limit initial workstation memory pressure. The existing game container occupies UDP7777 on the target host; the new [runtime profile](../ops/steam-server/README.md) therefore defaults to UDP17777 (verified free during preparation). Its first image/Compose deployment is recorded above. The Ubuntu22.04 base image was pulled successfully and its digest is pinned in the Dockerfile.

These are candidate commands until recorded execution establishes their result. `-ValidateOnly` checks paths, source identity and required files without creating directories or invoking UAT. It rejects missing prerequisites and installed engine distributions. It does not verify dependency completeness, compiler functionality or runnable packages.

The build command invokes the selected source engine's `RunUAT.bat BuildCookRun` with `-build -cook -stage -pak -archive -server -noclient -serverplatform=Linux -serverarchitecture=x64 -serverconfig=Development`. Arguments are grounded in the installed 5.8.3 owning `AutomationTool/AutomationUtils/ProjectParams.cs` declarations and `BuildCookRun.Automation.cs`; the matching source checkout must still execute them successfully. No run/deploy option is supplied. UAT writes its normal project/engine intermediates and logs; explicit staging/archive destinations are `Artifacts/Unreal/LinuxServer/Stage` and `Artifacts/Unreal/LinuxServer/Archive` under the repository's ignored artifact root. Archive subdirectory naming is UAT-owned.

The entry point does not download, install, clone, edit environment registration, delete outputs, copy to remote hosts or handle credentials. Reparse-point paths and ambiguous local paths are rejected. Successful UAT completion is package evidence only; E1 separate-server/two-client authority checks and Linux runtime verification remain required.

Native Linux diagnostic Client alternative: ordinary source `ShatteredFrontierClient Linux Development` cross-build is running with the existing v26 SDK, up to3 compile actions, and isolated game revision2a5b634. Log: `Artifacts/Unreal/Setup/Source-LinuxClient-Build.log`. Fresh LinuxClient portable cook and unchanged-receipt native staging are being prepared. This is a headless diagnostic route, not a product Linux-client commitment or successful gameplay evidence. Planned two-client test uses a separate temporary server network namespace without published ports; the existing LAN service and Windrose stay untouched.

Diagnostic tooling update: exporter `--target Server|Client` preserves the default Server route and complete original receipt closures; both-target synthetic tests passed, including wrong cook/receipt rejection. Separate `client-stage` uses offline native loose UAT and rejects existing output roots (including dangling symlinks) before cleanup. The Client recipe fixes UID998, headless flags and isolated endpoint; shell syntax and missing-role/invalid-role/nonisolated-endpoint/extra-argument rejection checks passed. Actual Client ELF dependency/image/runtime validation is still pending. The isolated2a project initially lacked its Editor module; a normal matching Launcher Editor compile passed97.35s, and fresh LinuxClient portable cooking is now running. No receipts/modules were copied or replaced.

Resource correction during LinuxClient preparation: simultaneous3-action C++ compilation and13 shader workers hit UBA's memory threshold (47.4/48.8GB committed); UBA stopped one compile action for retry. The uniquely identified owned cook process tree was stopped, then cooking restarted with process-only `-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=14`, confirmed2 local workers. Compilation resumed. This is a build-resource setting, not gameplay tuning or a security-policy change. Current cook log: `Source-LinuxClient-LooseCook-Bounded.log`; session63026. The failed/terminated attempts do not establish cook success.

The isolated diagnostic launcher passed independent source review after fixing launch-interruption ownership (fresh cidfiles/per-run labels) and late-failure handling (stop, persist, finalized-log/state scan, then COLLECTED). Bash syntax and invalid-image-input rejection passed. It has not run: actual Client dependency closure, image and graceful shutdown remain runtime gates.
