# Playable runtime release

- **Area:** backend
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b

## Goal

Deploy a compatible immutable Linux game/service/content release on the authorized LAN host, with matching client connection/gameplay evidence and rollback; otherwise record the precise external gate.

## State

Exact `ff59b35dd17fef0edd1a7b7f5b9161b867370bc8` game/client/service packages and immutable images are verified; the final operator packet is sealed. One unpublished packaged-game/services smoke passed and cleaned. Protected application still requires interactive operator authentication; the old healthy E1 game on UDP17777 is untouched. Later documentation commits do not change the frozen component identity.

## Evidence

Game image `sha256:3c6bf45ef93866b67be30aa35b8ca4f9c0c84da68f1d00b9b9fbbb7b62350b19` and service image `sha256:8d10c43fc5129028bce13987992efe52109cce514b92557ca52d948c3223fde2` both carry exact full frozen labels. Service107 hashes and game69 payload+2 installed-entrypoint hashes/UID998/ldd passed. Actual packaged local connection passed both NetCL0/Engine44/Game0/checksum1737972550; package/source/content manifests matched.

Private startup verified Silverbrook, six authored groups/21 NPCs, gathering19/stations3 and authenticated services without WorldContent.Tool fallback; zero published ports, readonly UID998, exit143/no OOM/restarts, all owned data/processes removed. PostgreSQL125 normal slots versus pool ceiling96 leaves29; prior unchanged SQL/security evidence is reused. Verified binary stdin build passed conservative peak13.966GB including4GiB margin against16.355GB free; no host context/archive or protected LV write. Final profile SHA256 is `cd31eb7381d4ca4dde38253a3f7ac37a827e66d3d527d7f24a460fc221e1d28e`. Public packet paths/hashes/commands are in the root orchestration report; no application or full remote gameplay acceptance is claimed.

## Blocked on

Privileged operator application. SSH key access works, but `sudo -n -v` fails; a host operator must authenticate interactively in the same root session that runs the sealed block. Then actual two-client remote gameplay/SQL/restart acceptance remains.

## Next step

Root may now request the concrete operator step using the sealed `operator-packet/operator-apply.md` and exact profile hash. After operator apply, verify the new container's image identity and actual two-client possession/NPC/loot/drop/pickup/privacy/SQL/restart behavior before declaring the release accepted. Retain old image/profile and new database for rollback/recovery.

## Rules for this task

Preserve old image, unrelated services/ports, least-privilege database roles and private credentials. No public binds, policy changes, new storage, or conversion of fixed development identities into a player feature. Only this lane may be edited; shared main permits ignored evidence only.

## Outcome

Pending.
