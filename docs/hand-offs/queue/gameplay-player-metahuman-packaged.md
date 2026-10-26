# Restore the intended MetaHuman in packaged gameplay

- **Area:** gameplay
- **Owner of the next step:** root integration/release lane B
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-a (review branch); integration C:\Projects\SF-lane-b

## Goal

The packaged player renders the intended MetaHuman rather than the Unreal mannequin, with the existing appearance and animation path intact.

## State

Owner observed the mannequin in deployed ff59. Root confirmed `PLAYER_METAHUMAN_MISSING`: assembled face Blueprint and retarget omitted from package. Approved existing assemblies are now in a canonical ignored, hash-verified cache; no new art/assembly generation. A client-only CookDir contract/preflight is integrated; source default-Orlando load succeeded in the actual Editor renderer (`PLAYER_METAHUMAN_SPAWNED`). Actual cook and packaged avatar acceptance remain open.

## Evidence

Preflight verifies1181files/1,802,840,646bytes against pinned manifest SHA2569329b4fe9def930604dd546398b0e1719459f529455f7474391c6d9b128fd0b0, all9configured face BPs and retarget/male/female outfits. LaneA Built junction targets canonical cache; cache bytes unchanged. `ClientCookContract.json`/`Get-ClientCookContract.ps1` emit supported client-only UAT CookDir roots. No globalINI/server/runtime changes or UE job in this slice. Parent: `ui-live-client-regressions.md`.

Static checks PASS: all approved hashes/bytes and9+3required packages; missing cache, server target, incomplete census and modified manifest all rejected. Built junctions in lanes A/B/C/D point to the unchanged canonical cache. Local evidence: `Artifacts/PlayerMetaHuman-client-preflight.json` and `Artifacts/PlayerMetaHuman-preflight-rejections.txt`. These do not establish cooked hard-reference closure or visual acceptance.

## Blocked on

Actual cook/dependency closure and packaged avatar review; editor/cook work must use the single Unreal job lease.

## Next step

Use preflight `.UatArguments` for client cook only, verify all9BPs/retarget/outfits/hard closure in client containers and avatar exclusion in server containers. Capture actual intended avatar/animation/outfit and receiver-head attachment for fresh Sol/owner review. Source/cache invariance and matching package identity remain release gates.

## Rules for this task

Reuse the existing approved MetaHuman assets and appearance identity. No new character art or substituted avatar. Capture actual packaged animation, appearance and receiver-head attachment for independent review and owner sign-off.

## Outcome

Open.
