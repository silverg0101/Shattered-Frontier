# Playable runtime release

- **Area:** backend
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b

## Goal

Deploy a compatible immutable Linux game/service/content release on the authorized LAN host, with matching client connection/gameplay evidence and rollback; otherwise record the precise external gate.

## State

Release identity is frozen at `ff59b35dd17fef0edd1a7b7f5b9161b867370bc8`. Exact tracked service export/image and fresh unpublished service smoke passed; game/client packages remain with the package worker. Reviewed runtime/bootstrap code is unchanged from `19fe2914`; protected runtime and the old healthy E1 game on UDP17777 remain untouched. Later documentation commits do not change the frozen component identity.

## Evidence

Frozen service image `sha256:8d10c43fc5129028bce13987992efe52109cce514b92557ca52d948c3223fde2` has exact full revision/UID998; 107 manifest hashes passed. Fresh PostgreSQL18.6/hosts/role/bearer/content/resource smoke passed and cleaned. PostgreSQL has 125 normal slots versus the enforced 96 pool ceiling (29 headroom). Earlier unchanged SQL persistence/security evidence is retained in the root orchestration report; no redundant whole suite was rerun.

The package worker independently matched frozen BackendServices/proto/content manifests. Operator packet has actual frozen service identity/tool hash; game image and final profile hash remain pending. Exact normal-home game staging path is steam-owned and write-probed; home/Docker share about 16.38GB free, while the protected game LV has about 81.56GB. Use actual Linux size plus conservative build/cache peak and margin before transfer; no game-LV write or bypass is authorized. All earlier pre-freeze/unverified images are nonfinal. Remote playable/production acceptance is not established.

## Blocked on

Verified matching LinuxServer/Win64Client packages and actual game image; final profile/hash; privileged operator application. SSH key access works, but `sudo -n -v` fails; a host operator must authenticate interactively in the session that runs the reviewed application block.

## Next step

Receive the package worker's exact Linux bytes/manifests, verify disk peak/headroom and matching frozen inputs, build/check the actual game image in authorized normal staging or verified stdin context, then finalize the profile/operator hashes. Only then may root request the reviewed privileged operator step; remote two-client gameplay/SQL/restart acceptance still follows.

## Rules for this task

Preserve old image, unrelated services/ports, least-privilege database roles and private credentials. No public binds, policy changes, new storage, or conversion of fixed development identities into a player feature. Only this lane may be edited; shared main permits ignored evidence only.

## Outcome

Pending.
