# Silverbrook LAN development release candidate

This is the bounded replacement for the old E1 image, not production authentication or a successful deployment. Preserve `/srv/shattered-frontier/runtime` and the old image until the replacement passes. Ordinary `steam` can build immutable images and stage files under its home; the protected runtime and its private profile require an authorized operator. `sudo -n` currently refuses access. Do not use Docker privileges to bypass that gate.

## Release closure

Freeze one full Git revision after NPC/loot integration. Build LinuxServer and matching Win64 Client from the pinned source UE 5.8.3 route in `docs/e1-linux-server-build.md`; retain both receipts, engine revision, content hashes and package hashes. A Launcher editor connection does not establish source-client compatibility. The Linux package must include the server-only gRPC module, runtime JSON and cooked Silverbrook/NPC/item assets. Preserve actual network-version checks.

`pwsh tools/Build-PlayableServiceRelease.ps1 -OutputDirectory <new-absolute-directory-outside-checkout>` exports both portable managed hosts, the owner-only migration executable, matching release content, four base SQL files, a revision record and SHA256 manifest. It builds/copies exclusively from a tracked Git archive of the frozen revision, including backend SDK/locks, proto and any root SDK/MSBuild/NuGet inputs; ignored checkout files cannot enter it. The source snapshot defaults to `<output>.source`. Checkout/output/snapshot must be disjoint, with new output/snapshot directories and no reparse ancestors. It refuses dirty source and uses locked dependencies with SDK 10.0.402. Run it at the same frozen revision as the game. Build on the normal Docker account with `docker build --build-arg SF_REVISION=<full-revision> -t shattered-frontier-services:<full-revision> <verified-context>`; keep its immutable image ID. The pinned official ASP.NET runtime is 10.0.13. This proves artifact construction only. All candidates built before the tracked-snapshot fix are superseded and must not be used as final releases.

## Required runtime candidate before operator application

The old strict profile accepts only the old game settings. Do not modify it. The runnable `Runtime/playable-runtime.py` generates a distinct `shattered-frontier-playable` project under `/srv/shattered-frontier/playable`; review its source/bootstrap and final public profile before operator application. It writes generated private Compose and credentials as operator-owned 0600 files under a 0700 directory. Required constraints:

- One private network namespace shared by PostgreSQL, Gameplay.Host, InventoryLedger.Host and game. Their existing loopback-only database/HTTP2 listeners remain private; publish only the already approved `192.168.50.133:17777/udp`. No host networking, public HTTP listener or database port publication.
- Persistent PostgreSQL data under the existing game volume, separate from old Saved data. PostgreSQL 18.6 is pinned by digest in the generator; apply never pulls. The shipped 128-connection ceiling covers 96 pool connections (ledger 12 plus gameplay 24/12/24/16/8), administration and PostgreSQL reserved slots.
- One database containing `inventory`, `character_state`, `housing`, `exchange`, and `group`; repair requires inventory and exchange in the same database. Distinct NOLOGIN schema owners have database CREATE for owner migrations; LOGIN runtime roles get DML only. Owner credentials and migration grants never reach runtime containers. Runtime tables/sequences get only the grants required by their store; no schema/database CREATE, role membership, superuser, createdb, createrole or bypassrls. Never reuse a development test cluster for player data.
- `SchemaMigration.Tool playable-init` accepts a **new empty database only**, installs four exported bases and all five owner migrations, then grants runtime DML. Version/checksum/build/history values remain owner-only; the constrained `singleton=true` key permits column UPDATE only where existing FOR SHARE admission locks require it. Inventory gets repair's exact Exchange grants; CharacterState gets SELECT on three recovery tables in Housing/Exchange. `playable-verify` reconnects as all five runtime roles and checks versions, row-lock admission, DDL/version-write refusal, owner isolation and cross-schema fencing. Existing data requires reviewed backup/migration acceptance; init does not overwrite it. A failed partial initialization requires operator inspection, not a destructive automatic retry. Fsync, synchronous_commit and full_page_writes remain enabled.
- Private, stable per-host bearer credentials and runtime connection strings in operator-owned files, mode 0600. Gameplay requires `GAMEPLAY_HOST_PORT`, four `GAMEPLAY_*_CONNECTION` values, optional Group connection (required for party gameplay), `GAMEPLAY_CONTENT_ROOT=/opt/services/content`, `GAMEPLAY_ZONE_ID=zone/greenlands_silverbrook` and `SF_GAMEPLAY_TOKEN`; ledger requires its port, connection and `SF_INVENTORY_LEDGER_TOKEN`. Keep bearer values stable across restart because crafting derives its replay seed from them. Never log them or place them in image/client/source/command arguments.
- The separately selected `sf-playable-entrypoint` launches `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook`, enables ledger/services and restore-on-connect, and explicitly selects the existing guarded `-SFDevIdentity` admission. Fixed development character identities remain test identities, not account/session authentication. Starter-kit grants and fixture faults remain absent. The original E1 entrypoint is preserved for rollback.
- Preserve non-root runtime execution, read-only roots, bounded tmpfs/logs/resources, dropped capabilities, no-new-privileges and owned writable mounts. Start database, apply owner migrations, start and probe both hosts, then start game. Startup markers alone are insufficient for gameplay acceptance.

