# E1 steam-server runtime

The user selected this Ubuntu/Docker host and authorized allocation, setup and deployment. `tools/Provision-SteamServer.sh` prepares a separate200GiB LV/mount and non-root account; it never formats an existing LV. The other game container, OS LV and second SSD remain separate.

Build context is an immutable release directory containing `package/` (the verified UAT LinuxServer archive), this `Dockerfile`, and `entrypoint.sh`. Build with host `sfserver` UID/GID arguments and `SF_REVISION` set to the full game Git commit. The Dockerfile requires exactly one dedicated-server executable and redirects its Saved directory to the shared writable mount. It fails on an unexpected archive layout rather than guessing. No engine source, credentials or build caches belong in the context. Compose never pulls the game image; application requires its local revision label to match the release tag.

Compose is installed separately under `/srv/shattered-frontier/runtime`; its private `.env` supplies the image tag, actual account IDs and verified host LAN address. Defaults bind loopback. Do not use `0.0.0.0` or router forwarding for this unauthenticated E1 fixture. Non-root execution, read-only image, dropped capabilities, separate writable Saved/home mounts and resource limits apply. The entrypoint follows UAT's code-project bootstrap convention by passing the project name before the map, and sets executable permissions at image build time. The healthcheck observes the world-ready marker; it does not prove tick progress, player capacity or authentication.

| Deployment setting | Default / bounds | Units and meaning |
| --- | --- | --- |
| SF_IMAGE | Required verified local release tag | Versioned container artifact; operator chooses a built release |
| SF_UID / SF_GID | Required actual sfserver IDs; nonzero | Host runtime identity; setup-owned, not a tuning control |
| SF_BIND_ADDRESS | 127.0.0.1; approved host loopback/LAN IPv4 only | Host interface; no public exposure |
| SF_GAME_PORT | 17777; integer1024–65535 | UDP listener and published port; existing game uses7777 |
| SF_CPUS | 4; integer1–4 | CPU quota in logical CPU equivalents for initial E1 |
| SF_MEMORY_GIB | 8; integer2–16 | Container memory ceiling in GiB |

Only host operators may change this deployment profile. Values take effect on container recreation; image/code/content remain release-controlled. Run `python3 validate-profile.py .env` before applying it: this rejects unknown/duplicate fields, unsafe interpolation, invalid resource/port bounds, public/wildcard binds and incorrect runtime IDs. The launcher also validates the port; Docker validates its own fields. These are local development controls, not live GM settings or acceptance of production configuration management. Changing release requires successful package checks and a separate runtime/connection check; retain the previous image for rollback.

After verifying the package/image, `python3 validate-profile.py .env --apply` validates the profile and Compose configuration, then starts only this named project. It clears inherited environment overrides so Compose uses the checked values. Stop it with `docker compose --env-file .env -f compose.yaml stop` from that runtime directory. Public access, durable game data and production backups remain separate gates. See [build/setup evidence](../../docs/e1-linux-server-build.md) for actual state; these files alone are not a successful deployment.

## Native packaging environment

`tools/Prepare-NativeLinuxBuild.sh` prepares a separate `sfbuild` identity and `native-build-5.8.3/` under the verified game LV. It refuses unrelated storage/accounts and active build processes; it does not format or repurpose another filesystem. `NativeBuild.Dockerfile` supplies the matching Ubuntu native tool environment. Build dependencies, caches and source are excluded from the runtime image.

`run-native-job.sh setup` runs Epic's matching dependency/native-SDK setup; `uat-help` compiles and exercises native Linux UAT. Launch these root-owned scripts on steam-server from `/srv/shattered-frontier/runtime/native/`. Jobs run as UID997, limited to2CPU/8GiB with read-only container root, no host Docker socket, no published ports and a single build-directory bind. Launches serialize and check remaining disk budgets plus25GiB headroom. Setup has download access; source identity, logs, image digest and actual outcomes belong in the [build record](../../docs/e1-linux-server-build.md). Stopped containers retain bounded diagnostic logs.

`tools/Export-NativeLinuxInputs.py` exports the original Linux Server/UnrealPak receipts and their complete products/dependencies, project sources/config/content and a portable loose cook. Its new archive and SHA256 manifest preserve paths and reject links, missing/case-mismatched inputs, traversal and Zen-backed cook markers. Run `python tools/Export-NativeLinuxInputs.py --self-test` for bounded synthetic validation; actual export arguments are in the build record. Transfer and verify the resulting archive before extraction as sfbuild into the matching native environment. Export alone is not staging or deployment evidence.
