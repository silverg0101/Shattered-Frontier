# Existing-store LAN runtime update

- **Area:** backend
- **Owner of next step:** agent
- **Worktree:** C:\Projects\SF-lane-e
- **Branch:** codex/runtime-existing-store-update
- **State:** prepared; no live update, image build or deletion authorized yet

## Blocker and bounded exit

The next client/game release needs the existing ff59 database, Saved data and bearer credentials. Exit: document and test an existing-store switch/rollback with immutable matching images and unchanged schema contracts, then establish a measured Docker disk budget from the actual new context. Final source freeze, visual owner approval and live application remain outside this preparation.

## Existing update path

No new runtime command is needed. `playable-runtime.py start` already reloads the existing private secrets, validates image/full-revision labels, renders the same root mounts, runs `playable-verify` on the nonempty database and recreates hosts/game. `prepare` refuses the existing root; `init` is fresh-database-only. The rollout document now specifies guards, stop/start and rollback without either initialization action.

Focused POSIX test `test_existing_start_changes_release_without_reinitializing_or_rotating_secrets` checks a new immutable service profile against existing private files/data, unchanged database configuration and credentials, verification-only startup and existing-root prepare refusal. All4 runtime tests passed under WSL. Existing real PostgreSQL/schema-role/restart evidence remains applicable only while its inputs are unchanged; this test does not claim a real new-image rollout.

## Disk evidence and cleanup proposal — no deletion performed

Read-only host inventory: Docker root `/var/lib/docker` on `/` has11,379,589,120 bytes available. Existing game LV has81,488,478,208 bytes, which does not increase Docker capacity. Previous context3,223,664,640 bytes required `3*context + 4GiB = 13,965,961,216`, currently short2,586,372,096. New MetaHuman package size is not yet known: recompute from its actual verified context before any build.

Protect active game `3c6bf45ef93866b67be30aa35b8ca4f9c0c84da68f1d00b9b9fbbb7b62350b19`, active services `8d10c43fc5129028bce13987992efe52109cce514b92557ca52d948c3223fde2`, old E1 rollback `42b382b6e3d581388be3bc8341a10ba7ff08876e9b0788457f71cb8e53d6f0ce`, PostgreSQL, every referenced image and all unrelated Forge/games/monitoring resources.

Candidate completed ff59 build-input cache only: `8muqtnxlm3y60x3yxp3f05bcd` (HTTP buildkit-session input,22:41:13UTC) and `zx4ymgw66qotsxiofvopj34zn` (`copy /context /`,22:43:18UTC). Both are nonshared/reclaimable, displayed3.224GB each, matching this task's owned ff59 binary-stdin build and72-file context. Proposal: review these exact records for selective removal before the next build; never broad prune. Their displayed size is an estimate; remeasure free bytes after any separately authorized removal. Active image layers are not candidates. Seven superseded service images are unreferenced but share base layers and do not establish sufficient recoverable space; no image removal proposed. The older `int03-0b` image lacks this task's provenance and is excluded.

Ignored evidence: `Artifacts/runtime-update-cache.jsonl`, `Artifacts/runtime-update-unused-sf-images.json`. Frozen `Artifacts/PackagedServer/Matching-ff59b35d-20261025-175956` remains unchanged; its source branch `codex/game-package-ff59b35d@a13248f5` is preserved.

## Next

Review this candidate, freeze the matching release, measure its actual context, resolve the exact disk gate and seal both new/rollback profiles. Only after owner approval apply the documented existing-store update; verify identity/health and resume the mandatory native personal-corpse → Alpha bag → ground drop → rendered Beta/native-F pickup → SQL/privacy → controlled restart acceptance. Current ff59 remains live; do not restart or modify its data during preparation.