## Operator final step and rollback

Fill immutable matching image IDs, reviewed profile paths and package/hash evidence before requesting the operator step. `game_image` and `services_image` are local `sha256:` image IDs with the same full revision label; the game also requires the pinned source-engine label and playable-entrypoint contract. The final public profile is:

```json
{"mode":"lan-development","project":"shattered-frontier-playable","revision":"FULL_FROZEN_REVISION","services_image":"sha256:SERVICE_IMAGE_ID","game_image":"sha256:GAME_IMAGE_ID","root":"/srv/shattered-frontier/playable","bind_address":"192.168.50.133"}
```

All resource controls below are integer host-operator settings, effective on recreation, safe fallback to the shipped default. They are local development controls, not a production GM/config-service surface. Aggregate CPU may not exceed 8 equivalents and memory may not exceed 24 GiB; formula/topology/identity are versioned code.

| Setting | Default | Bounds / units |
| --- | --- | --- |
| `game_cpus`, `game_memory_gib` | 4, 8 | 1–4 CPU equivalents; 2–16 GiB |
| `service_cpus`, `service_memory_gib` (each host) | 1, 1 | 1–2 CPU equivalents; 1–2 GiB |
| `database_cpus`, `database_memory_gib` | 1, 2 | 1–2 CPU equivalents; 1–4 GiB |
| `database_connections` | 128 | 108–160 PostgreSQL connections, minimum includes pool/admin/reserved headroom |

Operator access is a real host-privilege gate: `steam` belongs to the sudo group, but its SSH key does not grant sudo authentication; `sudo -n -v` fails and the old runtime profile is private. When the final matching artifacts/profile are ready, an authorized operator opens an interactive SSH terminal as `steam`, runs `sudo -i`, and enters the account's sudo credential locally at that prompt (or uses an already privileged host session). Run the complete reviewed command block in that same root shell; a sudo timestamp from a different terminal is not sufficient. The operator's sudo password never belongs in chat, files, environment variables or command arguments; no sudoers/security-policy change is required.

After source/security review and matching-package verification, the privileged operator verifies the release manifest, installs the immutable reviewed runtime tool/profile into a root-owned release directory, verifies those installed copies against the final published hashes, and runs these exact actions with their final absolute paths:

```sh
python3 RELEASE/Runtime/playable-runtime.py PROFILE.json prepare
python3 RELEASE/Runtime/playable-runtime.py PROFILE.json check
docker stop shattered-frontier-game-1
python3 RELEASE/Runtime/playable-runtime.py PROFILE.json init
python3 RELEASE/Runtime/playable-runtime.py PROFILE.json verify
```

`prepare` is new-directory-only and creates stable private credentials plus correctly owned data/Saved/home children. `check` verifies image provenance and quiet Compose syntax without starting containers. `init` starts PostgreSQL, bootstraps once, starts both hosts, requires authenticated and denied-bearer probes, then starts the game. Later `start` uses verification instead of bootstrap. No command bypasses protected host permissions. Do not change Forge, other games, router/firewall, disks or security policy.

An unpublished service-only rehearsal uses `mode=services-check`, project `sf-playable-check-<12 hex>`, no game image, and a new root under the verified canonical home `/srv/steam/steamhome/sf-release-candidates/` (`/home/steam` itself is a symlink, so it is refused as a runtime root). It uses a disposable named PostgreSQL volume, publishes no ports, and isolates its namespace from external networking. The same `prepare`, `check`, `init`, `verify`, `stop`, `start` actions prove host/role/restart contracts without touching the protected runtime. `cleanup` can remove only this check project's containers and volume; LAN data never has that action.

Acceptance requires both service authenticated probes, all expected store version gates and least-privilege checks; Silverbrook NPC/gathering startup; two distinct matching-client possessions; an actual server-authoritative corpse open/take and deliberate ground drop/pickup with observer privacy; SQL receipt/item read-back; and restart/reconnect retaining the same state. Record actual observations and failures. Reuse focused local evidence only where its inputs match; it does not replace remote acceptance.

Rollback runs `python3 RELEASE/Runtime/playable-runtime.py PROFILE.json stop` without deleting its database, then the privileged operator restores the untouched old runtime with `cd /srv/shattered-frontier/runtime && python3 validate-profile.py .env --apply`. The validator rechecks the old immutable image and existing approved LAN profile. This restores the old E1 fixture; it does not migrate new durable player data backward. Keep the candidate data for diagnosis and recovery.
